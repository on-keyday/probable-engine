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

            ::BIO* read_ssl = nullptr;
            ::BIO* write_ssl = nullptr;
            ::BIO* write_base = nullptr;
            ::BIO* read_base = nullptr;

            void call_ssl_error() {
                numerr = ERR_get_error();
                ERR_print_errors_cb(
                    [](const char* str, size_t len, void* u) -> int {
                        auto buf = (StringBuffer*)u;
                        buf->append_back(str, len);
                        buf->append_back("\n");
                        return 1;
                    },
                    (void*)errmsg);
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
                        if (!read_ssl) {
                            if (!::BIO_new_bio_pair(&read_ssl, 0, &write_ssl, 0)) {
                                errmsg->set("failed call ::BIO_new_bio_pair with read_ssl and write_ssl\n");
                                call_ssl_error();
                                progress = 0;
                                return false;
                            }
                            if (!::BIO_new_bio_pair(&read_base, 0, &write_base, 0)) {
                                errmsg->set("failed call ::BIO_new_bio_pair with read_base and write_base\n");
                                call_ssl_error();
                                progress = 0;
                                return false;
                            }
                            ::BIO_up_ref(read_ssl);
                            ::BIO_up_ref(write_ssl);
                            ::BIO_up_ref(read_base);
                            ::BIO_up_ref(write_base);
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
                        ::SSL_set0_rbio(ssl, read_base);
                        ::SSL_set0_wbio(ssl, write_ssl);
                        progress = 4;
                    }
                    case 4: {
                    }
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