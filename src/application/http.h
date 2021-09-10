#pragma once
#include "http1.h"
#include "http2.h"

namespace socklib {
    struct HttpClient {
       private:
        union {
            std::shared_ptr<HttpConn> h1;
            std::shared_ptr<Http2Context> h2;
        } conn;
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
                    return nullptr;
                }
            }
            auto hosts = urlctx.host + (urlctx.port.size() ? ":" + urlctx.port : "");
            if (!secure || strncmp("http/1.1", (const char*)data, 8) == 0) {
                conn.h1 = std::make_shared<HttpClientConn>(std::move(tcon), std::move(hosts), std::move(path), std::move(query));
                version = 1;
            }
            else if (strncmp("h2", (const char*)data, 2) == 0) {
                if (!tcon->write("PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n")) {
                    return false;
                }
                conn.h2 = std::make_shared<Http2Context>(std::move(tcon), std::move(hosts));
                conn.h2->streams[0] = H2Stream(0, conn.h2.get());
                conn.h2->server = false;
                version = 2;
                auto& h = conn.h2->streams[1];
                h = H2Stream(1, conn.h2.get());
                h.path = std::move(path);
                h.query = std::move(query);
                conn.h2->maxid = 1;
            }
            else {
                return false;
            }
            return true;
        }

        HttpConn::Header* GET(const char* path) {
            if (version == 0) return nullptr;
            std::string tmp;
            if (!path) {
                if (version == 2) {
                    H2Stream* st;
                    if (!conn.h2->get_stream(st)) {
                        return nullptr;
                    }
                    tmp = st->path + st->query;
                }
                else {
                    tmp = conn.h1->path() + conn.h1->query();
                }
                path = tmp.c_str();
            }
        }
    };
}  // namespace socklib