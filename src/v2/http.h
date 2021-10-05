/*
    socklib - simple socket library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once

#include "http1.h"
#include "http2.h"

#include <deque>

namespace socklib {
    namespace v2 {

        template <class String, class Header, class Body, template <class...> class Map, class Table>
        struct RequestProxy : INetAppConn {
           protected:
            RequestContext<String, Header, Body> ctx;
            Http2RequestContext<String, Map, Header, Body, Table> h2ctx;
            std::shared_ptr<InetConn> conn;

           public:
            virtual std::shared_ptr<InetConn>& borrow() override {
                return conn;
            }

            virtual std::shared_ptr<InetConn> hijack() override {
                return std::move(conn);
            }

            virtual bool enable_conn() const override {
                if (!conn) {
                    return false;
                }
                ConnStat stat;
                conn->stat(stat);
                if (!any(stat.status & ConnStatus::has_fd)) {
                    return false;
                }
                return true;
            }

            virtual void close(CancelContext* cancel) override {
                if (conn) {
                    conn->close(cancel);
                }
            }

            TCPError tcp_error() const {
                return ctx.tcperr;
            }

            HttpError http_error() const {
                return ctx.err;
            }

            RequestPhase state() const {
                return ctx.phase;
            }

            void set_httpversion(int version) {
                ctx.http_version = version;
            }

            void set_ipversion(int version) {
                ctx.ip_version = version;
            }
        };

        template <class String, class Header, class Body, template <class...> class Map, class Table>
        struct ClientRequestProxy : RequestProxy<String, Header, Body, Map, Table> {
            using base_t = RequestProxy<String, Header, Body, Map, Table>;
            using http1client_t = Http1Client<String, Header, Body>;
            using http2client_t = Http2Client<String, Map, Header, Body, Table>;
            using opener_t = HttpBase<String, Header, Body>;
            using h2readctx_t = Http2ReadContext<String, Map, Header, Body, Table>;
            using h1readctx_t = Http1ReadContext<String, Header, Body>;

           private:
            std::shared_ptr<IReadContext> readbuf = nullptr;
            h1readctx_t* h1buf = nullptr;
            h2readctx_t* h2buf = nullptr;

           public:
            ClientRequestProxy() {
            }

            Header& requestHeader() {
                return this->ctx.request;
            }

            Body& requestBody() {
                return this->ctx.requestbody;
            }

            const Header& responseHeader() {
                return this->ctx.response;
            }

            const Body& responseBody() {
                return this->ctx.responsebody;
            }

            void set_cacert(const String& cacert) {
                this->ctx.cacert = cacert;
            }

            bool set_default_path(DefaultPath defpath) {
                if (defpath == DefaultPath::abs_url || defpath == DefaultPath::host_port) {
                    return false;
                }
                this->ctx.default_path = defpath;
                return true;
            }

            void set_default_schme(HttpDefaultScheme scehme) {
                this->ctx.default_scehme = scehme;
            }

           private:
            bool h2request(CancelContext* cancel = nullptr) {
                if (!h2buf) {
                    h1buf = nullptr;
                    auto tmp = std::make_shared<h2readctx_t>(this->ctx, this->h2ctx);
                    h2buf = tmp.get();
                    readbuf = tmp;
                }
                if (this->ctx.tcperr != TCPError::not_reopened) {
                    http2client_t::init(this->h2ctx);
                    if (!http2client_t::send_connection_preface(this->conn, this->ctx, cancel)) {
                        return false;
                    }
                    if (!http2client_t::send_first_settings(this->conn, this->ctx, this->h2ctx, cancel)) {
                        return false;
                    }
                }
                return http2client_t::request(this->conn, *h2buf, cancel);
            }

           public:
            bool request(const String& method, const String& url, CancelContext* cancel = nullptr) {
                this->ctx.url = url;
                this->ctx.method = method;
                if (!opener_t::open(this->conn, this->ctx, cancel)) {
                    return false;
                }
                if (this->ctx.resolved_version == 2) {
                    return h2request(cancel);
                }
                else {
                    if (this->ctx.http_version == 2) {
                        //unimplemented h2c
                    }
                    else {
                        if (!h1buf) {
                            h2buf = nullptr;
                            auto tmp = std::make_shared<h1readctx_t>(this->ctx);
                            h1buf = tmp.get();
                            readbuf = tmp;
                        }
                        return http1client_t::request(this->conn, this->ctx, cancel);
                    }
                }
                return false;
            }

            bool response(CancelContext* cancel) {
                this->ctx.response.clear();
                this->ctx.responsebody.clear();
                if (this->ctx.resolved_version == 2) {
                    return http2client_t::response(this->conn, *h2buf, cancel);
                }
                else {
                    return http1client_t::response(this->conn, *h1buf, cancel);
                }
            }
        };

        template <class String, class Header, class Body, template <class...> class Map, class Table>
        struct ServerRequestProxy : RequestProxy<String, Header, Body, Map, Table> {
            const Header& requestHeader() {
                return this->ctx.request;
            }

            const Body& requestBody() {
                return this->ctx.requestbody;
            }

            Header& responseHeader() {
                return this->ctx.response;
            }

            Body& responseBody() {
                return ctx.responsebody;
            }
        };

        template <class String = std::string, class Header = std::multimap<String, String>, class Body = String,
                  template <class...> class Map = std::map, class Table = std::deque<std::pair<String, String>>>
        std::shared_ptr<ClientRequestProxy<String, Header, Body, Map, Table>> make_request() {
            return std::make_shared<ClientRequestProxy<String, Header, Body, Map, Table>>();
        }

        template <class String = std::string, class Header = std::multimap<String, String>, class Body = String,
                  template <class...> class Map = std::map, class Table = std::deque<std::pair<String, String>>>
        std::shared_ptr<ServerRequestProxy<String, Header, Body, Map, Table>> accept_request() {
            return std::make_shared<ServerRequestProxy<String, Header, Body, Map, Table>>();
        }
    }  // namespace v2
}  // namespace socklib
