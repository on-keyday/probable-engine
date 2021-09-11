#pragma once
#include "sockbase.h"
#include <reader.h>

#include <memory>
namespace socklib {
    struct UDP {
        static std::shared_ptr<Conn> open(unsigned short fromport = 0, unsigned short toport = 0, const char* host = nullptr, const char* service = nullptr) {
            ::addrinfo *info = nullptr, hint{0};
            if (host) {
                hint.ai_family = AF_INET;
                hint.ai_socktype = SOCK_DGRAM;
                if (::getaddrinfo(host, service, &hint, &info) != 0) {
                    return nullptr;
                }
                if (toport != 0) {
                    ::sockaddr_in* s = (::sockaddr_in*)info->ai_addr;
                    s->sin_port = toport;
                }
            }
            auto sock = ::socket(AF_INET, SOCK_DGRAM, 0);
            if (sock < 0) return nullptr;
            if (fromport != 0) {
                ::sockaddr_in6 addr{0};
                addr.sin6_family = AF_INET6;
                addr.sin6_port = commonlib2::translate_byte_net_and_host<unsigned short>(&fromport);
                socklen_t len = sizeof(addr);
                if (::bind(sock, (::sockaddr*)&addr, len) < 0) {
                    return nullptr;
                }
            }
            return std::make_shared<Conn>(sock, info);
        }
    };
}  // namespace socklib