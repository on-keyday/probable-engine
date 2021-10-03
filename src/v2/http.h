#pragma once

#include "http1.h"

namespace socklib {
    namespace v2 {

        template <class String, class Header, class Body>
        struct RequestProxy : INetAppConn {
           protected:
            RequestContext<String, Header, Body> ctx;
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

        template <class String, class Header, class Body>
        struct ClientRequestProxy : RequestProxy<String, Header, Body> {
            using base_t = RequestProxy<String, Header, Body>;
            using http1client_t = Http1Client<String, Header, Body>;
            using opener_t = HttpBase<String, Header, Body>;

           private:
            HttpReadContext<String, Header, Body> readbuf;

           public:
            ClientRequestProxy()
                : readbuf(this->ctx) {}

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

            bool request(const String& method, const String& url, CancelContext* cancel = nullptr) {
                this->ctx.url = url;
                this->ctx.method = method;
                if (!opener_t::open(this->conn, this->ctx, cancel, 1)) {
                    return false;
                }
                if (this->ctx.resolved_version == 2) {
                    //unimplemented h2
                }
                else {
                    if (this->ctx.http_version == 2) {
                        //unimplemented h2c
                    }
                    else {
                        return http1client_t::request(this->conn, this->ctx, cancel);
                    }
                }
                return false;
            }

            bool response(CancelContext* cancel) {
                return http1client_t::response(this->conn, readbuf, cancel);
            }
        };

        template <class String, class Header, class Body>
        struct ServerRequestProxy : RequestProxy<String, Header, Body> {
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

        template <class String = std::string, class Header = std::multimap<String, String>, class Body = String>
        std::shared_ptr<ClientRequestProxy<String, Header, Body>> make_request() {
            return std::make_shared<ClientRequestProxy<String, Header, Body>>();
        }

        template <class String = std::string, class Header = std::multimap<String, String>, class Body = String>
        std::shared_ptr<ServerRequestProxy<String, Header, Body>> accept_request() {
            return std::make_shared<ServerRequestProxy<String, Header, Body>>();
        }
    }  // namespace v2
}  // namespace socklib