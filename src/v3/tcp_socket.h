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
#include <basic_helper.h>
namespace socklib {
    namespace v3 {

        constexpr ::SOCKET invalid_socket = ~0;
        constexpr size_t int_max = ~(unsigned int)commonlib2::msb_on<int>();

        /*struct TCPInSetting : IInputSetting<ContextType::tcp_socket> {
            int ip_version = 0;
        };

        struct TCPOutSetting : IOutputSetting<ContextType::tcp_socket> {
            ::SOCKET sock = invalid_socket;
        };*/

        struct TCPContext : Context {
            OVERRIDE_CONTEXT(ContextType::tcp_socket)
            ::SOCKET sock = invalid_socket;
            size_t write_offset = 0;
            DnsContext* dns = nullptr;
            ::addrinfo* current = nullptr;
            ::SOCKET tmp = invalid_socket;
            int ip_version = 0;
        };

        struct TCPConn : Conn {
           private:
            TCPContext* ctx = nullptr;

           public:
            TCPConn() {}

           private:
            State connect(ContextManager& m) {
                auto append_host_service = [&] {
                    //DnsOutSetting osetting;
                    ctx->add_report("host ");
                    ctx->add_report(ctx->dns->host->c_str());
                    ctx->add_report(" ,service ");
                    ctx->add_report(ctx->dns->service->c_str());
                };
                switch (ctx->get_progress()) {
                    case 3: {
                        if (!ctx->current) {
                            ctx->report("address not resolved. ");
                            append_host_service();
                            return false;
                        }
                        if (ctx->tmp != invalid_socket) {
                            ::closesocket(ctx->tmp);
                            ctx->tmp = invalid_socket;
                        }
                        ctx->tmp = ::socket(ctx->current->ai_family, ctx->current->ai_socktype, ctx->current->ai_protocol);
                        if (ctx->tmp < 0 || ctx->tmp == invalid_socket) {
                            ctx->current = ctx->current->ai_next;
                            return StateValue::inprogress;
                        }
                        u_long flag = 1;
                        ::ioctlsocket(ctx->tmp, FIONBIO, &flag);
                        ctx->increment();
                    }
                    case 4:
                        if (::connect(ctx->tmp, ctx->current->ai_addr, ctx->current->ai_addrlen) == 0) {
                            close(m);
                            ctx->sock = ctx->tmp;
                            ctx->tmp = invalid_socket;
                            return true;
                        }
                        ctx->increment();
                    case 5: {
                        ::timeval timeout = {0};
                        ::fd_set baseset = {0}, sucset = {0}, errset = {0};
                        FD_ZERO(&baseset);
                        FD_SET(ctx->tmp, &baseset);
                        memcpy(&sucset, &baseset, sizeof(::fd_set));
                        memcpy(&errset, &baseset, sizeof(::fd_set));
                        timeout.tv_usec = 1;
                        auto res = ::select(ctx->tmp + 1, nullptr, &sucset, &errset, &timeout);
                        if (res < 0) {
                            ::closesocket(ctx->tmp);
                            ctx->tmp = invalid_socket;
                            ctx->report("connect failed on select() calling. ", get_socket_error());
                            append_host_service();
                            return false;
                        }
                        if (FD_ISSET(ctx->tmp, &errset)) {
                            ::closesocket(ctx->tmp);
                            ctx->tmp = invalid_socket;
                            ctx->report("connect failed on FD_ISSET(&errset) calling. ");
                            append_host_service();
                            return false;
                        }
                        if (FD_ISSET(ctx->tmp, &sucset)) {
                            ctx->sock = ctx->tmp;
                            ctx->tmp = invalid_socket;
                            return true;
                        }
                        return StateValue::inprogress;
                    }
                }
            }

           public:
            virtual State open(ContextManager& m) override {
                if (auto e = m.check_id(ctx); !e) {
                    return e;
                }
                if (!ctx->test_and_set_state<CtxState::opening>()) {
                    return false;
                }
                switch (ctx->get_progress()) {
                    case 0:
                        if (!get_base()) {
                            ctx->report("need base for DNS");
                            return false;
                        }
                        ctx->increment();
                    case 1: {
                        size_t t = 0;
                        if (ctx->dns = m.get_in_link<DnsContext>(&t); !ctx->dns || t != 1) {
                            ctx->report("need DnsContext at base to resolve address");
                            return false;
                        }
                        ctx->increment();
                    }
                    case 2:
                        if (auto e = Conn::open(m); e == StateValue::inprogress) {
                            return e;
                        }
                        else if (!e) {
                            ctx->report(nullptr);
                            return e;
                        }
                        else {
                            ctx->current = ctx->dns->addr;
                        }
                        ctx->increment();
                    case 3:
                    case 4:
                    case 5:
                        if (auto e = connect(m); e = StateValue::inprogress) {
                            return e;
                        }
                        else if (!e) {
                            ctx->current = nullptr;
                            ctx->dns = nullptr;
                            return e;
                        }
                        ctx->increment();
                    default:
                        ctx->report(nullptr);
                        ctx->current = nullptr;
                        ctx->dns = nullptr;
                        ctx = nullptr;
                        break;
                }
                return true;
            }

           private:
            template <CtxState st>
            State check_valid(ContextManager& m, const char* data, size_t size) {
                if (auto e = m.check_id(ctx); !e) {
                    return e;
                }
                if (!ctx->test_and_set_state<st>()) {
                    return false;
                }
                if (!data || !size) {
                    ctx->report("need not null data and not 0 size.");
                    ctx->write_offset = 0;
                    return false;
                }
                if (ctx->sock == invalid_socket) {
                    ctx->report("need open() before ");
                    ctx->add_report(state_value(st));
                    ctx->write_offset = 0;
                    return false;
                }
                return true;
            }

           public:
            virtual State write(ContextManager& m, const char* data, size_t size) {
                if (auto e = check_valid<CtxState::writing>(m, data, size); !e) {
                    return e;
                }
                if (size < ctx->write_offset) {
                    ctx->report("invalid write progress. size < write_offset");
                    ctx->write_offset = 0;
                    return false;
                }
                size_t sz = size - ctx->write_offset <= int_max ? size - ctx->write_offset : int_max;
                auto res = ::send(ctx->sock, data + ctx->write_offset, sz, 0);
                if (res < 0) {
                    if (get_socket_error() == EWOULDBLOCK) {
                        return StateValue::inprogress;
                    }
                    ctx->report("write failed on ::send() calling.", get_socket_error());
                    ctx->write_offset = 0;
                    return false;
                }
                if (sz < int_max) {
                    ctx->report(nullptr);
                    ctx->write_offset = 0;
                    ctx = nullptr;
                    return true;
                }
                ctx->write_offset += int_max;
                return StateValue::inprogress;
            }

            virtual State read(ContextManager& m, char* data, size_t size, size_t& red) override {
                if (auto e = check_valid<CtxState::reading>(m, data, size); !e) {
                    return e;
                }
                if (size > int_max) {
                    size = int_max;
                }
                auto res = ::recv(ctx->sock, data, (int)size, 0);
                if (res < 0) {
                    if (get_socket_error() == EWOULDBLOCK) {
                        ctx->reset_state();
                        return StateValue::inprogress;
                    }
                    ctx->report("read failed on ::recv() calling.", get_socket_error());
                    return false;
                }
                red = (size_t)res;
                if (red < size) {
                    ctx = nullptr;
                    return true;
                }
                return StateValue::inprogress;
            }
            /*

            virtual Context* get_base() override {
                if (!base) return nullptr;
                return std::addressof(*base);
            }

            virtual bool set_setting(const InputSetting& set) override {
                if (auto tcp = cast_<TCPInSetting>(&set)) {
                    ip_version = tcp->ip_version;
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
                        err->errmsg = errmsg->c_str();
                        err->numerr = numerr;
                    }
                    return true;
                }
                if (base) {
                    return base->has_error(err);
                }
                return false;
            }*/

            virtual State close(ContextManager& m) override {
                if (auto e = m.check_id(ctx); !e) {
                    return e;
                }
                if (ctx->tmp != invalid_socket) {
                    ::closesocket(ctx->tmp);
                    ctx->tmp = invalid_socket;
                }
                if (ctx->sock != invalid_socket) {
                    ::shutdown(ctx->sock, SD_BOTH);
                    ::closesocket(ctx->sock);
                    ctx->sock = invalid_socket;
                }
                ctx->report(nullptr);
                ctx->current = nullptr;
                ctx->dns = nullptr;
                ctx->write_offset = 0;
                //Conn::close(m);
                return true;
            }

            virtual ~TCPConn() {}
        };
    }  // namespace v3
}  // namespace socklib
