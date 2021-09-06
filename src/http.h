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
                return nullptr;
            }
            auto tcon = TCP::open_secure(urlctx.host.c_str(), port, urlctx.scheme.c_str(), true,
                                         cacert, urlctx.scheme == "https", "\x02h2\x08http/1.1", 3, true);
            if (!tcon) {
                return nullptr;
            }
            const unsigned char* data = nullptr;
            unsigned int len = 0;
            SSL_get0_alpn_selected((SSL*)tcon->get_ssl(), &data, &len);
            if (!data) {
                return nullptr;
            }
            if (strncmp("http/1.1", (const char*)data, 8) == 0) {
                conn.h1 = std::make_shared<HttpClientConn>(std::move(tcon), urlctx.host + (urlctx.port.size() ? ":" + urlctx.port : ""), std::move(path), std::move(query));
                version = 1;
            }
            else if (strncmp("h2", (const char*)data, 2) == 0) {
                if (!tcon->write("PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n")) {
                    return false;
                }
                conn.h2 = std::make_shared<HttpClientConn>(std::move(tcon));
                conn.h2->streams[0] = H2Stream(0, conn.h2.get());
                conn.h2->server = false;
                version = 2;
                conn.h2->streams[1] = H2Stream(1, conn.h2.get());
            }
        }
    };
}  // namespace socklib