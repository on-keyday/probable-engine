/*
    socklib - simple socket library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "hpack.h"
#include "http_base.h"
#include <net_helper.h>

namespace socklib {
    namespace v2 {
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
            flow_control = 3,
            settings_timeout = 4,
            stream_closed = 5,
            frame_size = 6,
            refused_stream = 7,
            cancel = 8,
            compression = 9,
            connect = 10,
            enhance_your_clam = 11,
            inadequate_security = 12,
            http_1_1_required = 13,

            unimplemented = ~0,
            need_window_update = unimplemented - 1,
        };

        BEGIN_ENUM_ERROR_MSG(H2Error)
        ENUM_ERROR_MSG(H2Error::none, "no error")
        ENUM_ERROR_MSG(H2Error::protocol, "protocol error")
        ENUM_ERROR_MSG(H2Error::internal, "internal error")
        ENUM_ERROR_MSG(H2Error::flow_control, "flow control error")
        ENUM_ERROR_MSG(H2Error::settings_timeout, "settings timeout")
        ENUM_ERROR_MSG(H2Error::stream_closed, "stream closed")
        ENUM_ERROR_MSG(H2Error::frame_size, "frame size error")
        ENUM_ERROR_MSG(H2Error::cancel, "request canceled")
        ENUM_ERROR_MSG(H2Error::compression, "commpression error")
        ENUM_ERROR_MSG(H2Error::refused_stream, "stream refused")
        ENUM_ERROR_MSG(H2Error::enhance_your_clam, "enhance your clam")
        ENUM_ERROR_MSG(H2Error::inadequate_security, "inadequate security")
        ENUM_ERROR_MSG(H2Error::http_1_1_required, "HTTP/1.1 required")
        ENUM_ERROR_MSG(H2Error::connect, "CONNECT method error")
        END_ENUM_ERROR_MSG

        enum class H2PredefinedSetting : unsigned short {
            header_table_size = 1,
            enable_push = 2,
            max_concurrent_streams = 3,
            initial_window_size = 4,
            max_frame_size = 5,
            max_header_list_size = 6,
        };

        constexpr std::uint16_t key(H2PredefinedSetting k) {
            return std::uint16_t(k);
        }

        using H2Err = commonlib2::EnumWrap<H2Error, H2Error::none, H2Error::internal>;

        struct H2Weight {
            std::int32_t depends_id = 0;
            std::uint8_t weight = 0;
            bool exclusive = false;
        };

#define H2TYPE_PARAMS template <class String, template <class...> class Map, class Header, class Body, class Table>

#define TEMPLATE_PARAM <String, Map, Header,Body, Table>

#define DEF_H2TYPE(NAME) \
    H2TYPE_PARAMS        \
    struct NAME

#define DEC_FRAME(NAME) DEF_H2TYPE(NAME)

#define H2FRAME H2Frame TEMPLATE_PARAM

#define DEF_FRAME(NAME) DEC_FRAME(NAME) : H2FRAME

#define DETECTTYPE_RET(TYPE, FUNC, RET, ...)          \
    virtual TYPE TEMPLATE_PARAM* FUNC() __VA_ARGS__ { \
        return RET;                                   \
    }

#define DETECTTYPE(TYPE, FUNC) DETECTTYPE_RET(TYPE, FUNC, nullptr)
#define THISTYPE(TYPE, FUNC) DETECTTYPE_RET(TYPE, FUNC, this, override)

        H2TYPE_PARAMS
#ifdef COMMONLIB2_HAS_CONCEPTS
        requires StringType<String>
#endif
        struct Http2RequestContext {
            using settings_t = Map<std::uint16_t, std::uint32_t>;
            using table_t = Table;
            H2Error err = H2Error::none;
            HpackError hpackerr = HpackError::none;
            settings_t remote_settings;
            settings_t local_settings;
            table_t remote_table;
            table_t local_table;
        };

        DEC_FRAME(H2DataFrame);
        DEC_FRAME(H2HeaderFrame);
        DEC_FRAME(H2PriorityFrame);
        DEC_FRAME(H2RstStreamFrame);
        DEC_FRAME(H2SettingsFrame);
        DEC_FRAME(H2PushPromiseFrame);
        DEC_FRAME(H2PingFrame);
        DEC_FRAME(H2GoAwayFrame);
        DEC_FRAME(H2WindowUpdateFrame);

        H2TYPE_PARAMS
        struct H2Frame {
            using h2request_t = Http2RequestContext<String, Map, Header, Body, Table>;
            using string_t = String;
            using header_t = Header;
            using hpack_t = Hpack<String, Table, Header>;
            using writer_t = commonlib2::Serializer<string_t>;
            using reader_t = commonlib2::Deserializer<string_t&>;
            using rawframe_t = commonlib2::HTTP2Frame<string_t>;
            using body_t = Body;
#define USING_H2FRAME                                  \
    using string_t = typename H2FRAME::string_t;       \
    using header_t = typename H2FRAME::header_t;       \
    using h2request_t = typename H2FRAME::h2request_t; \
    using hpack_t = typename H2FRAME::hpack_t;         \
    using writer_t = typename H2FRAME::writer_t;       \
    using reader_t = typename H2FRAME::reader_t;       \
    using rawframe_t = typename H2FRAME::rawframe_t;   \
    using body_t = typename H2FRAME::body_t

           protected:
            H2FType type = H2FType::unknown;
            H2Flag flag = H2Flag::none;
            std::int32_t streamid = 0;

           public:
            constexpr H2Frame(H2FType type)
                : type(type) {}

            virtual H2Err parse(rawframe_t& v, h2request_t&) {
                type = (H2FType)v.type;
                flag = (H2Flag)v.flag;
                streamid = v.id;
                return true;
            }

            //framesize will be Length octet
            virtual H2Err serialize(std::uint32_t framesize, writer_t& se, h2request_t&) {
                return serialize_impl(framesize, streamid, type, flag, se);
            }

           protected:
            static H2Err serialize_impl(std::uint32_t len, std::int32_t streamid, H2FType type, H2Flag flag, commonlib2::Serializer<string_t>& se) {
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

            static H2Err remove_padding(string_t& buf, int len, std::uint8_t& padding) {
                std::uint8_t d = (std::uint8_t)buf[0];
                buf.erase(0, 1);
                if ((int)d > len) {
                    return H2Error::frame_size;
                }
                buf.erase(len - d);
                padding = d;
                return true;
            }

            static H2Err write_depends(H2Weight& w, writer_t& se) {
                if (w.depends_id <= 0) {
                    return false;
                }
                std::uint32_t mask = w.exclusive ? commonlib2::msb_on<std::uint32_t>() : 0;
                std::uint32_t to_write = (std::uint32_t)w.depends_id;
                to_write |= mask;
                to_write = commonlib2::translate_byte_net_and_host<std::uint32_t>(&to_write);
                se.write(to_write);
                se.write(w.weight);
                return true;
            }

            static H2Err read_depends(H2Weight& w, string_t& buf) {
                reader_t se(buf);
                std::uint32_t id_ = 0;
                if (!se.read_ntoh(w.depends_id)) {
                    return false;
                }
                w.exclusive = (bool)(w.depends_id & commonlib2::msb_on<std::uint32_t>());
                w.depends_id &= ~commonlib2::msb_on<std::uint32_t>();
                if (!se.read_ntoh(w.weight)) {
                    return false;
                }
                buf.erase(0, 5);
                return true;
            }

            template <class F>
            static H2Err write_continuous(int streamid, F&& write_header, writer_t& se, std::uint32_t fsize,
                                          std::string& hpacked, H2Flag flag, H2Flag flagcpy, std::uint8_t& padding, std::uint8_t plus) {
                size_t idx = 0;
                if (!any(flag & H2Flag::padded)) {
                    padding = 0;
                }
                else {
                    plus += 1;
                }
                if (hpacked.size() + padding + plus <= fsize) {
                    if (auto e = write_header((std::uint32_t)(hpacked.size() + padding + plus), hpacked); !e) {
                        return e;
                    }
                }
                else {
                    flag = flagcpy;
                    std::string_view view(hpacked.data(), fsize - (padding + plus));
                    if (auto e = write_header(fsize, view); !e) {
                        return e;
                    }
                    size_t idx = fsize - (padding + plus);
                    while (hpacked.size() - idx) {
                        if (hpacked.size() - idx <= fsize) {
                            view = std::string_view(hpacked.data() + idx, hpacked.size());
                            idx = hpacked.size();
                            H2Frame::serialize_impl(view.size(), streamid, H2FType::continuation, H2Flag::end_headers, se);
                            se.write(view);
                        }
                        else {
                            view = std::string_view(hpacked.data() + idx, fsize);
                            idx += fsize;
                            H2Frame::serialize_impl(view.size(), streamid, H2FType::continuation, H2Flag::none, se);
                            se.write(view);
                        }
                    }
                }
                return true;
            }

           public:
            DETECTTYPE(H2DataFrame, data)
            DETECTTYPE(H2HeaderFrame, header)
            DETECTTYPE(H2PriorityFrame, priority)
            DETECTTYPE(H2RstStreamFrame, rst_stream)
            DETECTTYPE(H2SettingsFrame, settings)
            DETECTTYPE(H2PushPromiseFrame, push_promise)
            DETECTTYPE(H2PingFrame, ping)
            DETECTTYPE(H2GoAwayFrame, goaway)
            DETECTTYPE(H2WindowUpdateFrame, window_update)

            bool is_set(H2Flag iflag) const {
                return any(iflag & flag);
            }

            virtual ~H2Frame() noexcept {}
        };

        DEF_H2TYPE(Http2ReadContext)
            : IReadContext {
            USING_H2FRAME;
            using h1request_t = RequestContext<string_t, header_t, body_t>;
            using errorhandle_t = ErrorHandler<string_t, header_t, body_t>;
            h1request_t& req;
            h2request_t& ctx;
            string_t rawdata;

            Http2ReadContext(h1request_t & r, h2request_t & c)
                : req(r), ctx(c) {}

            virtual void on_error(std::int64_t errcode, CancelContext * cancel, const char* msg) override {
                errorhandle_t::on_error(req, errcode, cancel, msg);
            }

            virtual void append(const char* ptr, size_t size) override {
                rawdata.append(ptr, size);
            }
        };

        DEF_H2TYPE(Http2Reader) {
            USING_H2FRAME;
            using h2readcontext_t = Http2ReadContext<String, Map, Header, Body, Table>;
            static H2Err read_a_frame(std::shared_ptr<InetConn> & conn, h2readcontext_t & read, rawframe_t & frame, CancelContext* cancel = nullptr) {
                if (!conn) return false;
                commonlib2::Reader<string_t&> r(read.rawdata);
                if (!read.rawdata.size()) {
                    if (!conn->read(read, cancel)) {
                        return false;
                    }
                }
                for (;;) {
                    r.readwhile(commonlib2::http2frame_reader, frame);
                    if (frame.continues) {
                        if (!conn->read(read, cancel)) {
                            return false;
                        }
                        continue;
                    }
                    if (!frame.succeed) {
                        return H2Error::protocol;
                    }
                    break;
                }
                rawdata.erase(0, 9 + frame.buf.size());
                return true;
            }

            template <class Frame>
            H2Err get_frame(rawframe_t & frame, std::shared_ptr<H2FRAME> & res) {
                res = std::make_shared<Frame>();
                return res->parse(frame);
            }

            H2Err make_frame(std::shared_ptr<H2FRAME> & res, rawframe_t & frame) {
#define F(TYPE) TYPE TEMPLATE_PARAM
                switch (frame.type) {
                    case 0:
                        return get_frame<F(H2DataFrame)>(frame, res);
                    case 1:
                        return get_frame<F(H2HeaderFrame)>(frame, res);
                    case 2:
                        return get_frame<F(H2PriorityFrame)>(frame, res);
                    case 3:
                        return get_frame<F(H2RstStreamFrame)>(frame, res);
                    case 4:
                        return get_frame<F(H2SettingsFrame)>(frame, res);
                    case 5:
                        return get_frame<F(H2PushPromiseFrame)>(frame, res);
                    case 6:
                        return get_frame<F(H2PingFrame)>(frame, res);
                    case 7:
                        return get_frame<F(H2GoAwayFrame)>(frame, res);
                    case 8:
                        return get_frame<F(H2WindowUpdateFrame)>(frame, res, this);
                    case 9:
                        return H2Error::protocol;
                    default:
                        return H2Error::unimplemented;
                }
#undef F
            }

            static H2Err read(std::shared_ptr<InetConn> & conn, std::shared_ptr<H2FRAME> & res, h2readcontext_t & ctx, CancelContext* cancel = nullptr) {
                rawframe_t frame;
                if (auto e = read_a_frame(conn, ctx, frame, cancel); !e) {
                    return e;
                }
                return make_frame(res, frame);
            }
        };

        DEF_FRAME(H2DataFrame) {
            USING_H2FRAME;

           private:
            string_t data_;
            std::uint8_t padding = 0;

           public:
            H2DataFrame()
                : H2FRAME(H2FType::data) {}

            string_t& payload() {
                return data_;
            }
            std::uint8_t padlen() const {
                return padding;
            }
            H2Err parse(rawframe_t & v, h2request_t & t) override {
                H2FRAME::parse(v, t);
                if (any(this->flag & H2Flag::padded)) {
                    if (auto e = H2FRAME::remove_padding(v.buf, v.len, this->padding); !e) {
                        t.err = e;
                        return t.err;
                    }
                }
                data_ = std::move(v.buf);
                return true;
            }

            H2Err serialize(std::uint32_t fsize, writer_t & se, h2request_t & t) override {
                size_t idx = 0;
                unsigned char plus = 0;
                H2Flag flagcpy = this->flag;
                this->flag &= ~H2Flag::end_stream;
                if (!any(this->flag & H2Flag::padded)) {
                    padding = 0;
                }
                else {
                    plus = 1;
                }
                size_t willsize = fsize - padding - plus, padoct = padding + plus;
                if (willsize > t.local_settings[key(H2PredefinedSetting::max_frame_size)]) {
                    t.err = H2Error::frame_size;
                    return t.err;
                }
                H2FRAME::serialize((int)willsize, se, t);
                if (any(this->flag & H2Flag::padded)) {
                    se.write(padding);
                }
                se.write_byte(data_);
                for (auto i = 0; i < padding; i++) {
                    se.write('\0');
                }
                return true;
            }

            THISTYPE(H2DataFrame, data)
        };

        DEF_FRAME(H2HeaderFrame) {
            USING_H2FRAME;

            H2HeaderFrame()
                : H2FRAME(H2FType::header) {}

           private:
            header_t header_;
            H2Weight weight;
            std::uint8_t padding = 0;

           public:
            header_t& header_map() {
                return header_;
            }

            bool get_weight(H2Weight & w) const {
                if (!any(this->flag & H2Flag::priority)) return false;
                w.exclusive = weight.exclusive;
                w.depends_id = weight.depends;
                w.weight = weight.weight;
                return true;
            }

            std::uint8_t padlen() const {
                return padding;
            }

            H2Err parse(rawframe_t & v, h2request_t & t) override {
                H2FRAME::parse(v, t);
                if (any(this->flag & H2Flag::padded)) {
                    if (auto e = H2FRAME::remove_padding(v.buf, v.len, this->padding); !e) {
                        return e;
                    }
                }
                if (any(this->flag & H2Flag::priority)) {
                    if (auto e = H2FRAME::read_depends(this->weight, v.buf)) {
                        return e;
                    }
                }
                /*if (!any(this->flag & H2Flag::end_headers)) {}*/
                if (auto e = hpack_t::decode(header_, v.buf, t.remote_table, t.remote_settings[key(H2PredefinedSetting::header_table_size)]); !e) {
                    t.hpackerr = e;
                    t.err = H2Error::compression;
                    return t.err;
                }
                return true;
            }

            H2Err serialize(std::uint32_t fsize, writer_t & se, h2request_t & t) override {
                string_t hpacked;
                if (auto e = hpack_t::template encode<true>(header_, hpacked, t.local_table, t.local_settings[key(H2PredefinedSetting::header_table_size)]); !e) {
                    t.hpackerr = e;
                    t.err = H2Error::compression;
                    return t.err;
                }
                H2Flag flagcpy = this->flag;
                std::uint8_t plus = 0;
                this->flag |= H2Flag::end_headers;
                if (any(this->flag & H2Flag::priority)) {
                    plus += 5;
                }
                auto write_header = [&](std::uint32_t len, auto& to_write) -> H2Err {
                    if (auto e = H2FRAME::serialize(len, se, t); !e) {
                        t.err = e;
                        return t.err;
                    }
                    if (any(this->flag & H2Flag::padded)) {
                        se.write(padding);
                    }
                    if (any(this->flag & H2Flag::priority)) {
                        if (auto e = H2FRAME::write_depends(weight, se); !e) {
                            t.err = e;
                            return false;
                        }
                    }
                    se.write_byte(to_write);
                    for (auto i = 0; i < padding; i++) {
                        se.write('\0');
                    }
                    return H2Err(true);
                };
                if (auto e = H2FRAME::write_continuous(this->streamid, write_header, se, fsize, hpacked, this->flag, flagcpy, padding, plus); !e) {
                    t.err = e;
                    return e;
                }
                return true;
            }
        };

        DEF_FRAME(H2PriorityFrame) {
            USING_H2FRAME;

            constexpr H2PriorityFrame()
                : H2FRAME(H2FType::priority) {}

           private:
            H2Weight weight;

           public:
            void get_weight(H2Weight & w) const {
                w.exclusive = weight.exclusive;
                w.depends_id = weight.depends;
                w.weight = weight.weight;
            }

            H2Err parse(rawframe_t & v, h2request_t & t) override {
                H2Frame::parse(v, t);
                if (auto e = read_depends(weight, v.buf); !e) {
                    t.err = e;
                    return e;
                }
                return true;
            }

            H2Err serialize(std::uint32_t fsize, writer_t & se, h2request_t & t) override {
                if (auto e = H2FRAME::serialize(5, se, t); !e) {
                    t.err = e;
                    return e;
                }
                return write_depends(weight, se);
            }

            THISTYPE(H2PriorityFrame, priority)
        };

        DEF_FRAME(H2RstStreamFrame) {
            USING_H2FRAME;
            constexpr H2RstStreamFrame()
                : H2FRAME(H2FType::rst_stream) {}

           private:
            std::uint32_t errcode = 0;

           public:
            std::uint32_t code() const {
                return errcode;
            }

            H2Err parse(rawframe_t & v, h2request_t & t) override {
                H2Frame::parse(v, t);
                if (v.len != 4) {
                    t.err = H2Error::frame_size;
                    return t.err;
                }
                reader_t se(v.buf);
                if (!se.read_ntoh(errcode)) {
                    t.err = H2Error::internal;
                    return false;
                }
                return true;
            }

            H2Err serialize(std::uint32_t fsize, writer_t & se, h2request_t & t) override {
                if (auto e = H2Frame::serialize(4, se, t)) {
                    t.err = e;
                    return e;
                }
                se.write_hton(errcode);
                return true;
            }

            THISTYPE(H2RstStreamFrame, rst_stream);
        };

        DEF_FRAME(H2SettingsFrame) {
            USING_H2FRAME;
            using settings_t = typename h2request_t::settings_t;
            constexpr H2SettingsFrame()
                : H2FRAME(H2FType::settings) {}

           private:
            settings_t oldset;
            settings_t newset;

           public:
            H2Err parse(rawframe_t & v, h2request_t & t) override {
                H2Frame::parse(v, t);
                if (this->streamid != 0) {
                    return H2Error::protocol;
                }
                if (any(this->flag & H2Flag::ack)) {
                    if (v.len != 0) {
                        return H2Error::frame_size;
                    }
                    return true;
                }
                if (v.len % 6) {
                    return H2Error::frame_size;
                }
                reader_t se(v.buf);
                while (!se.base_reader().ceof()) {
                    std::uint16_t key = 0;
                    std::uint32_t value = 0;
                    if (!se.read_ntoh(key)) {
                        t.err = H2Error::internal;
                        return false;
                    }
                    if (!se.read_ntoh(value)) {
                        t.err = H2Error::internal;
                        return false;
                    }
                    newset[key] = value;
                    auto& tmp = t.remote_settings[key];
                    oldset[key] = value;
                    tmp = value;
                }
                return true;
            }

            H2Err serialize(std::uint32_t fsize, writer_t & se, h2request_t & t) override {
                if (any(this->flag & H2Flag::ack)) {
                    H2FRAME::serialize(0, se, t);
                    return true;
                }
                auto ressize = newset.size() * 6;
                if (fsize < ressize) {
                    return H2Error::frame_size;
                }
                H2Frame::serialize((std::uint32_t)ressize, se, t);
                for (auto& s : newset) {
                    auto& tmp = t.local_settings[s.first];
                    oldset[s.first] = tmp;
                    tmp = s.second;
                    se.write_hton(s.first);
                    se.write_hton(s.second);
                }
                return true;
            }

            THISTYPE(H2SettingsFrame, settings);
        };

#undef H2FRAME
#undef DETECTTYPE_RET
#undef THISTYPE
#undef TEMPLATE_PARAM
#undef DEF_FRAME
#undef DEF_H2TYPE
#undef DEC_FRAME
#undef DETECTTYPE
#undef USING_H2FRAME

#undef TRY
    }  // namespace v2

}  // namespace socklib
