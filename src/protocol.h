#pragma once

#include "protcolbase.h"
#include <net_helper.h>

#include <thread>

namespace socklib {
    struct HttpConn {
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
    };

    struct HttpClientConn : HttpConn {
       public:
        HttpClientConn(std::shared_ptr<Conn>&& in, std::string&& hostname, std::string&& path)
            : HttpConn(std::move(in), std::move(hostname), std::move(path)) {}

        Header& response() {
            return header;
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
            return sent;
        }

        bool recv() {
            if (!sent || recving) return false;
            recving = true;
            commonlib2::Reader<socklib::SockReader> r(conn);
            header = commonlib2::parse_httpresponse(r);
            recving = false;
            return header.size() != 0;
        }

        template <class F>
        void recv_async(F&& f) {
            waiting = true;
            std::thread([&, this]() {
                f(*this, recv());
                waiting = false;
            }).detach();
        }

        ~HttpClientConn() {
            while (waiting)
                Sleep(5);
        }
    };

    struct Http {
        static std::shared_ptr<HttpClientConn> open(const char* url, bool encoded = false, const char* cacert = nullptr) {
            using R = commonlib2::Reader<std::string>;
            commonlib2::URLContext<std::string> urlctx;
            R(url).readwhile(commonlib2::parse_url, urlctx);
            if (!urlctx.succeed) return nullptr;
            if (!urlctx.scheme.size()) {
                urlctx.scheme = "http";
            }
            else {
                if (urlctx.scheme != "http" && urlctx.scheme != "https") {
                    return nullptr;
                }
            }
            if (!urlctx.path.size()) {
                urlctx.path = "/";
            }
            std::string finally;
            std::string path, query;
            if (!encoded) {
                commonlib2::URLEncodingContext<std::string> encctx;
                encctx.path = true;
                R(urlctx.path).readwhile(path, commonlib2::url_encode, &encctx);
                if (encctx.failed) return nullptr;
                encctx.query = true;
                encctx.path = false;
                R(urlctx.query).readwhile(query, commonlib2::url_encode, &encctx);
            }
            else {
                path = urlctx.path;
                query = urlctx.query;
            }
            unsigned short port = 0;
            if (urlctx.port.size()) {
                R(urlctx.port) >> port;
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