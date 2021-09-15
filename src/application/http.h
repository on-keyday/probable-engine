#pragma once
#include "http1.h"
#include "http2.h"

namespace socklib {
    struct HttpCookie {
        std::string key;
        std::string value;

        std::time_t max_age;
        std::time_t expires;
        std::string domain;
        std::string path;
        bool secure = false;
        bool httponly = false;
    };

    struct HttpClient {
       private:
        std::shared_ptr<AppLayer> conn;
        Http2Context* h2 = nullptr;
        HttpClientConn* h1 = nullptr;
        int version = 0;

       public:
        OpenErr open(const char* url, bool encoded = false, const char* cacert = nullptr) {
            close();
            HttpRequestContext ctx;
            if (!Http1::setuphttp(url, encoded, ctx)) {
                return false;
            }
            bool secure = ctx.url.scheme == "https";
            OpenErr e;
            auto tcon = Http1::open_tcp_conn(ctx, cacert, &e, "\x02h2\x08http/1.1", 12);
            if (!tcon) {
                return e;
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
                auto tmp = Http2::init_object(tcon, ctx, std::move(ctx.path), std::move(ctx.query));
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

        std::string host() {
            if (version == 1) {
                return h1->host;
            }
            else if (version == 2) {
                return h2->host();
            }
            return std::string();
        }

        std::string url() {
            if (version == 1 && h1) {
                return h1->url();
            }
            else if (version == 2 && h2) {
                return h2->url();
            }
            return std::string();
        }

        int http_version() {
            return version;
        }

        OpenErr reopen(const char* url, bool encoded = false, const char* cacert = nullptr) {
            if (!conn || !url) return OpenError::invalid_condition;
            std::string urlstr;
            Http1::fill_urlprefix(host(), url, urlstr, conn->borrow()->get_ssl() ? "https" : "http");
            urlstr += url;
            HttpRequestContext ctx;
            if (!Http1::setuphttp(url, encoded, ctx)) {
                return OpenError::parse_url;
            }
            auto& borrow = conn->borrow();
            auto e = Http1::reopen_tcp_conn(borrow, ctx, cacert, "\x02h2\x08http/1.1", 12);
            if (!e) {
                if (e == OpenError::needless_to_reopen) {
                    if (h1) {
                        h1->path_ = std::move(ctx.path);
                        h1->query_ = std::move(ctx.query);
                        h1->response().clear();
                    }
                    else if (h2) {
                        H2Stream* str = nullptr;
                        if (!h2->make_stream(str, ctx.path, ctx.query)) {
                            return false;
                        }
                    }
                }
                return e;
            }
            const unsigned char* data = nullptr;
            unsigned int len = 0;
            if (borrow->get_ssl()) {
                SSL_get0_alpn_selected((SSL*)borrow->get_ssl(), &data, &len);
                if (!data) {
                    return false;
                }
            }
            if (!borrow->get_ssl() || strncmp("http/1.1", (const char*)data, 8) == 0) {
                if (h1) {
                    h1->host = ctx.host_with_port();
                    h1->path_ = std::move(ctx.path);
                    h1->query_ = std::move(ctx.query);
                    h1->response().clear();
                }
                else {
                    auto hijack = conn->hijack();
                    close();
                    auto tmp = std::make_shared<HttpClientConn>(std::move(hijack), ctx.host_with_port(), std::move(ctx.path), std::move(ctx.query));
                    h1 = tmp.get();
                    conn = tmp;
                }
                version = 1;
            }
            else if (strncmp("h2", (const char*)data, 2) == 0) {
                if (!h2) {
                    auto hijack = conn->hijack();
                    close();
                    auto tmp = Http2::init_object(hijack, ctx, std::move(ctx.path), std::move(ctx.query));
                    h2 = tmp.get();
                    conn = tmp;
                }
                else {
                    h2->clear();
                    h2->host_ = ctx.host_with_port();
                    std::shared_ptr<Http2Context> tmp(h2, [](Http2Context*) {});
                    Http2::init_streams(tmp, std::move(ctx.path), std::move(ctx.query));
                }
                if (!conn->borrow()->write(h2_connection_preface)) {
                    return false;
                }
                version = 2;
                h2->streams[0].send_settings({});
            }
            else {
                return false;
            }
            return true;
        }

       private:
        HttpConn::Header* Http2method(const char* method, std::vector<std::string>& spl, HttpConn::Header&& header = HttpConn::Header(),
                                      const char* data = nullptr, size_t size = 0, CancelContext* cancel = nullptr) {
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
            size_t suspend = 0;
            bool has_body = data && size;
            tmph.emplace(":method", method);
            tmph.emplace(":authority", h2->host());
            tmph.emplace(":path", st->path());
            tmph.emplace(":scheme", h2->borrow()->get_ssl() ? "https" : "http");
            if (has_body) {
                tmph.emplace("content-length", std::to_string(size));
            }
            header.erase("host");
            header.erase(":body");
            tmph.merge(header);
            if (auto e = st->send_header(tmph, false, 0, !has_body); !e) {
                return nullptr;
            }
            auto sending_body = [&]() -> H2Err {
                if (auto e = st->send_data(data, size, &suspend, false, 0, true); !e) {
                    if (e != H2Error::need_window_update) {
                        return e;
                    }
                }
                return true;
            };
            H2Stream *st0 = nullptr, *result = nullptr;
            h2->get_stream(0, st0);
            if (has_body) {
                if (auto e = sending_body(); !e) {
                    st0->send_goaway(e);
                    h2->close();
                    return nullptr;
                }
            }
            bool ok = false;
            while (h2->recvable() || Selecter::waitone(h2->borrow(), 60, 0, cancel)) {
                std::shared_ptr<H2Frame> frame;
                if (auto e = h2->recv(frame); !e) {
                    st0->send_goaway(e);
                    h2->close();
                    return nullptr;
                }
                st = nullptr;
                if (auto e = h2->apply(frame, st); !e) {
                    st0->send_goaway(e);
                    h2->close();
                    return nullptr;
                }
                if (auto d = frame->data()) {  //to update flow control window
                    st0->send_windowupdate((int)d->payload().size());
                    st->send_windowupdate((int)d->payload().size());
                }
                else if (auto w = frame->window_update(); w && st->streamid != 0) {
                    if (size && size != suspend) {
                        if (auto e = sending_body(); !e) {
                            st0->send_goaway(e);
                            h2->close();
                            return nullptr;
                        }
                    }
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

       public:
        HttpConn::Header* method(const char* method, const char* path = nullptr, HttpConn::Header&& header = HttpConn::Header(),
                                 const char* data = nullptr, size_t size = 0, CancelContext* cancel = nullptr) {
            if (version == 0 || !method) return nullptr;
            auto spl = commonlib2::split(path ? path : "/", "?", 1);
            if (spl.size() < 1) return nullptr;
            if (version == 2) {
                return Http2method(method, spl, std::forward<HttpConn::Header>(header), data, size, cancel);
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
            if (conn) {
                conn->close();
                conn.reset();
                h1 = nullptr;
                h2 = nullptr;
                version = 0;
            }
        }

        bool is_open() const {
            return (bool)conn;
        }

        ~HttpClient() noexcept {
            close();
        }
    };

}  // namespace socklib
