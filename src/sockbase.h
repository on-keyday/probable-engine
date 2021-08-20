/*license*/
#pragma once

#ifdef _WIN32
#ifdef __MINGW32__
#define _WIN32_WINNT 0x0501
#endif
#include <WinSock2.h>
#include <WS2tcpip.h>
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#define closesocket close
#define ioctlsocket ioctl
#define SD_SEND SHUT_WR
#define SD_RECEIVE SHUT_RD
#define SD_BOTH SHUT_RDWR
#endif

#include <string>
#include <string.h>

#if !defined(_WIN32) || (defined(__GNUC__) && !defined(__clang__))
#define memcpy_s(dst, dsz, src, ssz) memcpy(dst, src, ssz)
#define strcpy_s(dst, dsz, src) strcpy(dst, src)
#endif

#ifndef USE_OPENSSL
#define USE_OPENSSL 1
#endif

#if USE_OPENSSL
#include <openssl/ssl.h>
#endif

namespace socklib {
    constexpr int invalid_socket = -1;
    constexpr size_t intmaximum = static_cast<size_t>(static_cast<unsigned int>(~0) >> 1);

    struct Network {
       private:
        static bool& flag() {
            static bool f = false;
            return f;
        }

        static bool Init_impl() {
            flag() = true;
#ifdef _WIN32
            WSADATA data{0};
            if (WSAStartup(MAKEWORD(2, 2), &data)) {
                return false;
            }
#endif
            return true;
        }

       public:
        static bool initialized() {
            return flag();
        }

        static bool Init() {
            static bool res = Init_impl();
            return res;
        }

        static void CheckInit() {
            if (!initialized()) {
                throw std::exception("not initialized net work");
            }
        }

        static void Clean() {
#ifdef _WIN32
            WSACleanup();
#endif
        }
    };

    struct Conn {
       private:
        friend struct Selecter;
        int sock = invalid_socket;
        addrinfo* addr = nullptr;

       public:
        static void copy_addrinfo(addrinfo*& addr, addrinfo* addrin) {
            addr = new addrinfo(*addrin);
            addr->ai_next = nullptr;
            addr->ai_canonname = nullptr;
            addr->ai_addr = nullptr;
            if (addrin->ai_canonname) {
                size_t sz = strlen(addrin->ai_canonname) + 1;
                addr->ai_canonname = new char[sz]();
                strcpy_s(addr->ai_canonname, sz, addrin->ai_canonname);
            }
            if (addrin->ai_addr) {
                addr->ai_addr = (sockaddr*)new char[addrin->ai_addrlen]();
                memcpy_s(addr->ai_addr, addr->ai_addrlen, addrin->ai_addr, addrin->ai_addrlen);
            }
        }

        static void del_addrinfo(addrinfo*& addr) {
            if (!addr) return;
            delete[](char*) addr->ai_addr;
            delete[] addr->ai_canonname;
            delete addr;
            addr = nullptr;
        }

       private:
        bool cmp_addrinfo(::addrinfo* info) const {
            if (addr == info) return true;
            if (!addr || !info) return false;
            if (addr->ai_addrlen != info->ai_addrlen || addr->ai_family != info->ai_family ||
                addr->ai_flags != info->ai_flags ||
                addr->ai_protocol != info->ai_protocol || addr->ai_socktype != info->ai_socktype) {
                return false;
            }
            if (memcmp(addr->ai_addr, info->ai_addr, addr->ai_addrlen) != 0) {
                return false;
            }
            if (strcmp(addr->ai_canonname, info->ai_canonname) != 0) {
                return false;
            }
            return true;
        }

       protected:
        int err = 0;
        bool suspend = false;

       public:
        static void set_os_error(int& err) {
#ifdef _WIN32
            err = WSAGetLastError();
#else
            err = errno;
#endif
        }

       protected:
        bool is_waiting(int e) {
#ifdef _WIN32
            return e == WSAEWOULDBLOCK;
#else
            return e == EAGAIN;
#endif
        }

       public:
        constexpr Conn() {}

        Conn(Conn&& from) {
            close();
            this->sock = from.sock;
            this->addr = from.addr;
            from.sock = invalid_socket;
            from.addr = nullptr;
        }

        Conn(int sok, addrinfo* addrin)
            : sock(sok) {
            Network::CheckInit();
            if (addrin) {
                copy_addrinfo(addr, addrin);
            }
        }

        const void* raw_addrinfo() const {
            return addr;
        }

        virtual bool is_secure() const {
            return false;
        }

        bool addr_same(addrinfo* info) const {
            return cmp_addrinfo(info);
        }

       protected:
        bool resetable(int sok, addrinfo* addrin) {
            if (sok == invalid_socket) return false;
            if (sock == sok && cmp_addrinfo(addrin)) return false;
            return true;
        }

        bool reset_impl(int sok, addrinfo* addrin) {
            close();
            sock = sok;
            if (addrin) {
                copy_addrinfo(addr, addrin);
            }
            return true;
        }

       public:
        virtual bool reset(int sok, addrinfo* addrin) {
            if (!resetable(sok, addrin)) return false;
            return reset_impl(sok, addrin);
        }

        virtual bool reset(SSL_CTX*, SSL*, int, addrinfo*) {
            return false;
        }

        void set_suspend(bool flag) {
            suspend = flag;
        }

        virtual void close() {
            if (sock != invalid_socket) {
                ::shutdown(sock, SD_BOTH);
                ::closesocket(sock);
                sock = invalid_socket;
            }
            del_addrinfo(addr);
        }

        virtual bool is_opened() const {
            return sock != invalid_socket;
        }

        virtual bool write(const char* data, size_t size) {
            if (!is_opened()) return 0;
            size_t count = 0;
            size_t sz = size;
            while (sz) {
                while (true) {
                    auto res = ::send(sock, data + count, sz < intmaximum ? (int)sz : (int)intmaximum, 0);
                    if (res < 0) {
                        set_os_error(err);
                        if (!suspend && is_waiting(err)) {
                            continue;
                        }
                        return false;
                    }
                    break;
                }
                if (sz < intmaximum) break;
                count += intmaximum;
                sz -= intmaximum;
            }
            return true;
        }

        virtual bool write(const std::string& str) {
            return write(str.data(), str.size());
        }

        virtual bool writeto(const char* data, size_t size) {
            if (!is_opened()) return false;
            size_t count = 0;
            size_t sz = size;
            while (sz) {
                while (true) {
                    auto res = ::sendto(sock, data + count, sz < intmaximum ? (int)sz : (int)intmaximum, 0, addr->ai_addr, addr->ai_addrlen);
                    if (res < 0) {
                        set_os_error(err);
                        if (!suspend && is_waiting(err)) {
                            continue;
                        }
                        return false;
                    }
                    break;
                }
                if (sz < intmaximum) break;
                count += intmaximum;
                sz -= intmaximum;
            }
            return true;
        }

        [[nodiscard]] virtual size_t read(char* data, size_t size) {
            if (!is_opened()) return 0;
            int res = 0;
            while (true) {
                res = ::recv(sock, data, size < intmaximum ? (int)size : intmaximum, 0);
                if (res < 0) {
                    set_os_error(err);
                    if (!suspend && is_waiting(err)) {
                        continue;
                    }
                    return ~0;
                }
                break;
            }
            return (size_t)res;
        }

        [[nodiscard]] virtual size_t read(std::string& buf) {
            size_t ret = 0;
            while (true) {
                char tmp[1024] = {0};
                auto res = read(tmp, 1024);
                if (res == ~0 || res == 0) {
                    return res;
                }
                buf.append(tmp, res);
                ret += res;
                if (res < 1024) {
                    break;
                }
            }
            return ret;
        }

        virtual void* get_sslctx() {
            return nullptr;
        }

        virtual ~Conn() noexcept {
            close();
        }
    };

#if USE_OPENSSL

    struct SecureConn : Conn {
       private:
        SSL* ssl = nullptr;
        SSL_CTX* ctx = nullptr;
        bool nodelctx = false;

       public:
        constexpr SecureConn() {}
        SecureConn(SSL_CTX* ctx, SSL* ssl, int fd, addrinfo* info, bool nodelctx = false)
            : ctx(ctx), ssl(ssl), Conn(fd, info), nodelctx(nodelctx) {}

        virtual bool is_opened() const override {
            return ssl != nullptr || Conn::is_opened();
        }

        virtual bool is_secure() const override {
            return true;
        }

        virtual bool write(const char* data, size_t size) override {
            if (!ssl) return Conn::write(data, size);
            while (true) {
                size_t w;
                if (!SSL_write_ex(ssl, data, size, &w)) {
                    if (ssl_failed()) {
                        return false;
                    }
                    continue;
                }
                break;
            }
            return true;
        }

        virtual bool write(const std::string& str) override {
            return write(str.data(), str.size());
        }

       private:
        bool noshutdown = false;

        bool ssl_failed() {
            auto reason = SSL_get_error(ssl, 0);
            bool retry = false;
            switch (reason) {
                case (SSL_ERROR_WANT_READ):
                case (SSL_ERROR_WANT_WRITE):
                    return false;
                case (SSL_ERROR_SYSCALL):
                    set_os_error(err);
                    if (!suspend && is_waiting(err)) return false;
                    break;
                default:
                    break;
            }
            noshutdown = true;
            return true;
        }

       public:
        [[nodiscard]] virtual size_t read(char* data, size_t size) override {
            if (!ssl) return Conn::read(data, size);
            size_t red = 0;
            while (!SSL_read_ex(ssl, data, size, &red)) {
                if (!ssl_failed()) continue;
                return ~0;
            }
            return red;
        }

        [[nodiscard]] virtual size_t read(std::string& buf) override {
            if (!ssl) return Conn::read(buf);
            size_t ret = 0;
            while (true) {
                char tmp[1024] = {0};
                auto res = read(tmp, 1024);
                if (res == ~0) {
                    return res;
                }
                buf.append(tmp, res);
                ret += res;
                if (res < 1024) {
                    break;
                }
            }
            return ret;
        }

        virtual void close() override {
            if (ssl) {
                if (!noshutdown) {
                    while (true) {
                        auto res = SSL_shutdown(ssl);
                        if (res < 0) {
                            if (!ssl_failed()) continue;
                            break;
                        }
                        else if (res == 0) {
                            continue;
                        }
                        break;
                    }
                }
                SSL_free(ssl);
                ssl = nullptr;
            }
            Conn::close();
        }

        virtual bool reset(SSL_CTX* ctx, SSL* ssl, int fd, addrinfo* info) override {
            if (!resetable(fd, info)) {
                return false;
            }
            close();
            Conn::reset_impl(fd, info);
            this->ssl = ssl;
            this->ctx = ctx;
            return true;
        }

        virtual bool reset(int fd, addrinfo* info) override {
            if (!resetable(fd, info)) {
                return false;
            }
            close();
            Conn::reset_impl(fd, info);
            return true;
        }

        virtual void* get_sslctx() override {
            return ctx;
        }

        ~SecureConn() {
            close();
            if (ctx && !nodelctx) SSL_CTX_free(ctx);
        }
    };
#endif

}  // namespace socklib