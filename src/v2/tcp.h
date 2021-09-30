#pragma once

#include "iconn.h"
#include <memory>

namespace socklib {
    namespace v2 {

        struct TCPOpenContext {
        };

        struct TCP {
            std::shared_ptr<InetConn> open() {
            }
        };
    }  // namespace v2
}  // namespace socklib