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

    struct H2Stream {
        int streamid = 0;
        H2StreamState state = H2StreamState::idle;
        std::string path;
        std::string query;
        HttpConn::Header header;
        std::string payload;
        Http2Conn* conn = nullptr;
        int depend = 0;
        std::uint8_t weight = 0;
        bool exclusive = false;

        std::int32_t window = 0;

        std::uint32_t errorcode = 0;
        H2Stream() {}

        H2Stream(int id, Http2Conn* c)
            : streamid(id), conn(c) {}

        H2Err recv_apply(std::shared_ptr<H2Frame>& frame) {
            TRY(conn && frame && (frame->streamid == streamid));
            if (auto h = frame->header()) {
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
            }
            else if (auto d = frame->data()) {
                payload.append(d->data_);
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
            }
            return true;
        }

        H2Err send_data(const char* data, size_t size, bool padding = false, std::uint8_t padlen = 0, bool endstream = false) {
            TRY(conn && streamid != 0);
            H2DataFrame frame;
            frame.streamid = streamid;
            frame.data_ = std::string(data, size);
            if (padding) {
                frame.flag |= H2Flag::padded;
                frame.padding = padlen;
            }
            if (endstream) {
                frame.flag |= H2Flag::end_stream;
            }
            return conn->send(frame);
        }

        H2Err send_header(const HttpConn::Header& header,
                          bool padding = false, std::uint8_t padlen = 0, bool endstream = false,
                          bool has_priority = false, bool exclusive = false, int depends = 0, std::uint8_t weight = 0) {
            TRY(conn && streamid != 0);
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
                auto& s = conn->local_settings;
                constexpr auto k = [](H2PredefinedSetting c) {
                    return (unsigned short)c;
                };
                s[k(H2PredefinedSetting::header_table_size)] = 4096;
                s[k(H2PredefinedSetting::enable_push)] = 1;
                s[k(H2PredefinedSetting::max_concurrent_streams)] = ~0;
                s[k(H2PredefinedSetting::initial_window_size)] = 65535;
                s[k(H2PredefinedSetting::max_frame_size)] = 16384;
                s[k(H2PredefinedSetting::max_header_list_size)] = ~0;
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
    };

    struct Http2Context : Http2Conn {
        bool server = false;
        std::map<int, H2Stream> streams;
        int maxid = 0;
        std::string host;
        Http2Context(std::shared_ptr<Conn>&& conn, std::string&& host)
            : Http2Conn(std::move(conn)), host(std::move(host)) {}

        H2Err apply(std::shared_ptr<H2Frame>& frame, H2Stream*& stream) {
            stream = nullptr;
            TRY((bool)frame);
            TRY(frame->streamid >= 0);
            if (auto found = streams.find(frame->streamid); found != streams.end()) {
                stream = &found->second;
                return found->second.recv_apply(frame);
            }
            else {
                auto& tmp = streams[frame->streamid] = H2Stream(frame->streamid, this);
                stream = &tmp;
                return tmp.recv_apply(frame);
            }
        }

        bool make_stream(int id, H2Stream*& stream, const std::string& path, const std::string& query) {
            if (id <= 0) return false;
            if (auto found = streams.find(id); found != streams.end()) {
                return false;
            }
            if ((server && id % 2) || (!server && 1 != id % 2)) {
                return false;
            }
            auto& tmp = streams[id] = H2Stream(id, this);
            tmp.path = path;
            tmp.query = query;
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
    };

    struct Http2 {
        static std::shared_ptr<Http2Context> open(const char* url, bool encoded = false, const char* cacert = nullptr) {
            unsigned short port = 0;
            commonlib2::URLContext<std::string> urlctx;
            std::string path, query;
            if (!Http1::setuphttp(url, encoded, port, urlctx, path, query, "https", "https")) {
                return nullptr;
            }
            auto conn = TCP::open_secure(urlctx.host.c_str(), port, "https", true, cacert, true, "\2h2", 3, true);
            if (!conn) {
                return nullptr;
            }
            const unsigned char* data = nullptr;
            unsigned int len = 0;
            SSL_get0_alpn_selected((SSL*)conn->get_ssl(), &data, &len);
            if (len != 2 || !data || data[0] != 'h' || data[1] != '2') {
                return nullptr;
            }
            if (!conn->write("PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n", 24)) {
                return nullptr;
            }
            auto ret = std::make_shared<Http2Context>(std::move(conn), urlctx.host + (urlctx.port.size() ? ":" + urlctx.port : ""));
            ret->streams[0] = H2Stream(0, ret.get());
            ret->server = false;
            auto& tmp = ret->streams[1] = H2Stream(1, ret.get());
            tmp.path = std::move(path);
            tmp.query = std::move(query);
            return ret;
        }
    };
#undef TRY
}  // namespace socklib