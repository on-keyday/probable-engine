#pragma once

#include "../common/platform.h"
#include "../common/cancel.h"
#include <enumext.h>

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
            bool cancel_when_block = false;
            void (*errhandler)(void* ctx, std::int64_t ecode, CancelContext* cancel, const char* msg);
            void* usercontext = nullptr;

            const char* bufptr() override {
                return ptr;
            }

            std::uint64_t flags() override {
                return cancel_when_block ? 1 : 0;
            }

            size_t size() override {
                return bufsize;
            }

            bool done() override {
                return true;
            }

            void on_error(std::int64_t ecode, CancelContext* cancel, const char* msg) override {
                if (errhandler) {
                    errhandler(usercontext, ecode, cancel, msg);
                }
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
            bool cancel_when_block = false;
            void append(const char* read, size_t size) override {
                buf.append(read, size);
            }

            std::uint64_t flags() override {
                return cancel_when_block ? 1 : 0;
            }
        };

        enum class ResetIndex {
            socket = 0,
            addrinfo = 1,
            ssl = 2,
            ssl_ctx = 3,
            nodel_ctx = 4
        };

        struct IResetContext {
            virtual std::uintptr_t context(size_t index) = 0;
        };

        enum class ConnType {
            none,
            tcp_socket,
            tcp_over_ssl,
            file,
        };

        enum class ConnStatus {
            none = 0,
            has_fd = 0x1,
            secure = 0x2,
        };

        DEFINE_ENUMOP(ConnStatus)

        struct ConnStat {
            ConnType type = ConnType::none;
            ConnStatus status = ConnStatus::none;
            union {
                struct {
                    ::SSL* ssl = nullptr;
                    ::SSL_CTX* ssl_ctx = nullptr;
                    ::addrinfo* addrinfo = nullptr;
                } net;
            };
        };

        //IConn Interface - all connection base
        struct IConn {
            virtual bool write(IWriteContext& towrite, CancelContext* cancel = nullptr) = 0;
            virtual bool read(IReadContext& toread, CancelContext* cancel = nullptr) = 0;
            virtual void close(CancelContext* cancel = nullptr) = 0;
            virtual bool reset(IResetContext& set) = 0;
            virtual bool stat(ConnStat&) const = 0;
            virtual ~IConn() = 0;
        };

        //InetConn - base of all internet connection
        struct InetConn : IConn {
           protected:
            ::addrinfo* info = nullptr;

           public:
            static bool copy_addrinfo(::addrinfo*& to, const ::addrinfo* from) {
                if (to || !from) return false;
                to = new addrinfo(*from);
                to->ai_next = nullptr;
                to->ai_canonname = nullptr;
                to->ai_addr = nullptr;
                if (from->ai_canonname) {
                    size_t sz = strlen(to->ai_canonname) + 1;
                    to->ai_canonname = new char[sz]();
                    strcpy_s(to->ai_canonname, sz, from->ai_canonname);
                }
                if (from->ai_addr) {
                    to->ai_addr = (sockaddr*)new char[from->ai_addrlen]();
                    memcpy_s(to->ai_addr, to->ai_addrlen, from->ai_addr, from->ai_addrlen);
                }
                return true;
            }

            static bool cmp_addrinfo(const ::addrinfo* left, const ::addrinfo* right) {
                if (left == right) return true;
                if (!left || !right) return false;
                if (left->ai_addrlen != right->ai_addrlen || left->ai_family != right->ai_family ||
                    left->ai_flags != right->ai_flags ||
                    left->ai_protocol != right->ai_protocol || left->ai_socktype != right->ai_socktype) {
                    return false;
                }
                if (memcmp(left->ai_addr, right->ai_addr, right->ai_addrlen) != 0) {
                    return false;
                }
                if (left->ai_canonname != right->ai_canonname && strcmp(left->ai_canonname, right->ai_canonname) != 0) {
                    return false;
                }
                return true;
            }

            static void del_addrinfo(addrinfo*& del) {
                if (!del) return;
                delete[](char*) del->ai_addr;
                delete[] del->ai_canonname;
                delete del;
                del = nullptr;
            }

            InetConn(::addrinfo* p) {
                copy_addrinfo(info, p);
            }

            virtual bool ipaddress(IReadContext& toread) const {
                if (!info) return false;
                char buf[75] = {0};
                if (info->ai_family == AF_INET) {
                    sockaddr_in* addr4 = (sockaddr_in*)info->ai_addr;
                    inet_ntop(info->ai_family, &addr4->sin_addr, buf, 75);
                }
                else if (info->ai_family == AF_INET6) {
                    sockaddr_in6* addr6 = (sockaddr_in6*)info->ai_addr;
                    inet_ntop(info->ai_family, &addr6->sin6_addr, buf, 75);
                }
                int offset = 0;
                if (auto tmp = std::string_view(buf); tmp.find(".") != ~0 && tmp.find("::ffff:") == 0) {
                    offset = 7;
                }
                toread.append(buf + offset, ::strlen(buf) - offset);
                return true;
            }

            virtual bool reset(IResetContext& set) override {
                del_addrinfo(info);
                return copy_addrinfo(info, (::addrinfo*)set.context((size_t)ResetIndex::addrinfo));
            }

            virtual bool stat(ConnStat& st) const override {
                st.net.addrinfo = info;
                return true;
            }
        };

        constexpr auto invalid_socket = -1;
        constexpr size_t intmaximum = (std::uint32_t(~0) >> 1);

        struct SocketReset : IResetContext {
            int sock = invalid_socket;
            ::SSL* ssl = nullptr;
            ::SSL_CTX* ctx = nullptr;
            bool nodelctx = false;
            ::addrinfo* addr = nullptr;
            virtual std::uintptr_t context(size_t index) override {
                switch (index) {
                    case 0:
                        return std::uint32_t(sock);
                    case 1:
                        return (std::uintptr_t)addr;
                    case 2:
                        return (std::uintptr_t)ssl;
                    case 3:
                        return (std::uintptr_t)ctx;
                    case 4:
                        return nodelctx;
                    default:
                        return 0;
                }
            }
        };

        //StreamConn - connection for tcp socket (SOCK_STREAM)
        struct StreamConn : InetConn {
           protected:
            int sock = invalid_socket;

           public:
            StreamConn(int s, ::addrinfo* p)
                : sock(s), InetConn(p) {}

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
                sock = (int)ctx.context((size_t)ResetIndex::socket);
                InetConn::reset(ctx);
                return true;
            }

            virtual bool stat(ConnStat& st) const override {
                st.type = ConnType::tcp_socket;
                st.status = (sock == invalid_socket ? ConnStatus::none : ConnStatus::has_fd);
                InetConn::stat(st);
                return true;
            }

            virtual ~StreamConn() {
                close();
            }
        };

        //SecureStreamConn - secure connection for tcp socket (SOCK_STREAM)
        struct SecureStreamConn : StreamConn {
           protected:
            ::SSL* ssl = nullptr;
            ::SSL_CTX* ctx = nullptr;
            bool noshutdown = false;
            bool nodelctx = false;

           public:
            SecureStreamConn(::SSL* issl, ::SSL_CTX* ictx, int sock, ::addrinfo* info, bool nodelctx = false)
                : ssl(issl), ctx(ictx), nodelctx(nodelctx), StreamConn(sock, info) {}

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
                            std::string errstr = "ssl";
                            if (ctx.reason() == CancelReason::ssl_error || ctx.reason() == CancelReason::os_error) {
                                errstr += ":";
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
                            toread.on_error(ctx.err, &ctx, errstr.c_str());
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
                TimeoutContext timeout(10);
                close(&timeout);
                ssl = (::SSL*)set.context((size_t)ResetIndex::ssl);
                ctx = (::SSL_CTX*)set.context((size_t)ResetIndex::ssl_ctx);
                nodelctx = (bool)set.context((size_t)ResetIndex::nodel_ctx);
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

            virtual bool stat(ConnStat& st) const override {
                st.type = ConnType::tcp_over_ssl;
                st.status = (sock == invalid_socket ? ConnStatus::none : ConnStatus::has_fd);
                st.status != (ssl ? ConnStatus::secure : ConnStatus::none);
                st.net.ssl = ssl;
                st.net.ssl_ctx = ctx;
                InetConn::stat(st);
                return true;
            }

            virtual ~SecureStreamConn() {
                TimeoutContext timeout(10);
                close(&timeout);
                if (ctx && !nodelctx) {
                    ::SSL_CTX_free(ctx);
                }
                ctx = nullptr;
            }
        };

    }  // namespace v2
}  // namespace socklib
