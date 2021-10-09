/*
    socklib - simple socket library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once

#include "../common/platform.h"
#include "../common/cancel.h"
#include <enumext.h>
#include <memory>

namespace socklib {
    namespace v2 {
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
            nodel_ctx = 4,
            del_flag = 5
        };

        enum class DelFlag {
            none,
            not_del_ssl = 0x1,
            not_del_ctx = 0x2,
        };

        DEFINE_ENUMOP(DelFlag)

        struct IResetContext {
            virtual std::uintptr_t context(size_t index) = 0;
        };

        enum class ConnType {
            none,
            tcp_socket,
            tcp_over_ssl,
            file,
            websocket,
            udp_socket,
            udp_over_ssl,
        };

        enum class ConnStatus {
            none = 0,
            has_fd = 0x1,
            secure = 0x2,
            streaming = 0x4,
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
                } net{0};
            };

            constexpr ~ConnStat() {}
        };

        //IConn Interface - all connection base
        struct IConn {
            virtual bool write(IWriteContext& towrite, CancelContext* cancel = nullptr) = 0;
            virtual bool write(const char* data, size_t size, CancelContext* cancel = nullptr) {
                WriteContext w;
                w.ptr = data;
                w.bufsize = size;
                return write(w, cancel);
            }
            virtual bool write(const char* str, CancelContext* cancel = nullptr) {
                if (!str) {
                    return true;
                }
                return write(str, ::strlen(str), cancel);
            }
            virtual bool read(IReadContext& toread, CancelContext* cancel = nullptr) = 0;
            virtual void close(CancelContext* cancel = nullptr) = 0;
            virtual bool reset(IResetContext& set) = 0;
            virtual bool stat(ConnStat&) const = 0;
            virtual ~IConn() {}
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

        constexpr auto invalid_socket = (SOCKET)-1;
        constexpr size_t intmaximum = (std::uint32_t(~0) >> 1);

        struct SocketReset : IResetContext {
            SOCKET sock = invalid_socket;
            ::SSL* ssl = nullptr;
            ::SSL_CTX* ctx = nullptr;
            bool nodelctx = false;
            ::addrinfo* addr = nullptr;
            DelFlag delflag = DelFlag::none;
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
                    case 5:
                        return (std::uintptr_t)delflag;
                    default:
                        return 0;
                }
            }
        };

        //IAppConn - application layer connection interface
        template <class BaseConn>
        struct IAppConn {
            virtual std::shared_ptr<BaseConn>& borrow() = 0;
            virtual std::shared_ptr<BaseConn> hijack() = 0;
            virtual bool enable_conn() const = 0;
            virtual void close(CancelContext* cancel) = 0;
        };

        using INetAppConn = IAppConn<InetConn>;

    }  // namespace v2
}  // namespace socklib
