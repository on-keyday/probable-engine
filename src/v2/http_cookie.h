/*
    socklib - simple socket library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once

#include <extutil.h>

namespace socklib {
    namespace v2 {

        enum class SameSite {
            default_mode,
            lax_mode,
            strict_mode,
            none_mode,
        };

        template <class String>
        struct Cookie {
            using string_t = String;

            string_t name;
            string_t value;

            string_t path;
            string_t domain;
            string_t expires;

            int maxage = 0;
            bool secure = false;
            bool httponly = false;
            SameSite samesite = SameSite::default_mode;
        };

        enum class CookieError {
            none,
            no_cookie,
            no_keyvalue,
            no_attrkeyvalue,
            multiple_same_attr,
            unknown_samesite,
        };

        template <class String, class Vec>
        struct CookieParser {
            using string_t = String;
            using vector_t = Vec;
            using cookie_t = Cookie<String>;
            CookieError parse(const string_t& raw, cookie_t& cookie) {
                auto data = commonlib2::split<string_t, const char*, vector_t>(raw, "; ");
                if (data.size() == 0) {
                    return CookieError::no_cookie;
                }
                auto keyval = commonlib2::split<string_t, const char*, vector_t>(data[0], "=", 1);
                if (keyval.size() != 2) {
                    return CookieError::no_keyvalue;
                }
                cookie.name = keyval[0];
                cookie.value = keyval[1];
                for (auto& attr : data) {
                    if (attr == "HttpOnly") {
                        if (cookie.httponly) {
                            return CookieError::multiple_same_attr;
                        }
                        cookie.httponly = true;
                    }
                    else if (attr == "Secure") {
                        if (cookie.secure) {
                            return CookieError::multiple_same_attr;
                        }
                        cookie.secure = true;
                    }
                    else {
                        auto elm = commonlib2::split<string_t, const char*, vector_t>(attr, "=", 1);
                        if (elm.size() != 2) {
                            return CookieError::no_attrkeyvalue;
                        }
                        if (elm[0] == "Expires") {
                            if (cookie.expires.size()) {
                                return CookieError::multiple_same_attr;
                            }
                            cookie.expires = elm[1];
                        }
                        else if (elm[0] == "Path") {
                            if (cookie.path.size()) {
                                return CookieError::multiple_same_attr;
                            }
                            cookie.path = elm[1];
                        }
                        else if (elm[0] == "Domain") {
                            if (cookie.domain.size()) {
                                return CookieError::multiple_same_attr;
                            }
                            cookie.domain = elm[1];
                        }
                        else if (elm[0] == "SameSite") {
                            if (cookie.samesite != SameSite::default_mode) {
                                return CookieError::multiple_same_attr;
                            }
                            if (elm[1] == "Strict") {
                                cookie.samesite = SameSite::strict_mode;
                            }
                            else if (elm[1] == "Lax") {
                                cookie.samesite = SameSite::lax_mode;
                            }
                            else if (elm[1] == "None") {
                                cookie.samesite = SameSite::none_mode;
                            }
                            else {
                                return CookieError::unknown_samesite;
                            }
                        }
                        else if (elm[0] == "Max-Age") {
                            if (cookie.maxage != 0) {
                                return CookieError::multiple_same_attr;
                            }
                            commonlib2::Reader(elm[1]) >> cookie.maxage;
                        }
                    }
                }
                return CookieError::none;
            }
        };

    }  // namespace v2
}  // namespace socklib