#pragma once
#include "protcolbase.h"
#include <net_helper.h>

#include "hpack.h"

namespace socklib {
    enum class H2FType : unsigned char {
        data = 0x0,
        header = 0x1,
        priority = 0x2,
        rst_stream = 0x3,
        settings = 0x4,
        push_promise = 0x5,
        ping = 0x6,
        goaway = 0x7,
        window_update = 0x8,
        continuation = 0x9,
        unknown = 0xff,
    };

    DEFINE_ENUMOP(H2FType)

    enum class H2Flag : unsigned char {
        end_stream = 0x1,
        end_headers = 0x4,
        padded = 0x8,
        priority = 0x20,
        ack = 0x1,
        none = 0x0
    };

    DEFINE_ENUMOP(H2Flag)

    enum class H2Error {
        none = 0,
        protocol = 1,
        internal = 2,
        frame_size = 6,
        compression = 9,
        unimplemented = ~0,
    };

    using H2Err = commonlib2::EnumWrap<H2Error, H2Error::none, H2Error::internal>;

    /*struct H2Err {
        H2Error errcode;
        constexpr Err(H2Error e)
            : errcode(e) {}
        constexpr Err(bool e)
            : errcode(e ? H2Error::none : H2Error::internal) {}
        operator bool() const {
            return errcode == H2Error::none;
        }
    };*/

    struct Http2Conn;

    struct H2Frame {
        H2FType type;
        H2Flag flag;
        int streamid;
        constexpr H2Frame() {}
        virtual H2Err parse(commonlib2::HTTP2Frame<std::string>& v, Http2Conn*) {
            type = (H2FType)v.type;
            flag = (H2Flag)v.flag;
            streamid = v.id;
            return true;
        }
    };

    struct H2DataFrame;
    struct H2HeaderFrame;

    template <class Frame, class ConnPtr>
    H2Err get_frame(commonlib2::HTTP2Frame<std::string>& v, std::shared_ptr<H2Frame>& frame, ConnPtr self) {
        frame = std::make_shared<Frame>();
        return frame->parse(v, self);
    }

    struct Http2Conn : AppLayer {
       private:
        friend struct H2HeaderFrame;
        size_t streamid_max = 0;
        Hpack::DynamicTable local;
        Hpack::DynamicTable remote;
        commonlib2::Reader<SockReader> r;

       public:
        Http2Conn(std::shared_ptr<Conn>&& in)
            : r(conn), AppLayer(std::move(in)) {}

       protected:
        H2Err make_frame(commonlib2::HTTP2Frame<std::string>& frame, std::shared_ptr<H2Frame>& res) {
            switch (frame.type) {
                case 0:
                    return get_frame<H2DataFrame>(frame, res, this);
                default:
                    return H2Error::protocol;
            }
        }

       public:
        H2Err recv(std::shared_ptr<H2Frame>& res) {
            commonlib2::HTTP2Frame<std::string> frame;
            r.readwhile(commonlib2::http2frame_reader, frame);
            if (!frame.succeed) {
                return false;
            }
            get_frame<H2DataFrame>(frame, res, this);
            return true;
        }
    };

    struct H2DataFrame : H2Frame {
        std::string data;
        H2DataFrame() {}
        H2Err parse(commonlib2::HTTP2Frame<std::string>& v, Http2Conn* t) override {
            H2Frame::parse(v, t);
            if (any(flag & H2Flag::padded)) {
                unsigned char d = (unsigned char)v.buf[0];
                v.buf.erase(0, 1);
                if ((int)d > v.len) {
                    return H2Error::frame_size;
                }
                v.buf.erase(v.len - d);
            }
            data = std::move(v.buf);
            return true;
        }
    };

    struct H2HeaderFrame : H2Frame {
        HttpConn::Header header;
        H2Err parse(commonlib2::HTTP2Frame<std::string>& v, Http2Conn* t) override {
            H2Frame::parse(v, t);
            if (!Hpack::decode(header, v.buf, t->remote)) {
                return H2Error::compression;
            }
        }
    };
}  // namespace socklib