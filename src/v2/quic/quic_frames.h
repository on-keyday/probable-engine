/*
    socklib - simple socket library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once

#include "../../common/platform.h"

#include <serializer.h>
#include <cassert>

namespace socklib {
    namespace v2 {
        enum class QuicFrameType : std::uint8_t {
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
            connection_close_apperror = 0x1d,
            handshake_done = 0x1e,
        };

        template <class String>
        struct QuicIntCoder {
           private:
            using writer_t = commonlib2::Serializer<String>;
            using reader_t = commonlib2::Deserializer<String&>;
            template <class T>
            static constexpr T msb() {
                return commonlib2::msb_on<T>();
            }

            template <class T>
            static constexpr T msb2() {
                return (msb() >> 1);
            }

            template <class T>
            static constexpr T mask_bit() {
                return msb<T>() | msb2<T>();
            }

            template <class Ty>
            static bool read_num(reader_t& r, std::uint64_t& value, int* bytes) {
                Ty v;
                if (!r.read_ntoh(v)) {
                    return false;
                }
                value = (v & ~mask_bit<Ty>());
                if (bytes) {
                    *bytes = sizeof(Ty);
                }
                return true;
            }

           public:
            static bool decode(reader_t& r, std::uint64_t& value, int* bytes = nullptr) {
                if (!r.base_reader().readable()) return false;
                std::uint8_t h = r.base_reader().achar();
                std::uint8_t mask = (h & mask_bit<std::uint8_t>();
                bool result=false;
                switch(mask){
                    case 0:
                        r.base_reader().increment();
                        value = h;
                        result = true;
                        if (bytes) {
                            *bytes = 1;
                        }
                        break;
                    case 0x40:
                        result = read_num<std::uint16_t>(r, value);
                        break;
                    case 0x80:
                        result = read_num<std::uint32_t>(r, value);
                        break;
                    case 0xC0:
                        result = read_num<std::uint64_t>(r, value);
                        break;
                    default:
                        assert(false);
                }
                return result;
            }

            static bool encode(writer_t& w, std::uint64_t value, int* bytes = nullptr) {
                if (value & mask_bit<std::uint64_t>()) {
                    return false;
                }
                if (value < 64) {
                    w.template write_as<std::uint8>(value);
                    if (bytes) {
                        *bytes = 1;
                    }
                }
                else if (value < 16384) {
                    value |= msb2<std::uint16_t>();
                    w.template write_as<std::uint16_t>(value);
                    if (bytes) {
                        *bytes = 2;
                    }
                }
                else if (value < 1073741824) {
                    value |= msb<std::uint32_t>();
                    w.template write_as<std::uint32_t>(value);
                    if (bytes) {
                        *bytes = 4;
                    }
                }
                else if (value < 4611686018427387904) {
                    value |= mask_bit<std::uint64_t>();
                    w.template write_as<std::uint64_t>(value);
                    if (bytes) {
                        *bytes = 8;
                    }
                }
                else {
                    assert(false);
                }
                return true;
            }
        };

        template <class String>
        struct QuicPacketNumCoder {
            using writer_t = commonlib2::Serializer<String>;
            using reader_t = commonlib2::Deserializer<String&>;
            static bool encode(writer_t& w, std::uint64_t all_packetnum, std::uint64_t largest_acked) {
                std::uint64_t unacked = 0;
                if (largest_acked == 0) {
                    unacked = all_packetnum + 1;
                }
                else {
                    unacked = all_packetnum - largest_acked;
                }
            }
        };

    }  // namespace v2
}  // namespace socklib