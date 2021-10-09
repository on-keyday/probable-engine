/*
    socklib - simple socket library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once

#include "http_base.h"

#include <extutil.h>

namespace socklib {
    namespace v2 {

        enum class SameSite {
            default_mode,
            lax_mode,
            strict_mode,
            none_mode,
        };

        BEGIN_ENUM_STRING_MSG(SameSite, get_samesite)
        ENUM_STRING_MSG(SameSite::lax_mode, "Lax")
        ENUM_STRING_MSG(SameSite::strict_mode, "Strict")
        ENUM_STRING_MSG(SameSite::none_mode, "None")
        END_ENUM_STRING_MSG("")

        enum class DayName : std::uint8_t {
            unset,
            sun,
            mon,
            tue,
            wed,
            thu,
            fri,
            sat,
        };

        BEGIN_ENUM_STRING_MSG(DayName, get_dayname)
        ENUM_STRING_MSG(DayName::sun, "Sun")
        ENUM_STRING_MSG(DayName::mon, "Mon")
        ENUM_STRING_MSG(DayName::tue, "Tue")
        ENUM_STRING_MSG(DayName::wed, "Wed")
        ENUM_STRING_MSG(DayName::thu, "Thu")
        ENUM_STRING_MSG(DayName::fri, "Fri")
        ENUM_STRING_MSG(DayName::sat, "Sat")
        END_ENUM_STRING_MSG("")

        enum class Month : std::uint8_t {
            unset,
            jan,
            feb,
            mar,
            apr,
            may,
            jun,
            jul,
            aug,
            sep,
            oct,
            nov,
            dec
        };

        BEGIN_ENUM_STRING_MSG(Month, get_month)
        ENUM_STRING_MSG(Month::jan, "Jan")
        ENUM_STRING_MSG(Month::feb, "Feb")
        ENUM_STRING_MSG(Month::mar, "Mar")
        ENUM_STRING_MSG(Month::apr, "Apr")
        ENUM_STRING_MSG(Month::may, "May")
        ENUM_STRING_MSG(Month::jun, "Jun")
        ENUM_STRING_MSG(Month::jul, "Jul")
        ENUM_STRING_MSG(Month::aug, "Aug")
        ENUM_STRING_MSG(Month::sep, "Sep")
        ENUM_STRING_MSG(Month::oct, "Oct")
        ENUM_STRING_MSG(Month::nov, "Nov")
        ENUM_STRING_MSG(Month::dec, "Dec")
        END_ENUM_STRING_MSG("")

        struct Date {
            DayName dayname = DayName::unset;
            std::uint8_t day = 0;
            Month month = Month::unset;
            std::uint8_t hour = 0;
            std::uint8_t minute = 0;
            std::uint8_t second = 0;
            std::uint16_t year = 0;
        };

        enum class DateError {
            none,
            not_date,
            no_dayname,
            not_dayname,
            no_day,
            not_day,
            not_month,
            no_year,
            not_year,
        };

        using DateErr = commonlib2::EnumWrap<DateError, DateError::none, DateError::not_date>;

        template <class String, template <class...> class Vec>
        struct DateParser {
            using string_t = String;
            using strvec_t = Vec<string_t>;

           private:
            template <class E, int max, class F>
            static bool get_expected(E& toset, string_t& src, F& f) {
                commonlib2::Reader r(src);
                for (auto i = 1; i <= max; i++) {
                    if (r.ahead(f(E(i)))) {
                        toset = DayName(i);
                        return true;
                    }
                }
                return false;
            }

            static bool set_two(auto& toset, auto& src, int offset = 0) {
                auto cast_to_num = [](unsigned char c) {
                    return c - '0';
                };
                auto high = cast_to_num(src[offset]);
                auto low = cast_to_num(src[offset + 1]);
                if (high < 0 || high > 9 || low < 0 || low > 9) {
                    return false;
                }
                toset += high * 10 + low;
                return true;
            };

           public:
            static DateErr parse(const string_t& raw, Date& date) {
                auto data = commonlib2::split<string_t, const char*, strvec_t>(raw, " ");
                auto check_size = [](auto& src, size_t expect) {
                    return src.size() == expect;
                };
                if (data.size() != 6) {
                    return DateError::not_date;
                }
                if (!check_size(data[0], 4) || data[0][data[0].size() - 1] != ',') {
                    return DateError::no_dayname;
                }
                if (get_expected(date.dayname, data[0], get_dayname)) {
                    return DateError::not_dayname;
                }
                if (!check_size(data[1], 2)) {
                    return DateError::no_day;
                }
                date.day = 0;
                if (!set_two(date.day, data[1])) {
                    return DateError::not_day;
                }
                if (get_expected(date.month, data[2], get_month)) {
                    return DateError::not_month;
                }
                if (!check_size(data[3], 4)) {
                    return DateError::no_day;
                }
                date.year = 0;
                if (!set_two(date.year)) {
                    return DateError::not_year;
                }
                date.year *= 100;
                if (!set_two(date.year, 2)) {
                    return DateError::not_year;
                }
            }
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

            bool not_allowed_prefix_rule = false;
        };

        enum class CookieError {
            none,
            no_cookie,
            no_keyvalue,
            no_attrkeyvalue,
            multiple_same_attr,
            unknown_samesite,
        };

        using CookieErr = commonlib2::EnumWrap<CookieError, CookieError::none, CookieError::no_cookie>;

        template <class String, class Header, class Body, template <class...> class Vec>
        struct CookieParser {
            using string_t = String;
            using cookie_t = Cookie<String>;
            using cookies_t = Vec<cookie_t>;
            using base_t = HttpBase<String, Header, Body>;
            using strvec_t = Vec<string_t>;

            static void set_by_cookie_prefix(cookie_t& cookie) {
                if (commonlib2::Reader(cookie.name).ahead("__Secure-")) {
                    cookie.secure = true;
                }
                else if (commonlib2::Reader(cookie.name).ahead("__Host-")) {
                    cookie.domain.clear();
                    cookie.path = "/";
                }
            }

            static bool verify_cookie_prefix(cookie_t& cookie) {
                if (commonlib2::Reader(cookie.name).ahead("__Secure-")) {
                    if (!cookie.secure) {
                        cookie.not_allowed_prefix_rule = true;
                    }
                }
                else if (commonlib2::Reader(cookie.name).ahead("__Host-")) {
                    if (cookie.path != "/" || cookie.domain.size()) {
                        cookie.not_allowed_prefix_rule = true;
                    }
                }
                return !cookie.not_allowed_prefix_rule;
            }

            static void write_set_cookie(string_t& towrite, cookie_t& cookie) {
                towrite = cookie.name;
                towrite += "=";
                towrite += cookie.value;
            }

            static CookieErr parse(const string_t& raw, cookies_t& cookies) {
                auto data = commonlib2::split<string_t, const char*, strvec_t>(raw, "; ");
                for (auto& v : data) {
                    auto keyval = commonlib2::split<string_t, const char*, strvec_t>(v, "=", 1);
                    if (keyval.size() != 2) {
                        return CookieError::no_keyvalue;
                    }
                    auto cookie = cookie_t{.name = keyval[0], .value = keyval[1]};
                    set_by_cookie_prefix(cookie);
                    cookies.push_back(cookie);
                }
                return true;
            }

            static CookieErr parse(const string_t& raw, cookie_t& cookie) {
                using commonlib2::str_eq;
                auto data = commonlib2::split<string_t, const char*, strvec_t>(raw, "; ");
                if (data.size() == 0) {
                    return CookieError::no_cookie;
                }
                auto keyval = commonlib2::split<string_t, const char*, strvec_t>(data[0], "=", 1);
                if (keyval.size() != 2) {
                    return CookieError::no_keyvalue;
                }

                cookie.name = keyval[0];
                cookie.value = keyval[1];
                for (auto& attr : data) {
                    auto cmp = [](auto& a, auto& b) {
                        return str_eq(a, b, base_t::header_cmp);
                    };
                    if (cmp(attr, "HttpOnly")) {
                        if (cookie.httponly) {
                            return CookieError::multiple_same_attr;
                        }
                        cookie.httponly = true;
                    }
                    else if (cmp(attr, "Secure")) {
                        if (cookie.secure) {
                            return CookieError::multiple_same_attr;
                        }
                        cookie.secure = true;
                    }
                    else {
                        auto elm = commonlib2::split<string_t, const char*, strvec_t>(attr, "=", 1);
                        if (elm.size() != 2) {
                            return CookieError::no_attrkeyvalue;
                        }
                        if (cmp(elm[0], "Expires")) {
                            if (cookie.expires.size()) {
                                return CookieError::multiple_same_attr;
                            }
                            cookie.expires = elm[1];
                        }
                        else if (cmp(elm[0], "Path")) {
                            if (cookie.path.size()) {
                                return CookieError::multiple_same_attr;
                            }
                            cookie.path = elm[1];
                        }
                        else if (cmp(elm[0], "Domain")) {
                            if (cookie.domain.size()) {
                                return CookieError::multiple_same_attr;
                            }
                            cookie.domain = elm[1];
                        }
                        else if (cmp(elm[0], "SameSite")) {
                            if (cookie.samesite != SameSite::default_mode) {
                                return CookieError::multiple_same_attr;
                            }
                            if (cmp(elm[1], "Strict")) {
                                cookie.samesite = SameSite::strict_mode;
                            }
                            else if (cmp(elm[1], "Lax")) {
                                cookie.samesite = SameSite::lax_mode;
                            }
                            else if (cmp(elm[1], "None")) {
                                cookie.samesite = SameSite::none_mode;
                            }
                            else {
                                return CookieError::unknown_samesite;
                            }
                        }
                        else if (cmp(elm[0], "Max-Age")) {
                            if (cookie.maxage != 0) {
                                return CookieError::multiple_same_attr;
                            }
                            commonlib2::Reader(elm[1]) >> cookie.maxage;
                        }
                    }
                }
                verify_cookie_prefix(cookie);
                return CookieError::none;
            }
        };

    }  // namespace v2
}  // namespace socklib