#pragma once
#include "hpack.h"
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

        template <class String, template <class...> class Map>
#ifdef COMMONLIB2_HAS_CONCEPTS
        requires StringType<String>
#endif
        struct Http2RequestContext {
        };

#define TRY(...) \
    if (auto e = (__VA_ARGS__); !e) return e
#define DEC_FRAME(NAME) template <class String, template <class...> class Map> \
struct NAME
        DEC_FRAME(H2DataFrame);
        DEC_FRAME(H2HeaderFrame);
        DEC_FRAME(H2PriorityFrame);
        DEC_FRAME(H2RstStreamFrame);
        DEC_FRAME(H2SettingsFrame);
        DEC_FRAME(H2PushPromiseFrame);
        DEC_FRAME(H2PingFrame);
        DEC_FRAME(H2GoAwayFrame);
        DEC_FRAME(H2WindowUpdateFrame);

        template <class String, template <class...> class Map>
        struct H2Frame {
            using h2request_t = Http2RequestContext<String, Map>;
            using string_t = String;

           protected:
            H2FType type = H2FType::unknown;
            H2Flag flag = H2Flag::none;
            std::int32_t streamid = 0;

           public:
            constexpr H2Frame() {}
            constexpr H2Frame(H2FType type)
                : type(type) {}

            virtual H2Err parse(commonlib2::HTTP2Frame<string_t>& v, h2request_t&) {
                type = (H2FType)v.type;
                flag = (H2Flag)v.flag;
                streamid = v.id;
                return true;
            }

            //framesize will be Length octet
            virtual H2Err serialize(std::uint32_t framesize, commonlib2::Serializer<string_t>& se, h2request_t&) {
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

            static H2Err read_depends(bool& exclusive, int& id, std::uint8_t& weight, string_t& buf) {
                commonlib2::Deserializer<string_t&> se(buf);
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
                    std::string_view view(hpacked.data(), fsize - (padding + plus));
                    TRY(write_header(fsize, view));
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
#define DETECTTYPE(TYPE, FUNC)          \
    virtual TYPE<String, Map>* FUNC() { \
        return nullptr;                 \
    }
            DETECTTYPE(H2DataFrame, data)
            DETECTTYPE(H2HeaderFrame, header)
            DETECTTYPE(H2PriorityFrame, priority)
            DETECTTYPE(H2RstStreamFrame, rst_stream)
            DETECTTYPE(H2SettingsFrame, settings)
            DETECTTYPE(H2PushPromiseFrame, push_promise)
            DETECTTYPE(H2PingFrame, ping)
            DETECTTYPE(H2GoAwayFrame, goaway)
            DETECTTYPE(H2WindowUpdateFrame, window_update)

#undef DETECTTYPE

            bool is_set(H2Flag iflag) const {
                return any(iflag & flag);
            }

            virtual ~H2Frame() noexcept {}
        };
#define DEF_FRAME(NAME) DEC_FRAME(NAME) : H2Frame<String, Map>

        DEF_FRAME(H2DataFrame){

        };
#undef DEF_FRAME
#undef DEC_FRAME
#undef TRY
    }  // namespace v2
}  // namespace socklib