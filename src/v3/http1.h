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

        struct Http1Context : IContext<ContextType::http1> {
           private:
            std::shared_ptr<Context> base;
            URL* url = nullptr;
            HeaderContext* request = nullptr;
            int status = 0;
            HeaderContext* response = nullptr;
            HeaderFlag hflag = HeaderFlag::verify_header | HeaderFlag::verify_status;
            CtxState state = CtxState::free;
            int progress = 0;
            errno_t numerr = 0;
            bool thru = false;
            StringBuffer* errmsg = nullptr;
            StringBuffer* tmpbuf1 = nullptr;
            StringBuffer* tmpbuf2 = nullptr;

           public:
            virtual State open() {
                if (state != CtxState::free && state != CtxState::opening) {
                    errmsg->clear();
                    errmsg->set("invalid state. now ");
                    errmsg->append_back(state_value(state));
                    errmsg->append_back(", need free or opening");
                    numerr = -1;
                    return false;
                    return false;
                }
                state = CtxState::opening;
                progress = 0;
                auto res = Context::open();
                if (res != StateValue::inprogress) {
                    state = CtxState::free;
                }
                return res;
            }

            virtual State write(const char* data, size_t size) override {
            }

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
            }
        };
    }  // namespace v3
}  // namespace socklib
