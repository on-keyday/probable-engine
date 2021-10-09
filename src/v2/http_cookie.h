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

            invalid = 0xff,
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
            dec,

            invalid = 0xff,
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

        constexpr Date get_invalid_date() {
            return Date{
                .dayname = DayName::invalid,
                .day = 0xff,
                .month = Month::invalid,
                .hour = 0xff,
                .minute = 0xff,
                .second = 0xff,
                .year = 0xffff};
        }

        constexpr auto invalid_date = get_invalid_date();

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
            no_time,
            no_hour,
            not_hour,
            no_minute,
            not_minute,
            no_second,
            not_second,
            not_GMT,
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
                        toset = E(i);
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

            static DateErr set_time(Date& date, string_t& src) {
                auto check_size = [](auto& src) {
                    return src.size() == 2;
                };
                auto time = commonlib2::split<string_t, const char*, strvec_t>(src, ":");
                if (time.size() != 3) {
                    return DateError::no_time;
                }
                date.hour = 0;
                date.minute = 0;
                date.second = 0;
                if (!check_size(time[0])) {
                    return DateError::no_hour;
                }
                if (!set_two(date.hour, time[0]) || date.hour > 23) {
                    return DateError::not_hour;
                }
                if (!check_size(time[1])) {
                    return DateError::no_minute;
                }
                if (!set_two(date.minute, time[1]) || date.minute > 59) {
                    return DateError::not_minute;
                }
                if (!check_size(time[2])) {
                    return DateError::no_second;
                }
                if (!set_two(date.second, time[2]) || date.second > 61) {
                    return DateError::not_second;
                }
                return true;
            }

           public:
            static void replace_to_parse(string_t& raw) {
                for (auto& i : raw) {
                    if (i == '-') {
                        i = ' ';
                    }
                }
            }

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
                if (!get_expected<DayName, 7>(date.dayname, data[0], get_dayname)) {
                    return DateError::not_dayname;
                }
                if (!check_size(data[1], 2)) {
                    return DateError::no_day;
                }
                date.day = 0;
                if (!set_two(date.day, data[1]) || date.day < 1 || date.day > 31) {
                    return DateError::not_day;
                }
                if (!get_expected<Month, 12>(date.month, data[2], get_month)) {
                    return DateError::not_month;
                }
                if (!check_size(data[3], 4)) {
                    return DateError::no_day;
                }
                date.year = 0;
                if (!set_two(date.year, data[3])) {
                    return DateError::not_year;
                }
                date.year *= 100;
                if (!set_two(date.year, data[3], 2) || date.year > 9999) {
                    return DateError::not_year;
                }
                if (auto e = set_time(date, data[4]); !e) {
                    return e;
                }
                if (data[5] != "GMT") {
                    return DateError::not_GMT;
                }
                return true;
            }
#ifndef _WIN32
#define gettime_s(tm, time) (*tm = gmtime(time))
#endif
        };

        struct UTCLocalDiff {
           private:
            static time_t diff_impl() {
                auto calc_diff = [] {
                    time_t now_local = time(nullptr), now_utc = 0;
                    ::tm tminfo = {0};
                    gmtime_s(&tminfo, &now_local);
                    now_utc = mktime(&tminfo);
                    return now_local - now_utc;
                };
                /*time_t first = calc_diff(), second = 0;
                while (true) {
                    second = calc_diff();
                    if (first == second) {
                        return first;
                    }
                    first = second;
                }*/
                return calc_diff();
            }

           public:
            static time_t diff() {
                static time_t diff_v = diff_impl();
                return diff_v;
            }
        };

        template <class String>
        struct DateWriter {
            using string_t = String;

           private:
            static void set_two(std::uint32_t src, string_t& towrite) {
                auto high = src / 10;
                auto low = src - high * 10;
                towrite += high + '0';
                towrite += low + '0';
            }

           public:
            static DateErr write(string_t& towrite, const Date& date) {
                if (date.dayname == DayName::unset || date.dayname == DayName::invalid) {
                    return DateError::not_dayname;
                }
                towrite += get_dayname(date.dayname);
                towrite += ", ";
                if (date.day < 1 || date.day > 31) {
                    return DateError::not_day;
                }
                set_two(date.day, towrite);
                towrite += ' ';
                if (date.month == Month::unset || date.month == Month::invalid) {
                    return DateError::not_month;
                }
                towrite += get_month(date.month);
                towrite += ' ';
                if (date.year > 9999) {
                    return DateError::not_year;
                }
                set_two(date.year / 100, towrite);
                set_two(date.year - (date.year / 100) * 100, towrite);
                towrite += ' ';
                if (date.hour > 23) {
                    return DateError::not_hour;
                }
                set_two(date.hour, towrite);
                towrite += ':';
                if (date.minute > 59) {
                    return DateError::not_minute;
                }
                set_two(date.minute, towrite);
                towrite += ':';
                if (date.second > 61) {
                    return DateError::not_minute;
                }
                set_two(date.second, towrite);
                towrite += " GMT";
                return true;
            }
        };

        struct TimeConvert {
            static DateErr from_time_t(time_t time, Date& date) {
                if (time < 0) {
                    return DateError::not_date;
                }
                ::tm tminfo = {0};
                gmtime_s(&tminfo, &time);
                date.dayname = DayName(1 + tminfo.tm_wday);
                date.day = (std::uint8_t)tminfo.tm_mday;
                date.month = Month(1 + tminfo.tm_mon);
                date.year = (std::uint16_t)tminfo.tm_year + 1900;
                date.hour = (std::uint8_t)tminfo.tm_hour;
                date.minute = (std::uint8_t)tminfo.tm_min;
                date.second = (std::uint8_t)tminfo.tm_sec;
                return true;
            }

            static DateErr to_time_t(time_t& time, const Date& date) {
                ::tm tminfo = {0};
                if (date.year < 1900) {
                    return DateError::not_year;
                }
                tminfo.tm_year = date.year - 1900;
                if (date.month == Month::unset) {
                    return DateError::not_month;
                }
                tminfo.tm_mon = int(date.month) - 1;
                if (date.day < 1 || date.day > 31) {
                    return DateError::not_day;
                }
                tminfo.tm_mday = date.day;
                if (date.hour > 23) {
                    return DateError::not_hour;
                }
                tminfo.tm_hour = date.hour;
                if (date.minute > 59) {
                    return DateError::not_minute;
                }
                tminfo.tm_min = date.minute;
                if (date.second > 61) {
                    return DateError::not_second;
                }
                tminfo.tm_sec = date.second;
                time = mktime(&tminfo);
                if (time == -1) {
                    return DateError::not_date;
                }
                time += UTCLocalDiff::diff();
                return true;
            }
        };

        bool operator==(const Date& a, const Date& b) {
            union {
                Date d;
                std::uint64_t l;
            } a_c{a}, b_c{b};
            return a_c.l == b_c.l;
        }

        time_t operator-(const Date& a, const Date& b) {
            time_t a_t = 0, b_t = 0;
            TimeConvert::to_time_t(a_t, a);
            TimeConvert::to_time_t(b_t, b);
            return a_t - b_t;
        }

        enum class CookieFlag {
            none,
            path_set = 0x1,
            domain_set = 0x2,
            not_allow_prefix_rule = 0x4,
        };

        DEFINE_ENUMOP(CookieFlag);

        template <class String>
        struct Cookie {
            using string_t = String;
            string_t name;
            string_t value;

            string_t path;
            string_t domain;
            Date expires;

            int maxage = 0;
            bool secure = false;
            bool httponly = false;
            SameSite samesite = SameSite::default_mode;

            CookieFlag flag = CookieFlag::none;
        };

        enum class CookieError {
            none,
            no_cookie,
            no_keyvalue,
            no_attrkeyvalue,
            multiple_same_attr,
            unknown_samesite,
            invalid_url,
        };

        using CookieErr = commonlib2::EnumWrap<CookieError, CookieError::none, CookieError::no_cookie>;

        template <class String, template <class...> class Vec>
        struct CookieParser {
            using string_t = String;
            using cookie_t = Cookie<String>;
            using cookies_t = Vec<cookie_t>;
            using util_t = HttpUtil<String>;
            using strvec_t = Vec<string_t>;
            using url_t = commonlib2::URLContext<string_t>;

            static void set_by_cookie_prefix(cookie_t& cookie) {
                if (commonlib2::Reader<string_t&>(cookie.name).ahead("__Secure-")) {
                    cookie.secure = true;
                }
                else if (commonlib2::Reader<string_t&>(cookie.name).ahead("__Host-")) {
                    cookie.flag &= ~CookieFlag::domain_set;
                    cookie.path = "/";
                }
            }

            static bool verify_cookie_prefix(cookie_t& cookie) {
                if (commonlib2::Reader<string_t&>(cookie.name).ahead("__Secure-")) {
                    if (!cookie.secure) {
                        cookie.flag |= CookieFlag::not_allow_prefix_rule;
                    }
                }
                else if (commonlib2::Reader<string_t&>(cookie.name).ahead("__Host-")) {
                    if (cookie.path != "/" || any(cookie.flag & CookieFlag::domain_set)) {
                        cookie.flag |= CookieFlag::not_allow_prefix_rule;
                    }
                }
                return any(cookie.flag & CookieFlag::not_allow_prefix_rule);
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

            static CookieErr parse(const string_t& raw, cookie_t& cookie, const url_t& url) {
                if (!url.host.size() || !url.path.size()) {
                    return CookieError::invalid_url;
                }
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
                        return str_eq(a, b, util_t::header_cmp);
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
                            if (cookie.expires != Date{}) {
                                return CookieError::multiple_same_attr;
                            }
                            DateParser<string_t, Vec>::replace_to_parse(elm[1]);
                            if (!DateParser<string_t, Vec>::parse(elm[1], cookie.expires)) {
                                cookie.expires = invalid_date;
                            }
                        }
                        else if (cmp(elm[0], "Path")) {
                            if (cookie.path.size()) {
                                return CookieError::multiple_same_attr;
                            }
                            cookie.path = elm[1];
                            cookie.flag |= CookieFlag::path_set;
                        }
                        else if (cmp(elm[0], "Domain")) {
                            if (cookie.domain.size()) {
                                return CookieError::multiple_same_attr;
                            }
                            cookie.domain = elm[1];
                            cookie.flag |= CookieFlag::domain_set;
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
                if (!any(cookie.flag & CookieFlag::path_set)) {
                    cookie.path = url.path;
                }
                if (!any(cookie.flag & CookieFlag::domain_set)) {
                    cookie.domain = url.host;
                }
                return CookieError::none;
            }

            template <class Header>
            static CookieErr parse_set_cookie(Header& header, cookies_t& cookies, const url_t& url) {
                for (auto& h : header) {
                    if (header_cmp(h.first, "Set-Cookie")) {
                        cookie_t cookie;
                        auto err = parse(h.second, cookie, url);
                        if (!err) return err;
                        cookies.push_back(std::move(cookie));
                    }
                }
                return true;
            }
        };

        enum class PathDomainState {
            none,
            same_path = 0x01,
            sub_path = 0x02,
            same_origin = 0x10,
            sub_origin = 0x20,
        };

        DEFINE_ENUMOP(PathDomainState)

        template <class String>
        struct CookieWriter {
            using string_t = String;
            using cookie_t = Cookie<string_t>;

           private:
            static bool check_path_and_domain(cookie_t& info, cookie_t& cookie) {
                if (any(cookie.flag & CookieFlag::domain_set)) {
                    if (info.domain.find(cookie.domain) == ~0) {
                        return false;
                    }
                }
                else {
                    if (info.domain != cookie.domain) {
                        return false;
                    }
                }
                if (any(cookie.flag & CookieFlag::path_set)) {
                    if (info.path.find(cookie.path) != 0) {
                        return false;
                    }
                }
                else {
                    if (info.path != cookie.path) {
                        return false;
                    }
                }
                return true;
            }

            static bool check_expires(cookie_t& info, cookie_t& cookie) {
                time_t now = ::time(nullptr), prevtime = 0;
                Date nowdate;
                TimeConvert::from_time_t(now, nowdate);
                TimeConvert::to_time_t(prevtime, info.expires);
                if (cookie.maxage) {
                    if (cookie.maxage <= 0) {
                        return false;
                    }
                    if (prevtime + cookie.maxage < now) {
                        return false;
                    }
                }
                else if (cookie.expires != Date{} && cookie.expires != invalid_date) {
                    if (cookie.expires - nowdate <= 0) {
                        return false;
                    }
                }
                return true;
            }

           public:
            template <class Cookies>
            static bool write(string_t& towrite, cookie_t& info, Cookies& cookies) {
                if (!info.domain.size() || !info.path.size() || info.expires == Date{} || info.expires == invalid_date) {
                    return false;
                }
                for (size_t i = 0; i < cookies.size(); i++) {
                    auto del_cookie = [&] {
                        cookies.erase(cookies.begin() + i);
                        i--;
                    };
                    cookie_t& cookie = cookies[i];
                    if (cookie.secure && !info.secure) {
                        continue;
                    }
                    if (!check_expires(info, cookie)) {
                        del_cookie();
                        continue;
                    }
                    if (!check_path_and_domain(info, cookie)) {
                        continue;
                    }
                    if (towrite.size()) {
                        towrite += "; ";
                    }
                    towrite += cookie.name;
                    towrite += "=";
                    towrite += cookie.value;
                }
                return towrite.size() != 0;
            }
        };

    }  // namespace v2
}  // namespace socklib
