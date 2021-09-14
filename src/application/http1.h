#pragma once

#include "../transport/tcp.h"
#include "../transport/conn_struct_base.h"
#include <net_helper.h>

#include <thread>
#include <atomic>

namespace socklib {

    constexpr const char* reason_phrase(unsigned short status, bool dav = false) {
        switch (status) {
            case 100:
                return "Continue";
            case 101:
                return "Switching Protocols";
            case 103:
                return "Early Hints";
            case 200:
                return "OK";
            case 201:
                return "Created";
            case 202:
                return "Accepted";
            case 203:
                return "Non-Authoritative Information";
            case 204:
                return "No Content";
            case 205:
                return "Reset Content";
            case 206:
                return "Partial Content";
            case 300:
                return "Multiple Choices";
            case 301:
                return "Moved Permently";
            case 302:
                return "Found";
            case 303:
                return "See Other";
            case 304:
                return "Not Modified";
            case 307:
                return "Temporary Redirect";
            case 308:
                return "Permanent Redirect";
            case 400:
                return "Bad Request";
            case 401:
                return "Unauthorized";
            case 402:
                return "Payment Required";
            case 403:
                return "Forbidden";
            case 404:
                return "Not Found";
            case 405:
                return "Method Not Allowed";
            case 406:
                return "Not Acceptable";
            case 407:
                return "Proxy Authentication Required";
            case 408:
                return "Request Timeout";
            case 409:
                return "Conflict";
            case 410:
                return "Gone";
            case 411:
                return "Length Required";
            case 412:
                return "Precondition Failed";
            case 413:
                return "Payload Too Large";
            case 414:
                return "URI Too Long";
            case 415:
                return "Unsupported Media Type";
            case 416:
                return "Range Not Satisfiable";
            case 417:
                return "Expectation Failed";
            case 418:
                return "I'm a teapot";
            case 421:
                return "Misdirected Request";
            case 425:
                return "Too Early";
            case 426:
                return "Upgrade Required";
            case 429:
                return "Too Many Requests";
            case 431:
                return "Request Header Fields Too Large";
            case 451:
                return "Unavailable For Legal Reasons";
            case 500:
                return "Internal Server Error";
            case 501:
                return "Not Implemented";
            case 502:
                return "Bad Gateway";
            case 503:
                return "Service Unavailable";
            case 504:
                return "Gateway Timeout";
            case 505:
                return "HTTP Version Not Supported";
            case 506:
                return "Variant Also Negotiates";
            case 510:
                return "Not Extended";
            case 511:
                return "Network Authentication Required";
            default:
                break;
        }
        if (dav) {
            switch (status) {
                case 102:
                    return "Processing";
                case 207:
                    return "Multi-Status";
                case 208:
                    return "Already Reported";
                case 226:
                    return "IM Used";
                case 422:
                    return "Unprocessable Entity";
                case 423:
                    return "Locked";
                case 424:
                    return "Failed Dependency";
                case 507:
                    return "Insufficient Storage";
                case 508:
                    return "Loop Detected";
                default:
                    break;
            }
        }
        return "Unknown";
    }

    struct HttpConn : public AppLayer {
        friend struct Http1;
        using Header = std::multimap<std::string, std::string>;

       protected:
        Header header;
        std::string host;
        std::string path_;
        std::string query_;
        bool done = false;
        bool recving = false;
        std::uint32_t waiting = 0;
        std::string tmpbuffer;
        HttpConn(std::shared_ptr<Conn>&& in, std::string&& hostname, std::string&& path, std::string&& query)
            : AppLayer(std::move(in)), host(hostname), path_(path), query_(query) {}

       protected:
        bool send_detail(std::string& res, const Header& header, const char* body, size_t bodylen) {
            if (!conn) return false;
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
            return conn->write(res);
        }

       public:
        bool wait() const {
            return waiting;
        }

        const std::string& path() const {
            return path_;
        }

        const std::string& query() const {
            return query_;
        }

        std::string url() const {
            if (!host.size()) return path_ + query_;
            return (conn->get_ssl() ? "https://" : "http://") + host + path_ + query_;
        }
    };

    struct HttpClientConn : HttpConn {
        std::string _method;

       public:
        HttpClientConn(std::shared_ptr<Conn>&& in, std::string&& hostname, std::string&& path, std::string&& query)
            : HttpConn(std::move(in), std::move(hostname), std::move(path), std::move(query)) {}

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
            res += path_;
            res += query_;
            res += " HTTP/1.1\r\nhost: ";
            res += host;
            res += "\r\n";
            done = send_detail(res, header, body, bodylen);
            if (done) _method = method;
            return done;
        }

        bool recv(bool igbody = false, CancelContext* cancel = nullptr) {
            if (!done || recving) return false;
            recving = true;
            commonlib2::Reader<SockReader> r(SockReader(conn, cancel));
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
            if (!commonlib2::parse_httpresponse(r, header, igbody)) {
                return false;
            }
            r.readable();
            recving = false;
            return header.size() != 0;
        }

        template <class F, class... Args>
        void recv(F&& f, bool igbody, Args&&... args) {
            std::shared_ptr<HttpClientConn> self(
                this, +[](HttpClientConn*) {});
            f(self, recv(igbody), std::forward<Args>(args)...);
        }

        template <class F, class... Args>
        bool recv_async(F&& f, bool igbody, Args&&... args) {
            if (waiting) return false;
            waiting++;
            std::thread([&, this]() {
                std::shared_ptr<HttpClientConn> self(
                    this, +[](HttpClientConn*) {});
                f(self, recv(igbody), std::forward<Args>(args)...);
                waiting--;
            }).detach();
            return true;
        }

        ~HttpClientConn() {
            while (waiting)
                Sleep(5);
        }
    };

    struct HttpServerConn : HttpConn {
        HttpServerConn(std::shared_ptr<Conn>&& in)
            : HttpConn(std::move(in), std::string(), std::string(), std::string()) {}

        Header& request() {
            return header;
        }

        bool recv(CancelContext* cancel = nullptr) {
            if (recving) return false;
            recving = true;
            commonlib2::Reader<SockReader> r(SockReader(conn, cancel));
            header.clear();
            if (!commonlib2::parse_httprequest(r, header)) {
                recving = false;
                return false;
            }
            if (auto found = header.find(":path"); found != header.end()) {
                if (auto ch = commonlib2::split(found->second, "?", 1); ch.size() == 2) {
                    found->second = ch[0];
                    header.emplace(":query", "?" + ch[1]);
                    path_ = ch[0];
                    query_ = "?" + ch[1];
                }
                else {
                    path_ = ch[0];
                }
            }
            recving = false;
            done = true;
            return true;
        }

        bool send(unsigned short statuscode = 200, const char* phrase = "OK", const Header& header = Header(), const char* body = nullptr, size_t bodylen = 0) {
            if (!done) return false;
            if (statuscode < 100 && statuscode > 999) return false;
            if (!phrase || phrase[0] == 0 || std::string(phrase).find("\r") != ~0 || std::string(phrase).find("\n") != ~0) return false;
            std::string res = "HTTP/1.1 ";
            res += std::to_string(statuscode);
            res += " ";
            res += phrase;
            res += "\r\n";
            return send_detail(res, header, body, bodylen);
        }
    };

    struct Http1 {
        friend struct WebSocket;
        friend struct Http2;
        friend struct HttpClient;
        friend struct HttpServer;

       private:
        static bool
        setuphttp(const char* url, bool encoded, unsigned short& port, commonlib2::URLContext<std::string>& urlctx, std::string& path, std::string& query,
                  const char* normal = "http", const char* secure = "https", const char* defaultval = "http") {
            using R = commonlib2::Reader<std::string>;
            R(url).readwhile(commonlib2::parse_url, urlctx);
            if (!urlctx.succeed) return false;
            if (!urlctx.scheme.size()) {
                urlctx.scheme = defaultval;
            }
            else {
                if (urlctx.scheme != normal && urlctx.scheme != secure) {
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
            /*if (urlctx.scheme == "http") {
                conn = TCP::open(urlctx.host.c_str(), port, "http", true);
            }
            else {*/
            conn = TCP::open_secure(urlctx.host.c_str(), port, urlctx.scheme.c_str(), true,
                                    cacert, urlctx.scheme == "https", nullptr, 0, true);
            //}
            if (!conn) return nullptr;
            return std::make_shared<HttpClientConn>(std::move(conn), urlctx.host + (urlctx.port.size() ? ":" + urlctx.port : ""), std::move(path), std::move(query));
        }

        static bool reopen(std::shared_ptr<HttpClientConn>& conn, const char* url, bool encoded = false, const char* cacert = nullptr) {
            commonlib2::URLContext<std::string> urlctx;
            unsigned short port = 0;
            std::string path, query;
            if (!setuphttp(url, encoded, port, urlctx, path, query)) {
                return false;
            }
            if (conn) {
                auto res = TCP::reopen_secure(conn->conn, urlctx.host.c_str(), port, urlctx.scheme.c_str(), true,
                                              cacert, urlctx.scheme == "https", nullptr, 0, true);
                if (!res) return false;
                conn->host = urlctx.host + (urlctx.port.size() ? ":" + urlctx.port : "");
                conn->path_ = path;
                conn->query_ = query;
            }
            else {
                auto tmp = TCP::open_secure(urlctx.host.c_str(), port, urlctx.scheme.c_str(), true,
                                            cacert, urlctx.scheme == "https", nullptr, 0, true);
                if (!tmp) return false;
                conn = std::make_shared<HttpClientConn>(std::move(tmp), urlctx.host + (urlctx.port.size() ? ":" + urlctx.port : ""), std::move(path), std::move(query));
            }
            return true;
        }

        static std::shared_ptr<HttpServerConn> serve(Server& sv, unsigned short port = 80, size_t timeout = 10, IPMode mode = IPMode::both) {
            std::shared_ptr<Conn> conn = TCP::serve(sv, port, timeout, "http", true, mode);
            if (!conn) return nullptr;
            return std::make_shared<HttpServerConn>(std::move(conn));
        }
    };
}  // namespace socklib