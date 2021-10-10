/*
    socklib - simple socket library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once

#include "../../common/platform.h"

namespace socklib {
    namespace v2 {
        enum class QuicPacketType {
            initial = 0x00,
            zero_rtt = 0x01,
            handshake = 0x02,
            retry = 0x03,
        };

#define TYPE_FUNCTION(TYPE, FUNC, RESULT, ...) \
    virtual TYPE* FUNC() __VA_ARGS__ {         \
        return RESULT;                         \
    }

#define TYPE_CONSTRUCT(TYPE, BASE, ENUM) \
    TYPE()                               \
        : BASE(ENUM) {}

#define QUIC_PACKET_CONSTRUCT(TYPE, ENUM) TYPE_CONSTRUCT(TYPE, QuicPacket, ENUM)
#define QUIC_TYPEOVERRIDE(TYPE, ENUM, FUNC) \
    QUIC_PACKET_CONSTRUCT(TYPE, ENUM)       \
    TYPE_FUNCTION(TYPE, FUNC, this, override)

#define QUIC_INITIAL TYPE_FUNCTION(InitialPacket, initial, nullptr)
#define QUIC_INITIAL_OVERRIDE QUIC_TYPEOVERRIDE(InitialPacket, QuicPacketType::initial, initial)
#define QUIC_ZERORTT TYPE_FUNCTION(ZeroRTTPacket, zero_rtt, nullptr)
#define QUIC_ZERORTT_OVERRIDE QUIC_TYPEOVERRIDE(InitialPacket, QuicPacketType::zero_rtt, zero_rtt)
#define QUIC_HANDSHAKE TYPE_FUNCTION(HandshakePacket, handshake, nullptr)
#define QUIC_HANDSHAKE_OVERRIDE QUIC_TYPEOVERRIDE(HandshakePacket, QuicPacketType::handshake, handshake)
#define QUIC_RETRY TYPE_FUNCTION(RetryPacket, retry, nullptr)
#define QUIC_RETRY_OVERRIDE QUIC_TYPEOVERRIDE(RetryPacket, QuicPacketType::retry, retry)

        struct QuicPacket {
            QuicPacketType type;
            QuicPacket(QuicPacketType t)
                : type(t) {}
        };

        struct InitialPacket : QuicPacket {
            InitialPacket()
                : QuicPacket(QuicPacketType::initial) {}
        };

        struct ZeroRTTPacket : QuicPacket {
            ZeroRTTPacket()
                : QuicPacket(QuicPacketType::zero_rtt) {}
        };
    }  // namespace v2
}  // namespace socklib