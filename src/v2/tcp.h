#pragma once

#include "iconn.h"
#include <reader.h>
#include <memory>

namespace socklib {
    namespace v2 {

        enum class TCPError {
            resolve_address,
            register_cacert,
            connect,
            canceled,
        };

        template <class String>
        struct TCPOpenContext {
            using string_t = String;
            string_t host;
            string_t service;
            std::uint16_t port = 0;
            bool secure = false;
            string_t cacert;
            int ip_version = 0;
            TCPError err;
        };

        template <class Conn, class String>
        struct TCP {
            static bool resolve_detail(const char* host, const char* service, int ipver, ::addringo*& info){} {
                ::addrinfo hint = {0};
                hint.ai_socktype = SOCK_STREAM;
                switch (ipver) {
                    case 4:
                        hint.ai_family = AF_INET;
                        break;
                    case 6:
                        hint.ai_family = AF_INET6;
                        break;
                    default:
                        hint.ai_family = AF_UNSPEC;
                        break;
                }
                return ::getaddrinfo(ctx.host, ctx.service, &hint, &info) != 0;
            }

            template <class C>
            static bool resolve(TCPOpenContext<C*>& ctx, ::addringo*& info) {
                return resolve_detail(ctx.host, ctx.service, ctx.ip_version, info);
            }

            template <class Str>
            static bool resolve(TCPOpenContext<Str>& ctx) {
                return resolve_detail(ctx.host.c_str(), ctx.service.c_str(), ctx.ip_version, info);
            }

            static bool connect_loop(::addrinfo* info, int& sock, TCPOpenContext<String>& ctx, CancelContext* cancel) {
                OsErrorContext canceler(cancel);
                auto tmp = ::socket(p->ai_family, p->ai_socktype, p->ai_protocol);
                if (tmp < 0) continue;
                ::ioctlsocket(tmp, FIONBIO, &flag);
                auto res = ::connect(tmp, info->ai_addr, info->ai_addrlen);
                if (res == 0) {
                    sock = tmp;
                    flag = 0;
                    ::ioctlsocket(tmp, FIONBIO, &flag);
                    return true;
                }
                ::timeval timeout = {0};
                ::fd_set baseset = {0}, sucset = {0}, errset = {0};
                FD_ZERO(&baseset);
                FD_SET(tmp, &baseset);
                while (true) {
                    memcpy(&sucset, &baseset, sizeof(::fd_set));
                    memcpy(&errset, &baseset, sizeof(::fd_set));
                    timeout.tv_sec = 0;
                    timeout.tv_usec = 1;
                    res = ::select(tmp + 1, nullptr, &sucset, &errset, &timeout);
                    if (res < 0) {
                        ::closesocket(tmp);
                        return false;
                    }
                    if (FD_ISSET(tmp, &errset)) {
                        ::closesocket(tmp);
                        ctx.err = TCPError::connect;
                        return false;
                    }
                    if (FD_ISSET(tmp, &sucset)) {
                        sock = tmp;
                        selected = p;
                        flag = 0;
                        ::ioctlsocket(tmp, FIONBIO, &flag);
                        return true;
                    }
                    if (canceler.on_cancel()) {
                        ::closesocket(tmp);
                        ctx.err = TCPError::canceled;
                        return false;
                    }
                }
            }

            static bool connect(std::shared_ptr<Conn>& ret, TCPOpenContext<String>& ctx, CancelContext* cancel, ::addrinfo*& info) {
                std::uint16_t port = commonlib2::translate_byte_net_and_host<std::uint16_t>(&ctx.port);
                for (auto p = info; p; p = p->ai_next) {
                    sockaddr_in* addr = (sockaddr_in*)p->ai_addr;
                    if (port) {
                        addr->sin_port = port;
                    }
                    int sock = 0;
                    if (connect_loop(p, sock, ctx, cancel)) {
                    }
                }
            }

            static std::shared_ptr<InetConn> open(TCPOpenContext<String>& ctx, CancelContext* cancel = nullptr) {
                std::shared_ptr<Conn> ret;
                ::addrinfo* info;
                if (!resolve(ctx, info)) {
                    ctx.err = TCPError::resolve_address;
                    return nullptr;
                }
            }
        };
    }  // namespace v2
}  // namespace socklib