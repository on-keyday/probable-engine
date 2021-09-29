#pragma once

#include "iconn.h"

namespace socklib {
    namespace v2 {

        template <class String, class Header, class Body>
        struct RequestContext {
            using string_t = String;
            using header_t = Header;
            using body_t = Body;
            string_t url;
            string_t proxy;
            header_t request;
            body_t requestbody;
            header_t response;
            body_t responsebody;
        };
    }  // namespace v2
}  // namespace socklib