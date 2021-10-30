/*
    socklib - simple socket library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <net_helper.h>
#include "string_buffer.h"

namespace socklib {
    namespace v3 {
        struct URL {
            virtual bool parse(const char*, size_t) = 0;
            virtual bool parse(const char*) = 0;
            virtual const char* scheme() = 0;
            virtual bool set_scheme(const char*) = 0;
            virtual const char* host() = 0;
            virtual bool set_host(const char*) = 0;
            virtual const char* user() = 0;
            virtual bool set_user(const char*) = 0;
            virtual const char* password() = 0;
            virtual bool set_password(const char*) = 0;
            virtual const char* path() = 0;
            virtual bool set_path(const char*) = 0;
            virtual const char* query() = 0;
            virtual bool set_query(const char*) = 0;
            virtual const char* tag() = 0;
            virtual bool set_tag(const char*) = 0;
            virtual const char* opaque() = 0;
            virtual bool set_opaque(const char*) = 0;
        };

        template <class String>
        struct URL_impl {
            using buffer_t = StringBuffer_impl<std::remove_cvref_t<String>>;
            commonlib2::URLContext<String> url;

            virtual bool parse(const char* u, size_t s) override {
                commonlib2::Reader r(commonlib2::Sized(u, s));
                r.readwhile(commonlib2::parse_url, &url);
            }
            virtual bool parse(const char*) override {
            }
            virtual const char* scheme() = 0;
            virtual bool set_scheme(const char*) = 0;
            virtual const char* host() = 0;
            virtual bool set_host(const char*) = 0;
            virtual const char* user() = 0;
            virtual bool set_user(const char*) = 0;
            virtual const char* password() = 0;
            virtual bool set_password(const char*) = 0;
            virtual const char* path() = 0;
            virtual bool set_path(const char*) = 0;
            virtual const char* query() = 0;
            virtual bool set_query(const char*) = 0;
            virtual const char* tag() = 0;
            virtual bool set_tag(const char*) = 0;
            virtual const char* opaque() = 0;
            virtual bool set_opaque(const char*) = 0;
        };
    }  // namespace v3
}  // namespace socklib