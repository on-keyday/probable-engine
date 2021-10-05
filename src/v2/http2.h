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
        struct StreamFlowChecker {
            using h2request_t = Http2RequestContext TEMPLATE_PARAM;
            static bool data_sendable(H2StreamState state) {
                return state == H2StreamState::open || state == H2StreamState::half_closed_remote;
            }

            static bool header_handleable(H2StreamState state) {
                return state == H2StreamState::idle;
            }

            static H2StreamState sent_endstream(H2StreamState state) {
                if (state == H2StreamState::open) {
                    return H2StreamState::half_closed_local;
                }
                else {
                    return H2StreamState::closed;
                }
            }

            static H2StreamState handle_rst_stream() {
                return H2StreamState::closed;
            }

            static H2StreamState handled_header(H2StreamState state) {
                if (state == H2StreamState::idle) {
                    return H2StreamState::open;
                }
                return H2StreamState::closed;
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
            using basewriter_t = H2Writer TEMPLATE_PARAM;
            using stream_t = H2Stream;
            using checker_t = StreamFlowChecker TEMPLATE_PARAM;
            using settings_t = typename h2request_t::settings_t;
            using manager_t = StreamManager TEMPLATE_PARAM;
            using header_t = Header;

            static stream_t* get_stream(F(H2Frame) & frame, std::int32_t id, h2request_t& ctx) {
                auto found = ctx.streams.find(id);
                if (!hframe.set_id(req.streamid) || found == ctx.streams.end()) {
                    ctx.err = H2Error::protocol;
                    return ctx.err;
                }
                return &found->second;
            }

            static H2Err set_http2_header(header_t& towrite, h1request_t& req) {
                auto set_header = [&](auto& header) -> H2Err {
                    for (auto& h : header) {
                        if (auto e = base_t::is_valid_field(h, req); e < 0) {
                            ctx.err = H2Error::http1_semantics_error;
                            return ctx.err;
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
                    return H2Error::none;
                };
                if (ctx.server) {
                    if (auto e = set_header(req.response); !e) {
                        return e;
                    }
                    towrite.emplace(":status", std::to_string(req.statuscode).c_str())
                }
                else {
                    if (auto e = set_header(req.request); !e) {
                        return e;
                    }
                    string_t path;
                    base_t::write_path(path, req);
                    towrite.emplace(":method", req.method);
                    towrite.emplace(":authority", urlparser_t::host_with_port(req.parsed));
                    towrite.emplace(":path", path);
                    towrite.emplace(":scheme", req.parsed.scheme);
                }
                return true;
            }

            static H2Err write_header(conn_t& conn, h1request_t& req, h2request_t& ctx, bool closable = false,
                                      CancelContext* cancel = nullptr, H2Weight* weight = nullptr, std::uint8_t* padlen = nullptr) {
                F(H2HeaderFrame)
                hframe;
                if (req.streamid <= 0) {
                    ctx.err = H2Error::protocol;
                    return false;
                }
                auto stream = get_stream(hframe, req.streamid, ctx);
                if (!stream) {
                    return ctx.err;
                }
                if (!checker_t::header_handlable(stream->state)) {
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
                if (auto e = set_http2_header(towrite, req); !e) {
                    return e;
                }
                if (closable) {
                    hframe.add_flag(H2Flag::end_stream);
                }
                hframe.add_flag(H2Flag::end_headers);
                auto e = basewriter_t::write(conn, hframe, req, ctx, cancel);
                if (e) {
                    stream->state = checker_t::handled_header(stream->state);
                    if (closable) {
                        stream->state = checker_t::sent_endstream(stream->state);
                    }
                }
                return e;
            }

            static H2Err write_data(conn_t& conn, h1request_t& req, h2request_t& ctx, bool closeable = true,
                                    CancelContext* cancel = nullptr, std::uint8_t* padlen = nullptr) {
                F(H2DataFrame)
                dframe;
                if (req.streamid <= 0) {
                    ctx.err = H2Error::protocol;
                    return ctx.err;
                }
                auto stream = get_stream(dframe, req.streamid, ctx);
                if (!stream) {
                    return ctx.err;
                }
                stream_t& stream0 = ctx.streams[0];
                if (padlen) {
                    dframe.set_padding(*padlen);
                    dframe.add_flag(H2Flag::padded);
                }
                if (!cheker_t::data_sendable(stream.state)) {
                    ctx.err = H2Error::protocol;
                    return ctx.err;
                }
                if (stream->remote_window <= 0 || stream0.remote_window <= 0) {
                    ctx.err = H2Error::need_window_update;
                    return ctx.err;
                }
                auto& body = ctx.server ? req.responsebody : req.requestbody;
                size_t total = body.size();
                if (total < stream->data_progress) {
                    return true;
                }
                size_t remainsize = total - stream.data_progress;
                size_t window = stream0.remote_window < stream->remote_window ? stream0.remote_window : stream->remote_window;
                size_t opt = 0;
                if (padlen) {
                    opt = (*padlen) + 1;
                    if (window < opt) {
                        return H2Error::need_window_update;
                    }
                }
                size_t towrite = remainsize < window - opt ? remainsize + opt : window;
                std::string_view view(body.data() + stream.data_progress, towrite - opt);
                auto check_end = [&] {
                    return stream->data_progress + towrite - opt == body.size();
                };
                if (check_end() && closeable) {
                    dframe.add_flag(H2Flag::end_stream);
                }
                for (auto c : view) {
                    dframe.payload().push_back(c);
                }
                auto e = basewriter_t::write(conn, dframe, req, ctx, cancel);
                if (e) {
                    stream0.remote_window -= towrite;
                    stream->remote_window -= towrite;
                    if (check_end()) {
                        ctx.err = H2Error::none;
                        if (closeable) {
                            stream->state = checker_t::sent_endstream(stream->state);
                        }
                    }
                    else {
                        ctx.err = H2Error::need_window_update;
                    }
                    stream->data_progress += towrite - opt;
                    return ctx.err;
                }
                return e;
            }

            static H2Err write_priority(conn_t& conn, h1request_t& req, h2request_t& ctx, const H2Weight& weight,
                                        CancelContext* cancel = nullptr) {
                F(H2PriorityFrame)
                pframe;
                if (req.streamid <= 0) {
                    ctx.err = H2Error::protocol;
                    return ctx.err;
                }
                if (!get_stream(pframe, req.streamid, ctx)) {
                    return ctx.err;
                }
                pframe.set_weight(weight);
                return basewriter_t::write(conn, pframe, req, ctx, cancel);
            }

            static H2Err write_rst_stream(conn_t& conn, h1request_t& req, h2request_t& ctx, std::uint32_t errorcode,
                                          CancelContext* cancel = nullptr) {
                F(H2RstStreamFrame)
                rframe;
                if (req.streamid <= 0) {
                    ctx.err = H2Error::protocol;
                    return ctx.err;
                }
                auto stream = get_stream(rframe, req.streamid, ctx);
                if (!stream) {
                    return ctx.err;
                }
                rframe.set_code(errorcode);
                auto e = basewriter_t::write(conn, rframe, req, ctx, cancel);
                if (e) {
                    stream->state = checker_t::handle_rst_stream();
                }
                return e;
            }

            static H2Err write_settings(conn_t& conn, h1request_t& req, h2request_t& ctx, bool ack, const settings_t& setting = settings_t(), CancelContext* cancel = nullptr, settings_t* old = nullptr) {
                F(H2SettingsFrame)
                sframe;
                sframe.set_id(0);
                if (ack) {
                    sframe.add_flag(H2Flag::ack);
                }
                else {
                    sframe.new_settings() = setting;
                }
                auto e = basewriter_t::write(conn, sframe, req, ctx, cancel);
                if (e) {
                    if (!ack && old) {
                        *old = std::move(sframe.old_settings());
                    }
                }
                return e;
            }

            static H2Err write_push_promise(conn_t& conn, h1request_t& req, h2request_t& ctx) {
                if (!ctx.server || !manager_t::enable_push(ctx)) {
                    return ctx.err;
                }
                F(H2PushPromiseFrame)
                pframe;
                pframe.header_map()
            }
#undef F
        };

#undef H2TYPE_PARAMS
#undef TEMPLATE_PARAM
    }  // namespace v2
}  // namespace socklib