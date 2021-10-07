#pragma once
#include "http1.h"
#include "http2.h"

namespace socklib {
    namespace v2 {
#define H2TYPE_PARAMS template <class String, template <class...> class Map, class Header, class Body, class Table>

#define TEMPLATE_PARAM <String, Map, Header,Body, Table>

        H2TYPE_PARAMS
        struct H2CClient {
            using conn_t = std::shared_ptr<InetConn>;
            using h1request_t = RequestContext<String, Header, Body>;
            using h2request_t = Http2RequestContext TEMPLATE_PARAM;
            using http1client_t = Http1Client<String, Header, Body>;
            using settingsframe_t = H2SettingsFrame TEMPLATE_PARAM;

            bool open(conn_t& conn, h1request_t& req, h2request_t& ctx, CancelContext* cancel = nullptr) {
                req.request.emplace("Connection", "Upgrade, HTTP2-Settings");
                req.request.emplace("Upgrade", "h2c");
                settingsframe_t frame;
                frame.new_settings() = ctx.local_settings;
                commonlib2::Serializer<String> se;
                frame.serialize(ctx, se, ctx);
                se.get().erase(0, 9);
                commonlib2::Base64Context bctx;
                bctx.nopadding = true;
                bctx.c62 = '-';
                bctx.c63 = '_';
                String base64_encoded;
                commonlib2::Reader(se.get()).readwhile(base64_encoded, commonlib2::base64_encode, &bctx);
                req.request.emplace("HTTP2-Settings", base64_encoded);
                if (!http1client_t::request(conn, req, cancel)) {
                    return false;
                }
                Http1ReadContext<String, Header, Body> read(req);
                req.flag |= RequestFlag::no_read_body;
                if (!http1client_t::response(conn, read, cancel)) {
                    return false;
                }
                if (req.statuscode != 101) {
                    req.err = HttpError::invalid_status;
                    return false;
                }
            }
        };

#undef H2TYPE_PARAMS
#undef TEMPLATE_PARAM
    }  // namespace v2
}  // namespace socklib