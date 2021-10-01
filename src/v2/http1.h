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
            response_recv,
            body_parsed,
            closed,
        };

        enum class HttpError {
            tcp_error,
            url_parse,
            url_encode,
            expected_scheme,
            alpn_failed,
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
            RequestPhase phase = RequestPhase::idle;
            string_t method;
            string_t url;
            parsed_t parsed;
            string_t default_path;
            string_t default_scheme;
            RequestFlag flag;
            string_t proxy;
            header_t request;
            body_t requestbody;
            header_t response;
            body_t responsebody;
            TCPError tcperr;
            HttpError err;
            int ip_version = 0;
            int http_version = 0;
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

            static bool open(std::shared_ptr<InetConn>& conn, request_t& req,
                             CancelContext* cancel = nullptr) {
                if (!urlparser_t::parse_request(req, "http", "https")) {
                    return false;
                }
                TCPOpenContext<String> tcpopen;
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
                std::shared_ptr<InetConn> tmpconn;
                if (!TCP<String>::open(tmpconn, tcpopen, cancel)) {
                    req.err = HttpError::tcp_error;
                    req.tcperr = tcpopen.err;
                    return false;
                }
                ConnStat stat;
                tmpconn->stat(stat);
                if (any(stat.status & ConnStatus::secure)) {
                    const unsigned char* data = nullptr;
                    const char** tmp = (const char**)&data;
                    unsigned int len = 0;
                    SSL_get0_alpn_selected((SSL*)stat.ssl, &data, &len);
                    if (!data) {
                        if (!any(req.falg & RequestFlag::ignore_alpn_failure)) {
                            req.err = HttpError::alpn_failed;
                            return false;
                        }
                        *tmp = "http/1.1";
                    }
                    bool result = false;
                    switch (req.http_version) {
                        case 1:
                            result = strncmp(tmp, "http/1.1", 8) == 0;
                            break;
                        case 2:
                            result = strncmp(tmp, "h2", 2) == 0;
                            break;
                        default:
                            result = strncmp(tmp, "h2", 2) == 0 || strncmp(tmp, "http/1.1", 8) == 0;
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
        struct HttpConn {
            using string_t = HttpBase<String, Header, Body>::string_t;
            using request_t = HttpBase<String, Header, Body>::request_t;
            using urlparser_t = HttpBase<String, Header, Body>::urlparser_t;
            std::shared_ptr<InetConn> conn;

            bool write_header(request_t& req, CancelContext* cancel) {
                WriteContext w;
                string_t towrite = req.method;
                towrite += ' ';
                towrite += req.parsed.path;
                towrite += req.parsed.query;
                towrite += ' ';
                towrite += "HTTP/1.1\r\n";
                towrite += "Host: ";
                towrite += urlparser_t::host_with_port(req.parsed);
                towrite += "\r\n";
                auto cmp = [](unsigned char c1, unsigned char c2) { return std::toupper(c1) == std::toupper(c2); };
                for (auto& h : req.request) {
                    if (commonlib2::str_eq(h.first, "host", cmp)) {
                    }
                }
                towrite += "\r\n";
                w.ptr = str.c_str();
                w.bufsize = str.size();
                conn->write(w, cancel);
            }

            bool request(request_t& req, CancelContext* cancel = nullptr) {
                if (!HttpBase::open(conn, req, cancel)) {
                    return false;
                }
                if (req.method.size() == 0) {
                    req.method = "GET";
                }
            }
        };

    }  // namespace v2
}  // namespace socklib