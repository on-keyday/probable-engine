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
    };

    enum class IPError {
        none,
        internel,
        version,
    };

    using IPErr = commonlib2::EnumWrap<IPError, IPError::none, IPError::internel>;

    IPErr parse_ipv4_header(const char* src, size_t len, net_device* dev) {
        if (!src || !dev) return false;

        if (len < IP_HEADER_MIN_LEN) {
            return false;
        }
        uint8_t version = src[0] >> 4;

        if (version == 4) {
            const IPv4Header* h = (const IPv4Header*)src;
        }
        else if (version == 6) {
        }
        else {
            return IPError::version;
        }
    }
}  // namespace socklib