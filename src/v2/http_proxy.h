/*
    socklib - simple socket library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "http_base.h"
namespace socklib {
    namespace v2 {
        template <class String, class Header, class Body>
        struct HttpProxy {
            using base_t = HttpBase<String, Header, Body>;
            using request_t = RequestContext<String, Header, Body>;
            bool open(std::shared_ptr<InetConn>& conn, request_t& req, CancelContext* cancel, int prev_version) {
                if (req.proxy.size() == 0) {
                    return false;
                }
                OpenHttpConnRequest<String> ctx;
                ctx.cacert = req.cacert;
                ctx.prev_version = prev_version;
                commonlib2::Reader r(req.proxy);
                r.readwhile(ctx.host, commonlib2::until, ':');
                if (r.expect(":")) {
                    r >> ctx.port;
                }
                auto res = base_t::open_connection(conn, ctx, ctx, ctx.port, ctx.prev_version);
                req.err = ctx.err;
                req.tcperr = ctx.tcperr;
                if (!res) {
                    return false;
                }
            }
        };
    }  // namespace v2
}  // namespace socklib