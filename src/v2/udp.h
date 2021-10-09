/*
    socklib - simple socket library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once

#include "iconn.h"

namespace socklib {
    namespace v2 {
        struct DgramConn : InetConn {
            SOCKET sock = invalid_socket;

            DgramConn(SOCKET s, ::addrinfo* remote)
                : sock(s), InetConn(remote) {}
        };
    }  // namespace v2
}  // namespace socklib
