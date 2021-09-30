#pragma once

#include "iconn.h"

namespace socklib {
    namespace v2 {

        enum class RequestPhase {
            idle,
            opening,
            request_sent,
            response_recv,
            body_parsed,
            closed,
        };

        template <class String, class Header, class Body>
        struct RequestContext {
            using string_t = String;
            using header_t = Header;
            using body_t = Body;
            RequestPhase phase;
            string_t url;
            string_t proxy;
            header_t request;
            body_t requestbody;
            header_t response;
            body_t responsebody;
        };

        struct HttpConn : private SecureStreamConn {
        };

    }  // namespace v2
}  // namespace socklib