#pragma once
#include "http1.h"
#include "http.h"
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
            void set_type(WsFType t) {
                type = t & WsFType::mask_opcode;
            }

            void set_continuous(bool f) {
                continuous = f;
            }

            void set_masked(bool f) {
                masked = f;
            }

            void set_maskkey(std::uint32_t key) {
                maskkey = key;
            }

            bool is_masked() {
                return masked;
            }

            string_t& get_data() {
                return data;
            }

            std::uint32_t get_maskkey() const {
                return maskkey;
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
                              std::uint32_t* maskkey = nullptr, CancelContext* cancel = nullptr) {
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
                    w.template write_as<std::uint8_t>(size | mmask);
                }
                else if (size <= 0xffff) {
                    w.template write_as<std::uint8_t>(126 | mmask);
                    w.write_hton((std::uint16_t)size);
                }
                else {
                    w.template write_as<std::uint8_t>(127 | mmask);
                    w.write_hton((std::uint8_t)size);
                }
                if (maskkey) {
                    std::uint8_t key = commonlib2::translate_byte_net_and_host<std::uint8_t>(&maskkey);
                    char* k = reinterpret_cast<char*>(&key);
                    w.write_byte(k, 4);
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
                    return true;
                };
                if (!read_if(2)) {
                    return false;
                }
                reader_t read(buf.buf);
                WsFType type;
                read.template read_as<std::uint8_t>(type);
                if (any(type & WsFType::mask_reserved)) {
                    return false;
                }
                frame.set_continuous(!any(type & WsFType::mask_fin));
                frame.set_type(type);
                std::uint8_t sz = 0;
                size_t size = 0, total = 2;
                read.template read(sz);
                if (sz & 0x80) {
                    frame.set_masked(true);
                    sz &= 0x7f;
                }
                else {
                    frame.set_masked(false);
                }
                if (sz <= 125) {
                    size = sz;
                }
                else if (sz == 126) {
                    if (!read_if(4)) {
                        return false;
                    }
                    std::uint16_t sh;
                    read.read_ntoh(sh);
                    size = sh;
                    total = 4;
                }
                else if (sz == 127) {
                    if (!read_if(10)) {
                        return false;
                    }
                    read.read_ntoh(size);
                    total = 10;
                }
                else {
                    return false;
                }
                if (frame.is_masked()) {
                    if (!read_if(total + 4)) {
                        return false;
                    }
                    std::uint32_t key;
                    read.read_ntoh(key);
                    frame.set_maskkey(key);
                    total += 4;
                }
                total += size;
                if (!read_if(total)) {
                    return false;
                }
                frame.get_data().clear();
                read.read_byte(frame.get_data(), size);
                if (frame.is_masked()) {
                    mask(frame.get_data(), frame.get_maskkey());
                }
                buf.buf.erase(0, total);
                return true;
            }
        };

        constexpr const char* ws_magic_guid = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

        template <class String>
        struct WebSocketConn : public InetConn {
            std::shared_ptr<InetConn> conn;
            ReadContext<String> buf;
            using frame_t = WsFrame<String>;
            using writer_t = commonlib2::Serializer<String>;
            using io_t = WebsocketIO<String>;

           private:
            bool binary = false;
            bool client = false;
            void (*cb)(void* ctx, frame_t& frame) = nullptr;
            void* ctx = nullptr;

           public:
            WebSocketConn(std::shared_ptr<InetConn>&& base)
                : conn(std::move(base)), InetConn(nullptr) {}

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

            bool write(IWriteContext& w, WsFType frame, std::uint32_t* maskkey, CancelContext* cancel) {
                std::uint32_t msksub = 0;
                if (client && !maskkey) {
                    msksub = std::random_device()();
                    maskkey = &msksub;
                }
                while (true) {
                    auto ptr = w.bufptr();
                    auto size = w.size();
                    if (!ptr || !size) break;
                    if (!io_t::write(conn, ptr, size, frame | WsFType::mask_fin, maskkey, cancel)) {
                        return false;
                    }
                    if (!w.done()) {
                        continue;
                    }
                    break;
                }
                return true;
            }

            virtual bool write(IWriteContext& w, CancelContext* cancel = nullptr) override {
                return write(w, binary ? WsFType::binary : WsFType::text, nullptr, cancel);
            }

            virtual bool write(const char* data, size_t size, CancelContext* cancel = nullptr) override {
                WriteContext w;
                w.ptr = data;
                w.bufsize = size;
                return write(w, cancel);
            }

            virtual bool write(const char* str, CancelContext* cancel = nullptr) override {
                if (!str) return true;
                return write(str, ::strlen(str), cancel);
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

            bool stat(ConnStat st) const {
                conn->stat(st);
                st.type = ConnType::websocket;
                return true;
            }

            void close_code(std::uint16_t code, CancelContext* cancel = nullptr) {
                writer_t write;
                write.write_hton(code);
                std::uint32_t mask = 0;
                if (client) {
                    mask = std::random_device()();
                }
                io_t::write(conn, write.get().data(), write.get().size(), WsFType::closing | WsFType::mask_fin, client ? &mask : nullptr, cancel);
                conn->close();
            }

            virtual void close(CancelContext* cancel = nullptr) override {
                close_code(1000);
            }

            virtual bool reset(IResetContext& set) override {
                return conn->reset(set);
            }
        };

        template <class String, class Header>
        struct WebSocektRequestContext {
            using string_t = String;
            using header_t = Header;
            string_t url;
            HttpError err = HttpError::none;
            TCPError tcperr = TCPError::none;
            string_t cacert;
            header_t request;
            std::uint16_t statuscode;
            header_t response;
            string_t responsebody;
            std::uint8_t ip_version = 0;
        };

        template <class String, class Header>
        struct WebSocket {
            using string_t = String;
            using baseclient_t = Http1Client<String, Header, String>;
            using base_t = HttpBase<String, Header, String>;
            using request_t = WebSocektRequestContext<String, Header>;
            using websocketconn_t = WebSocketConn<string_t>;

            static bool open(std::shared_ptr<websocketconn_t>& conn, request_t& req, CancelContext* cancel = nullptr) {
                RequestContext<String, Header, String> ctx;
                ctx.http_version = 1;
                ctx.ip_version = req.ip_version;
                ctx.method = "GET";
                ctx.default_scheme = HttpDefaultScheme::ws;
                ctx.flag = RequestFlag::no_read_body;
                ctx.url = req.url;
                std::shared_ptr<InetConn> baseconn;
                if (!base_t::open(baseconn, ctx, cancel, "ws", "wss")) {
                    req.err = ctx.err;
                    req.tcperr = ctx.tcperr;
                }
                std::random_device device;
                std::uniform_int_distribution<unsigned short> uni(0, 0xff);
                unsigned char bytes[16];
                for (auto i = 0; i < 16; i++) {
                    bytes[i] = uni(device);
                }
                commonlib2::Base64Context b64;
                std::string token;
                commonlib2::Reader(commonlib2::Sized(bytes)).readwhile(token, commonlib2::base64_encode, &b64);
                ctx.request = {{"Upgrade", "websocket"}, {"Connection", "Upgrade"}, {"Sec-WebSocket-Version", "13"}};
                ctx.request.emplace("Sec-WebSocket-Key", token);
                for (auto& h : req.request) {
                    ctx.request.emplace(h.first, h.second);
                }
                commonlib2::SHA1Context hash;
                std::string result;
                commonlib2::Reader(token + ws_magic_guid).readwhile(commonlib2::sha1, hash);
                commonlib2::Reader(commonlib2::Sized(hash.result)).readwhile(result, commonlib2::base64_encode, &b64);
                if (!baseclient_t::request(baseconn, ctx, cancel)) {
                    req.err = ctx.err;
                    req.tcperr = ctx.tcperr;
                    return false;
                }
                Http1ReadContext<String, Header, String> read(ctx);
                if (!baseclient_t::response(baseconn, read, cancel)) {
                    req.err = ctx.err;
                    req.tcperr = ctx.tcperr;
                    return false;
                }
                req.statuscode = ctx.statuscode;
                req.response = std::move(ctx.response);
                req.responsebody = std::move(ctx.responsebody);
                if (req.statuscode != 101) {
                    req.err = HttpError::invalid_status;
                    return false;
                }
                bool sec_websock = false;
                bool upgrade = false;
                bool connection_upgrade = false;
                for (auto& h : req.response) {
                    using commonlib2::str_eq;
                    if (!sec_websock && str_eq(h.first, "sec-websocket-accept", base_t::header_cmp)) {
                        if (h.second != result) {
                            req.err = HttpError::invalid_header;
                            return false;
                        }
                        sec_websock = true;
                    }
                    else if (!upgrade && str_eq(h.first, "upgrade", base_t::header_cmp)) {
                        if (!str_eq(h.second, "websocket", base_t::header_cmp)) {
                            req.err = HttpError::invalid_header;
                            return false;
                        }
                        upgrade = true;
                    }
                    else if (!connection_upgrade && str_eq(h.first, "connection", base_t::header_cmp), base_t::header_cmp) {
                        if (!str_eq(h.second, "upgrade", base_t::header_cmp)) {
                            req.err = HttpError::invalid_header;
                            return false;
                        }
                        connection_upgrade = true;
                    }
                    if (upgrade && connection_upgrade && sec_websock) {
                        break;
                    }
                }
                if (!upgrade || !connection_upgrade || !sec_websock) {
                    req.err = HttpError::invalid_header;
                    return false;
                }
                conn = std::make_shared<websocketconn_t>(std::move(baseconn));
                return true;
            }

            template <template <class...> class Map, class Table>
            static bool verify_accept(std::shared_ptr<ServerRequestProxy<String, Header, String, Map, Table>>& req) {
                if (!req) {
                    return false;
                }
                if (req->get_method() != "GET") {
                    return false;
                }
                Header& response = req->responseHeader();
                response.emplace("Upgrade", "websocket");
                response.emplace("Connection", "Upgrade");
                bool connection = false, upgrade = false, sec_websocket = false;
                for (auto& h : req->requestHeader()) {
                    using base_t = HttpBase<String, Header, String>;
                    using commonlib2::str_eq;
                    if (!connection && str_eq(h.first, "connection", base_t::header_cmp)) {
                        if (!str_eq(h.second, "upgrade", base_t::header_cmp)) {
                            return false;
                        }
                        connection = true;
                    }
                    else if (!upgrade && str_eq(h.first, "upgrade", base_t::header_cmp)) {
                        if (!str_eq(h.second, "websocket", base_t::header_cmp)) {
                            return false;
                        }
                        upgrade = true;
                    }
                    else if (!sec_websocket && str_eq(h.first, "sec-webSocket-key", base_t::header_cmp)) {
                        commonlib2::Base64Context b64;
                        commonlib2::SHA1Context hash;
                        std::string result;
                        commonlib2::Reader<String>(h.second + ws_magic_guid).readwhile(commonlib2::sha1, hash);
                        commonlib2::Reader(commonlib2::Sized(hash.result)).readwhile(result, commonlib2::base64_encode, &b64);
                        response.emplace("Sec-WebSocket-Accept", result);
                        sec_websocket = true;
                    }
                    if (connection && sec_websocket && upgrade) {
                        break;
                    }
                }
                if (!connection || !sec_websocket || !upgrade) {
                    return false;
                }
                return true;
            }

            template <template <class...> class Map, class Table>
            static std::shared_ptr<websocketconn_t> accept(std::shared_ptr<ServerRequestProxy<String, Header, String, Map, Table>>& req, bool verifyed = false) {
                if (!verifyed && !verify_accept(req)) {
                    return nullptr;
                }
                return std::make_shared<websocketconn_t>(req->hijack());
            }
        };
    }  // namespace v2
}  // namespace socklib