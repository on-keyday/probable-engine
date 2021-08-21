#pragma once
#include "protcolbase.h"
#include "http.h"
#include <serializer.h>

namespace socklib {
    enum class WsFType : unsigned char {
        empty = 0xF0,
        error = 0xF1,
        incomplete = 0xF2,
        opening = 0xF3,
        text = 0x01,
        binary = 0x02,
        ping = 0x09,
        pong = 0x0A,
        closing = 0x08,
        mask_fin = 0x80,
        mask_reserved = 0xA0,
        mask_opcode = 0x0f,
    };
    DEFINE_ENUMOP(WsFType)
    struct WsFrame {
       private:
        friend struct WebSocketConn;
        WsFType type = WsFType::empty;
        bool continuous = false;
        bool masked = false;
        unsigned int maskkey = 0;
        std::string data;

        void unmask() {
            unsigned int res = commonlib2::translate_byte_net_and_host<unsigned int>((char*)&maskkey);
            size_t count = 0;
            for (auto& c : data) {
                c ^= reinterpret_cast<char*>(&res)[count % 4];
            }
            masked = false;
        }

       public:
        const std::string& get_data() {
            if (masked) {
                unmask();
            }
            return data;
        }
    };
    struct WebSocketConn {
       private:
        std::shared_ptr<Conn> conn;
        std::string buffer;
        commonlib2::Deserializer<std::string&> dec;

        bool send_detail(const char* data, size_t len, WsFType frame) {
            if (!conn) return false;
            commonlib2::Serializer<std::string> s;
            if (any(frame & WsFType::mask_reserved)) {
                return false;
            }
            s.template write_as<unsigned char>(frame);
            if (!data) {
                len = 0;
            }
            if (len <= 125) {
                s.template write_as<unsigned char>(len);
            }
            else if (len <= 0xffff) {
                s.template write_as<unsigned char>(126);
                s.write_hton((unsigned short)len);
            }
            else {
                s.template write_as<unsigned char>(127);
                s.write_hton(len);
            }
            if (len) {
                s.write_byte(std::string_view(data, len));
            }
            return conn->write(s.get());
        }

       public:
        WebSocketConn(std::shared_ptr<Conn>&& in)
            : dec(buffer), conn(std::move(in)) {}

        bool send(const char* data, size_t len, bool as_binary = false) {
            return send_detail(data, len, (as_binary ? WsFType::binary : WsFType::text) | WsFType::mask_fin);
        }

        bool control(WsFType cmd, const char* data = nullptr, size_t len = 0) {
            if (cmd != WsFType::closing && cmd != WsFType::ping && cmd != WsFType::pong) {
                return false;
            }
            return send_detail(data, len, cmd | WsFType::mask_fin);
        }

        bool recv(WsFrame& frame) {
            auto readlim = [this](unsigned int lim) {
                while (buffer.size() < lim) {
                    if (!conn->read(buffer)) {
                        return false;
                    }
                }
                return true;
            };
            if (!readlim(2)) {
                return false;
            }
            dec.template read_as<unsigned char>(frame.type);
            if (any(frame.type & WsFType::mask_reserved)) {
                return false;
            }
            if (any(frame.type & WsFType::mask_fin)) {
                frame.continuous = false;
            }
            else {
                frame.continuous = true;
                frame.type &= WsFType::mask_opcode;
            }
            unsigned char sz = 0;
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
                if (!readlim(4)) {
                    return false;
                }
                unsigned short sh;
                dec.read_ntoh(sh);
                size = sh;
                total = 4;
            }
            else if (sz == 127) {
                if (!readlim(10)) {
                    return false;
                }
                dec.read_ntoh(size);
                total = 10;
            }
            else {
                return false;
            }
            if (frame.masked) {
                if (!readlim(total + 4)) {
                    return false;
                }
                dec.read_ntoh(frame.maskkey);
                total += 4;
            }
            total += size;
            if (!readlim(total)) {
                return false;
            }
            frame.data.resize(size);
            dec.read_byte(frame.data, size);
            buffer.erase(0, total);
            dec.base_reader().seek(0);
            return true;
        }

        void close() {
            if (conn && conn->is_opened()) {
                send_detail(nullptr, 0, WsFType::closing | WsFType::mask_fin);
                conn->close();
            }
        }
        ~WebSocketConn() {
            close();
        }
    };

    struct WebSocket {
        static std::shared_ptr<WebSocketConn> hijack_http(std::shared_ptr<HttpConn> in) {
            return std::make_shared<WebSocketConn>(std::move(in->hijack()));
        }
    };
}  // namespace socklib