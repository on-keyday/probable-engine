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
            ::SSL_CTX* ctx = nullptr;
            const char* alpn = nullptr;
            size_t alpnlen = 0;

            bool thru = false;

            errno_t numerr = 0;
            StringBuffer* errmsg = nullptr;
            StringBuffer* cacert = nullptr;

           public:
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