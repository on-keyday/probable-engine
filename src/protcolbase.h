#pragma once
#include <reader.h>

#include <memory>

#include "sockbase.h"
namespace socklib {
    struct TCP {
       private:
        static int open_detail(const std::shared_ptr<Conn>& conn, bool secure, addrinfo*& got, addrinfo*& selected, const char* host, unsigned short port = 0, const char* service = nullptr) {
            if (!Network::Init()) return invalid_socket;
            ::addrinfo hint = {0};
            hint.ai_socktype = SOCK_STREAM;
            hint.ai_family = AF_INET;

            if (getaddrinfo(host, service, &hint, &got) != 0) {
                return invalid_socket;
            }
            auto port_net = commonlib2::translate_byte_net_and_host<unsigned short>((char*)&port);
            int sock = invalid_socket;
            for (auto p = got; p; p = p->ai_next) {
                sockaddr_in* addr = (sockaddr_in*)p->ai_addr;
                if (port_net) {
                    addr->sin_port = port_net;
                }
                if (conn) {
                    if (conn->addr_same(p) && conn->is_secure() == secure) {
                        return 0;
                    }
                }
                auto tmp = ::socket(p->ai_family, p->ai_socktype, p->ai_protocol);
                if (tmp < 0) continue;
                if (::connect(tmp, p->ai_addr, p->ai_addrlen) == 0) {
                    sock = tmp;
                    selected = p;
                    break;
                }
                ::closesocket(tmp);
            }
            return sock;
        }

       public:
        static std::shared_ptr<Conn> open(const char* host, unsigned short port = 0, const char* service = nullptr, bool noblock = false) {
            std::shared_ptr<Conn> ret;
            /*::addrinfo *selected = nullptr, *got = nullptr;
            int sock = open_detail(nullptr, false, got, selected, host, port, service);
            if (sock == invalid_socket) {
                return nullptr;
            }
            if (noblock) {
                u_long l = 1;
                ::ioctlsocket(sock, FIONBIO, &l);
            }
            auto ret = std::make_shared<Conn>(sock, selected);
            ::freeaddrinfo(got);*/
            if (!reopen(ret, host, port, service, noblock)) {
                return nullptr;
            }
            return ret;
        }

       private:
        static bool setupssl(int sock, const char* host, SSL_CTX*& ctx, SSL*& ssl, const char* cacert = nullptr, const char* alpnstr = nullptr, int len = 0) {
            bool has_ctx = false, has_ssl = false;
            if (!ctx) {
                if (ssl) return false;
                ctx = SSL_CTX_new(TLS_method());
                if (!ctx) {
                    return false;
                }
            }
            else {
                has_ctx = true;
            }
            SSL_CTX_set_options(ctx, SSL_OP_NO_SSLv2);
            if (cacert) {
                SSL_CTX_load_verify_locations(ctx, cacert, nullptr);
            }
            if (alpnstr && len) {
                SSL_CTX_set_alpn_protos(ctx, (const unsigned char*)alpnstr, len);
            }
            if (!ssl) {
                ssl = SSL_new(ctx);
                if (!ssl) {
                    if (!has_ctx) SSL_CTX_free(ctx);
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
                if (!has_ctx) SSL_CTX_free(ctx);
                return false;
            }
            if (SSL_connect(ssl) != 1) {
                if (!has_ssl) SSL_free(ssl);
                if (!has_ctx) SSL_CTX_free(ctx);
                return false;
            }
            return true;
        }

       public:
        static std::shared_ptr<Conn> open_secure(const char* host, unsigned short port = 0, const char* service = nullptr, bool noblock = false, const char* cacert = nullptr, const char* alpnstr = nullptr, int len = 0) {
            std::shared_ptr<Conn> ret;
            /*::addrinfo *selected = nullptr, *got = nullptr;
            int sock = open_detail(nullptr, true, got, selected, host, port, service);
            if (sock == invalid_socket) {
                return nullptr;
            }
            SSL_CTX* ctx = nullptr;
            SSL* ssl = nullptr;
            if (!setupssl(sock, host, ctx, ssl, cacert, alpnstr, len)) {
                ::closesocket(sock);
                ::freeaddrinfo(got);
                return nullptr;
            }
            if (noblock) {
                u_long l = 1;
                ::ioctlsocket(sock, FIONBIO, &l);
            }
            auto ret = std::make_shared<SecureConn>(ctx, ssl, sock, selected);
            ::freeaddrinfo(got);*/
            if (!reopen_secure(ret, host, port, service, noblock, cacert, alpnstr, len)) {
                return nullptr;
            }
            return ret;
        }

        static bool reopen(std::shared_ptr<Conn>& conn, const char* host, unsigned short port, const char* service = nullptr, bool noblock = false) {
            ::addrinfo *selected = nullptr, *got = nullptr;
            int sock = open_detail(conn, false, got, selected, host, port, service);
            if (sock == invalid_socket) {
                return false;
            }
            else if (sock == 0) {
                return true;
            }
            if (noblock) {
                u_long l = 1;
                ::ioctlsocket(sock, FIONBIO, &l);
            }
            bool res = false;
            if (conn) {
                res = conn->reset(sock, selected);
            }
            else {
                conn = std::make_shared<Conn>(sock, selected);
                res = (bool)conn;
            }
            ::freeaddrinfo(got);
            return true;
        }

        static bool reopen_secure(std::shared_ptr<Conn>& conn, const char* host, unsigned short port = 0, const char* service = nullptr, bool noblock = false, const char* cacert = nullptr, const char* alpnstr = nullptr, int len = 0) {
            ::addrinfo *selected = nullptr, *got = nullptr;
            int sock = open_detail(conn, true, got, selected, host, port, service);
            if (sock == invalid_socket) {
                return false;
            }
            else if (sock == 0) {
                return true;
            }
            SSL_CTX* ctx = conn ? (SSL_CTX*)conn->get_sslctx() : nullptr;
            SSL* ssl = nullptr;
            if (!setupssl(sock, host, ctx, ssl, cacert, alpnstr, len)) {
                ::closesocket(sock);
                ::freeaddrinfo(got);
                return false;
            }
            if (noblock) {
                u_long l = 1;
                ::ioctlsocket(sock, FIONBIO, &l);
            }
            bool res = false;
            if (conn && conn->is_secure()) {
                res = conn->reset(ctx, ssl, sock, selected);
            }
            else {
                conn = std::make_shared<SecureConn>(ctx, ssl, sock, selected);
                res = (bool)conn;
            }
            ::freeaddrinfo(got);
            return res;
        }
    };

    struct SockReader {
       private:
        mutable std::shared_ptr<Conn> base;
        mutable std::string buffer;
        mutable bool on_eof = false;
        void (*callback)(void*, bool) = nullptr;
        void* ctx = nullptr;

       public:
        SockReader(std::shared_ptr<Conn> r, decltype(callback) cb = nullptr, void* ct = nullptr)
            : base(r), callback(cb), ctx(ct) {}
        void setcallback(decltype(callback) cb, void* ct) {
            callback = cb;
            ctx = ct;
        }
        size_t size() const {
            if (buffer.size() < 100) reading();
            return buffer.size() + 1;
        }

        bool reading() const {
            if (on_eof) return false;
            auto res = base->read(buffer);
            if (res == ~0 || res == 0) {
                on_eof = true;
            }
            if (callback) callback(ctx, on_eof);
            return !on_eof;
        }

        char operator[](size_t idx) const {
            if (buffer.size() > idx) {
                return buffer[idx];
            }
            if (!reading()) return char();
            return buffer[idx];
        }
    };

}  // namespace socklib