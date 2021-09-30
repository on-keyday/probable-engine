#pragma once

#include "iconn.h"
#include "tcp.h"

#include <net_helper.h>

namespace socklib {
    namespace v2 {

        enum class RequestPhase {
            idle,
            opening,
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
        };

        template <class String, class Header, class Body>
        struct RequestContext {
            using string_t = String;
            using header_t = Header;
            using body_t = Body;
            using parsed_t = commonlib2::URLContext<String>;
            RequestPhase phase;
            string_t method;
            string_t url;
            parsed_t parsed;
            string_t default_path;
            string_t default_scheme;
            bool urlencoded = false;
            bool use_proxy = false;
            string_t proxy;
            header_t request;
            body_t requestbody;
            header_t response;
            body_t responsebody;
            TCPError tcperr;
            HttpError err;
        };

        template <class String, class Header, class Body>
        struct URLParser {
            using string_t = String;
            using request_t = RequestContext<String, Header, Body>;
            static bool parse(request_t& req, const string_t& expect1 = string_t(), const string_t& expect2 = string_t()) {
                commonlib2::Reader(req.url).readwhile(commonlib2::parse_url, req.parsed);
                if (!req.parsed.succeed) {
                    req.err = HttpError::url_parse;
                    return false;
                }
                if (expect1 || expect2) {
                    if (req.parsed.scheme.size() && req.parsed.scheme != expect1 && req.parsed.scheme != expect2) {
                        req.err = HttpError::expected_scheme;
                        return false;
                    }
                }
                if (req.parsed.scheme.size() == 0) {
                    req.parsed.scheme = req.default_scheme;
                }
                if (req.parsed.path.size() == 0) {
                    req.parsed.path = req.default_path;
                }
                if (!req.urlencoded) {
                    commonlib2::URLEncodingContext<std::string> encctx;
                    string_t path, query;
                    encctx.path = true;
                    commonlib2::Reader(url.path).readwhile(path, commonlib2::url_encode, &encctx);
                    if (encctx.failed) {
                        req.err = HttpError::url_encode;
                        return true;
                    }
                    encctx.query = true;
                    encctx.path = false;
                    commonlib2::Reader(url.query).readwhile(query, commonlib2::url_encode, &encctx);
                    if (encctx.failed) {
                        req.err = HttpError::url_encode;
                        return true;
                    }
                    req.parsed.path = std::move(path);
                    req.parsed.query = std::move(query);
                }
                return true;
            }
        };

        template <class String, class Header, class Body>
        struct HttpConn {
            using parser_t = URLParser<String, Header, Body>;
            using request_t = RequestContext<String, Header, Body>;
            std::shared_ptr<InetConn> conn;
            bool request(request_t& req) {
                if (!parser_t::parse(req)) {
                    return false;
                }
            }
        };

    }  // namespace v2
}  // namespace socklib