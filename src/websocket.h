#pragma once
#include "protcolbase.h"
#include "http.h"
#include <serializer.h>
#include <random>

namespace socklib {
    enum class WsFType : unsigned char {
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

        bool is_close() const {
            return type == WsFType::closing;
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
    struct WebSocketConn : public AppLayer {
       protected:
        std::string buffer;
        commonlib2::Deserializer<std::string&> dec;
        bool closed = false;

        bool send_detail(const char* data, size_t len, WsFType frame, bool mask = false, unsigned int key = 0) {
            if (!conn) return false;
            if ((frame & WsFType::mask_opcode) == WsFType::closing && closed) {
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
            if (mask) {
                unsigned int maskkey = commonlib2::translate_byte_net_and_host<unsigned int>((char*)&key);
                char* k = reinterpret_cast<char*>(&maskkey);
                s.write_byte(k, 4);
                if (len) {
                    std::string masked(data, len);
                    size_t count = 0;
                    for (auto& c : masked) {
                        c = c ^ k[count % 4];
                        count++;
                    }
                    s.write_byte(masked);
                }
            }
            else if (len) {
                s.write_byte(std::string_view(data, len));
            }
            auto sent = conn->write(s.get());
            if (sent && (frame & WsFType::mask_opcode) == WsFType::closing) {
                closed = true;
                conn->close();
            }
            return sent;
        }

       public:
        WebSocketConn(std::shared_ptr<Conn>&& in)
            : dec(buffer), AppLayer(std::move(in)) {
        }

        virtual bool send(const char* data, size_t len, bool as_binary = true) {
            return send_detail(data, len, (as_binary ? WsFType::binary : WsFType::text) | WsFType::mask_fin);
        }

        virtual bool send_text(const char* data, bool as_binary = false) {
            return send(data, data ? strlen(data) : 0, as_binary);
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
            frame.data.clear();
            dec.read_byte(frame.data, size);
            buffer.erase(0, total);
            dec.base_reader().seek(0);
            if (frame.type == WsFType::closing) {
                closed = true;
                close();
            }
            return true;
        }

        bool recvable() const {
            return (bool)buffer.size();
        }

        void close() override {
            if (conn && conn->is_opened()) {
                if (!closed) send_detail(nullptr, 0, WsFType::closing | WsFType::mask_fin);
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

        bool send(const char* data, size_t len, bool as_binary = true) override {
            return send_key(data, len, std::random_device()(), as_binary);
        }

        bool send_text(const char* data, bool as_binary = false) override {
            return send(data, data ? strlen(data) : 0, as_binary);
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

        template <class F, class Ret, bool f = has_bool<F>::value>
        struct invoke_cb {
            template <class... Args>
            static Ret invoke(F&& in, Args&&... args) {
                return in(std::forward<Args>(args)...);
            }
        };

        template <class F, class Ret>
        struct invoke_cb<F, Ret, true> {
            template <class... Args>
            static Ret invoke(F&& in, Args&&... args) {
                if (!(bool)in) return true;
                return in(std::forward<Args>(args)...);
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
            if (auto found = req.find("connection"); found == req.end() || found->second != "Upgrade") {
                sendmsg(400, "Bad Request", "Request is not WebSocket upgrade");
                return nullptr;
            }
            if (auto found = req.find("upgrade"); found == req.end() || found->second != "websocket") {
                sendmsg(400, "Bad Request", "Request is not WebSocket upgrade");
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
            if (!invoke_cb<F, bool>::invoke(std::forward<F>(cb), req, h, conn)) {
                return nullptr;
            }
            if (!conn->send(101, "Switching Protocols", h)) {
                return nullptr;
            }
            return hijack_httpserver(conn);
        }

       public:
        template <class F = bool (*)(HttpConn::Header&, bool success, const char* reason)>
        static std::shared_ptr<WebSocketClientConn>
        open(const char* url, bool encoded = false, const char* cacert = nullptr, F&& cb = F()) {
            URLContext<std::string> ctx;
            std::string path, query;
            unsigned short port = 0;
            if (!Http::setuphttp(url, encoded, port, ctx, path, query, "ws", "wss")) {
                return nullptr;
            }
            auto httpurl = (ctx.scheme == "wss" ? "https://" : "http://") + ctx.host +
                           (ctx.port.size() ? ":" + ctx.port : "") + path + query;
            auto client = Http::open(httpurl.c_str(), true, cacert);
            if (!client) return nullptr;
            HttpConn::Header h = {{"Upgrade", "websocket"}, {"Connection", "Upgrade"}, {"Sec-WebSocket-Version", "13"}};
            std::random_device device;
            std::uniform_int_distribution<unsigned short> uni(0, 0xff);
            unsigned char bytes[16];
            for (auto i = 0; i < 16; i++) {
                bytes[i] = uni(device);
            }
            Base64Context b64;
            std::string token;
            commonlib2::Reader(Sized(bytes)).readwhile(token, commonlib2::base64_encode, &b64);
            h.emplace("Sec-WebSocket-Key", token);
            SHA1Context hash;
            std::string result;
            Reader(token + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11").readwhile(sha1, hash);
            Reader(Sized(hash.result)).readwhile(result, base64_encode, &b64);
            auto invoke = [&](auto& h, bool success, const char* reason) {
                return invoke_cb<F, bool>::invoke(std::forward<F>(cb), h, success, reason);
            };
            if (!invoke(h, true, "prepare")) {
                return nullptr;
            }
            if (!client->send("GET", h)) {
                invoke(h, false, "send");
                return nullptr;
            }
            if (!client->recv(true)) {
                invoke(h, false, "recv");
                return nullptr;
            }
            auto& res = client->response();
            if (auto found = res.find(":status"); found == res.end() || found->second != "101") {
                invoke(res, false, "status");
                return nullptr;
            }
            if (auto found = res.find("sec-websocket-accept"); found == res.end() || found->second != result) {
                invoke(res, false, "security");
                return nullptr;
            }
            if (auto found = res.find("upgrade"); found == res.end() || found->second != "websocket") {
                invoke(res, false, "upgrade");
                return nullptr;
            }
            if (auto found = res.find("connection"); found == res.end() || found->second != "Upgrade") {
                invoke(res, false, "connection");
                return nullptr;
            }
            if (!invoke(res, true, "verify")) {
                return nullptr;
            }
            return hijack_httpclient(client);
        }
    };
}  // namespace socklib