#pragma once
#include "http1.h"
#include "http2.h"

namespace socklib {
    struct HttpClient {
       private:
        std::shared_ptr<AppLayer> conn;
        Http2Context* h2 = nullptr;
        HttpClientConn* h1 = nullptr;
        int version = 0;

       public:
        bool open(const char* url, bool encoded = false, const char* cacert = nullptr) {
            HttpRequestContext ctx;
            if (!Http1::setuphttp(url, encoded, ctx)) {
                return false;
            }
            bool secure = ctx.url.scheme == "https";
            auto tcon = Http1::open_tcp_conn(ctx, cacert, "\x02h2\x08http/1.1", 12);
            if (!tcon) {
                return false;
            }
            const unsigned char* data = nullptr;
            unsigned int len = 0;
            if (secure) {
                SSL_get0_alpn_selected((SSL*)tcon->get_ssl(), &data, &len);
                if (!data) {
                    return false;
                }
            }
            auto hosts = ctx.host_with_port();
            if (!secure || strncmp("http/1.1", (const char*)data, 8) == 0) {
                auto tmp = std::make_shared<HttpClientConn>(std::move(tcon), std::move(hosts), std::move(ctx.path), std::move(ctx.query));
                h1 = tmp.get();
                conn = tmp;
                version = 1;
            }
            else if (strncmp("h2", (const char*)data, 2) == 0) {
                if (!tcon->write(h2_connection_preface)) {
                    return false;
                }
                auto tmp = Http2::init_object(tcon,ctx, std::move(ctx.path),std::move(ctx.query));
                h2 = tmp.get();
                conn = tmp;
                version = 2;
                h2->streams[0].send_settings({});
            }
            else {
                return false;
            }
            return true;
        }

        HttpConn::Header* Method(const char* method, const char* path, HttpConn::Header&& header = HttpConn::Header(),
                                 const char* data = nullptr, size_t size = 0, CancelContext* cancel = nullptr) {
            if (version == 0 || !method) return nullptr;
            auto spl = commonlib2::split(path, "?", 1);
            if (version == 2) {
                if (!h2) return nullptr;
                H2Stream* st = nullptr;
                if (!h2->get_stream(st)) {
                    return nullptr;
                }
                if (st->state == H2StreamState::closed) {
                    if (spl.size() > 1) {
                        if (!h2->make_stream(st, spl[0], "?" + spl[1])) {
                            return nullptr;
                        }
                    }
                    else {
                        if (!h2->make_stream(st, spl[0], std::string())) {
                            return nullptr;
                        }
                    }
                }
                HttpConn::Header tmph;
                tmph.emplace(":method", method);
                tmph.emplace(":authority", h2->host());
                tmph.emplace(":path", st->path());
                tmph.emplace(":scheme", h2->borrow()->get_ssl() ? "https" : "http");
                tmph.erase("host");
                tmph.erase(":body");
                tmph.merge(header);
                bool has_body = data && size;
                st->send_header(tmph, false, 0, !has_body);
                if (has_body) {
                    st->send_data(data, size, false, 0, true);
                }
                H2Stream *st0 = nullptr, *result = nullptr;
                h2->get_stream(0, st0);
                bool ok = false;
                while (h2->recvable() || Selecter::waitone(h2->borrow(), 10, 0, cancel)) {
                    std::shared_ptr<H2Frame> frame;
                    if (auto e = h2->recv(frame); !e) {
                        st0->send_goaway(e);
                        h2->close();
                        return nullptr;
                    }
                    H2Stream* st = nullptr;
                    if (auto e = h2->apply(frame, st); !e) {
                        st0->send_goaway(e);
                        h2->close();
                        return nullptr;
                    }
                    if (st->state == H2StreamState::closed) {
                        result = st;
                        ok = true;
                        break;
                    }
                }
                if (!ok) {
                    return nullptr;
                }
                st->header.emplace(":body", st->payload);
                return &st->header;
            }
            else if (version == 1) {
                if (!h1) return nullptr;
                if (h1->response().size()) {
                    h1->path_ = std::move(spl[0]);
                    if (spl.size() > 1) {
                        h1->query_ = "?" + std::move(spl[1]);
                    }
                    else {
                        h1->query_.clear();
                    }
                }
                if (!h1->send(method, header, data, size)) {
                    return nullptr;
                }
                if (!h1->recv(false, cancel)) {
                    return nullptr;
                }
                return &h1->response();
            }
            return nullptr;
        }

        void close() {
            conn->close();
            conn.reset();
            h1 = nullptr;
            h2 = nullptr;
            version = 0;
        }

        ~HttpClient() noexcept {
            close();
        }
    };
}  // namespace socklib