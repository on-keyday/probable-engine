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
                    std::uint32_t current_initial = frame->old_settings()[key(H2PredefinedSetting::initial_window_size)];
                    std::uint32_t new_initial = found->size;
                    for (auto& stpair : ctx.streams) {
                        //refer URL below (Qiita) (Japanese)
                        //https://qiita.com/Jxck_/items/622162ad8bcb69fa043d#%E9%80%94%E4%B8%AD%E3%81%A7%E3%81%AE-settings-frame
                        stream_t& stream = stpair.second;
                        std::int64_t& current_window = stream.remote_window;
                        std::int64_t& new_window = stream.remote_window;
                        new_window = new_initial - (current_initial - current_window);
                    }
                }
                return true;
            }
        };

        H2TYPE_PARAMS
        struct StreamManager {
            using h1request_t = RequestContext<String, Header, Body>;
            using h2request_t = Http2RequestContext TEMPLATE_PARAM;
            using stream_t = H2Stream TEMPLATE_PARAM;
            using settings_t = typename h2request_t::settings_t;
            using frame_t = H2Frame TEMPLATE_PARAM;

            static void set_default_settings_value(settings_t& s) {
                s[key(H2PredefinedSetting::header_table_size)] = 4096;
                s[key(H2PredefinedSetting::enable_push)] = 1;
                s[key(H2PredefinedSetting::max_concurrent_streams)] = ~0;
                s[key(H2PredefinedSetting::initial_window_size)] = 65535;
                s[key(H2PredefinedSetting::max_frame_size)] = 16384;
                s[key(H2PredefinedSetting::max_header_list_size)] = ~0;
            }

            static void set_initial_window_size(stream_t& stream, h2request_t& ctx) {
                stream.local_window = ctx.local_settings[key(H2PredefinedSetting::initial_window_size)];
                stream.remote_window = ctx.remote_settings[key(H2PredefinedSetting::initial_window_size)];
            }

            static void init_streams(h2request_t& ctx, bool server = false) {
                ctx = h2request_t{};
                ctx.server = server;
                set_default_settings_value(ctx.local_settings);
                set_default_settings_value(ctx.remote_settings);
                set_initial_window_size(ctx.streams[0]);
            }

            static bool verify_id(std::int32_t id, bool server = false) {
                if (server) {
                    return ((id) % 2 == 0);
                }
                else {
                    return ((id) % 2 == 1);
                }
            }

            static bool enable_push(h2request_t& ctx) {
                if (!ctx.remote_settings[key(H2PredefinedSetting::enable_push)]) {
                    ctx.err = H2Error::protocol;
                    return false;
                }
                return true;
            }

            static H2Err make_new_stream(std::int32_t& save, h2request_t& ctx) {
                std::int32_t id = 0;
                if (ctx.server && !enable_push(ctx)) {
                    return ctx.err;
                }
                id = verify_id(ctx.max_stream + 1, ctx.server) ? ctx.max_stream + 1 : ctx.max_stream + 2;
                set_initial_window_size(ctx.streams[id]);
                save = id;
                ctx.max_stream = id;
                return true;
            }

            static H2Err accept_frame(std::shared_ptr<frame_t>& frame, h2request_t& ctx) {
                if (!frame) return false;
                frame_t& f = *frame;
                std::int32_t id = f.get_id();
                if (auto found = ctx.streams.find(id); found == ctx.streams.end()) {
                    if (ctx.max_stream < id) {
                        ctx.err = H2Error::protocol;
                        return ctx.err;
                    }
                    set_initial_window_size(ctx.streams[id]);
                }
                return true;
            }
        };

        H2TYPE_PARAMS
        struct StreamFlowUpdater {
            using h2request_t = Http2RequestContext TEMPLATE_PARAM;
            static bool update() {
            }
        };

        H2TYPE_PARAMS
        struct H2FrmaeWriter {
#define F(TYPE) TYPE TEMPLATE_PARAM
            using conn_t = std::shared_ptr<InetConn>;
            using h1request_t = RequestContext<String, Header, Body>;
            using h2request_t = Http2RequestContext TEMPLATE_PARAM;
            using errorhandle_t = ErrorHandler<String, Header, Body>;
            using base_t = HttpBase<String, Header, Body>;
            using string_t = String;
            using urlparser_t = URLParser<String, Header, Body>;

            bool write_header(conn_t& conn, h1request_t& req, h2request_t& ctx, H2Weight* weight = nullptr, std::uint8_t* padlen = nullptr) {
                F(H2HeaderFrame)
                hframe;
                if (!hframe.set_id()) {
                    ctx.err = H2Error::protocol;
                    return false;
                }
                if (padlen) {
                    hframe.set_padding(*padlen);
                    hframe.add_flag(H2Flag::padded);
                }
                if (weight) {
                    hframe.set_weight(*weight);
                    hframe.add_flag(H2Flag::priority);
                }
                auto& towrite = hframe.header_map();
                auto set_header = [&](auto& header) {
                    for (auto& h : header) {
                        if (auto e = base_t::is_valid_field(h, req); e < 0) {
                            ctx.err = H2Error::http1_semantics_error;
                            return false;
                        }
                        else if (e == 0) {
                            continue;
                        }
                        string_t key, value;
                        auto to_lower = [](auto& base, auto& str) {
                            std::transform(base.begin(), base.end(), str.begin(), [](unsigned char c) { return std::tolower(c); });
                        };
                        to_lower(h.first, key);
                        to_lower(h.second, value);
                        towrite.emplace(key, value);
                    }
                    return false;
                };
                if (ctx.server) {
                    if (!set_header(req.response)) {
                        return false;
                    }
                    towrite.emplace(":status", std::to_string(req.statuscode).c_str())
                }
                else {
                    if (!set_header(req.request)) {
                        return false;
                    }
                    string_t path;
                    base_t::write_path(path, req);
                    towrite.emplace(":method", req.method);
                    towrite.emplace(":authority", urlparser_t::host_with_port(req.parsed));
                    towrite.emplace(":path", path);
                    towrite.emplace(":scheme", req.parsed.scheme);
                }
            }
#undef F
        };

#undef H2TYPE_PARAMS
#undef TEMPLATE_PARAM
    }  // namespace v2
}  // namespace socklib