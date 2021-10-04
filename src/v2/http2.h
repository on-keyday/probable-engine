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
            using updateframe_t = H2WindowUpdateFrame TEMPLATE_PARAM;
            using settingsframe_t = H2SettingsFrame TEMPLATE_PARAM;
            using stream_t = H2Stream TEMPLATE_PARAM;

            static bool update(h2request_t& ctx, updateframe_t* frame) {
                if (!frame) return false;
                auto up = frame->update();
                auto id = frame->get_id();
                stream_t& stream = ctx.streams[id];
                stream.remote_window += up;
                return true;
            }

            static bool resize(h2request_t& ctx, settingsframe_t* frame) {
                if (!frame) return false;
                auto& settings = frame->new_settings();
                if (auto found = settings.find(key(H2PredefinedSetting::initial_window_size)); found != settings.end()) {
                    std::uint32_t currentwindowsize = frame->old_settings()[key(H2PredefinedSetting::initial_window_size)];
                    std::uint32_t newwindowsize = found->size;
                }
                return true;
            }
        };
#undef H2TYPE_PARAMS
#undef TEMPLATE_PARAM
    }  // namespace v2
}  // namespace socklib