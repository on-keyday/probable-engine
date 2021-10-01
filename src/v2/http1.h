#pragma once

#include "iconn.h"
#include "tcp.h"

#include <net_helper.h>

namespace socklib {
    namespace v2 {

        enum class RequestPhase {
            idle,
            open_direct,
            open_proxy,
            request_sent,
            request_recving,
            request_recved,
            request_body_parsed,
            response_sent,
            response_recving,
            response_recved,
            response_body_parsed,
            closed,
            error,
        };

        enum class HttpError {
            tcp_error,
            url_parse,
            url_encode,
            expected_scheme,
            alpn_failed,
            not_accept_version,
        };

        enum class RequestFlag {
            url_encoded,
            use_proxy,
            ignore_alpn_failure,
        };

        DEFINE_ENUMOP(RequestFlag)

        template <class String, class Header, class Body>
        struct RequestContext {
            using string_t = String;
            using header_t = Header;
            using body_t = Body;
            using parsed_t = commonlib2::URLContext<String>;
            //input params
            string_t method;  //if not set, GET will set
            string_t url;
            string_t default_path;
            string_t default_scheme;
            RequestFlag flag;
            string_t proxy;
            header_t request;
            body_t requestbody;
            int ip_version = 0;
            int http_version = 0;
            void (*error_cb)(std::uint64_t code, CancelContext* cancel, const char* msg) = nullptr;

            //mutable params (overwritten)
            RequestPhase phase = RequestPhase::idle;
            std::uint16_t statuscode;
            header_t response;
            body_t responsebody;
            parsed_t parsed;
            int resolved_version = 0;
            int header_version = 0;
            TCPError tcperr;
            HttpError err;

            //temporary data
            string_t rawdata;
        };

        template <class String, class Header, class Body>
        struct URLParser {
            using string_t = String;
            using request_t = RequestContext<String, Header, Body>;
            using parsed_t = request_t::parsed_t;

            static string_t host_with_port(parsed_t& parsed) {
                string_t res = parsed.host;
                if (parsed.port.size()) {
                    res += ':';
                    res += parsed.port;
                }
                return res;
            }

            static bool parse_request(request_t& req, const string_t& expect1 = string_t(), const string_t& expect2 = string_t()) {
                return parse(req.url, req.parsed, req.default_scheme, req.default_path,
                             &req.err, any(req.flag & RequestFlag::url_encoded), expect1, expect2);
            }

            static bool parse(string_t& url, parsed_t& parsed,
                              const string_t& default_scheme, const string_t& default_path,
                              HttpError* err = nullptr, bool urlencoded = false,
                              const string_t& expect1 = string_t(), const string_t& expect2 = string_t()) {
                auto set_err = [&](HttpError e) {
                    if (err) {
                        *err = e;
                    }
                };
                parsed = parsed_t();
                commonlib2::Reader(url).readwhile(commonlib2::parse_url, parsed);
                if (!parsed.succeed) {
                    set_err(HttpError::url_parse);
                    return false;
                }
                if (expect1 || expect2) {
                    if (parsed.scheme.size() && parsed.scheme != expect1 && parsed.scheme != expect2) {
                        set_err(HttpError::expected_scheme);
                        return false;
                    }
                }
                if (parsed.scheme.size() == 0) {
                    parsed.scheme = default_scheme;
                }
                if (parsed.path.size() == 0) {
                    parsed.path = default_path;
                }
                if (!urlencoded) {
                    commonlib2::URLEncodingContext<std::string> encctx;
                    string_t path, query;
                    encctx.path = true;
                    commonlib2::Reader(parsed.path).readwhile(path, commonlib2::url_encode, &encctx);
                    if (encctx.failed) {
                        set_err(HttpError::url_encode);
                        return true;
                    }
                    encctx.query = true;
                    encctx.path = false;
                    commonlib2::Reader(parsed.query).readwhile(query, commonlib2::url_encode, &encctx);
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
            using parsed_t = request_t::parsed_t;

            static bool header_cmp(unsigned char c1, unsigned char c2) {
                return std::toupper(c1) == std::toupper(c2);
            };

            static bool open(std::shared_ptr<InetConn>& conn, request_t& req,
                             CancelContext* cancel = nullptr, int prev_version = 0) {
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
                    tcpope.stat.status = ConnStatus::secure;
                }
                tcpopen.ip_version = req.ip_version;
                tcpopen.host = req.parsed.host;
                tcpopen.service = req.parsed.scheme;
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
                        if (!any(req.falg & RequestFlag::ignore_alpn_failure)) {
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
                            result = strncmp(tmp, "http/1.1", 8) == 0;
                            break;
                        case 2:
                            req.resolved_version = 2;
                            result = strncmp(tmp, "h2", 2) == 0;
                            break;
                        default:
                            if (strncmp(tmp, "h2", 2) == 0) {
                                req.resolved_version = 2;
                                result = true;
                            }
                            else if (strncmp(tmp, "http/1.1", 8) == 0) {
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
        struct HttpHeaderWriter {
            using base_t = HttpBase<String, Header, Body>;
            using string_t = base_t::string_t;
            using request_t = base_t::request_t;

            bool write_header_common(string_t& towrite, std::shared_ptr<InetConn> conn, request_t& req, CancelContext* cancel) {
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
                w.ptr = str.c_str();
                w.bufsize = str.size();
                auto error = [&](std::uint64_t code, CancelContext* cancel, const char* msg) {
                    std::string_view m(msg);
                    if (m.find("ssl") != ~0) {
                        req.tcperr = TCPError::ssl_write;
                    }
                    else {
                        req.tcperr = TCPError::tcp_write;
                    }
                    req.err = HttpError::tcp_error;
                    if (req.error_cb) {
                        req.error_cb(code, cancel, msg);
                    }
                };
                w.errhandler = [](void* e, std::uint64_t code, CancelContext* cancel, const char* msg) {
                    auto f = (decltype(error)*)e;
                    (*f)(code, cancel, msg);
                };
                w.usercontext = &error;
                if (conn->write(w, cancel)) {
                    req.phase = RequestPhase::request_sent;
                    return true;
                }
                return false;
            }

            bool write_request(std::shared_ptr<InetConn> conn, request_t& req, CancelContext* cancel) {
                string_t towrite = req.method;
                towrite += ' ';
                towrite += req.parsed.path;
                towrite += req.parsed.query;
                towrite += ' ';
                towrite += "HTTP/1.1\r\n";
                towrite += "Host: ";
                towrite += urlparser_t::host_with_port(req.parsed);
                towrite += "\r\n";
                return write_header_common(towrite, conn, req, cancel);
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
            using request_t = base_t::request_t;
            using string_t = base_t::string_t;

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
                    req.header_version = 9;
                }
                return parse_header(r, body);
            }

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
        };

        template <class String, class Header, class Body>
        struct Http1Request {
            using base_t = HttpBase<String, Header, Body>;
            using string_t = base_t::string_t;
            using request_t = base_t::request_t;
            using urlparser_t = base_t::urlparser_t;
            using headerwriter_t = HttpHeaderWriter<String, Header, Body>;
            std::shared_ptr<InetConn> conn;

            bool request(request_t& req, CancelContext* cancel = nullptr) {
                if (!HttpBase::open(conn, req, cancel, 1)) {
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
        };

        template <class String, class Header, class Body>
        struct HttpReadContext : IReadContext {
            using base_t = HttpBase<String, Header, Body>;
            using request_t = base_t::request_t;
            using string_t = base_t::string_t;
            using httpparser_t = HttpHeaderReader<String, Header, Body>;
            request_t& req;
            bool eos = false;

            HttpReadContext(request_t& r)
                : req(r) {}

            virtual bool require() {
                return !eos;
            }

            virtual void append(const char* read, size_t size) override {
                if (req.phase == RequestPhase::request_recving) {
                    auto parse_header = [this, &] {
                        commonlib2::Reader<string_t&> r(req.rawdata);
                        if () {
                            eos = true;
                            req.phase = RequestPhase::error;
                            return false;
                        }
                    };
                    rawdata.append(read, size);
                    auto pos = rawdata.find("\r\n\r\n");
                    if (pos != ~0) {
                    }
                    pos = rawdata.find("\n\n");
                    if (pos != ~0) {
                    }
                }
                if (req.phase = RequestPhase::request_recved) {
                }
            }
        };

        template <class String, class Header, class Body>
        struct Http1Response {
            using base_t = HttpBase<String, Header, Body>;
            std::shared_ptr<InetConn> conn;

            bool request(request_t& req, CancelContext* cancel = nullptr) {
                if (req.phase == RequestPhase::idle) {
                    req.phase = RequestPhase::request_recving;
                }
                HttpReadContext read(req);
                return conn->read(read, cancel);
            }
        };

    }  // namespace v2
}  // namespace socklib