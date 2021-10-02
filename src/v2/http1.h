#pragma once

#include "iconn.h"
#include "tcp.h"

#include <net_helper.h>
#include <map>
#include <string.h>

namespace socklib {
    namespace v2 {
#ifdef COMMONLIB2_HAS_CONCEPTS
        template <class String>
        concept StringType = requires(String s) {
            { s.size() } -> commonlib2::Is_integral;
            {s.push_back('a')};
            {s.append(std::declval<const char*>(), std::declval<size_t>())};
            { s[0] } -> commonlib2::Is_integral;
            { s.find("string") } -> commonlib2::Is_integral;
            {s = std::declval<const String>()};
            {s = std::declval<const char*>()};
            {s += std::declval<const String>()};
            {s += std::declval<const char*>()};
            {s += 'a'};
            {s.erase(0, 1)};
            {s.clear()};
        };

        template <class Vector>
        concept VectorType = requires(Vector v) {
            {v.push_back('a')};
        };

        template <class Header, class String>
        concept HeaderType = requires(Header h) {
            {h.emplace(std::declval<String>(), std::declval<String>())};
        };
#endif
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
        };

        enum class RequestFlag : std::uint8_t {
            url_encoded = 0x1,
            use_proxy = 0x2,
            ignore_alpn_failure = 0x4,
            allow_http09 = 0x8,
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
        };

        BEGIN_ENUM_STRING_MSG(HttpDefaultScheme, default_scheme)
        ENUM_STRING_MSG(HttpDefaultScheme::https, "https")
        END_ENUM_STRING_MSG("http")

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
            string_t method;  //if not set, GET will set
            string_t url;
            DefaultPath default_path = DefaultPath::root;
            HttpDefaultScheme default_scheme = HttpDefaultScheme::http;
            RequestFlag flag;
            string_t proxy;
            header_t request;
            body_t requestbody;
            std::uint8_t ip_version = 0;
            std::uint8_t http_version = 0;
            string_t cacert;

            //response params
            RequestPhase phase = RequestPhase::idle;
            std::uint16_t statuscode;
            header_t response;
            body_t responsebody;

            //common params
            void (*error_cb)(std::uint64_t code, CancelContext* cancel, const char* msg) = nullptr;
            TCPError tcperr;
            HttpError err;
            std::uint8_t resolved_version = 0;
            std::uint8_t header_version = 0;
            parsed_t parsed;
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

        template <class String, class Header, class Body>
        struct HttpBase {
            using string_t = String;
            using urlparser_t = URLParser<String, Header, Body>;
            using request_t = RequestContext<String, Header, Body>;
            using parsed_t = typename request_t::parsed_t;

            static bool header_cmp(unsigned char c1, unsigned char c2) {
                return std::toupper(c1) == std::toupper(c2);
            };

            static bool open(std::shared_ptr<InetConn>& conn, request_t& req, CancelContext* cancel = nullptr, int prev_version = 0) {
                if (!urlparser_t::parse_request(req, "http", "https")) {
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
                if (req.parsed.scheme == "https") {
                    tcpopen.stat.status = ConnStatus::secure;
                }
                tcpopen.ip_version = req.ip_version;
                tcpopen.host = req.parsed.host;
                tcpopen.service = req.parsed.scheme;
                tcpopen.cacert = req.cacert;
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
                    return true;
                }
                ConnStat stat;
                tmpconn->stat(stat);
                if (any(stat.status & ConnStatus::secure)) {
                    const unsigned char* data = nullptr;
                    const char** tmp = (const char**)&data;
                    unsigned int len = 0;
                    SSL_get0_alpn_selected(stat.net.ssl, &data, &len);
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
                req.phase = RequestPhase::open_direct;
                conn = std::move(tmpconn);
                return true;
            }
        };

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
        };

        template <class String, class Header, class Body>
        struct HttpHeaderWriter {
            using base_t = HttpBase<String, Header, Body>;
            using urlparser_t = URLParser<String, Header, Body>;
            using string_t = typename base_t::string_t;
            using request_t = typename base_t::request_t;
            using errhandle_t = ErrorHandler<String, Header, Body>;

            static bool write_to_conn(std::shared_ptr<InetConn> conn, WriteContext& w, request_t& req, CancelContext* cancel) {
                auto error = [&](std::int64_t code, CancelContext* cancel, const char* msg) {
                    errhandle_t::on_error(req, code, cancel, msg);
                };
                w.errhandler = +[](void* e, std::int64_t code, CancelContext* cancel, const char* msg) {
                    auto f = (decltype(error)*)e;
                    (*f)(code, cancel, msg);
                };
                w.usercontext = &error;
                return conn->write(w, cancel);
            }

            static bool write_header_common(std::shared_ptr<InetConn>& conn, string_t& towrite, request_t& req, CancelContext* cancel) {
                for (auto& h : req.request) {
                    using commonlib2::str_eq;
                    if (str_eq(h.first, "host", base_t::header_cmp) || str_eq(h.first, "content-length", base_t::header_cmp)) {
                        continue;
                    }
                    if (h.first.find(":") != ~0 || h.first.find("\r") != ~0 || h.first.find("\n") != ~0 ||
                        h.second.find("\r") != ~0 || h.second.find("\n") != ~0) {
                        continue;
                    }
                    towrite += h.first;
                    towrite += ": ";
                    towrite += h.second;
                    towrite += "\r\n";
                }
                if (req.requestbody.size()) {
                    towrite += "Content-Length: ";
                    towrite += std::to_string(req.requestbody.size()).c_str();
                    towrite += "\r\n\r\n";
                    towrite.append(req.requestbody.data(), req.requestbody.size());
                }
                else {
                    towrite += "\r\n";
                }
                WriteContext w;
                w.ptr = towrite.c_str();
                w.bufsize = towrite.size();
                return write_to_conn(conn, w, req, cancel);
            }

            static bool write_request(std::shared_ptr<InetConn>& conn, request_t& req, CancelContext* cancel) {
                string_t towrite = req.method;
                towrite += ' ';
                if (req.default_path == DefaultPath::host_port) {
                    towrite += urlparser_t::host_with_port(req.parsed);
                }
                else if (req.default_path == DefaultPath::abs_url) {
                    towrite += urlparser_t::to_url(req.parsed);
                }
                else {
                    towrite += req.parsed.path;
                    towrite += req.parsed.query;
                }
                towrite += ' ';
                towrite += "HTTP/1.1\r\n";
                towrite += "Host: ";
                towrite += urlparser_t::host_with_port(req.parsed);
                towrite += "\r\n";
                return write_header_common(conn, towrite, req, cancel);
            }

            static bool write_response(std::shared_ptr<InetConn>& conn, request_t& req, CancelContext* cancel) {
                string_t towrite;
                if (req.header_version == 9) {
                    WriteContext w;
                    w.ptr = req.responsebody.data();
                    w.bufsize = req.responsebody.size();
                    if (write_to_conn(conn, w, req, cancel)) {
                        req.phase = RequestPhase::response_sent;
                        return true;
                    }
                    req.phase = RequestPhase::error;
                    return false;
                }
                else if (req.header_version == 10) {
                    towrite += "HTTP/1.0 ";
                }
                else {
                    towrite += "HTTP/1.1 ";
                }
                towrite += std::to_string(req.statuscode).c_str();
                towrite += ' ';
                towrite += reason_phrase(req.statuscode);
                towrite += "\r\n";
                if (write_header_common(conn, towrite, req, cancel)) {
                    req.phase = RequestPhase::response_sent;
                    return true;
                }
                req.phase = RequestPhase::error;
                return false;
            }
        };

        struct HttpBodyInfo {
            bool has_len = false;
            size_t size = 0;
            bool chunked = false;
        };

        template <class String, class Header, class Body>
        struct HttpHeaderReader {
            using base_t = HttpBase<String, Header, Body>;
            using request_t = typename base_t::request_t;
            using string_t = typename base_t::string_t;

            template <class Buf>
            static bool parse_header(request_t& req, commonlib2::Reader<Buf>& r, HttpBodyInfo& body) {
                using commonlib2::str_eq, commonlib2::getline, commonlib2::split;
                while (true) {
                    auto tmp = getline<string_t>(r, false);
                    if (tmp.size() == 0) break;
                    auto f = split(tmp, ":", 1);
                    if (f.size() < 2) return false;
                    while (f[1].size() && (f[1][0] == ' ' || f[1][0] == '\t')) f[1].erase(0, 1);
                    //std::transform(f[0].begin(), f[0].end(), f[0].begin(), [](char c) { return std::tolower((unsigned char)c); });
                    if (str_eq(f[0], "host", base_t::header_cmp)) {
                        auto h = split(f[1], ":", 1);
                        if (h.size() == 0) continue;
                        req.parsed.host = h[0];
                        if (h.size() == 2) {
                            req.parsed.port = h[1];
                        }
                    }
                    else if (!body.chunked && str_eq(f[0], "transfer-encoding", base_t::header_cmp) && f[1].find("chunked") != ~0) {
                        body.chunked = true;
                    }
                    else if (!body.has_len && str_eq(f[0], "content-length", base_t::header_cmp)) {
                        body.has_len = true;
                        commonlib2::Reader(f[1]) >> body.size;
                    }
                    req.request.emplace(f[0], f[1]);
                }
                return true;
            }

            template <class Buf>
            static bool parse_request(request_t& req, commonlib2::Reader<Buf>& r, HttpBodyInfo& body) {
                using commonlib2::str_eq, commonlib2::getline, commonlib2::split;
                auto status = split(getline(r, false), " ");
                if (status.size() < 2) return false;
                req.method = status[0];
                auto h = split(status[1], "?", 1);
                req.parsed.path = h[0];
                if (h.size() == 2) {
                    req.parsed.query = "?";
                    req.parsed.query += h[1];
                }
                if (status.size() == 3) {
                    if (status[2] == "HTTP/1.0") {
                        req.header_version = 10;
                    }
                    else if (status[2] == "HTTP/1.1") {
                        req.header_version = 11;
                    }
                }
                else {
                    if (!any(req.flag & RequestFlag::allow_http09)) {
                        req.err = HttpError::invalid_request_format;
                        return false;
                    }
                    req.header_version = 9;
                    return true;
                }
                return parse_header(req, r, body);
            }

            template <class Buf>
            static bool parse_response(request_t& req, commonlib2::Reader<Buf>& r, HttpBodyInfo& body) {
                using commonlib2::str_eq, commonlib2::getline, commonlib2::split;
                auto status = split(getline(r, false), " ", 2);
                if (status.size() < 2) return false;
                if (status[0] == "HTTP/1.1") {
                    req.header_version = 11;
                }
                else if (status[0] == "HTTP/1.0") {
                    req.header_version = 10;
                }
                commonlib2::Reader(status[1]) >> req.statuscode;
                return parse_header(req, r, body);
            }

            static bool read_body(request_t& req, HttpBodyInfo& bodyinfo, string_t& rawdata) {
                commonlib2::Reader<string_t&> r(rawdata);
                if (bodyinfo.chunked) {
                    r.expect("\r\n");
                    using commonlib2::getline;
                    bool noext = true;
                    string_t num = "0x";
                    getline(r, num, false, &noext);
                    if (noext) {
                        return true;
                    }
                    size_t chunksize;
                    commonlib2::Reader(num) >> chunksize;
                    if (chunksize == 0) {
                        req.phase = RequestPhase::body_recved;
                        return false;
                    }
                    if (r.readable() < chunksize) {
                        return true;
                    }
                    size_t nowsize = req.requestbody.size();
                    req.requestbody.resize(nowsize + chunksize);
                    if (r.read_byte(req.requestbody.data() + nowsize, chunksize, commonlib2::translate_byte_as_is, true) < chunksize) {
                        req.err = HttpError::read_body;
                        req.phase = RequestPhase::error;
                        return false;
                    }
                    r.expect("\r\n");
                    rawdata.erase(0, r.readpos());
                }
                else if (bodyinfo.has_len) {
                    if (rawdata.size() >= bodyinfo.size) {
                        if (r.read_byte(req.requestbody.data(), bodyinfo.size, commonlib2::translate_byte_as_is, true) < bodyinfo.size) {
                            req.err = HttpError::read_body;
                            req.phase = RequestPhase::error;
                        }
                        else {
                            req.phase = RequestPhase::body_recved;
                        }
                        rawdata.erase(0, bodyinfo.size);
                        return false;
                    }
                }
                else {
                    if (rawdata.size() == 0) {
                        req.phase = RequestPhase::body_recved;
                        return false;
                    }
                    for (size_t i = 0; i < rawdata.size(); i++) {
                        req.requestbody.push_back(rawdata[0]);
                    }
                    rawdata.clear();
                    return true;
                }
            }
        };

        template <class String, class Header, class Body>
        struct HttpReadContext : IReadContext {
            using base_t = HttpBase<String, Header, Body>;
            using request_t = typename base_t::request_t;
            using string_t = typename base_t::string_t;
            using httpparser_t = HttpHeaderReader<String, Header, Body>;
            using errhandle_t = ErrorHandler<String, Header, Body>;

            HttpReadContext(request_t& r)
                : req(r) {}

            bool nolen = false;
            request_t& req;
            bool eos = false;
            HttpBodyInfo bodyinfo;
            string_t rawdata;
            bool server = false;

            virtual bool require() override {
                return !eos;
            }

            virtual void on_error(std::int64_t errorcode, CancelContext* cancel, const char* msg) override {
                errhandle_t::on_error(req, errorcode, cancel, msg);
            }

            virtual void append(const char* read, size_t size) override {
                commonlib2::Reader<string_t&> r(rawdata);
                rawdata.append(read, size);
                if (req.phase == RequestPhase::request_recving) {
                    r.seekend();
                    auto parse_header = [&, this] {
                        r.seek(0);
                        if (server) {
                            if (!httpparser_t::parse_request(req, r, bodyinfo)) {
                                eos = true;
                                req.phase = RequestPhase::error;
                                return false;
                            }
                            if (req.header_version == 9) {
                                eos = true;
                                req.phase = RequestPhase::body_recved;
                            }
                            else {
                                req.phase = RequestPhase::request_recved;
                            }
                        }
                        else {
                            if (!httpparser_t::parse_response(req, r, bodyinfo)) {
                                eos = true;
                                req.phase = RequestPhase::error;
                                return false;
                            }
                            req.phase = RequestPhase::response_recved;
                        }
                        rawdata.erase(0, r.readpos());
                        nolen = !bodyinfo.chunked && !bodyinfo.has_len;
                        return true;
                    };
                    while (!r.ceof()) {
                        if (r.expect("\r\n\r\n") || r.expect("\n\n")) {
                            if (!parse_header()) {
                                return;
                            }
                            break;
                        }
                        r.increment();
                    }
                }
                if (req.phase == RequestPhase::request_recved) {
                    if (!httpparser_t::read_body(req, bodyinfo, rawdata)) {
                        eos = true;
                    }
                }
            }
        };

        template <class String, class Header, class Body>
        struct Http1Client {
            using base_t = HttpBase<String, Header, Body>;
            using string_t = typename base_t::string_t;
            using request_t = typename base_t::request_t;
            using urlparser_t = typename base_t::urlparser_t;
            using headerwriter_t = HttpHeaderWriter<String, Header, Body>;
            using readcontext_t = HttpReadContext<String, Header, Body>;

            static bool request(std::shared_ptr<InetConn>& conn, request_t& req, CancelContext* cancel = nullptr) {
                if (!base_t::open(conn, req, cancel, 1)) {
                    return false;
                }
                if (req.resolved_version != 1) {
                    req.err = HttpError::not_accept_version;
                    return false;
                }
                if (req.method.size() == 0) {
                    req.method = "GET";
                }
                return headerwriter_t::write_request(conn, req, cancel);
            }

            static bool response(std::shared_ptr<InetConn>& conn, readcontext_t& read, CancelContext* cancel = nullptr) {
                if (!conn) return false;
                if (req.phase == RequestPhase::request_sent) {
                    req.phase = RequestPhase::response_recving;
                }
                read.nolen = false;
                read.server = false;
                if (!conn->read(read, cancel)) {
                    return read.nolen
                }
                return true;
            }
        };

        template <class String, class Header, class Body>
        struct Http1Server {
            using base_t = HttpBase<String, Header, Body>;
            using request_t = typename base_t::request_t;
            using httpwriter_t = HttpHeaderWriter<String, Header, Body>;
            using readcontext_t = HttpReadContext<String, Header, Body>;

            static bool request(std::shared_ptr<InetConn>& conn, readcontext_t& read, CancelContext* cancel = nullptr) {
                if (!conn) return false;
                if (read.req.phase == RequestPhase::idle) {
                    read.req.phase = RequestPhase::request_recving;
                }
                read.nolen = false;
                read.server = true;
                if (!conn->read(read, cancel)) {
                    return read.nolen
                }
                return true;
            }

            static bool response(std::shared_ptr<InetConn>& conn, request_t& req, CancelContext* cancel = nullptr) {
                if (!conn) return false;
                if (req.phase != RequestPhase::response_body_recved) {
                    req.err = HttpError::invalid_phase;
                    return false;
                }
                if (httpwriter_t::write_response(req, cancel)) {
                    req.phase = RequestPhase::idle;
                }
                req.phase = RequestPhase::error;
                return false;
            }
        };

    }  // namespace v2
}  // namespace socklib