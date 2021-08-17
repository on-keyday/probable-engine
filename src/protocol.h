#pragma once

#include "protcolbase.h"
#include <net_helper.h>

#include <thread>

namespace socklib {
    struct HttpConn {
        friend struct Http;

       protected:
        std::shared_ptr<Conn> conn;
        using Header = std::multimap<std::string, std::string>;
        Header header;
        std::string host;
        std::string path;
        bool sent = false;
        bool recving = false;
        bool waiting = false;
        HttpConn(std::shared_ptr<Conn>&& in, std::string&& hostname, std::string&& path)
            : conn(std::move(in)), host(hostname), path(path) {}

       public:
        bool wait() const {
            return waiting;
        }

        void interrupt() {
            conn->set_suspend(true);
        }

        std::string url() const {
            return (conn->is_secure() ? "https://" : "http://") + host + path;
        }
    };

    struct HttpClientConn : HttpConn {
        std::string _method;

       public:
        HttpClientConn(std::shared_ptr<Conn>&& in, std::string&& hostname, std::string&& path)
            : HttpConn(std::move(in), std::move(hostname), std::move(path)) {}

        Header& response() {
            return header;
        }

        const std::string& method() {
            return _method;
        }

        bool send(const char* method, const Header& header = Header(), const char* body = nullptr, size_t bodylen = 0) {
            if (!method) return false;
            std::string res = method;
            res += " ";
            res += path;
            res += " HTTP/1.1\r\nHost: ";
            res += host;
            res += "\r\n";
            if (body && bodylen) {
                res += "content-length: ";
                res += std::to_string(bodylen);
                res += "\r\n";
            }
            for (auto& h : header) {
                std::string tmp;
                tmp.resize(h.first.size());
                std::transform(h.first.begin(), h.first.end(), tmp.begin(), [](auto c) { return std::tolower((unsigned char)c); });
                if (tmp.find("content-length") != ~0 || tmp.find("host") != ~0) {
                    continue;
                }
                if (h.first.find("\r") != ~0 || h.first.find("\n") != ~0 || h.second.find("\r") != ~0 || h.second.find("\n") != ~0) {
                    continue;
                }
                res += h.first;
                res += ": ";
                res += h.second;
                res += "\r\n";
            }
            res += "\r\n";
            if (body && bodylen) {
                res.append(body, bodylen);
            }
            sent = conn->write(res);
            if (sent) _method = method;
            return sent;
        }

        bool recv() {
            if (!sent || recving) return false;
            recving = true;
            commonlib2::Reader<socklib::SockReader> r(conn);
            struct {
                decltype(r)& r;
                decltype(header)& h;
                bool first = false;
                size_t prevpos = 0;
            } data{r, header};
            r.ref().setcallback(
                [](void* ctx, bool oneof) {
                    auto d = (decltype(&data))ctx;
                },
                &data);
            header.clear();
            if (!commonlib2::parse_httpresponse(r, header)) {
                return false;
            }
            recving = false;
            return header.size() != 0;
        }

        template <class F, class... Args>
        void recv(F&& f, Args&&... args) {
            std::shared_ptr<HttpClientConn> self(
                this, +[](HttpClientConn*) {});
            f(self, recv(), std::forward<Args>(args)...);
        }

        template <class F, class... Args>
        void recv_async(F&& f, Args&&... args) {
            waiting = true;
            std::thread([&, this]() {
                std::shared_ptr<HttpClientConn> self(
                    this, +[](HttpClientConn*) {});
                f(self, recv(), std::forward<Args>(args)...);
                waiting = false;
            }).detach();
        }

        void close() {
            conn->close();
        }

        ~HttpClientConn() {
            while (waiting)
                Sleep(5);
        }
    };

    struct Http {
       private:
        static bool
        setuphttp(const char* url, bool encoded, unsigned short& port, commonlib2::URLContext<std::string>& urlctx, std::string& path, std::string& query) {
            using R = commonlib2::Reader<std::string>;
            R(url).readwhile(commonlib2::parse_url, urlctx);
            if (!urlctx.succeed) return false;
            if (!urlctx.scheme.size()) {
                urlctx.scheme = "http";
            }
            else {
                if (urlctx.scheme != "http" && urlctx.scheme != "https") {
                    return false;
                }
            }
            if (!urlctx.path.size()) {
                urlctx.path = "/";
            }
            if (!encoded) {
                commonlib2::URLEncodingContext<std::string> encctx;
                encctx.path = true;
                R(urlctx.path).readwhile(path, commonlib2::url_encode, &encctx);
                if (encctx.failed) return false;
                encctx.query = true;
                encctx.path = false;
                R(urlctx.query).readwhile(query, commonlib2::url_encode, &encctx);
            }
            else {
                path = urlctx.path;
                query = urlctx.query;
            }
            if (urlctx.port.size()) {
                R(urlctx.port) >> port;
            }
            return true;
        }

       public:
        static std::shared_ptr<HttpClientConn>
        open(const char* url, bool encoded = false, const char* cacert = nullptr) {
            commonlib2::URLContext<std::string> urlctx;
            unsigned short port = 0;
            std::string path, query;
            if (!setuphttp(url, encoded, port, urlctx, path, query)) {
                return nullptr;
            }
            std::shared_ptr<Conn> conn;
            if (urlctx.scheme == "http") {
                conn = TCP::open(urlctx.host.c_str(), port, "http", true);
            }
            else {
                conn = TCP::open_secure(urlctx.host.c_str(), port, "https", true, cacert);
            }
            if (!conn) return nullptr;
            return std::make_shared<HttpClientConn>(std::move(conn), urlctx.host + (urlctx.port.size() ? ":" + urlctx.port : ""), path + query);
        }
    };
}  // namespace socklib