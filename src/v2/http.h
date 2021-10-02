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

            Header& request() {
                return ctx.request;
            }

            Body& requestBody() {
                return ctx.requestbody;
            }

            Header& response() {
                return ctx.response;
            }

            Body& responseBody() {
                return ctx.responsebody;
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
            using client_t = Http1Client<String, Header, Body>;

           private:
            HttpReadContext<String, Header, Body> readbuf;

           public:
            ClientRequestProxy()
                : readbuf(this->ctx) {}

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
                return client_t::request(this->conn, this->ctx, cancel);
            }

            bool response(CancelContext* cancel) {
                return client_t::response(this->conn, readbuf, cancel);
            }
        };

        template <class String = std::string, class Header = std::multimap<String, String>, class Body = String>
        std::shared_ptr<ClientRequestProxy<String, Header, Body>> make_request() {
            return std::make_shared<ClientRequestProxy<String, Header, Body>>();
        }
    }  // namespace v2
}  // namespace socklib