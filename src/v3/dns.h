/*
    socklib - simple socket library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "conn.h"
#include "string_buffer.h"

namespace socklib {
    namespace v3 {
        errno_t get_socket_error() {
#ifdef _WIN32
            return ::WSAGetLastError();
#else
            return errno;
#endif
        }

        struct DnsInSetting : IInputSetting<ContextType::dns> {
            const char* host = nullptr;
            const char* servoce = nullptr;
            int socket_type = 0;
            int socket_family = 0;
        };

        struct DnsOutSetting : IOutputSetting<ContextType::dns> {
            const char* host = nullptr;
            const char* service = nullptr;
            ::addrinfo* addr = nullptr;
        };

        struct DnsContext : Context {
            OVERRIDE_CONTEXT(ContextType::dns);
            StringBuffer* host = nullptr;
            StringBuffer* service = nullptr;
            int socktype = 0;
            int sockfamily = 0;
            ::addrinfo* addr = nullptr;
        };

        struct DnsConn : Conn {
           public:
            DnsConn(const StringBufferBuilder& bufbase) {}
            /* : host(bufbase.make()), service(bufbase.make())*/
            /*
            virtual bool set_setting(const InputSetting& set) override {
                if (auto dns = cast_<DnsInSetting>(&set)) {
                    host->clear();
                    host->set(dns->host);
                    service->clear();
                    service->set(dns->servoce);
                    socktype = dns->socket_type;
                    sockfamily = dns->socket_family;
                    return true;
                }
                return false;
            }

            virtual bool get_setting(OutputSetting& get) override {
                if (auto dns = cast_<DnsOutSetting>(&get)) {
                    dns->addr = addr;
                    dns->host = host->c_str();
                    dns->service = service->c_str();
                    return true;
                }
                return false;
            }*/

            virtual State open(ContextManager& m) override {
                auto got = m.get<DnsContext>();
                if (!got) {
                    return StateValue::fatal;
                }
                auto ctx = got;
                ::addrinfo hint = {0}, *info = nullptr;
                hint.ai_socktype = ctx->socktype;
                hint.ai_family = ctx->sockfamily;
                const char* hostname = ctx->host->size() ? ctx->host->c_str() : nullptr;
                const char* servicename = ctx->service->size() ? ctx->service->c_str() : nullptr;
                if (::getaddrinfo(hostname, servicename, &hint, &info) != 0) {
                    ctx->report("resolve address failed: host ", get_socket_error());
                    ctx->add_report(hostname ? hostname : "<NULL>");
                    ctx->add_report(" , service ");
                    ctx->add_report(servicename ? servicename : "<NULL>");
                    return false;
                }
                close(m);
                ctx->addr = info;
                ctx = nullptr;
                return true;
            }

            virtual State close(ContextManager& m) override {
                auto got = m.get<DnsContext>();
                if (!got) {
                    return StateValue::fatal;
                }
                if (got->addr) {
                    ::freeaddrinfo(got->addr);
                    got->addr = nullptr;
                }
                return true;
            }
            /*
            virtual bool has_error(ErrorInfo* err) override {
                if (errmsg->size()) {
                    if (err) {
                        err->type = ContextType::dns;
                        err->errmsg = errmsg->c_str();
                        err->numerr = errcode;
                    }
                    return true;
                }
                return false;
            }*/

            ~DnsConn() {
                /*close();
                delete host;
                delete service;
                delete errmsg;*/
            }
        };
    }  // namespace v3
}  // namespace socklib