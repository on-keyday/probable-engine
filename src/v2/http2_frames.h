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
            http1_semantics_error = unimplemented - 2
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
        ENUM_ERROR_MSG(H2Error::unimplemented, "unimplemented")
        ENUM_ERROR_MSG(H2Error::need_window_update, "need to receive WINDOW_UPDATE")
        ENUM_ERROR_MSG(H2Error::http1_semantics_error, "HTTP/1.1 semantics error")
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
        /*
        H2TYPE_PARAMS
#ifdef COMMONLIB2_HAS_CONCEPTS
        requires StringType<String>
#endif
        struct Http2RequestContext;
*/

        struct H2Stream {
            //using request_t = Http2RequestContext TEMPLATE_PARAM;
            //using string_t = String;
            //request_t* ctx;
            //std::int32_t id;
            std::int64_t local_window = 0;
            std::int64_t remote_window = 0;
            H2StreamState state = H2StreamState::idle;
            H2Weight weight;
            std::uint32_t errorcode = 0;

            //for data frame
            size_t data_progress = 0;
        };

        H2TYPE_PARAMS
        struct H2Frame;

        H2TYPE_PARAMS
#ifdef COMMONLIB2_HAS_CONCEPTS
        requires StringType<String>
#endif
        struct Http2RequestContext {
            using settings_t = Map<std::uint16_t, std::uint32_t>;
            using table_t = Table;
            using stream_t = H2Stream;
            using streams_t = Map<std::int32_t, stream_t>;
            using frame_t = H2FRAME;
            H2Error err = H2Error::none;
            HpackError hpackerr = HpackError::none;
            settings_t remote_settings;
            settings_t local_settings;
            table_t remote_table;
            table_t local_table;
            streams_t streams;
            std::uint64_t max_stream = 0;
            bool server = false;

            H2Err (*user_callback)(frame_t&, void*, const Http2RequestContext&, const RequestContext<String, Header, Body>&);
            void* userctx = nullptr;
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
            using h2request_t = Http2RequestContext TEMPLATE_PARAM;
            using string_t = String;
            using header_t = Header;
            using hpack_t = Hpack<String, Table, Header>;
            using writer_t = commonlib2::Serializer<string_t>;
            using reader_t = commonlib2::Deserializer<string_t&>;
            using rawframe_t = commonlib2::HTTP2Frame<string_t>;
            using body_t = Body;
            using errorhandle_t = ErrorHandler<String, Header, Body>;
#define USING_H2FRAME                                  \
    using string_t = typename H2FRAME::string_t;       \
    using header_t = typename H2FRAME::header_t;       \
    using h2request_t = typename H2FRAME::h2request_t; \
    using hpack_t = typename H2FRAME::hpack_t;         \
    using writer_t = typename H2FRAME::writer_t;       \
    using reader_t = typename H2FRAME::reader_t;       \
    using rawframe_t = typename H2FRAME::rawframe_t;   \
    using body_t = typename H2FRAME::body_t;           \
    using errorhandle_t = typename H2FRAME::errorhandle_t

           protected:
            H2FType type = H2FType::unknown;
            H2Flag flag = H2Flag::none;
            std::int32_t streamid = 0;

           public:
            constexpr H2Frame(H2FType type)
                : type(type) {}
            H2FType get_type() const {
                return type;
            }

            std::int32_t get_id() const {
                return streamid;
            }

            bool set_id(std::int32_t id) {
                if (id < 0) return false;
                streamid = id;
                return true;
            }

            void set_flag(H2Flag f) {
                flag = f;
            }

            void add_flag(H2Flag f) {
                flag |= f;
            }

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
            static H2Err serialize_impl(std::uint32_t len, std::int32_t streamid, H2FType type, H2Flag flag, writer_t& se) {
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
                commonlib2::HTTP2Frame<string_t> f;
                f.maxlen = read.ctx.local_settings[key(H2PredefinedSetting::max_frame_size)];
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

            H2Err read_continuous(rawframe_t & frame, std::shared_ptr<InetConn> & conn, h2readcontext_t & ctx, CancelContext* cancel = nullptr) {
                H2Flag flag = H2Flag(frmae.flag);
                if (!any(flag & H2Flag::end_headers)) {
                    while (true) {
                        rawframe_t cont;
                        if (auto e = read_a_frame(conn, ctx, cont, cancel); !e) {
                            ctx.ctx.err = e;
                            return e;
                        }
                        if (frame.id != cont.id) {
                            ctx.ctx.err = H2Error::protocol;
                            return ctx.ctx.err;
                        }
                        frame.buf += cont.buf;
                        if (any(H2Flag(cont.flag) & H2Flag::end_headers)) {
                            break;
                        }
                    }
                }
                return true;
            }

            H2Err make_frame(std::shared_ptr<H2FRAME> & res, rawframe_t & frame, std::shared_ptr<InetConn> & conn, h2readcontext_t & ctx, CancelContext* cancel = nullptr) {
#define F(TYPE) TYPE TEMPLATE_PARAM
                switch (frame.type) {
                    case 0:
                        return get_frame<F(H2DataFrame)>(frame, res);
                    case 1:
                        if (auto e = read_continuous(frame, conn, ctx, cancel); !e) {
                            return e;
                        }
                        return get_frame<F(H2HeaderFrame)>(frame, res);
                    case 2:
                        return get_frame<F(H2PriorityFrame)>(frame, res);
                    case 3:
                        return get_frame<F(H2RstStreamFrame)>(frame, res);
                    case 4:
                        return get_frame<F(H2SettingsFrame)>(frame, res);
                    case 5:
                        if (auto e = read_continuous(frame, conn, ctx, cancel); !e) {
                            return e;
                        }
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
                if (auto e = make_frame(res, frame, conn, ctx, cancel); !e) {
                    ctx.ctx.err = e;
                    return e;
                }
            }
        };

        DEF_FRAME(H2Writer) {
            USING_H2FRAME;
            using h1request_t = RequestContext<string_t, header_t, body_t>;
            static H2Err write(std::shared_ptr<InetConn> & conn, H2FRAME & frame, h1request_t & req, h2request_t & ctx, CancelContext * cancel) {
                if (!conn) return false;
                writer_t w;
                if (auto e = frame.serialize(ctx.remote_settings[key(H2PredefinedSetting::max_frame_size)], w, ctx)) {
                    return e;
                }
                WriteContext c;
                c.ptr = w.data().data();
                c.bufsize = w.data().size();
                if (!errorhandle_t::write_to_conn(conn, w, req, cancel)) {
                    ctx.err = H2Error::internal;
                    return ctx.err;
                }
                return true;
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

            void set_padding(std::uint8_t pad) {
                padding = pad;
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

            void set_weight(const H2Weight& w) {
                weight = w;
            }

            std::uint8_t padlen() const {
                return padding;
            }

            void set_padding(std::uint8_t pad) {
                padding = pad;
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

            void set_weight(const H2Weight& w) {
                weight = w;
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

            void set_code(std::uint32_t code) {
                errcode = code;
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
            settings_t& new_settings() {
                return newset;
            }

            settings_t& old_settings() {
                return oldset;
            }

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

        DEF_FRAME(H2PushPromiseFrame) {
            USING_H2FRAME;
            H2PushPromiseFrame()
                : H2FRAME(H2FType::push_promise) {}

           private:
            std::int32_t promiseid = 0;
            header_t header_;
            std::uint8_t padding = 0;

           public:
            header_t& header_map() {
                return header_;
            }

            std::uint8_t padlen() const {
                return padding;
            }

            std::int32_t id() const {
                return promiseid;
            }

            H2Err parse(rawframe_t & v, h2request_t & t) override {
                if (!t.remote_settings[(unsigned short)H2PredefinedSetting::enable_push]) {
                    t.err = H2Error::protocol;
                    return t.err;
                }
                H2Frame::parse(v, t);
                if (any(this->flag & H2Flag::padded)) {
                    if (auto e = H2FRAME::remove_padding(v.buf, v.len, padding); !e) {
                        t.err = e;
                        return e;
                    }
                }
                reader_t se(v.buf);
                if (!se.read_ntoh(promiseid)) {
                    t.err = H2Error::internal;
                    return false;
                }
                if (promiseid <= 0) {
                    t.err = H2Error::protocol;
                    return t.err;
                }
                v.buf.erase(0, 4);
                /*if (!any(this->flag & H2Flag::end_headers)) {
                    TRY(t->read_continuous(v.id, v.buf));
                }*/
                if (auto e = hpack_t::decode(header_, v.buf, t.remote_table, t.remote_settings[key(H2PredefinedSetting::header_table_size)]); !e) {
                    t.hapckerr = e;
                    t.err = H2Error::compression;
                    return t.err;
                }
                return true;
            }

            H2Err serialize(std::uint32_t fsize, writer_t & se, h2request_t & t) override {
                if (!t.local_settings[(unsigned short)H2PredefinedSetting::enable_push]) {
                    t.err = H2Error::protocol;
                    return t.err;
                }
                string_t hpacked;
                if (auto e = hpack_t::template encode<true>(header_, hpacked, t.local_table, t.local_settings[key(H2PredefinedSetting::header_table_size)]); !e) {
                    t.hpackerr = e;
                    t.err = H2Error::compression;
                    return t.err;
                }
                H2Flag flagcpy = this->flag;
                std::uint8_t plus = 0;
                this->flag |= H2Flag::end_headers;
                auto write_promise = [&](std::uint32_t len, auto& to_write) {
                    if (auto e = H2FRAME::serialize(len, se, t); !e) {
                        t.err = e;
                        return e;
                    }
                    if (any(this->flag & H2Flag::padded)) {
                        se.write(padding);
                    }
                    se.write_hton(promiseid);
                    se.write_byte(to_write);
                    for (auto i = 0; i < padding; i++) {
                        se.write('\0');
                    }
                    return H2Err(true);
                };
                if (auto e = H2FRAME::write_continuous(this->streamid, write_promise, se, fsize, hpacked, this->flag, flagcpy, padding, plus); !e) {
                    t.err = e;
                }
                this->flag = flagcpy;
                return true;
            }

            THISTYPE(H2PushPromiseFrame, push_promise)
        };

        DEF_FRAME(H2PingFrame) {
            USING_H2FRAME;
            constexpr H2PingFrame()
                : H2FRAME(H2FType::ping) {}

           private:
            std::uint8_t data_[8] = {0};

           public:
            std::uint8_t* payload() {
                return data_;
            }

            H2Err parse(rawframe_t & v, h2request_t & t) override {
                H2Frame::parse(v, t);
                if (v.len != 8) {
                    t.err = H2Error::frame_size;
                    return t.err;
                }
                if (v.id != 0) {
                    t.err = H2Error::protocol;
                    return t.err;
                }
                ::memmove(data_, v.buf.data(), 8);
                return true;
            }

            H2Err serialize(std::uint32_t fsize, writer_t & se, h2request_t & t) override {
                if (auto e = H2FRAME::serialize(8, se, t); !e) {
                    t.err = e;
                }
                se.write_byte(data_, 8);
                return true;
            }

            THISTYPE(H2PingFrame, ping);
        };

        DEF_FRAME(H2GoAwayFrame) {
            USING_H2FRAME;
            H2GoAwayFrame()
                : H2FRAME(H2FType::goaway) {}

           private:
            std::int32_t lastid = 0;
            std::uint32_t errcode = 0;
            string_t additionaldata;

           public:
            std::uint32_t code() const {
                return errcode;
            }

            std::int32_t id() const {
                return lastid;
            }

            void set_code(std::uint32_t code) {
                errcode = code;
            }

            void set_lastid(std::int32_t lid) {
                lastid = lid;
            }

            string_t& optdata() const {
                return additionaldata;
            }

            H2Err parse(rawframe_t & v, h2request_t & t) override {
                H2Frame::parse(v, t);
                reader_t se(v.buf);
                if (v.id != 0) {
                    t.err = H2Error::protocol;
                    return t.err;
                }
                if (se.read_ntoh(lastid)) {
                    t.err = H2Error::internal;
                    return t.err;
                }
                if (lastid < 0) {
                    t.err = H2Error::protocol;
                    return t.err;
                }
                if (se.read_ntoh(errcode)) {
                    t.err = H2Error::internal;
                    return t.err;
                }
                v.buf.erase(0, 8);
                additionaldata = std::move(v.buf);
                return true;
            }

            H2Err serialize(std::uint32_t fsize, writer_t & se, h2request_t & t) override {
                if (additionaldata.size() + 9 + 8 > fsize) {
                    return H2Error::frame_size;
                }
                if (auto e = H2FRAME::serialize((std::uint32_t)additionaldata.size() + 8, se, t)) {
                    t.err = e;
                    return e;
                }
                se.write_hton(lastid);
                se.write_hton(errcode);
                se.write_byte(additionaldata);
                return true;
            }

            THISTYPE(H2GoAwayFrame, goaway)
        };

        DEF_FRAME(H2WindowUpdateFrame) {
            USING_H2FRAME;
            constexpr H2WindowUpdateFrame()
                : H2FRAME(H2FType::window_update) {}

           private:
            std::int32_t value = 0;

           public:
            std::int32_t update() const {
                return value;
            }

            bool set_update(std::int32_t v) {
                if (v <= 0) {
                    return false;
                }
                value = v;
                return true;
            }

            H2Err parse(rawframe_t & v, h2request_t & t) override {
                H2Frame::parse(v, t);
                reader_t se(v.buf);
                if (v.len != 4) {
                    t.err = H2Error::frame_size;
                    return t.err;
                }
                if (!se.read_ntoh(value)) {
                    t.err = H2Error::internal;
                    return false;
                }
                if (value <= 0) {
                    t.err = H2Error::protocol;
                    return t.err;
                }
                return true;
            }

            H2Err serialize(std::uint32_t fsize, writer_t & se, h2request_t & t) override {
                if (value <= 0) {
                    t.err = H2Error::protocol;
                    return t.err;
                }
                if (auto e = H2Frame::serialize(4, se, t)) {
                    t.err = e;
                    return t.err;
                }
                se.write_hton(value);
                return true;
            }
        };  // namespace socklib

#undef H2FRAME
#undef DETECTTYPE_RET
#undef THISTYPE
#undef TEMPLATE_PARAM
#undef DEF_FRAME
#undef DEF_H2TYPE
#undef DEC_FRAME
#undef DETECTTYPE
#undef USING_H2FRAME
#undef H2TYPE_PARAMS
#undef TEMPLATE_PARAM

    }  // namespace v2

}  // namespace socklib
