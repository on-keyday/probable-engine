/*
    socklib - simple socket library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <cstdint>
#include <cstddef>

#include <basic_helper.h>

#include "net_device.h"

namespace socklib {
    constexpr size_t IP_HEADER_MIN_LEN = 20;

    enum class IPError {
        none,
        internel,
        version,
        header_length,
        packet_length,
        packet_lost,
        checksum,
    };

    using IPErr = commonlib2::EnumWrap<IPError, IPError::none, IPError::internel>;

    union ipv4addr_t {
        std::uint8_t by[4];
        std::uint16_t wo[2];
        std::uint32_t dw;
    };

    union ipv6addr_t {
        std::uint8_t by[16];
        std::uint16_t wo[8];
        std::uint32_t dw[4];
        std::uint64_t qw[2];
    };

    struct IPv4Header {
        std::uint8_t ver_and_hl;
        std::uint8_t type_of_service;
        std::uint16_t packetlen;
        std::uint16_t id;
        std::uint16_t flag_and_offset;
        std::uint8_t time_to_live;
        std::uint8_t protocol_number;
        std::uint16_t checksum;
        ipv4addr_t fromaddress;
        ipv4addr_t toaddress;
    };

    struct IPv6Header {
        std::uint8_t version;
        std::uint8_t traffic_class;
        std::uint16_t flow_label;
        std::uint16_t payloadlen;
        std::uint8_t next_header;
        std::uint8_t pop_limit;
        ipv6addr_t fromaddress;
        ipv6addr_t toaddress;
    };

    struct ip_interface {
        int protocol_family;
    };

    struct ip_protocol {
        ip_protocol* next = nullptr;
        char name[16] = {0};
        std::uint8_t type = 0;
        IPErr (*v4handler)(const std::uint8_t* data, size_t len, ip_interface* iface, IPv4Header* h) = nullptr;
        IPErr (*v6handler)(const std::uint8_t* data, size_t len, ip_interface* iface, IPv6Header* h) = nullptr;
    };

    struct ip_context {
        ip_protocol* protocols = nullptr;
    };

    constexpr std::uint16_t cheksum(std::uint16_t* addr, std::uint16_t count, std::uint16_t check, std::uint32_t init) {
        std::uint32_t sum = init, idx = 0;
        while (count > 1) {
            sum += commonlib2::translate_byte_net_and_host<std::uint16_t>(&addr[idx]);
            idx++;
            count -= 2;
        }
        sum -= check;
        while (sum >> 16) {
            sum = (sum & 0xffff) + (sum >> 16);
        }
        return ~(std::uint16_t)sum;
    }

    IPErr parse_ipv4header(const char* src, size_t len, net_device* dev, ip_context* ctx) {
        auto h = (const IPv4Header*)src;
        0x0f << 4;
        std::uint8_t hlen = (0x0f & h->ver_and_hl) << 2;
        if (len < hlen) {
            return IPError::header_length;
        }
        std::uint16_t packetlen = commonlib2::translate_byte_net_and_host<std::uint16_t>(&h->packetlen);
        if (len < packetlen) {
            return IPError::packet_length;
        }
        if (h->time_to_live == 0) {
            return IPError::packet_lost;
        }
        auto sum = commonlib2::translate_byte_net_and_host<std::uint16_t>(&h->checksum);
        if (cheksum((std::uint16_t*)h, hlen, sum, 0) != sum) {
            return IPError::checksum;
        }
    }

    IPErr parse_ip_header(const char* src, size_t len, net_device* dev, ip_context* ctx) {
        if (!src || !dev) return false;

        if (len < IP_HEADER_MIN_LEN) {
            return false;
        }
        uint8_t version = src[0] >> 4;

        if (version == 4) {
            return parse_ipv4header(src, len, dev, ctx);
        }
        else if (version == 6) {
        }
        else {
            return IPError::version;
        }
    }
}  // namespace socklib
