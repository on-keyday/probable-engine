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

        struct TCPInSetting : IInputSetting<ContextType::tcp_socket> {
            int ip_version = 0;
        };

        struct TCPOutSetting : IOutputSetting<ContextType::tcp_socket> {
            ::SOCKET sock = invalid_socket;
        };

        enum class TCPCtxState {
            free,
            opening,
            writing,
            reading,
        };

        BEGIN_ENUM_STRING_MSG(TCPCtxState, state_value)
        ENUM_STRING_MSG(TCPCtxState::opening, "opening")
        ENUM_STRING_MSG(TCPCtxState::writing, "writing")
        ENUM_STRING_MSG(TCPCtxState::reading, "reading")
        END_ENUM_STRING_MSG("free")

        struct TCPContext : IContext<ContextType::tcp_socket> {
           private:
            std::shared_ptr<Context> base;

            ::SOCKET sock = invalid_socket;

            StringBuffer* errmsg = nullptr;

            errno_t numerr = 0;

            TCPCtxState state = TCPCtxState::free;
            int progress = 0;

            size_t write_offset = 0;

            DnsContext* dns = nullptr;
            ::addrinfo* current = nullptr;
            ::SOCKET tmp = invalid_socket;

            int ip_version = 0;

           public:
            TCPContext(std::shared_ptr<Context> base, const StringBufferBuilder& bufbase)
                : base(base), errmsg(bufbase.make()) {}

           private:
            State connect() {
                auto append_host_service = [&] {
                    DnsOutSetting osetting;
                    dns->get_setting(osetting);
                    errmsg->append_back("host ");
                    errmsg->append_back(osetting.host);
                    errmsg->append_back(" ,service ");
                    errmsg->append_back(osetting.service);
                };
                switch (progress) {
                    case 3: {
                        if (!current) {
                            errmsg->set("address not resolved. ");
                            append_host_service();
                            return false;
                        }
                        if (tmp != invalid_socket) {
                            ::closesocket(tmp);
                            tmp = invalid_socket;
                        }
                        tmp = ::socket(current->ai_family, current->ai_socktype, current->ai_protocol);
                        if (tmp < 0 || tmp == invalid_socket) {
                            current = current->ai_next;
                            return StateValue::inprogress;
                        }
                        u_long flag = 1;
                        ::ioctlsocket(tmp, FIONBIO, &flag);
                        progress = 4;
                    }
                    case 4:
                        if (::connect(tmp, current->ai_addr, current->ai_addrlen) == 0) {
                            close();
                            sock = tmp;
                            tmp = invalid_socket;
                            return true;
                        }
                        progress = 5;
                    case 5: {
                        ::timeval timeout = {0};
                        ::fd_set baseset = {0}, sucset = {0}, errset = {0};
                        FD_ZERO(&baseset);
                        FD_SET(tmp, &baseset);
                        memcpy(&sucset, &baseset, sizeof(::fd_set));
                        memcpy(&errset, &baseset, sizeof(::fd_set));
                        timeout.tv_usec = 1;
                        auto res = ::select(tmp + 1, nullptr, &sucset, &errset, &timeout);
                        if (res < 0) {
                            ::closesocket(tmp);
                            tmp = invalid_socket;
                            errmsg->set("connect failed on select() calling. ");
                            append_host_service();
                            numerr = get_socket_error();
                            append_host_service();
                            progress = 0;
                            return false;
                        }
                        if (FD_ISSET(tmp, &errset)) {
                            ::closesocket(tmp);
                            tmp = invalid_socket;
                            errmsg->set("connect failed on FD_ISSET(&errset) calling. ");
                            append_host_service();
                            return false;
                        }
                        if (FD_ISSET(tmp, &sucset)) {
                            sock = tmp;
                            tmp = invalid_socket;
                            return true;
                        }
                        return StateValue::inprogress;
                    }
                }
            }

           public:
            virtual State open() override {
                if (state != TCPCtxState::free && state != TCPCtxState::opening) {
                    errmsg->clear();
                    errmsg->set("invalid state. now ");
                    errmsg->append_back(state_value(state));
                    errmsg->append_back(", need free or opening");
                    numerr = -1;
                    return false;
                }
                switch (progress) {
                    case 0:
                        state = TCPCtxState::opening;
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
                            state = TCPCtxState::free;
                            current = nullptr;
                            progress = 0;
                            return e;
                        }
                        progress = 6;
                    default:
                        dns = nullptr;
                        current = nullptr;
                        progress = 0;
                        state = TCPCtxState::free;
                        break;
                }
                return true;
            }

           private:
            template <TCPCtxState st>
            bool check_valid(const char* data, size_t size) {
                errmsg->clear();
                if (state != TCPCtxState::free && state != st) {
                    errmsg->set("invalid state. now ");
                    errmsg->append_back(state_value(state));
                    errmsg->append_back(", need free or ");
                    errmsg->append_back(state_value(st));
                    numerr = -1;
                    return false;
                }
                state = st;
                if (!data || !size) {
                    errmsg->set("need not null data and not 0 size.");
                    numerr = -1;
                    write_offset = 0;
                    state = TCPCtxState::free;
                    return false;
                }
                if (sock == invalid_socket) {
                    errmsg->set("need open() before ");
                    errmsg->append_back(state_value(st));
                    numerr = -1;
                    write_offset = 0;
                    state = TCPCtxState::free;
                    return false;
                }
                return true;
            }

           public:
            virtual State write(const char* data, size_t size) {
                if (!check_valid<TCPCtxState::writing>(data, size)) {
                    return false;
                }
                if (size < write_offset) {
                    errmsg->set("invalid write progress. size < write_offset");
                    numerr = -1;
                    write_offset = 0;
                    state = TCPCtxState::free;
                    return false;
                }
                size_t sz = size - write_offset <= int_max ? size - write_offset : int_max;
                auto res = ::send(sock, data + write_offset, sz, 0);
                if (res < 0) {
                    if (get_socket_error() == EWOULDBLOCK) {
                        return StateValue::inprogress;
                    }
                    errmsg->set("write failed on ::send() calling.");
                    numerr = get_socket_error();
                    state = TCPCtxState::free;
                    write_offset = 0;
                    return false;
                }
                if (sz < int_max) {
                    write_offset = 0;
                    state = TCPCtxState::free;
                    return true;
                }
                write_offset += int_max;
                return StateValue::inprogress;
            }

            virtual State read(char* data, size_t size, size_t& red) override {
                errmsg->clear();
                if (!check_valid<TCPCtxState::reading>(data, size)) {
                    return false;
                }
                if (size > int_max) {
                    size = int_max;
                }
                auto res = ::recv(sock, data, (int)size, 0);
                if (res < 0) {
                    if (get_socket_error() == EWOULDBLOCK) {
                        state = TCPCtxState::free;
                        return StateValue::inprogress;
                    }
                    errmsg->set("read failed on ::recv() calling.");
                    numerr = get_socket_error();
                    state = TCPCtxState::free;
                    return false;
                }
                red = (size_t)res;
                if (red < size) {
                    return true;
                }
                return StateValue::inprogress;
            }

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
            }

            virtual State close() {
                if (tmp != invalid_socket) {
                    ::closesocket(tmp);
                    tmp = invalid_socket;
                }
                if (sock != invalid_socket) {
                    ::shutdown(sock, SD_BOTH);
                    ::closesocket(sock);
                    sock = invalid_socket;
                }
                numerr = 0;
                state = TCPCtxState::free;
                progress = 0;
                dns = nullptr;
                current = nullptr;
                write_offset = 0;
                return true;
            }

            virtual ~TCPContext() {
                close();
                Context::close();
                delete errmsg;
            }
        };
    }  // namespace v3
}  // namespace socklib