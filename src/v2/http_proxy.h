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
                request_t proxyinfo;
                proxyinfo.default_path = DefaultPath::host_port;
                base_t::open_connection(conn, req, cancel, prev_version);
            }
        };
    }  // namespace v2
}  // namespace socklib