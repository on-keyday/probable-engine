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
            unsigned short port = 0;
            commonlib2::URLContext<std::string> urlctx;
            std::string path, query;
            if (!Http1::setuphttp(url, encoded, port, urlctx, path, query)) {
                return false;
            }
            bool secure = urlctx.scheme == "https";
            auto tcon = TCP::open_secure(urlctx.host.c_str(), port, urlctx.scheme.c_str(), true,
                                         cacert, secure, "\x02h2\x08http/1.1", 3, true);
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
            auto hosts = urlctx.host + (urlctx.port.size() ? ":" + urlctx.port : "");
            if (!secure || strncmp("http/1.1", (const char*)data, 8) == 0) {
                auto tmp = std::make_shared<HttpClientConn>(std::move(tcon), std::move(hosts), std::move(path), std::move(query));
                h1 = tmp.get();
                conn = tmp;
                version = 1;
            }
            else if (strncmp("h2", (const char*)data, 2) == 0) {
                if (!tcon->write("PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n")) {
                    return false;
                }
                auto tmp = std::make_shared<Http2Context>(std::move(tcon), std::move(hosts));
                auto& st0 = tmp->streams[0] = H2Stream(0, tmp.get());
                tmp->server = false;
                auto& h = tmp->streams[1];
                h = H2Stream(1, tmp.get());
                h.path_ = std::move(path);
                h.query_ = std::move(query);
                tmp->maxid = 1;
                h2 = tmp.get();
                conn = tmp;
                version = 2;
                st0.send_settings({});
            }
            else {
                return false;
            }
            return true;
        }

        HttpConn::Header* Method(const char* method, const char* path, HttpConn::Header&& header = HttpConn::Header(),
                                 const char* data = nullptr, size_t size = 0, CancelContext* cancel = nullptr) {
            if (version == 0 || !method) return nullptr;
            std::string tmp;
            if (version == 2) {
                if (!h2) return nullptr;
                H2Stream* st = nullptr;
                if (!h2->get_stream(st)) {
                    return nullptr;
                }
                if (st->state == H2StreamState::closed) {
                    auto spl = commonlib2::split(path, "?", 1);
                    if (spl.size() > 1) {
                        if (!h2->make_stream(st, spl[0], "?" + spl[1])) {
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

        ~HttpClient() noexcept {
            h1 = nullptr;
            h2 = nullptr;
            version = 0;
        }
    };
}  // namespace socklib