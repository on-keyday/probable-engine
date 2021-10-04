/*
    socklib - simple socket library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <cstdint>
#include <cstddef>
namespace socklib {
    struct net_device;

    struct device_func {
        int (*open)(net_device *dev);
        int (*close)(net_device *dev);
        int (*transmit)(net_device *dev, uint16_t type, const uint8_t *data, size_t len, const void *dst);
        int (*poll)(net_device *dev);
    };

    struct net_device {
        std::uint8_t name[16];
        std::uint16_t type = 0;
        std::uint16_t mtu = 0;
        std::uint16_t flags = 0;
        std::uint16_t header_len = 0;
        std::uint16_t address_len = 0;
        std::uint8_t address[16] = {0};
        union {
            std::uint8_t peer[16] = {0};
            std::uint8_t broadcast[16];
        };
        device_func *func;
        void *ctx = nullptr;
    };

    struct net_interface {
        int protocol_family;
    };

    struct ip_context;

    struct net_context {
        ip_context *ipctx;
    };

}  // namespace socklib
