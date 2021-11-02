/*
    socklib - simple socket library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "ssl.h"
#include "http_header.h"
#include "url.h"

namespace socklib {
    namespace v3 {
        struct Http1Context : Context {
            OVERRIDE_CONTEXT(ContextType::http1)
            URL* url = nullptr;
            HeaderContext* request = nullptr;
            int status = 0;
            HeaderContext* response = nullptr;
            HeaderFlag hflag = HeaderFlag::verify_header | HeaderFlag::verify_status;
            bool thru = false;
            StringBuffer* tmpbuf1 = nullptr;
            StringBuffer* tmpbuf2 = nullptr;
        };

        struct Http1Conn : Conn {
           private:
            std::shared_ptr<Context> base;

           public:
            virtual State open(ContextManager& m) override {
                if (m.check_id(ctx)) {
                }
                auto res = Conn::open(m);
                if (res != StateValue::inprogress) {
                    state = CtxState::free;
                }
                return res;
            }

            virtual State write(ContextManager& m, const char* data, size_t size) override {
            }

            /*
            virtual bool has_error(ErrorInfo* err) override {
                if (errmsg->size()) {
                    if (err) {
                        err->type = ContextType::http1;
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

            virtual Context* get_base() override {
                if (!base) return nullptr;
                return std::addressof(*base);
            }*/
        };
    }  // namespace v3
}  // namespace socklib
