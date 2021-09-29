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
            virtual void on_error(std::int64_t, CancelContext*) {}
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
            virtual void on_error(std::int64_t, CancelContext*) {}
        };

        template <class Buf>
        struct ReadContext : IReadContext {
            using buffer_t = Buf;
            buffer_t buf;
            void append(const char* read, size_t size) override {
                buf.append(read, size);
            }
        };

        //IConn Interface
        struct IConn {
            virtual bool write(IWriteContext& towrite, CancelContext* cancel = nullptr) = 0;
            virtual bool read(IReadContext& toread, CancelContext* cancel = nullptr) = 0;
            virtual bool close() = 0;
            virtual ~IConn() = 0;
        };

        constexpr auto invalid_socket = -1;
        constexpr size_t intmaximum = (std::uint32_t(~0) >> 1);

        struct StreamConn : IConn {
            int sock = invalid_socket;
            virtual bool write(IWriteContext& towrite, CancelContext* cancel = nullptr) override {
                //TODO:replace to OSErrorContext
                OsErrorContext ctx((bool)towrite.flags(), cancel);
                while (true) {
                    auto ptr = towrite.bufptr();
                    auto size = towrite.size();
                    if (!ptr || !size) break;
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
                                towrite.on_error(ctx.err, &ctx);
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
                            toread.on_error(ctx.err, &ctx);
                            return false;
                        }
                        continue;
                    }
                    toread.append(buf, (size_t)res);
                    if (res < 1024) {
                        break;
                    }
                }
                return true;
            }

            virtual bool close() override {
                if (sock == invalid_socket) return true;
                ::shutdown(sock, SD_BOTH);
                ::closesocket(sock);
                sock = invalid_socket;
                return true;
            }

            virtual ~StreamConn() {
                close();
            }
        };

    }  // namespace v2
}  // namespace socklib