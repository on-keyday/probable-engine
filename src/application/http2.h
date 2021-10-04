/*
    socklib - simple socket library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once

#include "http2_base.h"

namespace socklib {
//Rust like try
#define TRY(...) \
    if (auto _H_tryres = (__VA_ARGS__); !_H_tryres) return _H_tryres
    enum class H2StreamState {
        idle,
        open,
        reserved_local,
        reserved_remote,
        half_closed_local,
        half_closed_remote,
        closed
    };

    constexpr auto h2_connection_preface = "PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n";

    struct H2Stream {
        int streamid = 0;
        H2StreamState state = H2StreamState::idle;
        std::string path_;
        std::string query_;
        HttpConn::Header header;
        std::string payload;
        Http2Conn* conn = nullptr;
        int depend = 0;
        std::uint8_t weight = 0;
        bool exclusive = false;

        std::int32_t remote_window = 0;

        std::uint32_t errorcode = 0;
        H2Stream() {}

        H2Stream(int id, Http2Conn* c)
            : streamid(id), conn(c), remote_window(c ? c->remote_settings[(std::uint32_t)H2PredefinedSetting::initial_window_size] : 0xffff) {}

        const std::string& path() const {
            return path_;
        }

        const std::string& query() const {
            return query_;
        }

        H2Err recv_apply(std::shared_ptr<H2Frame>& frame) {
            TRY(conn && frame && (frame->streamid == streamid));
            if (auto h = frame->header()) {
                state = H2StreamState::open;
                for (auto& v : h->header_) {
                    header.insert(v);
                }
                if (any(h->flag & H2Flag::priority)) {
                    if (streamid == h->depends) {
                        return H2Error::protocol;
                    }
                    depend = h->depends;
                    weight = h->weight;
                    exclusive = h->exclusive;
                }
                if (h->is_set(H2Flag::end_stream)) {
                    state = H2StreamState::closed;
                }
            }
            else if (auto d = frame->data()) {
                payload.append(d->data_);
                if (d->is_set(H2Flag::end_stream)) {
                    state = H2StreamState::closed;
                }
            }
            else if (auto p = frame->priority()) {
                if (streamid == p->depends) {
                    return H2Error::protocol;
                }
                depend = p->depends;
                weight = p->weight;
                exclusive = p->exclusive;
            }
            else if (auto r = frame->rst_stream()) {
                errorcode = r->errcode;
                state = H2StreamState::closed;
                return (H2Error)errorcode;
            }
            else if (auto e = frame->goaway()) {
                errorcode = e->errcode;
                conn->close();
                return (H2Error)errorcode;
            }
            else if (auto p = frame->ping()) {
                if (!p->is_set(H2Flag::ack)) {
                    send_ping(p->data_, true);
                }
            }
            else if (auto s = frame->settings()) {
                if (!s->is_set(H2Flag::ack)) {
                    send_settings({}, true);
                }
            }
            else if (auto w = frame->window_update()) {
                if (streamid == 0) {
                    conn->remote_window += w->value;
                }
                else {
                    remote_window += w->value;
                }
            }
            return true;
        }

        [[nodiscard]] H2Err send_data(const char* data, size_t size, size_t* suspended = nullptr, bool padding = false, std::uint8_t padlen = 0, bool endstream = false) {
            TRY(conn && streamid != 0);
            if (conn->remote_window <= 0) {
                return H2Error::need_window_update;
            }
            if (remote_window <= 0) {
                return H2Error::need_window_update;
            }
            size_t remainsize = size;
            size_t offset = 0;
            if (suspended) {
                if (size < *suspended) {
                    return false;
                }
                remainsize -= *suspended;
                offset = *suspended;
            }
            H2DataFrame frame;
            if (padding) {
                frame.flag |= H2Flag::padded;
                frame.padding = padlen;
            }
            else {
                padlen = 0;
            }
            frame.streamid = streamid;
            std::int32_t window = remote_window < conn->remote_window ? remote_window : conn->remote_window;
            std::int32_t sent = remainsize + padlen < window - padlen ? remainsize : window - padlen;
            frame.data_ = std::string(data + offset, sent);
            if (sent == remainsize && endstream) {
                frame.flag |= H2Flag::end_stream;
                state = state == H2StreamState::open ? H2StreamState::half_closed_local : H2StreamState::closed;
            }
            if (auto e = conn->send(frame); !e) {
                return e;
            }
            remote_window -= sent;
            conn->remote_window -= sent;
            if (sent < remainsize) {
                if (suspended) {
                    *suspended += sent;
                }
                return H2Error::need_window_update;
            }
            else {
                if (suspended) {
                    *suspended = size;
                }
            }
            return true;
        }

        H2Err send_header(const HttpConn::Header& header,
                          bool padding = false, std::uint8_t padlen = 0, bool endstream = false,
                          bool has_priority = false, bool exclusive = false, int depends = 0, std::uint8_t weight = 0) {
            TRY(conn && streamid != 0);
            if (state != H2StreamState::idle) return false;
            state = H2StreamState::open;
            H2HeaderFrame frame;
            frame.streamid = streamid;
            frame.header_ = header;
            if (padding) {
                frame.flag |= H2Flag::padded;
                frame.padding = padlen;
            }
            if (endstream) {
                frame.flag |= H2Flag::end_stream;
            }
            if (has_priority) {
                frame.flag |= H2Flag::priority;
                frame.exclusive = exclusive;
                frame.depends = depends;
                frame.weight = weight;
            }
            return conn->send(frame);
        }

        template <class Setting = std::map<unsigned short, unsigned int>>
        H2Err send_settings(Setting&& settings, bool ack = false) {
            TRY(conn && streamid == 0);
            H2SettingsFrame frame;
            frame.streamid = 0;
            if (ack) {
                frame.flag |= H2Flag::ack;
                return conn->send(frame);
            }
            if (!conn->local_settings.size()) {
                conn->set_default_settings(conn->local_settings);
            }
            for (auto& st : settings) {
                conn->local_settings[st.first] = st.second;
            }
            return conn->send(frame);
        }

        H2Err send_windowupdate(int up) {
            TRY(conn);
            H2WindowUpdateFrame frame;
            frame.streamid = streamid;
            frame.value = up;
            return conn->send(frame);
        }

        H2Err send_goaway(H2Error error, const char* data = nullptr, size_t size = 0) {
            TRY(conn);
            H2GoAwayFrame frame;
            frame.errcode = (std::uint32_t)error;
            frame.lastid = conn->maxid;
            if (data && size) frame.additionaldata = std::string(data, size);
            return conn->send(frame);
        }

        H2Err send_rst_stream(H2Error error) {
            TRY(conn);
            H2RstStreamFrame frame;
            frame.errcode = (std::uint32_t)error;
            state = H2StreamState::closed;
            return conn->send(frame);
        }

        H2Err send_ping(const std::uint8_t* data, bool ack) {
            H2PingFrame frame;
            if (data) {
                for (auto i = 0; i < 8; i++) {
                    frame.data_[i] = data[i];
                }
            }
            if (ack) {
                frame.flag |= H2Flag::ack;
            }
            return conn->send(frame);
        }
    };

    struct Http2Context : Http2Conn {
       private:
        bool server = false;
        std::map<int, H2Stream> streams;
        std::string host_;
        friend struct Http2;
        friend struct HttpClient;

       public:
        Http2Context(std::shared_ptr<Conn>&& conn, std::string&& host)
            : Http2Conn(std::move(conn)), host_(std::move(host)) {}

        void clear() override {
            streams.clear();
            Http2Conn::clear();
        }

        H2Err apply(std::shared_ptr<H2Frame>& frame, H2Stream*& stream) {
            stream = nullptr;
            TRY((bool)frame);
            TRY(frame->streamid >= 0);
            if (auto found = streams.find(frame->streamid); found != streams.end()) {
                stream = &found->second;
                auto e = found->second.recv_apply(frame);
                if (auto s = frame->settings(); e && s) {
                    auto current = remote_settings[(std::uint16_t)H2PredefinedSetting::initial_window_size];
                    if (initial_window != current) {
                        for (auto& st : streams) {
                            st.second.remote_window = current - (initial_window - st.second.remote_window);
                        }
                        remote_window = current - (initial_window - remote_window);
                        initial_window = current;
                    }
                }
                return e;
            }
            else {
                auto& tmp = streams[frame->streamid] = H2Stream(frame->streamid, this);
                stream = &tmp;
                return tmp.recv_apply(frame);
            }
        }

        bool is_valid_id(int id) {
            return maxid < id && (server ? id % 2 == 0 : id % 2 == 1);
        }

        bool make_stream(H2Stream*& stream, const std::string& path, const std::string& query) {
            return make_stream(is_valid_id(maxid + 1) ? maxid + 1 : maxid + 2, stream, path, query);
        }

        bool get_stream(H2Stream*& stream) {
            return get_stream(maxid, stream);
        }

        bool make_stream(int id, H2Stream*& stream, const std::string& path, const std::string& query) {
            if (id <= 0) return false;
            if (auto found = streams.find(id); found != streams.end()) {
                return false;
            }
            if (!is_valid_id(id)) {
                return false;
            }
            maxid = id;
            auto& tmp = streams[id] = H2Stream(id, this);
            tmp.path_ = path;
            tmp.query_ = query;
            stream = &tmp;
            return true;
        }

        bool get_stream(int id, H2Stream*& stream) {
            if (id < 0) return false;
            if (auto found = streams.find(id); found != streams.end()) {
                stream = &found->second;
                return true;
            }
            return false;
        }

        const std::string& host() const {
            return host_;
        }

        std::string url() {
            H2Stream* st = nullptr;
            if (!conn || !host_.size() || !get_stream(st)) {
                return std::string();
            }
            return (conn->get_ssl() ? "https://" : "http://") + host_ + st->path_ + st->query_;
        }

        void close() override {
            if (conn) {
                streams[0].send_goaway(H2Error::none);
                conn->close();
            }
        }
    };

    struct Http2 {
        friend struct HttpClient;
        friend struct HttpServer;

       private:
        static void init_streams(std::shared_ptr<Http2Context>& ret, std::string&& path, std::string&& query) {
            ret->streams[0] = H2Stream(0, ret.get());
            ret->server = false;
            auto& tmp = ret->streams[1] = H2Stream(1, ret.get());
            tmp.path_ = std::move(path);
            tmp.query_ = std::move(query);
            ret->maxid = 1;
        }

        static std::shared_ptr<Http2Context> init_object(std::shared_ptr<Conn>& conn, HttpRequestContext& ctx) {
            auto ret = std::make_shared<Http2Context>(std::move(conn), ctx.host_with_port());
            init_streams(ret, std::move(ctx.path), std::move(ctx.query));
            return ret;
        }

        static bool verify_h2(std::shared_ptr<Conn>& conn) {
            const unsigned char* data = nullptr;
            unsigned int len = 0;
            SSL_get0_alpn_selected((SSL*)conn->get_ssl(), &data, &len);
            if (len != 2 || !data || data[0] != 'h' || data[1] != '2') {
                return false;
            }
            return true;
        }

        static OpenErr reopen_h2c(std::shared_ptr<Conn>& conn, std::shared_ptr<Http2Context>& ret, HttpRequestContext& ctx) {
            auto tmp = Http1::init_object(conn, ctx);
            H2SettingsFrame setting;
            commonlib2::Serializer<std::string> se;
            setting.serialize(16384, se, ret.get());
            se.get().erase(0, 9);
            commonlib2::Base64Context bctx;
            bctx.nopadding = true;
            bctx.c62 = '-';
            bctx.c63 = '_';
            std::string base64_encoded;
            commonlib2::Reader(se.get()).readwhile(base64_encoded, commonlib2::base64_encode, &bctx);
            tmp->send("GET", {{"Connection", "Upgrade, HTTP2-Settings"}, {"Upgrade", "h2c"}, {"HTTP2-Settings", base64_encoded}});
            TimeoutContext cancel(10);
            if (!tmp->recv(true, &cancel)) {
                return false;
            }
            tmp->hijack();
            auto& resp = tmp->response();
            if (auto code = resp.find(":status"); code == resp.end() || code->second != "101") {
                return OpenError::invalid_condition;
            }
            if (auto code = resp.find("connection"); code == resp.end() || code->second != "Upgrade") {
                return OpenError::invalid_condition;
            }
            if (auto code = resp.find("upgrade"); code == resp.end() || code->second != "h2c") {
                return OpenError::invalid_condition;
            }
            if (tmp->remain_buffer().size()) {
                ret->r.ref().ref() = std::move(tmp->remain_buffer());
            }
            return true;
        }

        static std::shared_ptr<Http2Context> open_h2c(std::shared_ptr<Conn>& conn, HttpRequestContext& ctx) {
            auto conncopy = conn;
            std::string pathcopy = ctx.path, querycopy = ctx.query;
            auto ret = init_object(conncopy, ctx);
            ctx.path = std::move(pathcopy);
            ctx.query = std::move(querycopy);
            if (!reopen_h2c(conn, ret, ctx)) {
                return nullptr;
            }
            return ret;
        }

       public:
        static OpenErr reopen(std::shared_ptr<Http2Context>& conn, HttpOpenContext& arg) {
            if (!conn || !arg.url) return OpenError::invalid_condition;
            std::string urlstr;
            Http1::fill_urlprefix(conn->host(), arg, urlstr, conn->borrow()->get_ssl() ? "https" : "http");
            urlstr += arg.url;
            arg.url = urlstr.c_str();
            HttpRequestContext ctx;
            if (!Http1::setuphttp(arg, ctx, "http", "https", "https")) {
                arg.err = OpenError::parse_url;
                return OpenError::parse_url;
            }
            if (auto e = Http1::reopen_tcp_conn(conn->borrow(), ctx, arg, "\2h2", 3); e == OpenError::needless_to_reopen) {
                H2Stream* st;
                if (!conn->make_stream(st, ctx.path, ctx.query)) {
                    return false;
                }
                return e;
            }
            else if (!e) {
                return e;
            }
            conn->clear();
            init_streams(conn, std::move(ctx.path), std::move(ctx.query));
            auto& base = conn->borrow();
            if (base->get_ssl()) {
                if (!verify_h2(conn->borrow())) {
                    arg.err = OpenError::verify;
                    return OpenError::verify;
                }
                if (!base->write(h2_connection_preface, 24)) {
                    return false;
                }
                return true;
            }
            else {
                return reopen_h2c(conn->borrow(), conn, ctx);
            }
        }

        static std::shared_ptr<Http2Context> open(HttpOpenContext& arg) {
            HttpRequestContext ctx;
            if (!Http1::setuphttp(arg, ctx, "http", "https", "https")) {
                arg.err = OpenError::parse_url;
                return nullptr;
            }
            bool secure = ctx.url.scheme == "https";
            auto conn = Http1::open_tcp_conn(ctx, arg, "\2h2", 3);
            if (!conn) {
                return nullptr;
            }
            if (secure) {
                if (!verify_h2(conn)) {
                    arg.err = OpenError::verify;
                    return nullptr;
                }
                if (!conn->write(h2_connection_preface, 24)) {
                    return nullptr;
                }
                return init_object(conn, ctx);
            }
            else {
                return open_h2c(conn, ctx);
            }
        }
    };
#undef TRY
}  // namespace socklib
