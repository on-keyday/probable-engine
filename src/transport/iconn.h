#pragma once

#include "platform.h"
#include "cancel.h"

namespace socklib {
    inline namespace v2 {
        struct IWriteContext {
            virtual const char* bufptr() = 0;
            virtual size_t size() = 0;
            virtual std::uint64_t flags() {
                return 0;
            }
            virtual bool done() = 0;
            virtual void on_error(std::int64_t, CancelContext*, const char* msg) {}
        };

        struct WriteContext : IWriteContext {
            const char* ptr = 0;
            size_t bufsize = 0;

            const char* bufptr() override {
                return ptr;
            }

            size_t size() override {
                return bufsize;
            }

            bool done() override {
                return true;
            }
        };

        struct IReadContext {
            virtual void append(const char* read, size_t size) = 0;
            virtual std::uint64_t flags() {
                return 0;
            }
            virtual void on_error(std::int64_t, CancelContext*, const char* msg) {}
            virtual bool require() {
                return false;
            }
        };

        template <class Buf>
        struct ReadContext : IReadContext {
            using buffer_t = Buf;
            buffer_t buf;
            void append(const char* read, size_t size) override {
                buf.append(read, size);
            }
        };

        struct IResetContext {
            virtual std::uintptr_t context(size_t index) = 0;
        };

        //IConn Interface
        struct IConn {
            virtual bool write(IWriteContext& towrite, CancelContext* cancel = nullptr) = 0;
            virtual bool read(IReadContext& toread, CancelContext* cancel = nullptr) = 0;
            virtual void close(CancelContext* cancel = nullptr) = 0;
            virtual bool reset(IResetContext& set) = 0;
            virtual ~IConn() = 0;
        };

        constexpr auto invalid_socket = -1;
        constexpr size_t intmaximum = (std::uint32_t(~0) >> 1);

        struct SocketReset : IResetContext {
            int sock = invalid_socket;
            ::SSL* ssl = nullptr;
            ::SSL_CTX* ctx = nullptr;
            bool nodelctx = false;
            virtual std::uintptr_t context(size_t index) override {
                switch (index) {
                    case 0:
                        return std::uint32_t(sock);
                    case 1:
                        return (std::uintptr_t)ssl;
                    case 2:
                        return (std::uintptr_t)ctx;
                    case 3:
                        return nodelctx;
                    default:
                        return 0;
                }
            }
        };

        struct StreamConn : IConn {
           protected:
            int sock = invalid_socket;

           public:
            virtual bool write(IWriteContext& towrite, CancelContext* cancel = nullptr) override {
                //TODO:replace to OSErrorContext
                OsErrorContext ctx((bool)towrite.flags(), cancel);
                while (true) {
                    auto ptr = towrite.bufptr();
                    auto size = towrite.size();
                    if (!ptr || !size) {
                        break;
                    }
                    size_t offset = 0;
                    while (size) {
                        auto res = 0;
                        if (size <= intmaximum) {
                            res = ::send(sock, ptr + offset, (int)size, 0);
                        }
                        else {
                            res = ::send(sock, ptr + offset, (int)intmaximum, 0);
                        }
                        if (res < 0) {
                            if (ctx.on_cancel()) {
                                towrite.on_error(ctx.err, &ctx, "");
                                return false;
                            }
                            continue;
                        }
                        if (size > intmaximum) {
                            size -= intmaximum;
                        }
                        else {
                            break;
                        }
                        offset += intmaximum;
                    }
                    if (towrite.done()) break;
                }
                return true;
            }

            virtual bool read(IReadContext& toread, CancelContext* cancel = nullptr) override {
                OsErrorContext ctx((bool)toread.flags(), cancel);
                while (true) {
                    char buf[1024] = {0};
                    int res = ::recv(sock, buf, 1024, 0);
                    if (res < 0) {
                        if (ctx.on_cancel()) {
                            toread.on_error(ctx.err, &ctx, "");
                            return false;
                        }
                        continue;
                    }
                    toread.append(buf, (size_t)res);
                    if (res < 1024) {
                        if (toread.require()) {
                            continue;
                        }
                        break;
                    }
                }
                return true;
            }

            virtual void close(CancelContext* cancel = nullptr) override {
                if (sock == invalid_socket) return;
                ::shutdown(sock, SD_BOTH);
                ::closesocket(sock);
                sock = invalid_socket;
            }

            virtual bool reset(IResetContext& ctx) override {
                close();
                sock = (int)ctx.context(0);
                return true;
            }

            virtual ~StreamConn() {
                close();
            }
        };

        struct SecureStreamConn : StreamConn {
           protected:
            ::SSL* ssl = nullptr;
            ::SSL_CTX* ctx = nullptr;
            bool noshutdown = false;
            bool nodelctx = false;

           public:
            virtual bool write(IWriteContext& towrite, CancelContext* cancel = nullptr) override {
                if (!ssl) StreamConn::write(towrite, cancel);
                SSLErrorContext ctx(ssl, cancel, (bool)towrite.flags());
                while (true) {
                    auto ptr = towrite.bufptr();
                    auto size = towrite.size();
                    size_t wrt = 0;
                    if (!ptr || !size) {
                        break;
                    }
                    while (!SSL_write_ex(ssl, ptr, size, &wrt)) {
                        if (ctx.on_cancel()) {
                            if (ctx.reason() == CancelReason::ssl_error || ctx.reason() == CancelReason::os_error) {
                                noshutdown = true;
                            }
                            towrite.on_error(ctx.err, &ctx, "ssl");
                            return false;
                        }
                    }
                    if (towrite.done()) {
                        break;
                    }
                }
                return true;
            }

            virtual bool read(IReadContext& toread, CancelContext* cancel = nullptr) override {
                if (!ssl) return StreamConn::read(toread, cancel);
                SSLErrorContext ctx(ssl, cancel, (bool)toread.flags());
                while (true) {
                    size_t red = 0;
                    char data[1024] = {0};
                    while (!SSL_read_ex(ssl, data, 1024, &red)) {
                        if (ctx.on_cancel()) {
                            if (ctx.reason() == CancelReason::ssl_error || ctx.reason() == CancelReason::os_error) {
                                std::string errstr;
                                ERR_print_errors_cb(
                                    [](const char* str, size_t len, void* u) -> int {
                                        (*(std::string*)u) += str;
                                        return 1;
                                    },
                                    (void*)&errstr);
                                if (ctx.sslerr == 1 && errstr.find("application data after close notify") != ~0) {
                                    continue;
                                }
                            }
                            noshutdown = true;
                            toread.on_error(ctx.err, &ctx, "ssl");
                            return false;
                        }
                    }
                    toread.append(data, red);
                    if (red < 1024) {
                        if (toread.require()) {
                            continue;
                        }
                        break;
                    }
                }
                return true;
            }

            virtual bool reset(IResetContext& set) override {
                close();
                ssl = (::SSL*)set.context(1);
                ctx = (::SSL_CTX*)set.context(2);
                nodelctx = (bool)set.context(3);
                return true;
            }

            virtual void close(CancelContext* cancel = nullptr) {
                if (ssl) {
                    if (!noshutdown) {
                        SSLErrorContext ctx(ssl, cancel);
                        while (true) {
                            auto res = SSL_shutdown(ssl);
                            if (res < 0) {
                                if (!ctx.on_cancel()) continue;
                                break;
                            }
                            else if (res == 0) {
                                continue;
                            }
                            break;
                        }
                    }
                    ::SSL_free(ssl);
                    ssl = nullptr;
                }
                StreamConn::close(cancel);
            }

            virtual ~SecureStreamConn() {
                TimeoutContext timeout(10);
                close(&timeout);
                if (ctx && nodelctx) {
                    ::SSL_CTX_free(ctx);
                }
                ctx = nullptr;
            }
        };

    }  // namespace v2
}  // namespace socklib