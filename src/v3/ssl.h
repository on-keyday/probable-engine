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
            const char* hostname = nullptr;
            bool thru = false;
            bool no_shutdown = false;
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

            StringBuffer* errmsg = nullptr;
            StringBuffer* cacert = nullptr;
            StringBuffer* hostname = nullptr;

            ::BIO* io_ssl = nullptr;
            ::BIO* io_base = nullptr;

            StringBuffer* tmpbuf = nullptr;

            CtxState state = CtxState::free;
            int progress = 0;
            errno_t numerr = 0;
            bool thru = false;
            bool no_shutdown = false;

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
                            BIO_flush(io_base);
                            tmpbuf->clear();
                        }
                    }
                    default:
                        break;
                }
                return true;
            }

            bool is_continue_error() {
                auto err = ::SSL_get_error(ssl, -1);
                switch (err) {
                    case SSL_ERROR_WANT_READ:
                    case SSL_ERROR_WANT_WRITE:
                    case SSL_ERROR_SYSCALL:
                        break;
                    default:
                        return false;
                }
                return true;
            }

            State connect() {
                switch (progress) {
                    case 4: {
                        auto res = ::SSL_connect(ssl);
                        if (res <= 0) {
                            if (!is_continue_error()) {
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
                if (state != CtxState::free && state != CtxState::opening) {
                    errmsg->clear();
                    errmsg->set("invalid state. now ");
                    errmsg->append_back(state_value(state));
                    errmsg->append_back(", need free or opening");
                    numerr = -1;
                    return false;
                }
                switch (progress) {
                    case 0: {
                        state = CtxState::opening;
                        auto res = Context::open();
                        if (!res) {
                            return res;
                        }
                        if (thru) {
                            state = CtxState::free;
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
                                state = CtxState::free;
                                return false;
                            }
                            SSL_CTX_set_options(ctx, SSL_OP_NO_SSLv2);
                        }
                        if (!::SSL_CTX_load_verify_locations(ctx, cacert->c_str(), nullptr)) {
                            errmsg->set("faild call ::SSL_CTX_load_verify_locations\n");
                            call_ssl_error();
                            progress = 0;
                            state = CtxState::free;
                        }
                        progress = 2;
                    }
                    case 2: {
                        if (!io_ssl) {
                            if (!::BIO_new_bio_pair(&io_ssl, 0, &io_base, 0)) {
                                errmsg->set("failed call ::BIO_new_bio_pair\n");
                                call_ssl_error();
                                progress = 0;
                                state = CtxState::free;
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
                            state = CtxState::free;
                            return false;
                        }
                        if (alpn) {
                            if (!::SSL_set_alpn_protos(ssl, (const unsigned char*)alpn, (unsigned int)alpnlen)) {
                                errmsg->set("failed call ::SSL_set_alpn_protos\n");
                                call_ssl_error();
                                ::SSL_free(ssl);
                                ssl = nullptr;
                                progress = 0;
                                state = CtxState::free;
                                return false;
                            }
                        }
                        if (hostname->size()) {
                            if (!SSL_set_tlsext_host_name(ssl, hostname->c_str())) {
                                errmsg->set("failed call SSL_set_tlsext_host_name\n");
                                call_ssl_error();
                                ::SSL_free(ssl);
                                ssl = nullptr;
                                progress = 0;
                                state = CtxState::free;
                                return false;
                            }
                            auto param = ::SSL_get0_param(ssl);
                            if (!::X509_VERIFY_PARAM_add1_host(param, hostname->c_str(), 0)) {
                                errmsg->set("failed call ::X509_VERIFY_PARAM_add1_host\n");
                                call_ssl_error();
                                ::SSL_free(ssl);
                                ssl = nullptr;
                                progress = 0;
                                state = CtxState::free;
                                return false;
                            }
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
                            state = CtxState::free;
                            progress = 0;
                            return e;
                        }
                    default:
                        state = CtxState::free;
                        progress = 0;
                        break;
                }
                return true;
            }

           private:
            template <CtxState st>
            bool check_valid() {
                if (state != CtxState::free && state != st) {
                    errmsg->set("invalid state. now ");
                    errmsg->append_back(state_value(state));
                    errmsg->append_back(", need free or opening");
                    numerr = -1;
                    return false;
                }
                if (!thru && !ssl) {
                    errmsg->set("call open() before ");
                    errmsg->append_back(state_value(st));
                    numerr = -1;
                    return false;
                }
                return true;
            }

           public:
            virtual State write(const char* data, size_t size) override {
                if (!check_valid<CtxState::writing>()) {
                    return false;
                }
                switch (progress) {
                    case 0:
                        state = CtxState::writing;
                        if (thru) {
                            if (auto e = Context::write(data, size); e == StateValue::inprogress) {
                                return e;
                            }
                            else {
                                state = CtxState::free;
                                return e;
                            }
                        }
                        progress = 1;
                    case 1: {
                        size_t w = 0;
                        if (::SSL_write_ex(ssl, data, size, &w) <= 0) {
                            if (!is_continue_error()) {
                                errmsg->set("failed call ::SSL_write_ex()\n");
                                call_ssl_error();
                                return false;
                            }
                        }
                        else {
                            state = CtxState::free;
                            progress = 0;
                            return true;
                        }
                    }
                    case 2:
                    case 3:
                    case 4:
                    case 5:
                        if (auto e = read_write_bio<2>(); e == StateValue::inprogress) {
                            return e;
                        }
                        else if (!e) {
                            progress = 0;
                            state = CtxState::free;
                            return true;
                        }
                        progress = 1;
                    default:
                        return StateValue::inprogress;
                }
            }

            virtual State read(char* data, size_t size, size_t& red) override {
                if (!check_valid<CtxState::reading>()) {
                    return false;
                }
                switch (progress) {
                    case 0:
                        state = CtxState::writing;
                        if (thru) {
                            if (auto e = Context::read(data, size, red); e == StateValue::inprogress) {
                                return e;
                            }
                            else {
                                state = CtxState::free;
                                return e;
                            }
                        }
                        progress = 1;
                    case 1: {
                        if (::SSL_read_ex(ssl, data, size, &red) <= 0) {
                            if (!is_continue_error()) {
                                errmsg->set("failed call ::SSL_write_ex()\n");
                                call_ssl_error();
                                return false;
                            }
                        }
                        else {
                            state = CtxState::free;
                            progress = 0;
                            return true;
                        }
                    }
                    case 2:
                    case 3:
                    case 4:
                    case 5:
                        if (auto e = read_write_bio<2>(); e == StateValue::inprogress) {
                            return e;
                        }
                        else if (!e) {
                            progress = 0;
                            state = CtxState::free;
                            return true;
                        }
                        progress = 1;
                    default:
                        return StateValue::inprogress;
                }
            }

            virtual Context* get_base() override {
                if (!base) return nullptr;
                return std::addressof(*base);
            }

            virtual bool set_setting(const InputSetting& set) override {
                if (auto sl = cast_<SSLInSetting>(&set)) {
                    alpn = sl->alpn;
                    alpnlen = sl->alpnlen;
                    if (sl->cacert) {
                        cacert->clear();
                        cacert->set(sl->cacert);
                    }
                    thru = sl->thru;
                    if (sl->hostname) {
                        hostname->clear();
                        hostname->set(sl->hostname);
                    }
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

           private:
            State close_impl(bool force_close) {
                if (state != CtxState::closing) {
                    state = CtxState::closing;
                    progress = 0;
                }
                switch (progress) {
                    case 0:
                        if (ssl && !no_shutdown && !force_close) {
                            auto res = ::SSL_shutdown(ssl);
                            if (res <= 0 && is_continue_error()) {
                                return StateValue::inprogress;
                            }
                            else {
                                progress = 5;
                                break;
                            }
                        }
                        else {
                            progress = 5;
                            break;
                        }
                    case 1:
                    case 2:
                    case 3:
                    case 4:
                        if (auto e = read_write_bio<1>(); e == StateValue::inprogress) {
                            return e;
                        }
                        else if (e) {
                            progress = 0;
                            return StateValue::inprogress;
                        }
                        progress = 5;
                    default:
                        break;
                }
                if (progress == 5) {
                    ::SSL_free(ssl);
                    ssl = nullptr;
                    progress = 6;
                }
                if (progress == 6) {
                    if (auto e = Context::close(); !force_close && e == StateValue::inprogress) {
                        return e;
                    }
                    progress = 7;
                }
                numerr = 0;
                state = CtxState::free;
                progress = 0;
                return true;
            }

           public:
            virtual State close() {
                return close_impl(false);
            }

            ~SSLContext() {
                size_t count = 0;
                while (count < 100 && close() == StateValue::inprogress) {
                    count++;
                }
                if (count == 100) {
                    close_impl(true);
                }
            }
        };
    }  // namespace v3
}  // namespace socklib
