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
        HttpConn::Header header;
        std::string payload;
        Http2Conn* conn = nullptr;
        int depend = 0;
        unsigned char weight = 0;
        bool exclusive = false;

        unsigned int errorcode = 0;

        H2Stream(int id, Http2Conn* c)
            : streamid(id), conn(c) {}

        H2Err recv_apply(std::shared_ptr<H2Frame>& frame) {
            TRY(frame && (frame->streamid == streamid));
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
        }

        H2Err send_data(const char* data, size_t size, bool padding = false, unsigned char padlen = 0, bool endstream = false) {
            H2DataFrame frame;
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
                          bool padding = false, unsigned char padlen = 0, bool endstream = false,
                          bool has_priority = false, bool exclusive = false, int depends = 0, unsigned char weight = 0) {
            H2HeaderFrame frame;
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
    };
#undef TRY
}  // namespace socklib