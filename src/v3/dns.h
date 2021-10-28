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

        struct DnsContext : IContext<ContextType::dns> {
           private:
            StringBuffer* host = nullptr;
            StringBuffer* service = nullptr;
            StringBuffer* errmsg = nullptr;
            int socktype = 0;
            int sockfamily = 0;
            ::addrinfo* addr = nullptr;
            errno_t errcode = 0;

           public:
            DnsContext(const StringBufferBuilder& bufbase)
                : host(bufbase.make()), service(bufbase.make()), errmsg(bufbase.make()) {}

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
            }

            virtual State open() override {
                errmsg->clear();
                ::addrinfo hint = {0}, *info = nullptr;
                hint.ai_socktype = socktype;
                hint.ai_family = sockfamily;
                const char* hostname = host->size() ? host->c_str() : nullptr;
                const char* servicename = service->size() ? service->c_str() : nullptr;
                if (::getaddrinfo(hostname, servicename, &hint, &info) != 0) {
                    errmsg->set("resolve address failed: host ");
                    errmsg->append_back(hostname ? hostname : "<NULL>");
                    errmsg->append_back(" , service ");
                    errmsg->append_back(servicename ? servicename : "<NULL>");
                    errcode = get_socket_error();
                    return false;
                }
                close();
                addr = info;
                return true;
            }

            virtual State close() override {
                if (addr) {
                    ::freeaddrinfo(addr);
                }
                return true;
            }

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
            }

            ~DnsContext() {
                close();
                delete host;
                delete service;
                delete errmsg;
            }
        };
    }  // namespace v3
}  // namespace socklib