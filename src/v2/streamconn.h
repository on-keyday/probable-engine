/*
    socklib - simple socket library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once

#include "iconn.h"

namespace socklib {
    namespace v2 {

        //StreamConn - connection for tcp socket (SOCK_STREAM)
        struct StreamConn : InetConn {
           protected:
            SOCKET sock = invalid_socket;

           public:
            StreamConn(int s, ::addrinfo* p)
                : sock(s), InetConn(p) {}

            virtual bool write(IWriteContext& towrite, CancelContext* cancel = nullptr) override {
                //TODO:replace to OSErrorContext
                OsErrorContext ctx((bool)towrite.flags(), cancel);
                while (true) {
                    auto ptr = towrite.bufptr();
                    auto size = towrite.size();
                    if (!ptr || !size) {
                        break;
                    }
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
                                towrite.on_error(ctx.err, &ctx, "");
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
                            toread.on_error(ctx.err, &ctx, "");
                            return false;
                        }
                        continue;
                    }
                    toread.append(buf, (size_t)res);
                    if (res < 1024) {
                        if (toread.require()) {
                            continue;
                        }
                        break;
                    }
                }
                return true;
            }

            virtual void close(CancelContext* cancel = nullptr) override {
                if (sock == invalid_socket) return;
                ::shutdown(sock, SD_BOTH);
                ::closesocket(sock);
                sock = invalid_socket;
            }

            virtual bool reset(IResetContext& ctx) override {
                StreamConn::close();
                sock = (int)ctx.context((size_t)ResetIndex::socket);
                InetConn::reset(ctx);
                return true;
            }

            virtual bool stat(ConnStat& st) const override {
                st.type = ConnType::tcp_socket;
                st.status = (sock == invalid_socket ? ConnStatus::none : ConnStatus::has_fd);
                st.status |= ConnStatus::streaming;
                InetConn::stat(st);
                return true;
            }

            virtual ~StreamConn() {
                close();
            }
        };

        //SecureStreamConn - secure connection for tcp socket (SOCK_STREAM)
        struct SecureStreamConn : StreamConn {
           protected:
            ::SSL* ssl = nullptr;
            ::SSL_CTX* ctx = nullptr;
            bool noshutdown = false;
            bool nodelctx = false;

           public:
            SecureStreamConn(::SSL* issl, ::SSL_CTX* ictx, int sock, ::addrinfo* info, bool nodelctx = false)
                : ssl(issl), ctx(ictx), nodelctx(nodelctx), StreamConn(sock, info) {}

            virtual bool write(IWriteContext& towrite, CancelContext* cancel = nullptr) override {
                if (!ssl) {
                    return StreamConn::write(towrite, cancel);
                }
                SSLErrorContext ctx(ssl, cancel, (bool)towrite.flags());
                while (true) {
                    auto ptr = towrite.bufptr();
                    auto size = towrite.size();
                    size_t wrt = 0;
                    if (!ptr || !size) {
                        break;
                    }
                    while (!SSL_write_ex(ssl, ptr, size, &wrt)) {
                        if (ctx.on_cancel()) {
                            if (ctx.reason() == CancelReason::ssl_error || ctx.reason() == CancelReason::os_error) {
                                noshutdown = true;
                            }
                            towrite.on_error(ctx.err, &ctx, "ssl");
                            return false;
                        }
                    }
                    if (towrite.done()) {
                        break;
                    }
                }
                return true;
            }

            virtual bool read(IReadContext& toread, CancelContext* cancel = nullptr) override {
                if (!ssl) return StreamConn::read(toread, cancel);
                SSLErrorContext ctx(ssl, cancel, (bool)toread.flags());
                while (true) {
                    size_t red = 0;
                    char data[1024] = {0};
                    while (!SSL_read_ex(ssl, data, 1024, &red)) {
                        if (ctx.on_cancel()) {
                            std::string errstr = "ssl";
                            if (ctx.reason() == CancelReason::ssl_error || ctx.reason() == CancelReason::os_error) {
                                errstr += ":";
                                ERR_print_errors_cb(
                                    [](const char* str, size_t len, void* u) -> int {
                                        (*(std::string*)u) += str;
                                        return 1;
                                    },
                                    (void*)&errstr);
                                if (ctx.sslerr == 1 && errstr.find("application data after close notify") != ~0) {
                                    continue;
                                }
                            }
                            noshutdown = true;
                            toread.on_error(ctx.err, &ctx, errstr.c_str());
                            return false;
                        }
                    }
                    toread.append(data, red);
                    if (red < 1024) {
                        if (toread.require()) {
                            continue;
                        }
                        break;
                    }
                }
                return true;
            }

            virtual bool reset(IResetContext& set) override {
                TimeoutContext timeout(10);
                DelFlag flag = (DelFlag)set.context((size_t)ResetIndex::del_flag);
                close_impl(&timeout, !any(DelFlag::not_del_ssl & flag));
                if (!any(flag & DelFlag::not_del_ctx) && !nodelctx) {
                    ::SSL_CTX_free(ctx);
                    ctx = nullptr;
                }
                ssl = (::SSL*)set.context((size_t)ResetIndex::ssl);
                ctx = (::SSL_CTX*)set.context((size_t)ResetIndex::ssl_ctx);
                nodelctx = (bool)set.context((size_t)ResetIndex::nodel_ctx);
                StreamConn::reset(set);
                return true;
            }

           private:
            void close_impl(CancelContext* cancel, bool del_ssl) {
                if (ssl) {
                    if (!noshutdown) {
                        SSLErrorContext ctx(ssl, cancel);
                        while (true) {
                            auto res = SSL_shutdown(ssl);
                            if (res < 0) {
                                if (!ctx.on_cancel()) continue;
                                break;
                            }
                            else if (res == 0) {
                                continue;
                            }
                            break;
                        }
                    }
                    if (del_ssl) ::SSL_free(ssl);
                    ssl = nullptr;
                }
                StreamConn::close(cancel);
            }

           public:
            virtual void close(CancelContext* cancel = nullptr) override {
                close_impl(cancel, true);
            }

            virtual bool stat(ConnStat& st) const override {
                st.type = ConnType::tcp_over_ssl;
                st.status = (sock == invalid_socket ? ConnStatus::none : ConnStatus::has_fd);
                st.status |= (ssl ? ConnStatus::secure : ConnStatus::none);
                st.status |= ConnStatus::streaming;
                st.net.ssl = ssl;
                st.net.ssl_ctx = ctx;
                InetConn::stat(st);
                return true;
            }

            virtual ~SecureStreamConn() {
                TimeoutContext timeout(10);
                close(&timeout);
                if (ctx && !nodelctx) {
                    ::SSL_CTX_free(ctx);
                }
                ctx = nullptr;
            }
        };
    }  // namespace v2
}  // namespace socklib
