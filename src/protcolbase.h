#pragma once
#include <reader.h>

#include <memory>

#include "sockbase.h"
namespace socklib {
    struct TCP {
       private:
        static int open_detail(addrinfo*& got, addrinfo*& selected, const char* host, unsigned short port = 0, const char* service = nullptr) {
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
            ::addrinfo *selected = nullptr, *got = nullptr;
            int sock = open_detail(got, selected, host, port, service);
            if (sock == invalid_socket) {
                return nullptr;
            }
            if (noblock) {
                u_long l = 1;
                ::ioctlsocket(sock, FIONBIO, &l);
            }
            auto ret = std::make_shared<Conn>(sock, selected);
            ::freeaddrinfo(got);
            return ret;
        }

        static std::shared_ptr<Conn> open_secure(const char* host, unsigned short port = 0, const char* service = nullptr, bool noblock = false, const char* cacert = nullptr, const char* alpnstr = nullptr, int len = 0) {
            ::addrinfo *selected = nullptr, *got = nullptr;
            int sock = open_detail(got, selected, host, port, service);
            if (sock == invalid_socket) {
                return nullptr;
            }
            SSL_CTX* ctx = nullptr;
            SSL* ssl = nullptr;
            ctx = SSL_CTX_new(TLS_method());
            if (!ctx) {
                ::closesocket(sock);
                ::freeaddrinfo(got);
                return nullptr;
            }
            SSL_CTX_set_options(ctx, SSL_OP_NO_SSLv2);
            if (cacert) {
                SSL_CTX_load_verify_locations(ctx, cacert, nullptr);
            }
            if (alpnstr && len) {
                SSL_CTX_set_alpn_protos(ctx, (const unsigned char*)alpnstr, len);
            }
            ssl = SSL_new(ctx);
            if (!ssl) {
                SSL_CTX_free(ctx);
                ::closesocket(sock);
                ::freeaddrinfo(got);
                return nullptr;
            }
            SSL_set_fd(ssl, sock);
            SSL_set_tlsext_host_name(ssl, host);
            auto param = SSL_get0_param(ssl);
            if (!X509_VERIFY_PARAM_add1_host(param, host, 0)) {
                SSL_free(ssl);
                SSL_CTX_free(ctx);
                ::closesocket(sock);
                ::freeaddrinfo(got);
                return nullptr;
            }
            if (SSL_connect(ssl) != 1) {
                SSL_free(ssl);
                SSL_CTX_free(ctx);
                ::closesocket(sock);
                ::freeaddrinfo(got);
                return nullptr;
            }
            if (noblock) {
                u_long l = 1;
                ::ioctlsocket(sock, FIONBIO, &l);
            }
            auto ret = std::make_shared<SecureConn>(ctx, ssl, sock, selected);
            ::freeaddrinfo(got);
            return ret;
        }
    };

    struct SockReader {
       private:
        mutable std::shared_ptr<Conn> base;
        mutable std::string buffer;
        mutable bool on_eof = false;

       public:
        SockReader(std::shared_ptr<Conn> r)
            : base(r) {}

        size_t size() const {
            if (buffer.size() < 100) reading();
            return buffer.size() + 1;
        }

        bool reading() const {
            if (on_eof) return false;
            auto res = base->read(buffer);
            if (res == ~0 || res == 0) {
                on_eof = true;
                return false;
            }
            return true;
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