/*
    socklib - simple socket library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once

#include "platform.h"

namespace socklib {
    namespace v2 {
        enum class QuicFType : std::uint8_t {
            padding = 0x00,
            ping = 0x01,
            ack = 0x02,
            ack_ecn = 0x03,
            reset_stream = 0x04,
            stop_sending = 0x05,
            crypto = 0x06,
            new_token = 0x07,
            stream = 0x08,
            stream_offset = 0x04,
            stream_len = 0x02,
            stream_fin = 0x01,
            max_data = 0x10,
            max_stream_data = 0x11,
            max_streams_bidi = 0x12,
            max_streams_uni = 0x13,
            data_blocked = 0x14,
            stream_data_blocked = 0x15,
            streams_blocked_bidi = 0x16,
            streams_blocked_uni = 0x17,
            new_connection_id = 0x18,
            retire_connection_id = 0x19,
            path_challenge = 0x1a,
            path_response = 0x1b,
            connection_close = 0x1c,
            connection_close_error = 0x1d,
            handshake_done = 0x1e,
        };
    }
}  // namespace socklib