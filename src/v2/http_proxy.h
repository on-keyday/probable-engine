/*
    socklib - simple socket library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "http.h"
namespace socklib {
    namespace v2 {
        template <class String, class Header, class Body, template <class...> class Map, class Table>
        struct ProxyConn : InetConn {
            ClientRequestProxy<String, Header, Body, Map, Table> req;
        };

        template <class String, class Header, class Body>
        struct Http1Proxy {
            using base_t = HttpBase<String, Header, Body>;
            using request_t = RequestContext<String, Header, Body>;

            bool open(std::shared_ptr<InetConn>& conn, request_t& req, CancelContext* cancel, int prev_version) {
                OpenHttpConnRequest<String> ctx;
                ctx.cacert = req.cacert;
                ctx.prev_version = prev_version;
                ctx.http_version = req.http_version;
                commonlib2::Reader r(req.proxy);
                r.readwhile(ctx.host, commonlib2::until<char>, ':');
                if (r.expect(":")) {
                    r >> ctx.port;
                }
                std::shared_ptr<InetConn> tmpconn = conn;
                auto res = base_t::open_connection(tmpconn, ctx, ctx, ctx.port, ctx.prev_version);
                req.err = ctx.err;
                req.tcperr = ctx.tcperr;
                if (!res) {
                    return false;
                }
                if (!base_t::verify_alpn(conn, req)) {
                    req.err = ctx.err;
                    return false;
                }
                conn = std::move(tmpconn);
                return true;
            }
        };
    }  // namespace v2
}  // namespace socklib