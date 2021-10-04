/*
    socklib - simple socket library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "http2_frames.h"
namespace socklib {
    namespace v2 {
#define H2TYPE_PARAMS template <class String, template <class...> class Map, class Header, class Body, class Table>

#define TEMPLATE_PARAM <String, Map, Header,Body, Table>

        H2TYPE_PARAMS
        struct WindowUpdater {
            using h2request_t = Http2RequestContext TEMPLATE_PARAM;
            using h1request_t = RequestContext<String, Header, Body>;
            using updateframe_t = H2WindowUpdateFrame TEMPLATE_PARAM;
            using stream_t = H2Stream TEMPLATE_PARAM;
            static H2Err update(h2request_t& ctx, h1request_t& req, updateframe_t* frame) {
                if (!frame) return false;
                auto up = frame->update();
                auto id = frame->get_id();
                stream_t& stream = ctx.streams[id];
            }
        };
#undef H2TYPE_PARAMS
#undef TEMPLATE_PARAM
    }  // namespace v2
}  // namespace socklib