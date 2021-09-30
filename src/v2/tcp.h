#pragma once

#include "iconn.h"
#include <memory>

namespace socklib {
    namespace v2 {

        template <class String>
        struct TCPOpenContext {
            using string_t = String;
            string_t host;
            string_t service;
            std::uint16_t port = 0;
            string_t cacert;
            int ip_version = 0;
        };

        struct TCP {
            template <class String>
            std::shared_ptr<InetConn> open(TCPOpenContext<String>& ctx, CancelContext* cancel = nullptr) {
            }
        };
    }  // namespace v2
}  // namespace socklib