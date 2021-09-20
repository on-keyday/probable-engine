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
        static bool waitone(const std::shared_ptr<Conn>& conn, unsigned long sec, unsigned long usec = 0, CancelContext* cancel = nullptr) {
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
            if (cancel && cancel->on_cancel()) {
                return false;
            }
            return false;
        }
    };

    enum class IPMode {
        both,
        v6only,
        v4only,
    };

    enum class OpenError {
        none,
        needless_to_reopen,
        unresolved_address,
        connect,
        memory,
        invalid_condition,
        verify,
        parse_url,
        value_not_found,
        unknown,
    };

    constexpr const char* error_message(socklib::OpenError e) {
        using E = socklib::OpenError;
        switch (e) {
            case E::none:
                return "no error";
            case E::needless_to_reopen:
                return "needless to reopen";
            case E::unresolved_address:
                return "host name not resolved";
            case E::memory:
                return "memory is full";
            case E::invalid_condition:
                return "condition is wrong";
            case E::verify:
                return "verify failed";
            case E::parse_url:
                return "parse url failed";
            case E::connect:
                return "connect error";
            case E::value_not_found:
                return "value not found";
            default:
                return "unknown error";
        }
    }

    using OpenErr = commonlib2::EnumWrap<OpenError, OpenError::none, OpenError::unknown>;

    struct TCP {
       private:
        static OpenErr open_detail(int& sock, const std::shared_ptr<Conn>& conn, bool secure, addrinfo*& got, addrinfo*& selected, const char* host, unsigned short port = 0, const char* service = nullptr, CancelContext* cancel = nullptr) {
            if (!Network::Init()) return invalid_socket;
            ::addrinfo hint = {0};
            hint.ai_socktype = SOCK_STREAM;
            hint.ai_family = AF_INET;
            TimeoutContext timer(60, cancel);
            OsErrorContext ctx(&timer);
            if (getaddrinfo(host, service, &hint, &got) != 0) {
                return OpenError::unresolved_address;
            }
            auto port_net = commonlib2::translate_byte_net_and_host<unsigned short>(&port);
            sock = invalid_socket;
            for (auto p = got; p; p = p->ai_next) {
                sockaddr_in* addr = (sockaddr_in*)p->ai_addr;
                if (port_net) {
                    addr->sin_port = port_net;
                }
                if (conn) {
                    if (conn->addr_same(p) && conn->is_secure() == secure && conn->is_opened()) {
                        return OpenError::needless_to_reopen;
                    }
                }
                auto tmp = ::socket(p->ai_family, p->ai_socktype, p->ai_protocol);
                if (tmp < 0) continue;
                u_long l = 1;
                ::ioctlsocket(tmp, FIONBIO, &l);
                auto res = ::connect(tmp, p->ai_addr, p->ai_addrlen);
                if (res == 0) {
                    sock = tmp;
                    selected = p;
                    l = 0;
                    ::ioctlsocket(tmp, FIONBIO, &l);
                    break;
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
                        break;
                    }
                    if (FD_ISSET(tmp, &errset)) {
                        ::closesocket(tmp);
                        break;
                    }
                    if (FD_ISSET(tmp, &sucset)) {
                        sock = tmp;
                        selected = p;
                        l = 0;
                        ::ioctlsocket(tmp, FIONBIO, &l);
                        break;
                    }
                    if (ctx.on_cancel()) {
                        ::closesocket(tmp);
                        return OpenError::connect;
                    }
                }
                if (sock != invalid_socket) {
                    break;
                }
            }
            return sock == invalid_socket ? OpenError::connect : OpenError::none;
        }

       public:
        static std::shared_ptr<Conn> open(const char* host, unsigned short port = 0, const char* service = nullptr, bool noblock = false, OpenErr* err = nullptr, CancelContext* cancel = nullptr) {
            std::shared_ptr<Conn> ret;
            if (auto e = reopen(ret, host, port, service, noblock, cancel); !e) {
                if (err) *err = e;
                return nullptr;
            }
            return ret;
        }

       private:
        static OpenErr setupssl(int sock, const char* host, SSL_CTX*& ctx, SSL*& ssl, const char* cacert = nullptr, const char* alpnstr = nullptr, int len = 0, bool strictverify = false) {
            bool has_ctx = false, has_ssl = false;
            if (!ctx) {
                if (ssl) return OpenError::invalid_condition;
                ctx = SSL_CTX_new(TLS_method());
                if (!ctx) {
                    return OpenError::memory;
                }
            }
            else {
                has_ctx = true;
            }
            SSL_CTX_set_options(ctx, SSL_OP_NO_SSLv2);
            if (cacert) {
                if (!SSL_CTX_load_verify_locations(ctx, cacert, nullptr)) {
                    return OpenError::verify;
                }
            }
            if (alpnstr && len) {
                SSL_CTX_set_alpn_protos(ctx, (const unsigned char*)alpnstr, len);
            }
            if (!ssl) {
                ssl = SSL_new(ctx);
                if (!ssl) {
                    if (!has_ctx) SSL_CTX_free(ctx);
                    return OpenError::memory;
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
                return OpenError::verify;
            }
            if (SSL_connect(ssl) != 1) {
                if (!has_ssl) SSL_free(ssl);
                if (!has_ctx) SSL_CTX_free(ctx);
                return OpenError::connect;
            }
            if (strictverify) {
                auto verify = SSL_get_peer_certificate(ssl);
                if (!verify) {
                    if (!has_ssl) SSL_free(ssl);
                    if (!has_ctx) SSL_CTX_free(ctx);
                    return OpenError::verify;
                }
                if (SSL_get_verify_result(ssl) != X509_V_OK) {
                    return OpenError::verify;
                }
                X509_free(verify);
            }
            return true;
        }

       public:
        static std::shared_ptr<Conn> open_secure(const char* host, unsigned short port = 0, const char* service = nullptr, bool noblock = false, const char* cacert = nullptr, bool secure = true, const char* alpnstr = nullptr, int len = 0, bool strictverify = false, OpenErr* err = nullptr, CancelContext* cancel = nullptr) {
            std::shared_ptr<Conn> ret;
            if (auto e = reopen_secure(ret, host, port, service, noblock, cacert, secure, alpnstr, len, strictverify); !e) {
                if (err) *err = e;
                return nullptr;
            }
            return ret;
        }

        static OpenErr reopen(std::shared_ptr<Conn>& conn, const char* host, unsigned short port, const char* service = nullptr, bool noblock = false, CancelContext* cancel = nullptr) {
            ::addrinfo *selected = nullptr, *got = nullptr;
            int sock = invalid_socket;
            auto err = open_detail(sock, conn, false, got, selected, host, port, service, cancel);
            if (!err) {
                return err;
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

        static OpenErr reopen_secure(std::shared_ptr<Conn>& conn, const char* host, unsigned short port = 0, const char* service = nullptr, bool noblock = false, const char* cacert = nullptr, bool secure = true, const char* alpnstr = nullptr, int len = 0, bool strictverify = false, CancelContext* cancel = nullptr) {
            ::addrinfo *selected = nullptr, *got = nullptr;
            int sock = invalid_socket;
            auto err = open_detail(sock, conn, true, got, selected, host, port, service, cancel);
            if (!err) {
                return err;
            }
            SSL_CTX* ctx = conn ? (SSL_CTX*)conn->get_sslctx() : nullptr;
            SSL* ssl = nullptr;
            if (secure) {
                if (auto e = setupssl(sock, host, ctx, ssl, cacert, alpnstr, len, strictverify); !e) {
                    ::closesocket(sock);
                    ::freeaddrinfo(got);
                    return e;
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
        static bool init_server(Server& sv, unsigned short port, const char* service, IPMode mode) {
            if (sv.sock == invalid_socket) {
                ::addrinfo* selected = nullptr;
                if (!sv.copy) {
                    if (!sv.get_nulladdrinfo(service)) {
                        return false;
                    }
                }
                int sock = invalid_socket;
                auto port_net = commonlib2::translate_byte_net_and_host<unsigned short>(&port);
                for (auto p = sv.copy; p; p = p->ai_next) {
                    sockaddr_in* addr = (sockaddr_in*)p->ai_addr;
                    if (port_net) {
                        addr->sin_port = port_net;
                    }
                    if (mode == IPMode::v4only) {
                        addr->sin_family = AF_INET;
                    }
                    sock = ::socket(p->ai_family, p->ai_socktype, p->ai_protocol);
                    if (sock < 0) {
                        Conn::set_os_error(sv.err);
                        continue;
                    }
                    unsigned int flag = 0;
                    if (mode == IPMode::both) {
                        if (::setsockopt(sock, IPPROTO_IPV6, IPV6_V6ONLY, (char*)&flag, sizeof(flag)) < 0) {
                            Conn::set_os_error(sv.err);
                            ::closesocket(sock);
                            sock = invalid_socket;
                            continue;
                        }
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
        static std::shared_ptr<Conn> serve(Server& sv, unsigned short port, size_t timeout = 10, const char* service = nullptr, bool noblock = false, IPMode mode = IPMode::both) {
            if (!Network::Init()) return nullptr;
            if (!init_server(sv, port, service, mode)) {
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
            info.ai_family = st.ss_family;
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

}  // namespace socklib
