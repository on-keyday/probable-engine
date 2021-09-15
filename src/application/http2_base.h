#pragma once
#include "../transport/tcp.h"
#include <net_helper.h>

#include "hpack.h"

namespace socklib {
    enum class H2FType : std::uint8_t {
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

    enum class H2Flag : std::uint8_t {
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
        need_window_update = unimplemented - 1,
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

    struct H2Weight {
        std::int32_t depends_id = 0;
        std::uint8_t weight = 0;
        bool exclusive = false;
    };

    struct H2Frame {
        friend struct H2Stream;
        friend struct Http2Context;

       protected:
        H2FType type = H2FType::unknown;
        H2Flag flag = H2Flag::none;
        std::int32_t streamid = 0;

       public:
        constexpr H2Frame() {}
        constexpr H2Frame(H2FType type)
            : type(type) {}

        virtual H2Err parse(commonlib2::HTTP2Frame<std::string>& v, Http2Conn*) {
            type = (H2FType)v.type;
            flag = (H2Flag)v.flag;
            streamid = v.id;
            return true;
        }

        //framesize will be Length octet
        virtual H2Err serialize(std::uint32_t framesize, commonlib2::Serializer<std::string>& se, Http2Conn*) {
            return serialize_impl(framesize, streamid, type, flag, se);
        }

       protected:
        static H2Err serialize_impl(std::uint32_t len, std::int32_t streamid, H2FType type, H2Flag flag, commonlib2::Serializer<std::string>& se) {
            if (len > 0xffffff || streamid < 0) {
                return H2Error::protocol;
            }
            len <<= 8;
            std::uint32_t len_net = commonlib2::translate_byte_net_and_host<std::uint32_t>(&len);
            se.write_byte((char*)&len_net, 3);
            se.template write_as<std::uint8_t>(type);
            se.template write_as<std::uint8_t>(flag);
            std::uint32_t id_net = commonlib2::translate_byte_net_and_host<int>(&streamid);
            se.template write_as<std::uint32_t>(id_net);
            return true;
        }

        static H2Err remove_padding(std::string& buf, int len, std::uint8_t& padding) {
            std::uint8_t d = (std::uint8_t)buf[0];
            buf.erase(0, 1);
            if ((int)d > len) {
                return H2Error::frame_size;
            }
            buf.erase(len - d);
            padding = d;
            return true;
        }

        static H2Err write_depends(bool exclusive, int id, std::uint8_t weight, commonlib2::Serializer<std::string>& se) {
            TRY(id >= 0);
            std::uint32_t mask = exclusive ? commonlib2::msb_on<std::uint32_t>() : 0;
            std::uint32_t to_write = (std::uint32_t)id;
            to_write |= mask;
            to_write = commonlib2::translate_byte_net_and_host<std::uint32_t>(&to_write);
            se.write(to_write);
            se.write(weight);
            return true;
        }

        static H2Err read_depends(bool& exclusive, int& id, std::uint8_t& weight, std::string& buf) {
            commonlib2::Deserializer<std::string&> se(buf);
            std::uint32_t id_ = 0;
            TRY(se.read_ntoh(id_));
            exclusive = (bool)(id_ & commonlib2::msb_on<std::uint32_t>());
            id_ &= ~commonlib2::msb_on<std::uint32_t>();
            id = (int)id_;
            TRY(se.read_ntoh(weight));
            buf.erase(0, 5);
            return true;
        }

        template <class F>
        static H2Err write_continuous(int streamid, F&& write_header, commonlib2::Serializer<std::string>& se, std::uint32_t fsize,
                                      std::string& hpacked, H2Flag flag, H2Flag flagcpy, std::uint8_t& padding, std::uint8_t plus) {
            size_t idx = 0;
            if (!any(flag & H2Flag::padded)) {
                padding = 0;
            }
            else {
                plus += 1;
            }
            if (hpacked.size() + padding + plus <= fsize) {
                TRY(write_header((std::uint32_t)(hpacked.size() + padding + plus), hpacked));
            }
            else {
                flag = flagcpy;
                std::string_view view(hpacked.data(), hpacked.data() + fsize - (padding + plus));
                TRY(write_header(fsize, view));
                size_t idx = fsize - (padding + plus);
                while (hpacked.size() - idx) {
                    if (hpacked.size() - idx <= fsize) {
                        view = std::string_view(hpacked.data() + idx, hpacked.data() + hpacked.size());
                        idx = hpacked.size();
                        H2Frame::serialize_impl(view.size(), streamid, H2FType::continuation, H2Flag::end_headers, se);
                        se.write(view);
                    }
                    else {
                        view = std::string_view(hpacked.data() + idx, hpacked.data() + idx + fsize);
                        idx += fsize;
                        H2Frame::serialize_impl(view.size(), streamid, H2FType::continuation, H2Flag::none, se);
                        se.write(view);
                    }
                }
            }
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

        bool is_set(H2Flag iflag) const {
            return any(iflag & flag);
        }

        virtual ~H2Frame() noexcept {}
    };

    template <class Frame, class ConnPtr>
    H2Err get_frame(commonlib2::HTTP2Frame<std::string>& v, std::shared_ptr<H2Frame>& frame, ConnPtr self) {
        frame = std::make_shared<Frame>();
        return frame->parse(v, self);
    }

    struct Http2Conn : AppLayer {
        using SettingTable = std::map<unsigned short, std::uint32_t>;

       protected:
        friend struct H2DataFrame;
        friend struct H2HeaderFrame;
        friend struct H2SettingsFrame;
        friend struct H2PushPromiseFrame;
        friend struct H2Stream;
        friend struct Http2;
        friend struct HttpClient;
        //size_t streamid_max = 0;
        Hpack::DynamicTable local_table;
        //std::int32_t local_table_size = 0;
        Hpack::DynamicTable remote_table;
        //std::int32_t remote_table_size = 0;
        commonlib2::Reader<SockReader> r;
        SettingTable remote_settings;
        SettingTable local_settings;
        int maxid = 0;
        std::int32_t initial_window = 0;
        std::int32_t remote_window = 0;

        void set_default_settings(SettingTable& table) {
            auto& s = table;
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

       public:
        Http2Conn(std::shared_ptr<Conn>&& in)
            : AppLayer(std::move(in)), r(conn) {
            set_default_settings(local_settings);
            set_default_settings(remote_settings);
            initial_window = 65535;
            remote_window = initial_window;
        }

        virtual void clear() {
            r.ref().ref().clear();
            local_table.clear();
            remote_table.clear();
            local_settings.clear();
            remote_settings.clear();
            set_default_settings(local_settings);
            set_default_settings(remote_settings);
        }

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
                    continue;
                }
                if (!frame.succeed) {
                    return false;
                }
                break;
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
                if (frame.flag & (std::uint8_t)H2Flag::end_headers) {
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
            TRY((bool)conn);
            commonlib2::Serializer<std::string> se;
            std::uint32_t& fsize = remote_settings[(unsigned short)H2PredefinedSetting::max_frame_size];
            if (fsize < 0xffff) {
                fsize = 0xffff;
            }
            else if (fsize > 0xffffff) {
                return H2Error::frame_size;
            }
            TRY(input.serialize(fsize, se, this));
            return conn->write(se.get());
        }

        bool recvable() {
            return (bool)r.ref().ref().size();
        }
    };

    struct H2DataFrame : H2Frame {
        H2DataFrame()
            : H2Frame(H2FType::data) {}
        friend struct H2Stream;

       private:
        std::string data_;
        std::uint8_t padding = 0;

       public:
        std::string& payload() {
            return data_;
        }
        std::uint8_t padlen() const {
            return padding;
        }
        H2Err parse(commonlib2::HTTP2Frame<std::string>& v, Http2Conn* t) override {
            H2Frame::parse(v, t);
            if (any(flag & H2Flag::padded)) {
                TRY(remove_padding(v.buf, v.len, padding));
            }
            data_ = std::move(v.buf);
            return true;
        }

        H2Err serialize(std::uint32_t fsize, commonlib2::Serializer<std::string>& se, Http2Conn* t) override {
            size_t idx = 0;
            unsigned char plus = 0;
            H2Flag flagcpy = flag;
            flag &= ~H2Flag::end_stream;
            if (!any(flag & H2Flag::padded)) {
                padding = 0;
            }
            else {
                plus = 1;
            }
            size_t willsize = fsize - padding - plus, padoct = padding + plus;
            auto write_payload = [&](auto& to_write, std::uint32_t len) {
                H2Frame::serialize(len, se, t);
                if (any(flag & H2Flag::padded)) {
                    se.write(padding);
                }
                se.write_byte(to_write);
                for (auto i = 0; i < padding; i++) {
                    se.write('\0');
                }
            };
            while (data_.size() - idx) {
                size_t appsize = data_.size() - idx;
                if (appsize + padoct <= fsize) {
                    flag = flagcpy;
                    std::string_view to_write(data_.data() + idx, data_.data() + data_.size());
                    idx = data_.size();
                    write_payload(to_write, (std::uint32_t)(appsize + padoct));
                }
                else {
                    std::string_view to_write(data_.data() + idx, data_.data() + idx + willsize);
                    idx += willsize;
                    write_payload(to_write, fsize);
                    TRY(t->conn->write(se.get()));
                    se.get().clear();
                }
            }
            flag = flagcpy;
            return true;
        }

        H2DataFrame* data() override {
            return this;
        }
    };

    struct H2HeaderFrame : H2Frame {
        H2HeaderFrame()
            : H2Frame(H2FType::header) {}
        friend struct H2Stream;

       private:
        HttpConn::Header header_;
        bool exclusive = false;
        int depends = 0;
        std::uint8_t weight = 0;
        //bool set_priority = false;
        std::uint8_t padding = 0;

       public:
        HttpConn::Header& header_map() {
            return header_;
        }

        bool get_weight(H2Weight& w) const {
            if (!any(flag & H2Flag::priority)) return false;
            w.exclusive = exclusive;
            w.depends_id = depends;
            w.weight = weight;
            return true;
        }

        std::uint8_t padlen() const {
            return padding;
        }

        H2Err parse(commonlib2::HTTP2Frame<std::string>& v, Http2Conn* t) override {
            H2Frame::parse(v, t);
            if (any(flag & H2Flag::padded)) {
                TRY(remove_padding(v.buf, v.len, padding));
            }
            if (any(flag & H2Flag::priority)) {
                TRY(read_depends(exclusive, depends, weight, v.buf));
                //set_priority = true;
            }
            if (!any(flag & H2Flag::end_headers)) {
                TRY(t->read_continuous(v.id, v.buf));
            }
            if (auto e = Hpack::decode(header_, v.buf, t->remote_table, t->remote_settings[(unsigned short)H2PredefinedSetting::header_table_size]); !e) {
                return H2Error::compression;
            }
            return true;
        }

        H2Err serialize(std::uint32_t fsize, commonlib2::Serializer<std::string>& se, Http2Conn* t) override {
            std::string hpacked;
            if (!Hpack::encode<true>(header_, hpacked, t->local_table, t->local_settings[(unsigned short)H2PredefinedSetting::header_table_size])) {
                return H2Error::compression;
            }
            H2Flag flagcpy = flag;
            std::uint8_t plus = 0;
            flag |= H2Flag::end_headers;
            /*if (!any(flag & H2Flag::padded)) {
                padding = 0;
            }
            else {
                plus = 1;
            }*/
            if (any(flag & H2Flag::priority)) {
                plus += 5;
            }
            auto write_header = [&](std::uint32_t len, auto& to_write) {
                TRY(H2Frame::serialize(len, se, t));
                if (any(flag & H2Flag::padded)) {
                    se.write(padding);
                }
                if (any(flag & H2Flag::priority)) {
                    TRY(write_depends(exclusive, depends, weight, se));
                }
                se.write_byte(to_write);
                for (auto i = 0; i < padding; i++) {
                    se.write('\0');
                }
                return H2Err(true);
            };
            TRY(write_continuous(streamid, write_header, se, fsize, hpacked, flag, flagcpy, padding, plus));
            return true;
        }

        H2HeaderFrame* header() override {
            return this;
        }
    };

    struct H2PriorityFrame : H2Frame {
        constexpr H2PriorityFrame()
            : H2Frame(H2FType::priority) {}
        friend struct H2Stream;

       private:
        bool exclusive = false;
        int depends = 0;
        std::uint8_t weight = 0;

       public:
        void get_weight(H2Weight& w) const {
            w.exclusive = exclusive;
            w.depends_id = depends;
            w.weight = weight;
        }

        H2Err parse(commonlib2::HTTP2Frame<std::string>& v, Http2Conn* t) override {
            H2Frame::parse(v, t);
            return read_depends(exclusive, depends, weight, v.buf);
        }

        H2Err serialize(std::uint32_t fsize, commonlib2::Serializer<std::string>& se, Http2Conn* t) override {
            TRY(H2Frame::serialize(5, se, t));
            return write_depends(exclusive, depends, weight, se);
        }

        H2PriorityFrame* priority() override {
            return this;
        }
    };

    struct H2RstStreamFrame : H2Frame {
        constexpr H2RstStreamFrame()
            : H2Frame(H2FType::rst_stream) {}
        friend struct H2Stream;

       private:
        std::uint32_t errcode = 0;

       public:
        std::uint32_t code() const {
            return errcode;
        }

        H2Err parse(commonlib2::HTTP2Frame<std::string>& v, Http2Conn* t) override {
            H2Frame::parse(v, t);
            commonlib2::Deserializer<std::string&> se(v.buf);
            TRY(se.read_ntoh(errcode));
            return true;
        }

        H2Err serialize(std::uint32_t fsize, commonlib2::Serializer<std::string>& se, Http2Conn* t) override {
            TRY(H2Frame::serialize(4, se, t));
            se.write_hton(errcode);
            return true;
        }

        H2RstStreamFrame* rst_stream() override {
            return this;
        }
    };

    struct H2SettingsFrame : H2Frame {
        constexpr H2SettingsFrame()
            : H2Frame(H2FType::settings) {}
        friend struct H2Stream;
        H2Err parse(commonlib2::HTTP2Frame<std::string>& v, Http2Conn* t) override {
            H2Frame::parse(v, t);
            if (streamid != 0) {
                return H2Error::protocol;
            }
            if (any(flag & H2Flag::ack)) {
                if (v.len != 0) {
                    return H2Error::frame_size;
                }
                return true;
            }
            if (v.len % 6) {
                return H2Error::frame_size;
            }
            commonlib2::Deserializer<std::string&> se(v.buf);
            while (!se.base_reader().ceof()) {
                unsigned short key = 0;
                std::uint32_t value = 0;
                TRY(se.read_ntoh(key));
                TRY(se.read_ntoh(value));
                t->remote_settings[key] = value;
            }
            return true;
        }

        H2Err serialize(std::uint32_t fsize, commonlib2::Serializer<std::string>& se, Http2Conn* t) override {
            auto ressize = t->local_settings.size() * 6;
            if (fsize < ressize) {
                return H2Error::frame_size;
            }
            if (any(flag & H2Flag::ack)) {
                TRY(H2Frame::serialize(0, se, t));
                return true;
            }
            TRY(H2Frame::serialize((std::uint32_t)ressize, se, t));
            for (auto& s : t->local_settings) {
                se.write_hton(s.first);
                se.write_hton(s.second);
            }
            return true;
        }

        H2SettingsFrame* settings() override {
            return this;
        }
    };

    struct H2PushPromiseFrame : H2Frame {
        H2PushPromiseFrame()
            : H2Frame(H2FType::push_promise) {}
        friend struct H2Stream;

       private:
        int promiseid = 0;
        HttpConn::Header header_;
        std::uint8_t padding = 0;

       public:
        HttpConn::Header& header_map() {
            return header_;
        }

        std::uint8_t padlen() const {
            return padding;
        }

        int id() const {
            return promiseid;
        }

        H2Err parse(commonlib2::HTTP2Frame<std::string>& v, Http2Conn* t) override {
            if (!t->remote_settings[(unsigned short)H2PredefinedSetting::enable_push]) {
                return H2Error::protocol;
            }
            H2Frame::parse(v, t);
            if (any(flag & H2Flag::padded)) {
                TRY(remove_padding(v.buf, v.len, padding));
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
            if (!Hpack::decode(header_, v.buf, t->remote_table, t->remote_settings[(unsigned short)H2PredefinedSetting::header_table_size])) {
                return H2Error::compression;
            }
            return true;
        }

        H2Err serialize(std::uint32_t fsize, commonlib2::Serializer<std::string>& se, Http2Conn* t) override {
            if (!t->local_settings[(unsigned short)H2PredefinedSetting::enable_push]) {
                return H2Error::protocol;
            }
            std::string hpacked;
            if (!Hpack::encode<true>(header_, hpacked, t->local_table, t->local_settings[(unsigned short)H2PredefinedSetting::header_table_size])) {
                return H2Error::compression;
            }
            H2Flag flagcpy = flag;
            std::uint8_t plus = 0;
            flag |= H2Flag::end_headers;
            auto write_promise = [&](std::uint32_t len, auto& to_write) {
                TRY(H2Frame::serialize(len, se, t));
                if (any(flag & H2Flag::padded)) {
                    se.write(padding);
                }
                se.write_hton(promiseid);
                se.write_byte(to_write);
                for (auto i = 0; i < padding; i++) {
                    se.write('\0');
                }
                return H2Err(true);
            };
            TRY(write_continuous(streamid, write_promise, se, fsize, hpacked, flag, flagcpy, padding, plus));
            flag = flagcpy;
            return true;
        }

        H2PushPromiseFrame* push_promise() override {
            return this;
        }
    };

    struct H2PingFrame : H2Frame {
        constexpr H2PingFrame()
            : H2Frame(H2FType::ping) {}
        friend struct H2Stream;

       private:
        std::uint8_t data_[8] = {0};

       public:
        std::uint8_t* payload() {
            return data_;
        }

        //bool ack = false;
        H2Err parse(commonlib2::HTTP2Frame<std::string>& v, Http2Conn* t) override {
            H2Frame::parse(v, t);
            if (v.len != 8) {
                return H2Error::frame_size;
            }
            if (v.id != 0) {
                return H2Error::protocol;
            }
            memmove(data_, v.buf.data(), 8);
            return true;
        }

        H2Err serialize(std::uint32_t fsize, commonlib2::Serializer<std::string>& se, Http2Conn* t) override {
            TRY(H2Frame::serialize(8, se, t));
            se.write_byte(data_, 8);
            return true;
        }

        H2PingFrame* ping() override {
            return this;
        }
    };

    struct H2GoAwayFrame : H2Frame {
        H2GoAwayFrame()
            : H2Frame(H2FType::goaway) {}
        friend struct H2Stream;

       private:
        int lastid = 0;
        std::uint32_t errcode = 0;
        std::string additionaldata;

       public:
        uint32_t code() const {
            return errcode;
        }
        int id() const {
            return lastid;
        }

        const std::string& optdata() const {
            return additionaldata;
        }

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

        H2Err serialize(std::uint32_t fsize, commonlib2::Serializer<std::string>& se, Http2Conn* t) override {
            if (additionaldata.size() + 9 + 8 > fsize) {
                return H2Error::frame_size;
            }
            TRY(H2Frame::serialize((std::uint32_t)additionaldata.size() + 8, se, t));
            se.write_hton(lastid);
            se.write_hton(errcode);
            se.write_byte(additionaldata);
            return true;
        }

        H2GoAwayFrame* goaway() override {
            return this;
        }
    };

    struct H2WindowUpdateFrame : H2Frame {
        constexpr H2WindowUpdateFrame()
            : H2Frame(H2FType::window_update) {}
        friend struct H2Stream;

       private:
        std::int32_t value = 0;

       public:
        std::int32_t update() const {
            return value;
        }

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

        H2Err serialize(std::uint32_t fsize, commonlib2::Serializer<std::string>& se, Http2Conn* t) override {
            if (value <= 0) {
                return H2Error::protocol;
            }
            TRY(H2Frame::serialize(4, se, t));
            se.write_hton(value);
            return true;
        }

        H2WindowUpdateFrame* window_update() override {
            return this;
        }
    };
#undef TRY
}  // namespace socklib