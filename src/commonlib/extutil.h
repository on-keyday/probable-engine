/*
    commonlib - common utility library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <string>
#include <vector>

#include "basic_helper.h"
#include "extension_operator.h"
#include "project_name.h"
#include "reader.h"

namespace PROJECT_NAME {

    template <class Buf, class Str>
    void getline(Reader<Buf>& r, Str& s, bool ext = true, bool* notext = nullptr) {
        r >> +[](b_char_type<Str> c) { return c != '\r' && c != '\n'; } >> s;
        if (r.expect("\r\n")) {
            if (ext) {
                s.push_back('\r');
                s.push_back('\n');
            }
        }
        else if (r.expect("\r")) {
            if (ext) {
                s.push_back('\r');
            }
        }
        else if (r.expect("\n")) {
            if (ext) {
                s.push_back('\n');
            }
        }
        else {
            if (notext) {
                *notext = true;
            }
        }
    }

    template <class Str = std::string, class Buf>
    Str getline(Reader<Buf>& r, bool ext = true, bool* notext = nullptr) {
        Str ret;
        getline(r, ret, ext, notext);
        return ret;
    }

    template <class Str = std::string, class Buf, class Vec = std::vector<Str>>
    Vec lines(Reader<Buf>& r, bool ext = true) {
        Vec ret;
        while (!r.ceof()) {
            ret.push_back(getline(r, ext));
        }
        return ret;
    }

    template <class Str = std::string, class Vec = std::vector<Str>>
    Vec lines(const Str& str, bool ext = true) {
        Reader<Refer<const Str>> r(str);
        return lines<Str, Refer<const Str>, Vec>(r, ext);
    }

    template <class C, class Buf = std::basic_string<C>, class Vec = std::vector<Buf>>
    Vec lines(const C* str, bool ext = true) {
        return lines<Buf, Vec>(Buf(str), ext);
    }

    template <class Str = std::string, class Buf, class Token = Str, class Vec = std::vector<Str>>
    Vec split(Reader<Buf>& r, const Token& token, size_t n = (size_t)-1, bool needafter = true) {
        Vec ret;
        size_t count = 0;
        while (!r.ceof() && count < n) {
            Str str;
            r.readwhile(str, untilincondition, [&](b_char_type<Buf>) { return !r.ahead(token); });
            if (!r.ceof() && !r.expect(token)) {
                return Vec();
            }
            ret.push_back(std::move(str));
            count++;
        }
        if (needafter && !r.ceof()) {
            Str str;
            r >> do_nothing<b_char_type<Buf>> >> str;
            ret.push_back(std::move(str));
        }
        return ret;
    }

    template <class Str = std::string, class Token = Str, class Vec = std::vector<Str>>
    Vec split(const Str& str, const Token& token, size_t n = (size_t)-1, bool needafter = true) {
        Reader<Refer<const Str>> r(str);
        return split<Str, Refer<const Str>, Token, Vec>(r, token, n);
    }

    template <class C, class Token, class Buf = std::basic_string<std::remove_cv_t<C>>, class Vec = std::vector<Buf>>
    Vec split(C* str, const Token& token, size_t n = (size_t)-1, bool needafter = true) {
        return split<Buf, Token, Vec>(Buf(str), token, n);
    }

    template <class Str = std::string, class Buf, class Vec = std::vector<Str>>
    Vec split_cmd(Reader<Buf>& r, size_t n = (size_t)-1, bool line = false) {
        Vec ret;
        size_t count = 0;
        while (!r.eof() && count < n) {
            Str str;
            int ctx = (int)line;
            r.readwhile(str, cmdline_read, &ctx);
            if (ctx == 0) {
                return Vec();
            }
            ret.push_back(std::move(str));
            count++;
        }
        if (!r.ceof()) {
            Str str;
            r >> do_nothing<b_char_type<Buf>> >> str;
            ret.push_back(std::move(str));
        }
        return ret;
    }

    template <class Str = std::string, class Vec = std::vector<Str>>
    Vec split_cmd(const Str& str, size_t n = (size_t)-1, bool line = false) {
        Reader<Refer<const Str>> r(str, ignore_space);
        return split_cmd<Str, Refer<const Str>, Vec>(r, n, line);
    }

    template <class C, class Buf = std::basic_string<std::remove_cv_t<C>>, class Vec = std::vector<Buf>>
    Vec split_cmd(C* str, size_t n = (size_t)-1, bool line = false) {
        return split_cmd<Buf, Vec>(Buf(str), n, line);
    }

    template <class Vec>
    void remove_strsymbol(Vec& vec) {
        for (auto& v : vec) {
            if (v.size() && is_string_symbol(v[0])) {
                v.erase(0, 1);
                v.pop_back();
            }
        }
    }

    template <class Buf, class Str, class cmp_t = cmpf_t<Buf>, class not_expect_t = nexpf_t<Buf>>
    bool str_eq(Reader<Buf>& r, Str&& token, cmp_t&& cmp = Reader<Buf>::default_cmp, not_expect_t&& nexpt = not_expect_t()) {
        return r.expect(token, nexpt, cmp) && r.eof();
    }

    template <class Buf, class Str, class cmp_t = cmpf_t<Buf>, class not_expect_t = nexpf_t<Buf>>
    bool str_eq(Buf&& base, Str&& token, cmp_t&& cmp = Reader<Buf>::default_cmp, not_expect_t&& nexpt = not_expect_t()) {
        Reader<Buf> r(std::forward<Buf>(base));
        return str_eq(r, token, cmp, nexpt);
    }

    template <class C, class Str, class cmp_t = cmpf_t<C*>, class not_expect_t = nexpf_t<C*>>
    bool str_eq(C* base, Str&& token, cmp_t&& cmp = Reader<C*>::default_cmp, not_expect_t&& nexpt = not_expect_t()) {
        Reader<C*> r(base);
        return str_eq(r, token, cmp, nexpt);
    }

}  // namespace PROJECT_NAME