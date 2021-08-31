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

    enum class H2PredefinedSetting : unsigned short {
        header_table_size = 1,
        enable_push = 2,
        max_concurrent_streams = 3,
        initial_window_size = 4,
        max_frame_size = 5,
        max_header_list_size = 6,
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

    struct H2DataFrame;
    struct H2HeaderFrame;
    struct H2PriorityFrame;
    struct H2RstStreamFrame;
    struct H2SettingsFrame;
    struct H2PushPromiseFrame;
    struct H2PingFrame;
    struct H2GoAwayFrame;
    struct H2WindowUpdateFrame;

//Rust like try
#define TRY(...) \
    if (auto _H_tryres = (__VA_ARGS__); !_H_tryres) return _H_tryres

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

        virtual H2Err serialize(commonlib2::Serializer<std::string&>& se) {
            return serialize_impl(0, se);  //must override!
        }

       protected:
        H2Err serialize_impl(int len, commonlib2::Serializer<std::string&>& se) {
            if (len < 0 || len > 0xffffff) {
                return H2Error::protocol;
            }
            int len_net = commonlib2::translate_byte_net_and_host<int>(&len);
        }

        H2Err remove_padding(std::string& buf, int len) {
            unsigned char d = (unsigned char)buf[0];
            buf.erase(0, 1);
            if ((int)d > len) {
                return H2Error::frame_size;
            }
            buf.erase(len - d);
            return true;
        }

        H2Err read_depends(bool& exclusive, int& id, unsigned char& weight, std::string& buf) {
            commonlib2::Deserializer<std::string&> se(buf);
            unsigned int id_ = 0;
            TRY(se.read_ntoh(id_));
            exclusive = (bool)(id_ & commonlib2::msb_on<unsigned int>());
            id_ &= ~commonlib2::msb_on<unsigned int>();
            id = (int)id_;
            TRY(se.read_ntoh(weight));
            buf.erase(0, 5);
            return true;
        }

       public:
        virtual H2DataFrame* data() {
            return nullptr;
        }
        virtual H2HeaderFrame* header() {
            return nullptr;
        }

        virtual H2PriorityFrame* priority() {
            return nullptr;
        }

        virtual H2RstStreamFrame* rst_stream() {
            return nullptr;
        }

        virtual H2SettingsFrame* settings() {
            return nullptr;
        }

        virtual H2PushPromiseFrame* push_promise() {
            return nullptr;
        }

        virtual H2PingFrame* ping() {
            return nullptr;
        }

        virtual H2GoAwayFrame* goaway() {
            return nullptr;
        }

        virtual H2WindowUpdateFrame* window_update() {
            return nullptr;
        }
    };

    template <class Frame, class ConnPtr>
    H2Err get_frame(commonlib2::HTTP2Frame<std::string>& v, std::shared_ptr<H2Frame>& frame, ConnPtr self) {
        frame = std::make_shared<Frame>();
        return frame->parse(v, self);
    }

    struct Http2Conn : AppLayer {
        using SettingTable = std::map<unsigned short, unsigned int>;

       private:
        friend struct H2HeaderFrame;
        friend struct H2SettingsFrame;
        friend struct H2PushPromiseFrame;
        size_t streamid_max = 0;
        Hpack::DynamicTable local;
        Hpack::DynamicTable remote;
        commonlib2::Reader<SockReader> r;
        SettingTable settings;

       public:
        Http2Conn(std::shared_ptr<Conn>&& in)
            : r(in), AppLayer(std::move(in)) {}

       protected:
        H2Err make_frame(commonlib2::HTTP2Frame<std::string>& frame, std::shared_ptr<H2Frame>& res) {
            switch (frame.type) {
                case 0:
                    return get_frame<H2DataFrame>(frame, res, this);
                case 1:
                    return get_frame<H2HeaderFrame>(frame, res, this);
                case 2:
                    return get_frame<H2PriorityFrame>(frame, res, this);
                case 3:
                    return get_frame<H2RstStreamFrame>(frame, res, this);
                case 4:
                    return get_frame<H2SettingsFrame>(frame, res, this);
                case 5:
                    return get_frame<H2PushPromiseFrame>(frame, res, this);
                case 6:
                    return get_frame<H2PingFrame>(frame, res, this);
                case 7:
                    return get_frame<H2GoAwayFrame>(frame, res, this);
                case 8:
                    return get_frame<H2WindowUpdateFrame>(frame, res, this);
                case 9:
                    return H2Error::protocol;
                default:
                    return H2Error::unimplemented;
            }
        }

        bool get_a_frame(commonlib2::HTTP2Frame<std::string>& frame) {
            for (;;) {
                r.readwhile(commonlib2::http2frame_reader, frame);
                if (frame.continues) {
                    TRY(r.ref().reading());
                }
            }
            if (!frame.succeed) {
                return false;
            }
            r.ref().ref().erase(0, 9 + frame.buf.size());
            r.seek(0);
            return true;
        }

        H2Err read_continuous(int id, std::string& buf) {
            while (true) {
                commonlib2::HTTP2Frame<std::string> frame;
                TRY(get_a_frame(frame));
                if (frame.id != id) {
                    return H2Error::protocol;
                }
                buf.append(frame.buf);
                if (frame.flag & (unsigned char)H2Flag::end_headers) {
                    break;
                }
            }
            return true;
        }

       public:
        H2Err recv(std::shared_ptr<H2Frame>& res) {
            commonlib2::HTTP2Frame<std::string> frame;
            TRY(get_a_frame(frame));
            return make_frame(frame, res);
        }

        H2Err send(H2Frame& input) {
        }

        bool recvable() {
            return (bool)r.ref().ref().size();
        }
    };

    struct H2DataFrame : H2Frame {
        std::string data_;
        H2Err parse(commonlib2::HTTP2Frame<std::string>& v, Http2Conn* t) override {
            H2Frame::parse(v, t);
            if (any(flag & H2Flag::padded)) {
                TRY(remove_padding(v.buf, v.len));
            }
            data_ = std::move(v.buf);
            return true;
        }

        H2DataFrame* data() override {
            return this;
        }
    };

    struct H2HeaderFrame : H2Frame {
        HttpConn::Header header_;
        bool exclusive = false;
        int depends = 0;
        unsigned char weight = 0;
        bool set_priority = false;
        H2Err parse(commonlib2::HTTP2Frame<std::string>& v, Http2Conn* t) override {
            H2Frame::parse(v, t);
            if (any(flag & H2Flag::padded)) {
                TRY(remove_padding(v.buf, v.len));
            }
            if (any(flag & H2Flag::priority)) {
                TRY(read_depends(exclusive, depends, weight, v.buf));
                set_priority = true;
            }
            if (!any(flag & H2Flag::end_headers)) {
                TRY(t->read_continuous(v.id, v.buf));
            }
            if (!Hpack::decode(header_, v.buf, t->remote)) {
                return H2Error::compression;
            }
            return true;
        }
        H2HeaderFrame* header() override {
            return this;
        }
    };

    struct H2PriorityFrame : H2Frame {
        bool exclusive = false;
        int depends = 0;
        unsigned char weight = 0;
        H2Err parse(commonlib2::HTTP2Frame<std::string>& v, Http2Conn* t) override {
            H2Frame::parse(v, t);
            return read_depends(exclusive, depends, weight, v.buf);
        }
        H2PriorityFrame* priority() override {
            return this;
        }
    };

    struct H2RstStreamFrame : H2Frame {
        unsigned int errcode = 0;

        H2Err parse(commonlib2::HTTP2Frame<std::string>& v, Http2Conn* t) override {
            H2Frame::parse(v, t);
            commonlib2::Deserializer<std::string&> se(v.buf);
            TRY(se.read_ntoh(errcode));
            return true;
        }

        H2RstStreamFrame* rst_stream() override {
            return this;
        }
    };

    struct H2SettingsFrame : H2Frame {
        bool ack = false;
        H2Err parse(commonlib2::HTTP2Frame<std::string>& v, Http2Conn* t) override {
            H2Frame::parse(v, t);
            if (streamid != 0) {
                return H2Error::protocol;
            }
            if (any(flag & H2Flag::ack)) {
                if (v.len != 0) {
                    return H2Error::frame_size;
                }
                ack = true;
                return true;
            }
            if (v.len % 6) {
                return H2Error::frame_size;
            }
            commonlib2::Deserializer<std::string&> se(v.buf);
            while (!se.base_reader().ceof()) {
                unsigned short key = 0;
                unsigned int value = 0;
                TRY(se.read_ntoh(key));
                TRY(se.read_ntoh(value));
                t->settings[key] = value;
            }
            return true;
        }

        H2SettingsFrame* settings() override {
            return this;
        }
    };

    struct H2PushPromiseFrame : H2Frame {
        int promiseid = 0;
        HttpConn::Header header_;
        H2Err parse(commonlib2::HTTP2Frame<std::string>& v, Http2Conn* t) override {
            H2Frame::parse(v, t);
            if (!t->settings[(unsigned short)H2PredefinedSetting::enable_push]) {
                return H2Error::protocol;
            }
            if (any(flag & H2Flag::padded)) {
                TRY(remove_padding(v.buf, v.len));
            }
            commonlib2::Deserializer<std::string&> se(v.buf);
            TRY(se.read_ntoh(promiseid));
            if (promiseid <= 0) {
                return H2Error::protocol;
            }
            v.buf.erase(0, 4);
            if (!any(flag & H2Flag::end_headers)) {
                TRY(t->read_continuous(v.id, v.buf));
            }
            if (!Hpack::decode(header_, v.buf, t->remote)) {
                return H2Error::compression;
            }
            return true;
        }

        H2PushPromiseFrame* push_promise() override {
            return this;
        }
    };

    struct H2PingFrame : H2Frame {
        unsigned char data_[8] = {0};
        bool ack = false;
        H2Err parse(commonlib2::HTTP2Frame<std::string>& v, Http2Conn* t) override {
            H2Frame::parse(v, t);
            if (v.len != 8) {
                return H2Error::frame_size;
            }
            if (v.id != 0) {
                return H2Error::protocol;
            }
            ack = any(flag & H2Flag::ack);
            memmove(data_, v.buf.data(), 8);
            return true;
        }

        H2PingFrame* ping() override {
            return this;
        }
    };

    struct H2GoAwayFrame : H2Frame {
        int lastid = 0;
        unsigned int errcode = 0;
        std::string additionaldata;
        H2Err parse(commonlib2::HTTP2Frame<std::string>& v, Http2Conn* t) override {
            H2Frame::parse(v, t);
            commonlib2::Deserializer<std::string&> se(v.buf);
            if (v.id != 0) {
                return H2Error::protocol;
            }
            TRY(se.read_ntoh(lastid));
            if (lastid < 0) {
                return H2Error::protocol;
            }
            TRY(se.read_ntoh(errcode));
            v.buf.erase(0, 8);
            additionaldata = std::move(v.buf);
            return true;
        }

        H2GoAwayFrame* goaway() override {
            return this;
        }
    };

    struct H2WindowUpdateFrame : H2Frame {
        int value = 0;
        H2Err parse(commonlib2::HTTP2Frame<std::string>& v, Http2Conn* t) override {
            H2Frame::parse(v, t);
            commonlib2::Deserializer<std::string&> se(v.buf);
            if (v.len != 4) {
                return H2Error::frame_size;
            }

            TRY(se.read_ntoh(value));
            if (value <= 0) {
                return H2Error::protocol;
            }
            return true;
        }

        H2WindowUpdateFrame* window_update() override {
            return this;
        }
    };
#undef TRY
}  // namespace socklib