/*
    socklib - simple socket library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "tcp_socket.h"

namespace socklib {
    namespace v3 {
        struct SSLInSetting : IInputSetting<ContextType::ssl> {
            const char* alpn = nullptr;
            size_t alpnlen = 0;
            const char* cacert = nullptr;
            bool thru = false;
        };

        struct SSLOutSetting : IOutputSetting<ContextType::ssl> {
            ::SSL* ssl = nullptr;
            ::SSL_CTX* ctx = nullptr;
        };

        struct SSLContext : IContext<ContextType::ssl> {
           private:
            std::shared_ptr<Context> base = 0;
            ::SSL* ssl = nullptr;
            ::SSL* tmpssl = nullptr;
            ::SSL_CTX* ctx = nullptr;
            const char* alpn = nullptr;
            size_t alpnlen = 0;

            bool thru = false;
            int progress = 0;
            errno_t numerr = 0;
            StringBuffer* errmsg = nullptr;
            StringBuffer* cacert = nullptr;

            ::BIO* io_ssl = nullptr;
            ::BIO* io_base = nullptr;

            StringBuffer* tmpbuf = nullptr;

            void call_ssl_error() {
                numerr = ERR_peek_last_error();
                ERR_print_errors_cb(
                    [](const char* str, size_t len, void* u) -> int {
                        auto buf = (StringBuffer*)u;
                        buf->append_back(str, len);
                        buf->append_back("\n");
                        return 1;
                    },
                    (void*)errmsg);
            }

            template <int progbase>
            State read_write_bio() {
                switch (progress) {
                    case progbase: {
                        tmpbuf->clear();
                        progress = prgbase + 1;
                    }
                    case progbase + 1: {
                        while (size_t sz = ::BIO_ctrl_pending(io_base)) {
                            char buf[1024] = {0};
                            size_t red = 0;
                            if (!::BIO_read_ex(buf, buf, 1024, &red)) {
                                errmsg->set("unknown error; ::BIO_read_ex failed when ::BIO_ctrl_pending");
                                call_ssl_error();
                                return false;
                            }
                            tmpbuf->append_back(buf, red);
                            if (red < 1024) {
                                break;
                            }
                        }
                        progress = progbase + 2;
                    }
                    case progbase + 2: {
                        if (tmpbuf->size()) {
                            if (auto e = Context::write(tmpbuf->c_str(), tmpbuf->size()); !e) {
                                return e;
                            }
                            tmpbuf->clear();
                        }
                        progress = progbase + 3;
                    }
                    case progbase + 3: {
                        if (BIO_should_read(io_ssl)) {
                            char buf[1024] = {0};
                            size_t red = 0;
                            if (auto e = Context::read(buf, 1024, red); e == StateValue::inprogress) {
                                tmpbuf->append_back(buf, red);
                                return e;
                            }
                            else if (!e) {
                                return false;
                            }
                            else {
                                tmpbuf->append_back(buf, red);
                            }
                            size_t written = 0;
                            if (!::BIO_write_ex(io_base, tmpbuf->c_str(), tmpbuf->size(), &written)) {
                                errmsg->set("unknown error; ::BIO_write_ex failed");
                                call_ssl_error();
                                return false;
                            }
                            tmpbuf->clear();
                        }
                    }
                    default:
                        break;
                }
                return true;
            }

            State connect() {
                switch (progress) {
                    case 4: {
                        auto res = ::SSL_connect(ssl);
                        if (res <= 0) {
                            auto code = ::SSL_get_error(ssl, -1);
                            switch (code) {
                                case SSL_ERROR_WANT_READ:
                                case SSL_ERROR_WANT_WRITE:
                                case SSL_ERROR_SYSCALL:
                                    break;
                                default:
                                    errmsg->set("failed call ::SSL_connect()\n");
                                    call_ssl_error();
                                    return false;
                            }
                        }
                        else {
                            return true;
                        }
                        progress = 5;
                    }
                    case 5:
                    case 6:
                    case 7:
                    case 8: {
                        if (auto e = read_write_bio<5>(); !e) {
                            return e;
                        }
                        progress = 9;
                        return StateValue::inprogress;
                    }
                    default:
                        break;
                }
                return false;
            }

           public:
            virtual State open() {
                switch (progress) {
                    case 0: {
                        auto res = Context::open();
                        if (!res) {
                            return res;
                        }
                        if (thru) {
                            return true;
                        }
                        progress = 1;
                    }
                    case 1: {
                        if (!ctx) {
                            ctx = SSL_CTX_new(TLS_method());
                            if (!ctx) {
                                errmsg->set("failed to make SSL_CTX.\n");
                                call_ssl_error();
                                progress = 0;
                                return false;
                            }
                            SSL_CTX_set_options(ctx, SSL_OP_NO_SSLv2);
                        }
                        progress = 2;
                    }
                    case 2: {
                        if (!io_ssl) {
                            if (!::BIO_new_bio_pair(&io_ssl, 0, &io_base, 0)) {
                                errmsg->set("failed call ::BIO_new_bio_pair\n");
                                call_ssl_error();
                                progress = 0;
                                return false;
                            }
                            ::BIO_up_ref(io_ssl);
                            ::BIO_up_ref(io_base);
                        }
                        else {
                            BIO_reset(io_ssl);
                            BIO_reset(io_base);
                        }
                        progress = 3;
                    }
                    case 3: {
                        ::SSL_free(ssl);
                        ssl = nullptr;
                        ssl = SSL_new(ctx);
                        if (!ssl) {
                            errmsg->set("failed call ::SSL_new\n");
                            call_ssl_error();
                            progress = 0;
                            return false;
                        }
                        ::SSL_set0_rbio(ssl, io_ssl);
                        ::SSL_set0_wbio(ssl, io_ssl);
                        progress = 4;
                    }
                    case 4:
                    case 5:
                    case 6:
                    case 7:
                    case 8:
                        if (auto e = connect(); e == StateValue::inprogress) {
                            return e;
                        }
                        else if (!e) {
                            progress = 0;
                            return e;
                        }
                    default:
                        progress = 0;
                        break;
                }
                return true;
            }
            virtual Context* get_base() override {
                if (!base) return nullptr;
                return std::addressof(*base);
            }

            virtual bool set_setting(const InputSetting& set) override {
                if (auto sl = cast_<SSLInSetting>(&set)) {
                    alpn = sl->alpn;
                    alpnlen = sl->alpnlen;
                    cacert->clear();
                    cacert->set(sl->cacert);
                    thru = sl->thru;
                    return true;
                }
                if (base) {
                    return base->set_setting(set);
                }
                return false;
            }

            virtual bool get_setting(OutputSetting& get) override {
                if (auto sl = cast_<SSLOutSetting>(&get)) {
                    sl->ctx = ctx;
                    sl->ssl = ssl;
                    return true;
                }
                if (base) {
                    return base->get_setting(get);
                }
                return false;
            }

            virtual bool has_error(ErrorInfo* err) override {
                if (errmsg->size()) {
                    if (err) {
                        err->type = ContextType::ssl;
                        err->errmsg = errmsg->c_str();
                        err->numerr = numerr;
                    }
                    return true;
                }
                if (base) {
                    return base->has_error(err);
                }
                return false;
            }
        };
    }  // namespace v3
}  // namespace socklib