#pragma once

#include "iconn.h"
#include <reader.h>
#include <callback_invoker.h>
#include <memory>

namespace socklib {
    namespace v2 {

        enum class TCPError {
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
            ssl_write,
            tcp_write,
            ssl_read,
            tcp_read,
            not_reopened,
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
            TCPError err;
            const char* alpnstr = nullptr;
            size_t len = 0;
            bool forceopen = false;
        };

        template <class String>
        struct Resolver {
           private:
            static bool resolve_detail(const char* host, const char* service, int ipver, ::addrinfo*& info) {
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
                if (::getaddrinfo(ctx.host, ctx.service, &hint, &info) != 0) {
                    ctx.err = TCPError::resolve_address;
                    return false;
                }
                return true;
            }

           public:
            template <class C>
            static bool resolve(TCPOpenContext<C*>& ctx, ::addrinfo*& info) {
                return resolve_detail(ctx.host, ctx.service, ctx.ip_version, info);
            }

            template <class Str>
            static bool resolve(TCPOpenContext<Str>& ctx, ::addrinfo*& info) {
                return resolve_detail(ctx.host.c_str(), ctx.service.c_str(), ctx.ip_version, info);
            }

            static bool connect_loop(::addrinfo* info, int& sock, TCPOpenContext<String>& ctx, CancelContext* cancel) {
                OsErrorContext canceler(cancel);
                auto tmp = ::socket(p->ai_family, p->ai_socktype, p->ai_protocol);
                if (tmp < 0) continue;
                u_long flag = 1;
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
            template <class String>
            static bool setupssl_detail(int sock, SSL_CTX*& sslctx, SSL*& ssl, TCPOpenContext<String>& ctx, const char* host, const char* cacert) {
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
                if (SSL_connect(ssl) != 1) {
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
            static bool setupssl(int sock, SSL_CTX*& sslctx, SSL*& ssl, TCPOpenContext<C*>& ctx) {
                return setupssl_detail(sock, sslctx, ssl, ctx, ctx.host, ctx.cacert);
            }

            template <class Str>
            static bool setupssl(int sock, SSL_CTX*& sslctx, SSL*& ssl, TCPOpenContext<Str>& ctx) {
                return setupssl_detail(sock, sslctx, ssl, ctx, ctx.host.c_str(), ctx.cacert.c_str());
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
                        ssl = stat.net.ssl;
                        sslctx = stat.net.ssl_ctx;
                    }
                    if (!SecureSetter::setupssl(sock, sslctx, ssl, ctx)) {
                        ::freeaddrinfo(info);
                        ::closesocket(sock);
                        return false;
                    }
                    if (res) {
                        SocketReset reset;
                        reset.addr = selected;
                        reset.sock = sock;
                        reset.ssl = ssl;
                        reset.sslctx = sslctx;
                        res->reset(reset);
                    }
                    else {
                        res = std::make_shared<SecureStreamConn>(ssl, sslctx, sock, selected);
                    }
                }
                ::freeaddrinfo(info);
                return true;
            }
        };
    }  // namespace v2
}  // namespace socklib