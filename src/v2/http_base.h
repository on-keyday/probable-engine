/*
    socklib - simple socket library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "iconn.h"
#include "tcp.h"

#include <net_helper.h>
#include <map>
#include <string.h>

#include "net_traits.h"

namespace socklib {
    namespace v2 {
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

        enum class RequestPhase : std::uint8_t {
            idle,
            open_direct,
            open_proxy,
            request_sending,
            request_sent,
            request_recving,
            request_recved,
            response_sent,
            response_recving,
            response_recved,
            body_recved,
            closed,
            error,
        };

        enum class HttpError : std::uint8_t {
            none,
            tcp_error,
            url_parse,
            url_encode,
            expected_scheme,
            alpn_failed,
            not_accept_version,
            invalid_request_format,
            read_body,
            transport_not_opened,
            invalid_phase,
            invalid_header,
            invalid_status,
        };

        enum class RequestFlag : std::uint8_t {
            none = 0,
            url_encoded = 0x1,
            use_proxy = 0x2,
            ignore_alpn_failure = 0x4,
            allow_http09 = 0x8,
            header_is_small = 0x10,
            invalid_header_is_error = 0x20,
            no_read_body = 0x40,
            no_need_len = 0x80,
        };

        DEFINE_ENUMOP(RequestFlag)

        enum class DefaultPath : std::uint8_t {
            root,
            index_html,
            index_htm,
            wildcard,
            robot_txt,
            abs_url,
            host_port,
        };

        BEGIN_ENUM_STRING_MSG(DefaultPath, default_path)
        ENUM_STRING_MSG(DefaultPath::index_html, "/index.html")
        ENUM_STRING_MSG(DefaultPath::index_htm, "/index.htm")
        ENUM_STRING_MSG(DefaultPath::wildcard, "*")
        ENUM_STRING_MSG(DefaultPath::robot_txt, "/robot.txt")
        END_ENUM_STRING_MSG("/")

        enum class HttpDefaultScheme : std::uint8_t {
            http,
            https,
            ws,
            wss,
        };

        BEGIN_ENUM_STRING_MSG(HttpDefaultScheme, default_scheme)
        ENUM_STRING_MSG(HttpDefaultScheme::https, "https")
        ENUM_STRING_MSG(HttpDefaultScheme::ws, "ws")
        ENUM_STRING_MSG(HttpDefaultScheme::wss, "wss")
        END_ENUM_STRING_MSG("http")

        BEGIN_ENUM_STRING_MSG(HttpDefaultScheme, default_port)
        ENUM_STRING_MSG(HttpDefaultScheme::https, "443")
        END_ENUM_STRING_MSG("80")

        template <class String, class Header, class Body>
#ifdef COMMONLIB2_HAS_CONCEPTS
        requires StringType<String> && HeaderType<Header, String> && VectorType<Body>
#endif
        struct RequestContext {
            using string_t = String;
            using header_t = Header;
            using body_t = Body;
            using parsed_t = commonlib2::URLContext<String>;
            //request params
            string_t method = string_t();  //if not set, GET will set
            string_t url = string_t();
            DefaultPath default_path = DefaultPath::root;
            HttpDefaultScheme default_scheme = HttpDefaultScheme::http;
            RequestFlag flag = RequestFlag::none;
            string_t proxy = string_t();
            header_t request = header_t();
            body_t requestbody = body_t();
            std::uint8_t ip_version = 0;
            std::uint8_t http_version = 0;
            string_t cacert = string_t();

            //response params
            RequestPhase phase = RequestPhase::idle;
            std::uint16_t statuscode = 0;
            header_t response = header_t();
            body_t responsebody = body_t();

            //common params
            void (*error_cb)(std::uint64_t code, CancelContext* cancel, const char* msg) = nullptr;
            TCPError tcperr = TCPError::none;
            HttpError err = HttpError::none;
            std::uint8_t resolved_version = 0;
            std::uint8_t header_version = 0;
            parsed_t parsed = parsed_t();

            //for http2 or later
            std::int32_t streamid = 0;
        };

        template <class String>
        struct HttpAcceptContext {
            int http_version = 0;
            HttpDefaultScheme scheme = HttpDefaultScheme::http;
            TCPAcceptContext<String> tcp;
        };

        template <class String, class Header, class Body>
        struct URLParser {
            using string_t = String;
            using request_t = RequestContext<String, Header, Body>;
            using parsed_t = typename request_t::parsed_t;

            static string_t host_with_port(parsed_t& parsed, const string_t& default_port = string_t()) {
                string_t res = parsed.host;
                if (parsed.port.size()) {
                    res += ':';
                    res += parsed.port;
                }
                else if (default_port.size()) {
                    res += ':';
                    res += default_port;
                }
                return res;
            }

            static string_t to_url(parsed_t& parsed) {
                string_t res = parsed.scheme;
                res += "://";
                res += parsed.host;
                if (parsed.port.size()) {
                    res += ':';
                    res += parsed.port;
                }
                res += parsed.path;
                res += parsed.query;
                return res;
            }

            static bool parse_request(request_t& req, const string_t& expect1 = string_t(), const string_t& expect2 = string_t()) {
                return parse(req.url, req.parsed, req.default_scheme, req.default_path,
                             &req.err, any(req.flag & RequestFlag::url_encoded), expect1, expect2);
            }

            static bool parse(string_t& url, parsed_t& parsed,
                              HttpDefaultScheme defscheme, DefaultPath defpath,
                              HttpError* err = nullptr, bool urlencoded = false,
                              const string_t& expect1 = string_t(), const string_t& expect2 = string_t()) {
                auto set_err = [&](HttpError e) {
                    if (err) {
                        *err = e;
                    }
                };
                parsed = parsed_t();
                commonlib2::Reader<string_t&>(url).readwhile(commonlib2::parse_url, parsed);
                if (!parsed.succeed) {
                    set_err(HttpError::url_parse);
                    return false;
                }
                if (expect1.size() || expect2.size()) {
                    if (parsed.scheme.size() && parsed.scheme != expect1 && parsed.scheme != expect2) {
                        set_err(HttpError::expected_scheme);
                        return false;
                    }
                }
                if (parsed.scheme.size() == 0) {
                    parsed.scheme = default_scheme(defscheme);
                }
                if (parsed.path.size() == 0) {
                    parsed.path = default_path(defpath);
                }
                if (!urlencoded) {
                    commonlib2::URLEncodingContext<std::string> encctx;
                    string_t path, query;
                    encctx.path = true;
                    commonlib2::Reader<string_t&>(parsed.path).readwhile(path, commonlib2::url_encode, &encctx);
                    if (encctx.failed) {
                        set_err(HttpError::url_encode);
                        return true;
                    }
                    encctx.query = true;
                    encctx.path = false;
                    commonlib2::Reader<string_t&>(parsed.query).readwhile(query, commonlib2::url_encode, &encctx);
                    if (encctx.failed) {
                        set_err(HttpError::url_encode);
                        return true;
                    }
                    parsed.path = std::move(path);
                    parsed.query = std::move(query);
                }
                return true;
            }
        };

        template <class String>
        struct HttpUtil {
            using string_t = String;
            static string_t translate_to_service(const string_t& scheme) {
                if (scheme == "ws") {
                    return "http";
                }
                else if (scheme == "wss") {
                    return "https";
                }
                return scheme;
            }

            static bool header_cmp(unsigned char c1, unsigned char c2) {
                return std::toupper(c1) == std::toupper(c2);
            };

            template <class KeyVal>
            static bool is_valid_field(KeyVal& h) {
                using commonlib2::str_eq;
                if (str_eq(h.first, "host", header_cmp) || str_eq(h.first, "content-length", header_cmp)) {
                    return false;
                }
                if (h.first.find(":") != ~0 || h.first.find("\r") != ~0 || h.first.find("\n") != ~0 ||
                    h.second.find("\r") != ~0 || h.second.find("\n") != ~0) {
                    return false;
                }
                return true;
            }
        };

        template <class String, class Header, class Body>
        struct HttpBase {
            using string_t = String;
            using urlparser_t = URLParser<String, Header, Body>;
            using request_t = RequestContext<String, Header, Body>;
            using parsed_t = typename request_t::parsed_t;
            using util_t = HttpUtil<String>;

            template <class KeyVal>
            static int is_valid_field(KeyVal& h, request_t& req) {
                if (!util_t::is_valid_field(h)) {
                    if (any(req.flag & RequestFlag::invalid_header_is_error)) {
                        req.err = HttpError::invalid_header;
                        return -1;
                    }
                    return 0;
                }
                return 1;
            }

            static bool open(std::shared_ptr<InetConn>& conn, request_t& req, CancelContext* cancel = nullptr,
                             const string_t& expect1 = "http", const string_t& expect2 = "https", int prev_version = 0) {
                if (!urlparser_t::parse_request(req, expect1, expect2)) {
                    return false;
                }
                TCPOpenContext<String> tcpopen;
                if (prev_version != 0 && req.http_version != prev_version) {
                    tcpopen.forceopen = true;
                }
                switch (req.http_version) {
                    case 1:
                        tcpopen.alpnstr = "\x08http/1.1";
                        tcpopen.len = 9;
                        break;
                    case 2:
                        tcpopen.alpnstr = "\x02h2";
                        tcpopen.len = 3;
                        break;
                    case 3:
                        //http3 unimplemented
                    default:
                        tcpopen.alpnstr = "\x02h2\x08http/1.1";
                        tcpopen.len = 12;
                        break;
                }
                tcpopen.stat.type = ConnType::tcp_over_ssl;
                if (req.parsed.scheme == "https" || req.parsed.scheme == "wss") {
                    tcpopen.stat.status = ConnStatus::secure;
                }
                tcpopen.ip_version = req.ip_version;
                tcpopen.host = req.parsed.host;
                tcpopen.service = util_t::translate_to_service(req.parsed.scheme);
                tcpopen.cacert = req.cacert;
                tcpopen.non_block = true;
                if (req.parsed.port.size()) {
                    commonlib2::Reader(req.parsed.port) >> tcpopen.port;
                }
                std::shared_ptr<InetConn> tmpconn = conn;
                if (!TCP<String>::open(tmpconn, tcpopen, cancel)) {
                    req.err = HttpError::tcp_error;
                    req.tcperr = tcpopen.err;
                    return false;
                }
                req.tcperr = tcpopen.err;
                if (req.tcperr == TCPError::not_reopened) {
                    req.phase = RequestPhase::open_direct;
                    return true;
                }
                ConnStat stat;
                tmpconn->stat(stat);
                if (any(stat.status & ConnStatus::secure)) {
                    const unsigned char* data = nullptr;
                    const char** tmp = (const char**)&data;
                    unsigned int len = 0;
                    ::SSL_get0_alpn_selected(stat.net.ssl, &data, &len);
                    if (!data) {
                        if (!any(req.flag & RequestFlag::ignore_alpn_failure)) {
                            req.err = HttpError::alpn_failed;
                            return false;
                        }
                        *tmp = "http/1.1";
                    }
                    bool result = false;
                    req.resolved_version = 0;
                    switch (req.http_version) {
                        case 1:
                            req.resolved_version = 1;
                            result = strncmp(*tmp, "http/1.1", 8) == 0;
                            break;
                        case 2:
                            req.resolved_version = 2;
                            result = strncmp(*tmp, "h2", 2) == 0;
                            break;
                        default:
                            if (strncmp(*tmp, "h2", 2) == 0) {
                                req.resolved_version = 2;
                                result = true;
                            }
                            else if (strncmp(*tmp, "http/1.1", 8) == 0) {
                                req.resolved_version = 1;
                                result = true;
                            }
                            break;
                    }
                    if (!result) {
                        req.err = HttpError::alpn_failed;
                        return false;
                    }
                }
                else {
                    req.resolved_version = 1;
                }
                req.phase = RequestPhase::open_direct;
                conn = std::move(tmpconn);
                return true;
            }

            static void write_path(string_t& towrite, request_t& req) {
                if (req.default_path == DefaultPath::host_port) {
                    towrite += urlparser_t::host_with_port(req.parsed, req.parsed.scheme == "https" ? default_port(HttpDefaultScheme::https) : default_port(HttpDefaultScheme::http));
                }
                else if (req.default_path == DefaultPath::abs_url) {
                    towrite += urlparser_t::to_url(req.parsed);
                }
                else {
                    towrite += req.parsed.path;
                    towrite += req.parsed.query;
                }
            }

            static std::shared_ptr<InetConn> accept(HttpAcceptContext<String>& ctx, CancelContext* cancel = nullptr) {
                ctx.tcp.service = util_t::translate_to_service(default_scheme(ctx.scheme));
                auto accepted = TCP<String>::accept(ctx.tcp, cancel);
                return accepted;
            }
        };

        template <class String, class Token>
        bool header_cmp(const String& a, const Token& b) {
            using util_t = HttpUtil<String>;
            return commonlib2::str_eq(a, b, util_t::header_cmp);
        }

        template <class String, class Header>
        auto get_header(const String& key, const Header& h) {
            using result_t = decltype(&h.begin()->second);
            if (auto it = std::find_if(h.begin(), h.end(), [&](auto& kv) { return header_cmp(kv.first, key); }); it != h.end()) {
                return &it->second;
            }
            return result_t(nullptr);
        }

        template <class String, class Header, class Vec>
        size_t get_headers(const String& key, const Header& h, Vec& values) {
            for (auto& kv : h) {
                if (header_cmp(kv.first, key)) {
                    values.push_back(kv.second);
                }
            }
            return values.size();
        }

        template <class Vec, class String, class Header>
        Vec get_headers(const String& key, const Header& h) {
            Vec v;
            get_headers(key, h, v);
            return v;
        }

        template <class String, class Header, class Body>
        struct ErrorHandler {
            using base_t = HttpBase<String, Header, Body>;
            using string_t = typename base_t::string_t;
            using request_t = typename base_t::request_t;
            static void on_error(request_t& req, std::int64_t code, CancelContext* cancel, const char* msg) {
                std::string_view m(msg);
                if (m.find("ssl") != ~0) {
                    req.tcperr = TCPError::ssl_io;
                }
                else {
                    req.tcperr = TCPError::tcp_io;
                }
                req.err = HttpError::tcp_error;
                if (req.error_cb) {
                    req.error_cb(code, cancel, msg);
                }
            }

            static bool write_to_conn(std::shared_ptr<InetConn>& conn, WriteContext& w, request_t& req, CancelContext* cancel) {
                auto error = [&](std::int64_t code, CancelContext* cancel, const char* msg) {
                    on_error(req, code, cancel, msg);
                };
                w.errhandler = +[](void* e, std::int64_t code, CancelContext* cancel, const char* msg) {
                    auto f = (decltype(error)*)e;
                    (*f)(code, cancel, msg);
                };
                w.usercontext = &error;
                return conn->write(w, cancel);
            }
        };
    }  // namespace v2
}  // namespace socklib
