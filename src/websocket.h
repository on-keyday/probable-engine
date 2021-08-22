#pragma once
#include "protcolbase.h"
#include "http.h"
#include <serializer.h>
#include <random>

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
        mask_reserved = 0x70,
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
            char* key = reinterpret_cast<char*>(&res);
            for (auto& c : data) {
                c = c ^ key[count % 4];
                count++;
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
    struct WebSocketConn : public AppLayer {
       protected:
        std::string buffer;
        commonlib2::Deserializer<std::string&> dec;
        bool closed = false;

        bool send_detail(const char* data, size_t len, WsFType frame, bool mask = false, unsigned int key = 0) {
            if (!conn) return false;
            if (any(frame & WsFType::closing) && closed) {
                return false;
            }
            commonlib2::Serializer<std::string> s;
            if (any(frame & WsFType::mask_reserved)) {
                return false;
            }
            s.template write_as<unsigned char>(frame);
            if (!data) {
                len = 0;
            }
            unsigned char mmask = 0;
            if (mask) {
                mmask = 0x80;
            }
            if (len <= 125) {
                s.template write_as<unsigned char>(len | mmask);
            }
            else if (len <= 0xffff) {
                s.template write_as<unsigned char>(126 | mmask);
                s.write_hton((unsigned short)len);
            }
            else {
                s.template write_as<unsigned char>(127 | mmask);
                s.write_hton((unsigned long long)len);
            }
            if (len) {
                if (!mask) {
                    s.write_byte(std::string_view(data, len));
                }
                else {
                    std::string masked(data, len);
                    unsigned int maskkey = commonlib2::translate_byte_net_and_host<unsigned int>((char*)&key);
                    char* k = reinterpret_cast<char*>(&maskkey);
                    size_t count = 0;
                    for (auto& c : masked) {
                        c = c ^ k[count % 4];
                        count++;
                    }
                    s.write_byte(masked);
                }
            }
            auto sent = conn->write(s.get());
            if (sent && any(frame & WsFType::closing)) {
                closed = true;
            }
            return sent;
        }

       public:
        WebSocketConn(std::shared_ptr<Conn>&& in)
            : dec(buffer), AppLayer(std::move(in)) {}

        virtual bool send(const char* data, size_t len, bool as_binary = false) {
            return send_detail(data, len, (as_binary ? WsFType::binary : WsFType::text) | WsFType::mask_fin);
        }

        virtual bool control(WsFType cmd, const char* data = nullptr, size_t len = 0) {
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
                frame.type &= WsFType::mask_opcode;
            }
            else {
                frame.continuous = true;
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
            dec.read_byte(frame.data, size);
            buffer.erase(0, total);
            dec.base_reader().seek(0);
            if (any(frame.type & WsFType::closing)) {
                closed = true;
                close();
            }
            return true;
        }

        void close() override {
            if (conn && conn->is_opened()) {
                if (!closed) send_detail(nullptr, 0, WsFType::closing | WsFType::mask_fin);
                conn->close();
            }
        }
        virtual ~WebSocketConn() {
            close();
        }
    };

    struct WebSocketServerConn : WebSocketConn {
        WebSocketServerConn(std::shared_ptr<Conn>&& in)
            : WebSocketConn(std::move(in)) {}
    };

    struct WebSocketClientConn : WebSocketConn {
        WebSocketClientConn(std::shared_ptr<Conn>&& in)
            : WebSocketConn(std::move(in)) {}

        bool send(const char* data, size_t len, bool as_binary = false) override {
            return send_key(data, len, std::random_device()(), as_binary);
        }

        bool control(WsFType cmd, const char* data = nullptr, size_t len = 0) override {
            return control_key(cmd, std::random_device()(), data, len);
        }

        bool send_key(const char* data, size_t len, unsigned int key, bool as_binary = false) {
            return send_detail(data, len, (as_binary ? WsFType::binary : WsFType::text) | WsFType::mask_fin, true, key);
        }

        bool control_key(WsFType cmd, unsigned int key = 0, const char* data = nullptr, size_t len = 0) {
            if (cmd != WsFType::closing && cmd != WsFType::ping && cmd != WsFType::pong) {
                return false;
            }
            return send_detail(data, len, cmd | WsFType::mask_fin, true, key);
        }
    };

    struct WebSocket {
        static std::shared_ptr<WebSocketServerConn> hijack_httpserver(std::shared_ptr<HttpServerConn> in) {
            return std::make_shared<WebSocketServerConn>(in->hijack());
        }

        static std::shared_ptr<WebSocketClientConn> hijack_httpclient(std::shared_ptr<HttpClientConn> in) {
            return std::make_shared<WebSocketClientConn>(in->hijack());
        }

       private:
        struct has_bool_impl {
            template <class F>
            static std::true_type has(decltype((bool)std::declval<F>(), (void)0)*);

            template <class F>
            static std::false_type has(...);
        };

        template <class F>
        struct has_bool : decltype(has_bool_impl::has<F>(nullptr)) {};

        template <class F, bool f = has_bool<F>::value>
        struct invoke_cb {
            static bool invoke(F&& in, const HttpConn::Header& r, HttpConn::Header& h,
                               std::shared_ptr<HttpServerConn>& c) {
                return in(r, h, c);
            }
        };

        template <class F>
        struct invoke_cb<F, true> {
            static bool invoke(F&& in, const HttpConn::Header& r, HttpConn::Header& h,
                               std::shared_ptr<HttpServerConn>& c) {
                if (!(bool)in) return true;
                return in(r, h, c);
            }
        };

       public:
        template <class F = bool (*)(const HttpConn::Header&, HttpConn::Header&, std::shared_ptr<HttpServerConn>&)>
        static std::shared_ptr<WebSocketServerConn>
        default_hijack_server_proc(std::shared_ptr<HttpServerConn>& conn, F&& cb = F()) {
            auto& req = conn->request();
            std::string method;
            if (auto found = req.find(":method"); found == req.end()) {
                return nullptr;
            }
            else {
                method = found->second;
            }
            socklib::HttpConn::Header h = {{"Upgrade", "websocket"}, {"Connection", "Upgrade"}};
            auto sendmsg = [&](unsigned short status, const char* msg, const char* data) {
                conn->send(status, msg, {{"content-type", "text/plain"}}, data, strlen(data));
            };
            if (method != "GET") {
                sendmsg(405, "Method Not Allowed", "method not allowed");
                return nullptr;
            }
            if (auto found = req.find("upgrade"); found == req.end() || found->second != "websocket") {
                sendmsg(400, "Bad Request", "Request is not WebSocket upgreade");
                return nullptr;
            }
            if (auto found = req.find("sec-websocket-key"); found != req.end()) {
                Base64Context b64;
                SHA1Context hash;
                std::string result;
                Reader(found->second + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11").readwhile(sha1, hash);
                Reader(Sized(hash.result)).readwhile(result, base64_encode, &b64);
                h.emplace("Sec-WebSocket-Accept", result);
            }
            else {
                sendmsg(400, "Bad Request", "Request has no Sec-WebSocket-Key header.");
                return nullptr;
            }
            if (!invoke_cb<F>::invoke(std::forward<F>(cb), req, h, conn)) {
                return nullptr;
            }
            if (!conn->send(101, "Switching Protocols", h)) {
                return nullptr;
            }
            return socklib::WebSocket::hijack_httpserver(conn);
        }
    };
}  // namespace socklib