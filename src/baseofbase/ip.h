#pragma once
#include <cstdint>
#include <cstddef>

#include <basic_helper.h>

#include "net_device.h"

namespace socklib {
    constexpr size_t IP_HEADER_MIN_LEN = 20;

    struct IPv4Header {
        std::uint8_t ver_and_hl;
        std::uint8_t type_of_service;
        std::uint16_t packetlen;
        std::uint16_t id;
        std::uint16_t flag_and_offset;
        std::uint8_t time_to_live;
        std::uint8_t protocol_number;
        std::uint16_t checksum;
        std::uint8_t fromaddress[4];
        std::uint8_t toaddress[4];
    };

    struct IPv6Header {
        std::uint8_t version;
        std::uint8_t traffic_class;
        std::uint16_t flow_label;
        std::uint16_t payloadlen;
        std::uint8_t next_header;
        std::uint8_t pop_limit;
        std::uint8_t fromaddress[16];
        std::uint8_t toaddress[16];
    };

    enum class IPError {
        none,
        internel,
        version,
        header_length,
        packet_length,
        packet_lost,
    };

    using IPErr = commonlib2::EnumWrap<IPError, IPError::none, IPError::internel>;

    constexpr std::uint16_t cheksum(std::uint16_t* addr, std::uint16_t count, std::uint32_t init) {
        std::uint32_t sum = init, idx = 0;
        while (count > 1) {
            sum += addr[idx];
            idx++;
            count -= 2;
        }
        if (count > 0) {
            sum += *(std::uint8_t*)&addr[idx];
        }
        while (sum >> 16) {
            sum = (sum & 0xffff) + (sum >> 16);
        }
        return ~(std::uint16_t)sum;
    }

    IPErr parse_ipv4header(const char* src, size_t len, net_device* dev) {
        auto h = (const IPv4Header*)src;
        0x0f << 4;
        std::uint8_t hlen = h->ver_and_hl << 4;
        if (len < hlen * 4) {
            return IPError::header_length;
        }
        std::uint16_t packetlen = commonlib2::translate_byte_net_and_host<std::uint16_t>(&h->packetlen);
        if (len < packetlen) {
            return IPError::packet_length;
        }
        if (h->time_to_live == 0) {
            return IPError::packet_lost;
        }
    }

    IPErr parse_ip_header(const char* src, size_t len, net_device* dev) {
        if (!src || !dev) return false;

        if (len < IP_HEADER_MIN_LEN) {
            return false;
        }
        uint8_t version = src[0] >> 4;

        if (version == 4) {
            return parse_ipv4header(src, len, dev);
        }
        else if (version == 6) {
        }
        else {
            return IPError::version;
        }
    }
}  // namespace socklib