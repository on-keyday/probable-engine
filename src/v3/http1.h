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
            int header_version = 0;
            const char* method = nullptr;
            HeaderContext* request = nullptr;
            int statuscode = 0;
            HeaderContext* response = nullptr;
            HeaderFlag hflag = HeaderFlag::verify_header | HeaderFlag::verify_status;
            bool thru = false;
            StringBuffer* tmpbuf1 = nullptr;
            StringBuffer* tmpbuf2 = nullptr;
        };

        struct Http1Conn : Conn {
           private:
            using header_t = HeaderIO;
            Http1Context* ctx;

           public:
            virtual State open(ContextManager& m) override {
                if (auto e = m.check_id(ctx); !e) {
                    return e;
                }
                auto res = Conn::open(m);
                if (res != StateValue::inprogress) {
                    ctx->report(nullptr);
                    ctx = nullptr;
                }
                return res;
            }

            virtual State write(ContextManager& m, const char* data, size_t size) override {
                switch (ctx->get_progress()) {
                    case 0:
                        ctx->tmpbuf1->clear();
                        ctx->tmpbuf2->clear();
                        ctx->request->remove("host");
                        ctx->request->remove("content-length");
                        ctx->request->remove("transfer-encoding");
                        ImportantHeader imh;
                        imh.host = ctx->url->host_port();
                        if (!header_t::write_request(*ctx->tmpbuf1,
                                                     ctx->method,
                                                     ctx->url->path_query(),
                                                     *ctx->request,
                                                     ctx->header_version,
                                                     ctx->hflag, &imh, ctx->tmpbuf2)) {
                            ctx->report("failed to write http header");
                            ctx = nullptr;
                            return false;
                        }
                        ctx->increment();
                    case 1:
                        if (auto e = Conn::write(m, ctx->tmpbuf1->c_str(), ctx->tmpbuf1->size()); e == StateValue::inprogress) {
                            return e;
                        }
                        else if (!e) {
                            ctx = nullptr;
                            return e;
                        }
                        ctx->increment();
                    case 2:
                        if (data && size) {
                            if (auto e = Conn::write(m, data, size); e == StateValue::inprogress) {
                                return e;
                            }
                            else if (!e) {
                                return e;
                            }
                        }
                    default:
                        ctx->report(nullptr);
                        ctx = nullptr;
                }
                return true;
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
