/*
    socklib - simple socket library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once

#include "iconn.h"
#include <reader.h>
#include <callback_invoker.h>
#include <memory>

namespace socklib {
    namespace v2 {

        enum class TCPError : std::uint8_t {
            none,
            initialize_network,
            resolve_address,
            register_cacert,
            connect,
            no_address_to_connect,
            canceled,
            memory,
            has_ssl_but_ctx,
            register_host_verify,
            ssl_connect,
            cert_not_found,
            cert_verify_failed,
            ssl_io,
            tcp_io,
            not_reopened,
            bind,
            listen,
            no_socket,
            wait_accept,
        };

        template <class String>
        struct TCPOpenContext {
            using string_t = String;
            string_t host;
            string_t service;
            std::uint16_t port = 0;
            ConnStat stat;  //to set stat
            string_t cacert;
            int ip_version = 0;
            TCPError err = TCPError::none;
            const char* alpnstr = nullptr;
            size_t len = 0;
            bool forceopen = false;
            bool non_block = true;
        };

        template <class String>
        struct TCPAcceptContext {
            using string_t = String;
            int acsock = invalid_socket;
            int ip_version = 0;
            std::uint16_t port = 0;
            string_t service;
            string_t servercert;
            string_t rootcert;
            ConnStat stat;  //to set stat
            bool non_block = true;
            ::SSL* ssl = nullptr;
            ::SSL_CTX* sslctx = nullptr;
            TCPError err = TCPError::none;
            ::addrinfo* info = nullptr;
            bool reuse_addr = true;
        };

        struct NetWorkInit {
            static NetWorkInit& instance() {
                static NetWorkInit inst;
                return inst;
            }

           private:
            NetWorkInit() {
                Init();
            }
            bool Init_impl() {
#ifdef _WIN32
                WSADATA data{0};
                if (WSAStartup(MAKEWORD(2, 2), &data)) {
                    return false;
                }
#endif
                return true;
            }

            ~NetWorkInit() {
#ifdef _WIN32
                WSACleanup();
#endif
            }

           public:
            bool Init() {
                static bool res = Init_impl();
                return res;
            }
        };

        template <class String>
        struct Resolver {
           private:
            static bool resolve_detail(TCPOpenContext<String>& ctx, const char* host, const char* service, int ipver, ::addrinfo*& info) {
                if (!NetWorkInit::instance().Init()) {
                    ctx.err = TCPError::initialize_network;
                    return false;
                }
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
                if (auto res = ::getaddrinfo(host, service, &hint, &info); res != 0) {
                    ctx.err = TCPError::resolve_address;
                    return false;
                }
                return true;
            }

           public:
            template <class C>
            static bool resolve(TCPOpenContext<C*>& ctx, ::addrinfo*& info) {
                return resolve_detail(ctx, ctx.host, ctx.service, ctx.ip_version, info);
            }

            template <class Str>
            static bool resolve(TCPOpenContext<Str>& ctx, ::addrinfo*& info) {
                return resolve_detail(ctx, ctx.host.data(), ctx.service.data(), ctx.ip_version, info);
            }

            static bool connect_loop(::addrinfo* info, int& sock, TCPOpenContext<String>& ctx, CancelContext* cancel) {
                OsErrorContext canceler(cancel);
                auto tmp = ::socket(info->ai_family, info->ai_socktype, info->ai_protocol);
                if (tmp < 0) return false;
                u_long flag = 1;
                ::ioctlsocket(tmp, FIONBIO, &flag);
                auto res = ::connect(tmp, info->ai_addr, info->ai_addrlen);
                auto when_connected = [&] {
                    sock = tmp;
                    if (!ctx.non_block) {
                        flag = 0;
                        ::ioctlsocket(tmp, FIONBIO, &flag);
                    }
                    return true;
                };
                if (res == 0) {
                    return when_connected();
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
                        return when_connected();
                    }
                    if (canceler.on_cancel()) {
                        ::closesocket(tmp);
                        ctx.err = TCPError::canceled;
                        return false;
                    }
                }
            }

            template <class Check = bool (*)(::addrinfo*, bool*)>
            static bool connect(TCPOpenContext<String>& ctx, CancelContext* cancel, int& sock, ::addrinfo* info, ::addrinfo*& selected, Check&& check = Check()) {
                std::uint16_t port = commonlib2::translate_byte_net_and_host<std::uint16_t>(&ctx.port);
                for (auto p = info; p; p = p->ai_next) {
                    ::sockaddr_in* addr = (::sockaddr_in*)p->ai_addr;
                    if (port) {
                        addr->sin_port = port;
                    }
                    bool result = false;
                    if (!commonlib2::invoke_cb<Check, bool>::invoke(std::forward<Check>(check), p, &result)) {
                        return result;
                    }
                    if (connect_loop(p, sock, ctx, cancel)) {
                        selected = p;
                        return true;
                    }
                }
                ctx.err = TCPError::no_address_to_connect;
                return false;
            }
        };

        struct SecureSetter {
           private:
            static bool connect_loop(int sock, SSL* ssl, CancelContext* cancel) {
                SSLErrorContext ctx(ssl, cancel);
                int err = 0;
                while (true) {
                    err = SSL_connect(ssl);
                    if (err < 0) {
                        if (ctx.on_cancel()) {
                            return false;
                        }
                        continue;
                    }
                    break;
                }
                return err == 1;
            }

            template <class String>
            static bool setupssl_detail(int sock, SSL_CTX*& sslctx, SSL*& ssl, TCPOpenContext<String>& ctx, const char* host, const char* cacert, CancelContext* cancel) {
                bool has_ctx = false, has_ssl = false;
                if (!sslctx) {
                    if (ssl) {
                        ctx.err = TCPError::has_ssl_but_ctx;
                        return false;
                    }
                    sslctx = SSL_CTX_new(TLS_method());
                    if (!sslctx) {
                        ctx.err = TCPError::memory;
                        return false;
                    }
                }
                else {
                    has_ctx = true;
                }
                SSL_CTX_set_options(sslctx, SSL_OP_NO_SSLv2);
                if (cacert) {
                    if (!SSL_CTX_load_verify_locations(sslctx, cacert, nullptr)) {
                        ctx.err = TCPError::register_cacert;
                        return false;
                    }
                }
                if (ctx.alpnstr && ctx.len) {
                    SSL_CTX_set_alpn_protos(sslctx, (const unsigned char*)ctx.alpnstr, ctx.len);
                }
                if (!ssl) {
                    ssl = SSL_new(sslctx);
                    if (!ssl) {
                        if (!has_ctx) SSL_CTX_free(sslctx);
                        ctx.err = TCPError::memory;
                        return false;
                    }
                }
                else {
                    has_ssl = true;
                    //SSL_set_alpn_protos(ssl, (const unsigned char*)ctx.alpnstr, ctx.len);
                }
                SSL_set_fd(ssl, sock);
                SSL_set_tlsext_host_name(ssl, host);
                auto param = SSL_get0_param(ssl);
                if (!X509_VERIFY_PARAM_add1_host(param, host, 0)) {
                    if (!has_ssl) SSL_free(ssl);
                    if (!has_ctx) SSL_CTX_free(sslctx);
                    ctx.err = TCPError::register_host_verify;
                    return false;
                }
                if (!connect_loop(sock, ssl, cancel)) {
                    if (!has_ssl) SSL_free(ssl);
                    if (!has_ctx) SSL_CTX_free(sslctx);
                    ctx.err = TCPError::ssl_connect;
                    return false;
                }

                auto verify = SSL_get_peer_certificate(ssl);
                if (!verify) {
                    if (!has_ssl) SSL_free(ssl);
                    if (!has_ctx) SSL_CTX_free(sslctx);
                    ctx.err = TCPError::cert_not_found;
                    return false;
                }
                if (SSL_get_verify_result(ssl) != X509_V_OK) {
                    X509_free(verify);
                    if (!has_ssl) SSL_free(ssl);
                    if (!has_ctx) SSL_CTX_free(sslctx);
                    ctx.err = TCPError::cert_verify_failed;
                    return false;
                }
                return true;
            }

           public:
            template <class C>
            static bool setupssl(int sock, SSL_CTX*& sslctx, SSL*& ssl, TCPOpenContext<C*>& ctx, CancelContext* cancel) {
                return setupssl_detail(sock, sslctx, ssl, ctx, ctx.host, ctx.cacert, cancel);
            }

            template <class Str>
            static bool setupssl(int sock, SSL_CTX*& sslctx, SSL*& ssl, TCPOpenContext<Str>& ctx, CancelContext* cancel) {
                return setupssl_detail(sock, sslctx, ssl, ctx, ctx.host.c_str(), ctx.cacert.c_str(), cancel);
            }
        };

        template <class String>
        struct ServerHandler {
            template <class C>
            static const char* get_data(C* str) {
                return str;
            }

            template <class Str>
            static const char* get_data(Str& str) {
                return str.data();
            }

            static void copy_list(::addrinfo*& to, const ::addrinfo* from) {
                if (!from || to) return;
                InetConn::copy_addrinfo(to, from);
                copy_list(to->ai_next, from->ai_next);
            }

            static void dellist(::addrinfo* del) {
                if (!del) return;
                dellist(del->ai_next);
                InetConn::del_addrinfo(del);
            }

            static bool get_this_server_address(TCPAcceptContext<String>& ctx) {
                if (!NetWorkInit::instance().Init()) {
                    ctx.err = TCPError::initialize_network;
                    return false;
                }
                ::addrinfo hints = {0}, *info = nullptr;
                hints.ai_family = AF_INET6;
                hints.ai_flags = AI_PASSIVE;
                hints.ai_socktype = SOCK_STREAM;
                if (::getaddrinfo(NULL, get_data(ctx.service), &hints, &info) != 0) {
                    ctx.err = TCPError::resolve_address;
                    return false;
                }
                copy_list(ctx.info, info);
                ::freeaddrinfo(info);
                return true;
            }

            static bool get_socket(int& sock, TCPAcceptContext<String>& ctx, ::addrinfo*& selected) {
                auto port_net = commonlib2::translate_byte_net_and_host<std::uint16_t>(&ctx.port);
                for (auto p = sv.copy; p; p = p->ai_next) {
                    sockaddr_in* addr = (sockaddr_in*)p->ai_addr;
                    if (port_net) {
                        addr->sin_port = port_net;
                    }
                    sock = ::socket(p->ai_family, p->ai_socktype, p->ai_protocol);
                    if (sock < 0) {
                        sock = invalid_socket;
                        continue;
                    }
                    u_long flag = 0;
                    switch (ctx.ip_version) {
                        case 4:
                            addr->sin_family = AF_INET;
                            break;
                        case 6:
                            addr->sin_family = AF_INET6;
                            flag = 1;
                            if (::setsockopt(sock, IPPROTO_IPV6, IPV6_V6ONLY, (char*)&flag, sizeof(flag)) < 0) {
                                ::closesocket(sock);
                                sock = invalid_socket;
                                continue;
                            }
                            break;
                        default:
                            addr->sin_family = AF_INET6;
                            flag = 0;
                            if (::setsockopt(sock, IPPROTO_IPV6, IPV6_V6ONLY, (char*)&flag, sizeof(flag)) < 0) {
                                ::closesocket(sock);
                                sock = invalid_socket;
                                continue;
                            }
                            break;
                    }
                    if (ctx.reuse_addr) {
                        if (::setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char*)&flag, sizeof(flag)) < 0) {
                            ::closesocket(sock);
                            sock = invalid_socket;
                            continue;
                        }
                    }
                    selected = p;
                    break;
                }
                if (sock < 0) {
                    ctx.err = TCPError::no_address_to_connect;
                    return false;
                }
                return true;
            }

            static bool init_server(TCPAcceptContext<String>& ctx) {
                if (ctx.acsock != invalid_socket) {
                    return true;
                }
                if (!get_this_server_address(ctx)) {
                    return false;
                }
                ctx.acsock = invalid_socket;
                ::addrinfo* selected = nullptr;
                if (!get_socket(ctx.acsock, ctx, selected)) {
                    return false;
                }
                if (::bind(sv.acsock, selected->ai_addr, selected->ai_addrlen) < 0) {
                    ctx.err = TCPError::bind;
                    ::closesocket(sv.acsock);
                    sv.acsock = invalid_socket;
                    return false;
                }
                if (::listen(sv.sock, 10000) < 0) {
                    ctx.err = TCPError::listen;
                    ::closesocket(sv.acsock);
                    sv.acsock = invalid_socket;
                    return false;
                }
                return true;
            }

            bool wait_signal(TCPAcceptContext<String>& ctx, CancelContext* cancel = nullptr) {
                if (ctx.acsock == invalid_socket) {
                    ctx.err = TCPError::no_socket;
                    return false;
                }
                ::fd_set baseset = {0}, work = {0};
                FD_ZERO(&baseset);
                FD_SET(ctx.acsock, &baseset);
                ::timeval time = {0};
                while (true) {
                    memcpy_s(&work, sizeof(work), &baseset, sizeof(baseset));
                    time.tv_sec = 0;
                    time.tv_usec = 1;
                    auto res = ::select(ctx.acsock + 1, &work, nullptr, nullptr, &time);
                    if (res < 0) {
                        ctx.err = TCPError::wait_accept;
                        return false;
                    }
                    if (cancel && cancel->on_cancel()) {
                        ctx.err = TCPError::canceled;
                        return false;
                    }
                    if (res == 0) {
                        continue;
                    }
                    if (FD_ISSET(ctx.acsock, &work)) {
                        break;
                    }
                }
                return true;
            }
        };

        template <class String>
        struct TCP {
            static bool open(std::shared_ptr<InetConn>& res, TCPOpenContext<String>& ctx, CancelContext* cancel = nullptr) {
                if (ctx.stat.type != ConnType::tcp_over_ssl) {
                    ctx.stat.type = ConnType::tcp_socket;
                }
                ::addrinfo *info = nullptr, *selected = nullptr;
                if (!Resolver<String>::resolve(ctx, info)) {
                    return false;
                }
                ConnStat stat;
                if (res) {
                    res->stat(stat);
                }
                bool not_reopen = false;
                int sock = invalid_socket;
                if (!Resolver<String>::connect(
                        ctx, cancel, sock, info, selected,
                        [&](::addrinfo* info, bool* result) {
                            if (!ctx.forceopen && ctx.stat.type == stat.type) {
                                if (any(stat.status & ConnStatus::has_fd)) {
                                    if (InetConn::cmp_addrinfo(info, stat.net.addrinfo)) {
                                        *result = false;
                                        ctx.err = TCPError::not_reopened;
                                        not_reopen = true;
                                        return false;
                                    }
                                }
                            }
                            return true;
                        })) {
                    return not_reopen;
                }
                if (ctx.stat.type == ConnType::tcp_socket) {
                    if (res) {
                        SocketReset reset;
                        reset.addr = selected;
                        reset.sock = sock;
                        res->reset(reset);
                    }
                    else {
                        res = std::make_shared<StreamConn>(sock, selected);
                    }
                }
                else {
                    ::SSL* ssl = nullptr;
                    ::SSL_CTX* sslctx = nullptr;
                    if (res) {
                        //ssl = stat.net.ssl;
                        sslctx = stat.net.ssl_ctx;
                    }
                    if (any(ctx.stat.status & ConnStatus::secure)) {
                        if (!SecureSetter::setupssl(sock, sslctx, ssl, ctx, cancel)) {
                            ::freeaddrinfo(info);
                            ::closesocket(sock);
                            return false;
                        }
                    }
                    if (res) {
                        SocketReset reset;
                        reset.addr = selected;
                        reset.sock = sock;
                        reset.ssl = ssl;
                        reset.ctx = sslctx;
                        reset.delflag = DelFlag::not_del_ctx;
                        res->reset(reset);
                    }
                    else {
                        res = std::make_shared<SecureStreamConn>(ssl, sslctx, sock, selected);
                    }
                }
                ::freeaddrinfo(info);
                return true;
            }

            static bool accept(std::shared_ptr<InetConn>& res, TCPAcceptContext<String>& ctx, CancelContext* cancel = nullptr) {
                if (!ServerHandler<String>::init_server(ctx)) {
                    return false;
                }
                if (!ServerHandler<String>::wait_signal(ctx, cancel)) {
                    return false;
                }
            }
        };
    }  // namespace v2
}  // namespace socklib
