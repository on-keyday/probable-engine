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

            void set_url(String& url) {
                ctx.url = url;
            }

            void set_httpversion(int version) {
                ctx.http_version = version;
            }

            void set_ipversion(int version) {
                ctx.ip_version = version;
            }
        };

        template <class String = std::string, class Header = std::multimap<String, String>, class Body = String>
        std::shared_ptr<RequestProxy<String, Header, Body>> make_request() {
            return std::make_shared<RequestProxy<String, Header, Body>>();
        }
    }  // namespace v2
}  // namespace socklib