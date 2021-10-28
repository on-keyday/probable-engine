/*
    socklib - simple socket library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "dns.h"
#include <memory>
#include <string>
namespace socklib {
    namespace v3 {

        constexpr ::SOCKET invalid_socket = ~0;

        struct TCPInSetting : IInputSetting<ContextType::tcp_socket> {
            int ip_version = 0;
            bool blocking = false;
        };

        struct TCPOutSetting : IOutputSetting<ContextType::tcp_socket> {
            ::SOCKET sock = invalid_socket;
        };

        struct TCPContext : IContext<ContextType::tcp_socket> {
           private:
            std::shared_ptr<Context> base;

            std::uint16_t port = 0;
            bool blocking = false;
            ::SOCKET sock = invalid_socket;

            StringBuffer* errmsg = nullptr;

            errno_t numerr = 0;

            int progress = 0;

            DnsContext* dns = nullptr;
            ::addrinfo* current = nullptr;
            ::SOCKET tmp = invalid_socket;

           public:
            TCPContext(std::shared_ptr<Context> base, const StringBufferBuilder& bufbase)
                : base(base), errmsg(bufbase.make()) {}

           private:
            State connect() {
                switch (progress) {
                    case 3:
                        if (!current) {
                            DnsOutSetting osetting;
                            dns->get_setting(osetting);
                            errmsg->set("address not resolved. host ");
                            errmsg->append_back(osetting.host);
                            errmsg->append_back(" ,service ");
                            errmsg->append_back(osetting.service);
                            return false;
                        }
                        tmp = ::socket(current->ai_family, current->ai_socktype, current->ai_protocol);
                        if (tmp < 0 || tmp == invalid_socket) {
                            current = current->ai_next;
                            return StateValue::inprogress;
                        }
                        u_long flag = 1;
                        ::ioctlsocket(tmp, FIONBIO, &flag);
                        progress = 4;
                    case 4:
                        if (::connect(tmp, current->ai_addr, current->ai_addrlen) == 0) {
                            close();
                            sock = tmp;
                        }
                }
            }

           public:
            virtual State open() override {
                switch (progress) {
                    case 0:
                        errmsg->clear();
                        if (!base) {
                            errmsg->set("need base for DNS");
                            numerr = -1;
                            return false;
                        }
                        progress = 1;
                    case 1:
                        for (auto p = get_base(); p; p = p->get_base()) {
                            if (dns = cast_<DnsContext>(p)) {
                                break;
                            }
                        }
                        if (!dns) {
                            errmsg->set("need DnsContext at base to resolve address");
                            numerr = -1;
                            progress = 0;
                            return false;
                        }
                        progress = 2;
                    case 2:
                        if (auto e = dns->open(); e == StateValue::inprogress) {
                            return e;
                        }
                        else if (!e) {
                            dns = nullptr;
                            progress = 0;
                            return e;
                        }
                        else {
                            DnsOutSetting osetting;
                            if (!dns->get_setting(osetting)) {
                                errmsg->set("unknown error: can't get addrinfo");
                                numerr = -1;
                                dns = nullptr;
                                progress = 0;
                                return false;
                            }
                            current = osetting.addr;
                        }
                        progress = 3;
                    case 3:
                    case 4:
                    case 5:
                        if (auto e = connect(); e = StateValue::inprogress) {
                            return e;
                        }
                        else if (!e) {
                            dns = nullptr;
                            progress = 0;
                            return e;
                        }
                        progress = 6;
                }
            }

            virtual Context* get_base() override {
                if (!base) return nullptr;
                return std::addressof(*base);
            }

            virtual bool set_setting(const InputSetting& set) override {
                if (auto tcp = cast_<TCPInSetting>(&set)) {
                    return true;
                }
                if (base) {
                    return base->set_setting(set);
                }
                return false;
            }

            virtual bool get_setting(OutputSetting& get) override {
                if (auto tcp = cast_<TCPOutSetting>(&get)) {
                    tcp->sock = sock;
                    return true;
                }
                if (base) {
                    return base->get_setting(get);
                }
                return false;
            }

            virtual bool has_error(ErrorInfo* err) override {
                if (errmsg->size()) {
                    if (err) {
                        err->type = ContextType::tcp_socket;
                    }
                    return true;
                }
                if (base) {
                    return base->has_error(err);
                }
                return false;
            }

            virtual ~TCPContext() {
                delete errmsg;
            }
        };
    }  // namespace v3
}  // namespace socklib