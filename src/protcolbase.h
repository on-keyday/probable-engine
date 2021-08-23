#pragma once
#include <reader.h>

#include <memory>

#include "sockbase.h"
namespace socklib {
    struct Server {
       private:
        friend struct TCP;
        int err = 0;
        int sock = invalid_socket;
        ::addrinfo* copy = nullptr;
        bool suspend = false;
        static void copy_list(::addrinfo*& to, const ::addrinfo* from) {
            if (!from || to) return;
            Conn::copy_addrinfo(to, from);
            copy_list(to->ai_next, from->ai_next);
        }

        static void dellist(::addrinfo* del) {
            if (!del) return;
            dellist(del->ai_next);
            Conn::del_addrinfo(del);
        }

        void copy_info(const ::addrinfo* from) {
            copy_list(copy, from);
        }

        bool get_nulladdrinfo(const char* service) {
            ::addrinfo hints = {0}, *info = nullptr;
            hints.ai_family = AF_INET6;
            hints.ai_flags = AI_PASSIVE;
            hints.ai_socktype = SOCK_STREAM;
            if (::getaddrinfo(NULL, service, &hints, &info) != 0) {
                Conn::set_os_error(err);
                return false;
            }
            copy_info(info);
            ::freeaddrinfo(info);
            return true;
        }

       public:
        std::string ipaddress_list(const char* service = "http") {
            if (!Network::Init()) {
                return std::string();
            }
            addrinfo infohint = {0};
            infohint.ai_socktype = SOCK_STREAM;
            addrinfo* info = nullptr;
            char hostname[256] = {0};
            gethostname(hostname, sizeof(hostname));
            getaddrinfo(hostname, nullptr, &infohint, &info);
            std::string ret;
            for (auto p = info; p; p = p->ai_next) {
                if (p != info) {
                    ret += "\n";
                }
                ret += Conn::get_ipaddress(p);
            }
            ::freeaddrinfo(info);
            return ret;
        }

        void set_suspend(bool flag) {
            suspend = flag;
        }

        bool suspended() const {
            return suspend;
        }

        ~Server() {
            if (sock != invalid_socket) {
                ::closesocket(sock);
            }
            dellist(copy);
        }
    };

    struct Selecter {
        static bool waitone(const std::shared_ptr<Conn>& conn, unsigned long sec, unsigned long usec = 0) {
            if (!conn) return false;
            ::timeval timer = {0};
            timer.tv_sec = sec;
            timer.tv_usec = usec;
            ::fd_set rset = {0};
            FD_ZERO(&rset);
            FD_SET(conn->sock, &rset);

            auto res = ::select(conn->sock, &rset, nullptr, nullptr, &timer);
            if (res <= 0) {
                if (res < 0) {
                    Conn::set_os_error(conn->err);
                }
                return false;
            }
            if (FD_ISSET(conn->sock, &rset)) {
                return true;
            }
            return false;
        }
    };

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
                    if (conn->addr_same(p) && conn->is_secure() == secure && conn->is_opened()) {
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
        static std::shared_ptr<Conn> open_secure(const char* host, unsigned short port = 0, const char* service = nullptr, bool noblock = false, const char* cacert = nullptr, bool secure = true, const char* alpnstr = nullptr, int len = 0) {
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
            if (!reopen_secure(ret, host, port, service, noblock, cacert, secure, alpnstr, len)) {
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

        static bool reopen_secure(std::shared_ptr<Conn>& conn, const char* host, unsigned short port = 0, const char* service = nullptr, bool noblock = false, const char* cacert = nullptr, bool secure = true, const char* alpnstr = nullptr, int len = 0) {
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
            if (secure) {
                if (!setupssl(sock, host, ctx, ssl, cacert, alpnstr, len)) {
                    ::closesocket(sock);
                    ::freeaddrinfo(got);
                    return false;
                }
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

       private:
        static bool init_server(Server& sv, unsigned short port, const char* service, bool ipv6) {
            if (sv.sock == invalid_socket) {
                ::addrinfo* selected = nullptr;
                if (!sv.copy) {
                    if (!sv.get_nulladdrinfo(service)) {
                        return false;
                    }
                }
                int sock = invalid_socket;
                auto port_net = commonlib2::translate_byte_net_and_host<unsigned short>((char*)&port);
                for (auto p = sv.copy; p; p = p->ai_next) {
                    sockaddr_in* addr = (sockaddr_in*)p->ai_addr;
                    if (port_net) {
                        addr->sin_port = port_net;
                    }
                    sock = ::socket(p->ai_family, p->ai_socktype, p->ai_protocol);
                    if (sock < 0) {
                        Conn::set_os_error(sv.err);
                        continue;
                    }
                    unsigned int flag = 0;
                    if (::setsockopt(sock, IPPROTO_IPV6, IPV6_V6ONLY, (char*)&flag, sizeof(flag)) < 0) {
                        Conn::set_os_error(sv.err);
                        ::closesocket(sock);
                        sock = invalid_socket;
                        continue;
                    }
                    flag = 1;
                    if (::setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char*)&flag, sizeof(flag)) < 0) {
                        Conn::set_os_error(sv.err);
                        ::closesocket(sock);
                        sock = invalid_socket;
                        continue;
                    }
                    selected = p;
                    break;
                }
                if (sock < 0) {
                    return false;
                }
                sv.sock = sock;
                if (::bind(sv.sock, selected->ai_addr, selected->ai_addrlen) < 0) {
                    Conn::set_os_error(sv.err);
                    ::closesocket(sv.sock);
                    sv.sock = invalid_socket;
                    return false;
                }
                if (::listen(sv.sock, 5) < 0) {
                    Conn::set_os_error(sv.err);
                    ::closesocket(sv.sock);
                    sv.sock = invalid_socket;
                    return false;
                }
            }
            return true;
        }

       public:
        static std::shared_ptr<Conn> serve(Server& sv, unsigned short port, size_t timeout = 10, const char* service = nullptr, bool ipv6 = true, bool noblock = false) {
            if (!Network::Init()) return nullptr;
            if (!init_server(sv, port, service, ipv6)) {
                return nullptr;
            }
            if (timeout) {
                ::fd_set baseset = {0}, work = {0};
                FD_ZERO(&baseset);
                FD_SET(sv.sock, &baseset);
                ::timeval time = {0};
                time.tv_usec = timeout;
                while (!sv.suspend) {
                    memcpy_s(&work, sizeof(work), &baseset, sizeof(baseset));
                    time.tv_sec = 0;
                    time.tv_usec = timeout;
                    auto res = ::select(sv.sock + 1, &work, nullptr, nullptr, &time);
                    if (res < 0) {
                        Conn::set_os_error(sv.err);
                        return nullptr;
                    }
                    if (res == 0) {
                        continue;
                    }
                    if (FD_ISSET(sv.sock, &work)) {
                        break;
                    }
                }
                if (sv.suspend) {
                    return nullptr;
                }
            }
            ::addrinfo info = {0};
            info.ai_socktype = SOCK_STREAM;
            info.ai_family = ipv6 ? AF_INET6 : AF_INET;
            info.ai_protocol = IPPROTO_TCP;
            ::sockaddr_storage st = {0};
            ::socklen_t addrlen = sizeof(st);
            int sock = ::accept(sv.sock, (::sockaddr*)&st, &addrlen);
            if (sock < 0) {
                Conn::set_os_error(sv.err);
                return nullptr;
            }
            if (sv.suspend) {
                ::closesocket(sock);
                return nullptr;
            }
            info.ai_addrlen = addrlen;
            info.ai_addr = (::sockaddr*)&st;
            if (noblock) {
                u_long l = 1;
                ::ioctlsocket(sock, FIONBIO, &l);
            }
            if (sv.suspend) {
                ::closesocket(sock);
                return nullptr;
            }
            return std::make_shared<Conn>(sock, &info);
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
            if (!base) {
                on_eof = true;
                return 0;
            }
            if (buffer.size() == 0) reading();
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
            if (!base) {
                on_eof = true;
                return char();
            }
            if (buffer.size() > idx) {
                return buffer[idx];
            }
            if (!reading()) return char();
            return buffer[idx];
        }

        bool eof() {
            return on_eof;
        }
    };

    struct AppLayer {
       protected:
        std::shared_ptr<Conn> conn;

       public:
        AppLayer(std::shared_ptr<Conn>&& in)
            : conn(std::move(in)) {}

        void interrupt(bool f = true) {
            conn->set_suspend(f);
        }

        std::shared_ptr<Conn> hijack() {
            return std::move(conn);
        }

        std::shared_ptr<Conn>& borrow() {
            return conn;
        }

        std::string ipaddress() {
            if (!conn) return std::string();
            return conn->ipaddress();
        }

        virtual void close() {
            if (conn) {
                conn->close();
            }
        }

        virtual ~AppLayer() {}
    };

}  // namespace socklib