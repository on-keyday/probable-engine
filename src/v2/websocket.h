#pragma once
#include "http1.h"
#include <serializer.h>
#include <random>

namespace socklib {
    namespace v2 {

        enum class WsFType : std::uint8_t {
            empty = 0xF0,
            continuous = 0x00,
            text = 0x01,
            binary = 0x02,
            closing = 0x08,
            ping = 0x09,
            pong = 0x0A,
            mask_fin = 0x80,
            mask_reserved = 0x70,
            mask_opcode = 0x0f,
        };

        DEFINE_ENUMOP(WsFType)

        template <class String>
        struct WsFrame {
            using string_t = String;

           private:
            WsFType type = WsFType::empty;
            bool continuous = false;
            bool masked = false;
            unsigned int maskkey = 0;
            string_t data;

           public:
            string_t& get_data() {
                return data;
            }

            bool is_(WsFType f) const {
                return type == f;
            }

            const char* frame_type() const {
                switch (type) {
                    case WsFType::binary:
                        return "binary";
                    case WsFType::text:
                        return "text";
                    case WsFType::ping:
                        return "ping";
                    case WsFType::pong:
                        return "pong";
                    case WsFType::closing:
                        return "close";
                    case WsFType::continuous:
                        return "continuous";
                    default:
                        return "unknown";
                }
            }

            bool frame_type(const char* frametype) const {
                if (!frametype) return false;
                return strcmp(frame_type(), frametype) == 0;
            }
        };

        template <class String>
        struct WebsocketIO {
            using conn_t = std::shared_ptr<InetConn>;
            using string_t = String;
            using writer_t = commonlib2::Serializer<string_t>;
            using frame_t = WsFrame<string_t>;
            using readctx_t = ReadContext<string_t>;
            using reader_t = commonlib2::Deserializer<string_t&>;

            template <class Buffer>
            static void mask(Buffer& data, std::uint32_t maskkey) {
                std::uint32_t res = commonlib2::translate_byte_net_and_host<std::uint32_t>(&maskkey);
                size_t count = 0;
                char* key = reinterpret_cast<char*>(&res);
                for (auto& c : data) {
                    c = c ^ key[count % 4];
                    count++;
                }
            }

            static bool write(conn_t& conn, const char* data, size_t size, WsFType frame,
                              std::int32_t* maskkey = nullptr, CancelContext* cancel = nullptr) {
                if (!conn) return false;
                if (any(frame & WsFType::mask_reserved)) {
                    return false;
                }
                writer_t w;
                w.template write_as<std::uint8_t>(frame);
                if (!data) {
                    size = 0;
                }
                std::uint8_t mmask = 0;
                if (maskkey) {
                    mmask = 0x80;
                }
                if (size <= 125) {
                    s.template write_as<std::uint8_t>(size | mmask);
                }
                else if (size <= 0xffff) {
                    s.template write_as<std::uint8_t>(126 | mmask);
                    s.write_hton((std::uint16_t)size);
                }
                else {
                    s.template write_as<std::uint8_t>(127 | mmask);
                    s.write_hton((std::uint8_t)size);
                }
                if (maskkey) {
                    std::uint8_t key = commonlib2::translate_byte_net_and_host<std::uint8_t>(&maskkey);
                    char* k = reinterpret_cast<char*>(&key);
                    s.write_byte(k, 4);
                    if (size) {
                        string_t masked(data, size);
                        mask(masked, *maskkey);
                        w.write_byte(masked);
                    }
                }
                else if (size) {
                    w.write_byte(data, size);
                }
                WriteContext c;
                c.ptr = w.get().data();
                c.bufsize = w.get().size();
                return conn->write(c, cancel);
            }

            static bool read(frame_t& frame, conn_t& conn, readctx_t& buf, CancelContext* cancel = nullptr) {
                auto read_if = [&](size_t size) {
                    if (buf.buf.size() < size) {
                        if (!conn->read(buf, cancel)) {
                            return false;
                        }
                    }
                };
                if (!read_if(2)) {
                    return false;
                }
                reader_t read(buf.buf);
                read.template read_as<std::uint8_t>(frame.type);
                if (any(frame.type & WsFType::mask_reserved)) {
                    return false;
                }
                if (any(frame.type & WsFType::mask_fin)) {
                    frame.continuous = false;
                    frame.type &= WsFType::mask_opcode;
                }
                else {
                    frame.continuous = true;
                }
                std::uint8_t sz = 0;
                size_t size = 0, total = 2;
                dec.template read(sz);
                if (sz & 0x80) {
                    frame.masked = true;
                    sz &= 0x7f;
                }
                else {
                    frame.masked = false;
                }
                if (sz <= 125) {
                    size = sz;
                }
                else if (sz == 126) {
                    if (!read_if(4)) {
                        return false;
                    }
                    std::uint16_t sh;
                    dec.read_ntoh(sh);
                    size = sh;
                    total = 4;
                }
                else if (sz == 127) {
                    if (!read_if(10)) {
                        return false;
                    }
                    dec.read_ntoh(size);
                    total = 10;
                }
                else {
                    return false;
                }
                if (frame.masked) {
                    if (!read_if(total + 4)) {
                        return false;
                    }
                    dec.read_ntoh(frame.maskkey);
                    total += 4;
                }
                total += size;
                if (!read_if(total)) {
                    return false;
                }
                frame.data.clear();
                dec.read_byte(frame.data, size);
                if (frame.masked) {
                    mask(frame.data, frame.maskkey);
                }
                buf.buf.erase(0, total);
                return true;
            }
        };

        constexpr const char* ws_magic_guid = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

        template <class String>
        struct WebSocketConn : InetConn {
            std::shared_ptr<InetConn> conn;
            ReadContext<String> buf;
            using frame_t = WsFrame<string_t>;
            bool binary = false;
            bool client = false;
            void (*cb)(void* ctx, frame_t& frame) = nullptr;
            void* ctx = nullptr;
            using io_t = WebsocketIO<String>;

            void set_mode(bool is_binary) {
                binary = is_binary;
            }

            void set_client(bool is_client) {
                client = is_client;
            }

            void set_callback(decltype(cb) cb, void* ctx = nullptr) {
                this->cb = cb;
                this->ctx = ctx;
            }

            virtual bool write(IWriteContext& w, CancelContext* cancel) override {
                while (true) {
                    auto ptr = w.bufptr();
                    auto size = w.size();
                    if (!ptr || !size) break;
                    std::uint32_t mask = 0;
                    if (client) {
                        mask = std::random_device()();
                    }
                    if (!io_t::write(conn, ptr, size, (binary ? WsFType::binary : WsFType::text) | WsFType::mask_fin, client ? &mask : nullptr, cancel)) {
                        return false;
                    }
                    if (!w.done()) {
                        continue;
                    }
                    break;
                }
                return true;
            }

            virtual bool read(IReadContext& read, CancelContext* cancel) override {
                while (true) {
                    frame_t frame;
                    if (!io_t::read(frame, conn, buf, cancel)) {
                        return false;
                    }
                    if (frame.is_(WsFType::closing)) {
                        conn->close();
                        return false;
                    }
                    if (frame.is_(WsFType::ping)) {
                        std::uint32_t mask = 0;
                        if (client) {
                            mask = std::random_device()();
                        }
                        if (!io_t::write(conn, frame.get_data().data(), frame.get_data().size(),
                                         WsFType::pong, client ? &mask : nullptr, cancel)) {
                            return false;
                        }
                    }
                    if (cb) {
                        cb(ctx, frame);
                    }
                    if (frame.is_(WsFType::binary) || frame.is_(WsFType::text)) {
                        read.append(frame.get_data().data(), frame.get_data().size());
                    }
                    if (read.require()) {
                        continue;
                    }
                    break;
                }
                return true;
            }

            static bool stat(ConnStat st) const {
                conn->stat(st);
                st.type = ConnType::websocket;
                return true;
            }

            virtual void close(CancelContext* cancel = nullptr) {
                std::uint32_t mask = 0;
                if (client) {
                    mask = std::random_device()();
                }
                io_t::write(conn, "\x00\x00\x03\xe8", 4, WsFType::closing | WsFType::mask_fin, client ? &mask : nullptr, cancel);
                conn->close();
            }
        };

        template <class String, class Header>
        struct WebSocket {
            using string_t = String;
            using baseclient_t = Http1Client<String, Header, String>;

            static bool open(const string_t& url) {
                RequestContext<String, Header, String> ctx;
            }
        };
    }  // namespace v2
}  // namespace socklib