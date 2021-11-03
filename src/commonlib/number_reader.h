/*
    commonlib - common utility library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once

#include "reader.h"
#include "enumext.h"
#include "char_judge.h"

#if (__cplusplus < 201703L) || (defined(__GNUC__) && __GNUC__ < 11)
#include <string>
#endif
#if __cplusplus >= 201703L
#include <charconv>
#endif

namespace PROJECT_NAME {

    enum class NumberFlag : unsigned int {
        none = 0x0,
        strict_hex = 0x1,
        must_sandwitchdot = 0x2,
        top_no_zero = 0x4,
        only_int = 0x8,

        floatf = 0x1000,
        expf = 0x2000,
        mustfloat = 0x4000,
        flag_reset = ~floatf & ~expf & ~mustfloat
    };

    DEFINE_ENUMOP(NumberFlag)

    template <class Char>
    struct NumberContext {
        Char dotsymbol = (Char)'.';
        NumberFlag flag = NumberFlag::none;
        int radix = 0;
        bool failed = false;
        bool (*judgenum)(Char) = nullptr;
    };

    template <class Char>
    struct NumberContextOld {
        int radix = 0;
        bool floatf = false;
        bool expf = false;
        bool failed = false;
        bool (*judgenum)(Char) = nullptr;
    };

    inline namespace old {
        template <class Buf, class Ret, class Char>
        bool number_old(Reader<Buf> *self, Ret &ret, NumberContextOld<Char> *&ctx, bool begin) {
            if (!ctx)
                return !begin;
            if (!self) {
                return true;
            }
            auto n = self->achar();
            auto increment = [&] {
                ret.push_back(n);
                self->increment();
                n = self->achar();
            };
            if (begin) {
                ctx->expf = false;
                ctx->failed = false;
                ctx->floatf = false;
                ctx->judgenum = nullptr;
                ctx->radix = 0;
                if (is_digit(n)) {
                    if (n != (Char)'0') {
                        ctx->radix = 10;
                        ctx->judgenum = is_digit;
                    }
                    increment();
                    return true;
                }
                else {
                    ctx->failed = true;
                    return false;
                }
            }
            bool proc = true, must = false;
            if (ctx->radix == 0) {
                if (n == (Char)'x' || n == (Char)'X') {
                    ctx->radix = 16;
                    ctx->judgenum = is_hex;
                }
                else if (n == (Char)'b' || n == (Char)'B') {
                    ctx->radix = 2;
                    ctx->judgenum = is_bin;
                }
                else if (is_oct(n)) {
                    ctx->radix = 8;
                    ctx->judgenum = is_oct;
                    proc = false;
                }
                else if (n == (Char)'.') {
                    ctx->floatf = true;
                    ctx->judgenum = is_digit;
                    ctx->radix = 10;
                }
                else {
                    ctx->radix = 10;
                    return true;
                }
                if (proc) {
                    increment();
                    must = true;
                }
            }
            else if (n == (Char)'.') {
                if (ctx->floatf) {
                    ctx->failed = true;
                    return true;
                }
                ctx->floatf = true;
                increment();
                must = true;
            }
            else if ((ctx->radix == 10 && (n == (Char)'e' || n == (Char)'E')) ||
                     (ctx->radix == 16 && (n == (Char)'p' || n == (Char)'P'))) {
                if (ctx->expf) {
                    ctx->failed = true;
                    return true;
                }
                ctx->expf = true;
                ctx->floatf = true;
                increment();
                if (n != (Char)'+' && n != (Char)'-') {
                    ctx->failed = true;
                    return true;
                }
                //ctx->radix=10;
                ctx->judgenum = is_digit<Char>;
                increment();
                must = true;
            }
            if (proc) {
                if (!ctx->judgenum)
                    return true;
                if (!ctx->judgenum(n)) {
                    if (must)
                        ctx->failed = true;
                    return true;
                }
            }
            ret.push_back(n);
            return false;
        }
    }  // namespace old

    template <class Buf, class Ret, class Char>
    bool number(Reader<Buf> *self, Ret &ret, NumberContext<Char> *&ctx, bool begin) {
        if (!ctx)
            return !begin;
        auto flag = [&ctx](NumberFlag in) {
            return any(ctx->flag & in);
        };
        auto not_struct_hex = [&flag, &ctx] {
            return flag(NumberFlag::strict_hex) && ctx->radix == 16 && !flag(NumberFlag::expf);
        };
        auto must_but_not_float = [&flag, &ctx] {
            return flag(NumberFlag::mustfloat) && !flag(NumberFlag::floatf);
        };
        if (!self) {
            if (must_but_not_float()) {
                ctx->failed = true;
            }
            if (not_struct_hex()) {
                ctx->failed = true;
            }
            return true;
        }
        auto n = self->achar();
        auto increment = [&] {
            ret.push_back(n);
            self->increment();
            n = self->achar();
        };
        if (begin) {
            //ctx->expf=false;
            ctx->failed = false;
            //ctx->floatf=false;
            ctx->judgenum = nullptr;
            ctx->radix = 0;
            //ctx->mustfloat=false;
            ctx->flag &= NumberFlag::flag_reset;
            return true;
        }
        bool must = false;
        if (ctx->radix == 0) {
            if (self->expect("0x") || self->expect("0X")) {
                ctx->radix = 16;
                ctx->judgenum = is_hex;
                ret.push_back((Char)'0');
                ret.push_back((Char)self->offset(-1));
                n = self->achar();
                if (n == ctx->dotsymbol) {
                    if (flag(NumberFlag::must_sandwitchdot) || flag(NumberFlag::only_int)) {
                        ctx->failed = true;
                        return true;
                    }
                    increment();
                    ctx->flag |= NumberFlag::floatf;
                }
            }
            else if (self->expect("0b") || self->expect("0B")) {
                ctx->radix = 2;
                ctx->judgenum = is_bin;
                ret.push_back((Char)'0');
                ret.push_back(self->offset(-1));
                n = self->achar();
            }
            else if (self->expect("0", [](Char c) { return !is_oct(c); })) {
                ctx->radix = 8;
                ctx->judgenum = is_oct;
                ret.push_back((Char)'0');
                n = self->achar();
            }
            else if (self->expect("0", [](Char c) { return !is_digit(c); })) {
                if (flag(NumberFlag::top_no_zero) || flag(NumberFlag::only_int)) {
                    ctx->failed = true;
                    return true;
                }
                ctx->flag |= NumberFlag::mustfloat;
                ctx->radix = 10;
                ctx->judgenum = is_digit;
                ret.push_back((Char)'0');
                n = self->achar();
            }
            else if (n == ctx->dotsymbol) {
                if (flag(NumberFlag::must_sandwitchdot) || flag(NumberFlag::only_int)) {
                    ctx->failed = true;
                    return true;
                }
                ctx->radix = 10;
                ctx->judgenum = is_digit;
                ctx->flag |= NumberFlag::floatf;
                increment();
                if (!is_digit(n)) {
                    ctx->failed = true;
                    return true;
                }
            }
            else if (is_digit(n)) {
                ctx->radix = 10;
                ctx->judgenum = is_digit;
            }
            else {
                ctx->failed = true;
                return true;
            }
            must = true;
        }
        bool dot = false;
        if (!flag(NumberFlag::only_int)) {
            if (n == ctx->dotsymbol) {
                if (flag(NumberFlag::floatf)) {
                    ctx->failed = true;
                    return true;
                }
                ctx->flag |= NumberFlag::floatf;
                if (ctx->radix == 8) {
                    if (flag(NumberFlag::top_no_zero)) {
                        ctx->failed = true;
                        return true;
                    }
                    ctx->radix = 10;
                }
                increment();
                dot = true;
            }
            if ((
                    (ctx->radix == 10 || ctx->radix == 8) && (n == (Char)'e' || n == (Char)'E')) ||
                (ctx->radix == 16 && (n == (Char)'p' || n == (Char)'P'))) {
                if (flag(NumberFlag::expf)) {
                    ctx->failed = true;
                    return true;
                }
                if (flag(NumberFlag::must_sandwitchdot) && dot) {
                    ctx->failed = true;
                    return true;
                }
                ctx->flag |= NumberFlag::expf | NumberFlag::floatf;
                increment();
                if (n != (Char)'+' && n != (Char)'-') {
                    ctx->failed = true;
                    return true;
                }
                ctx->judgenum = is_digit;
                if (ctx->radix == 8) {
                    if (flag(NumberFlag::top_no_zero)) {
                        ctx->failed = true;
                        return true;
                    }
                    ctx->radix = 10;
                }
                increment();
                must = true;
            }
        }
        if (!ctx->judgenum(n)) {
            if (ctx->radix == 8 && is_digit(n)) {
                if (flag(NumberFlag::top_no_zero)) {
                    ctx->failed = true;
                    return true;
                }
                ctx->flag |= NumberFlag::mustfloat;
                ctx->judgenum = is_digit;
            }
            else {
                if (must)
                    ctx->failed = true;
                if (must_but_not_float())
                    ctx->failed = true;
                if (not_struct_hex())
                    ctx->failed = true;
                if (flag(NumberFlag::must_sandwitchdot) && dot)
                    ctx->failed = true;
                return true;
            }
        }
        ret.push_back(n);
        return false;
    }

}  // namespace PROJECT_NAME

namespace PROJECT_NAME {
    template <class N>
    constexpr N msb_on() {
        return static_cast<N>(static_cast<N>(1) << (sizeof(N) * 8 - 1));
    }

    template <class N>
    constexpr bool is_unsigned() {
        return msb_on<N>() > 0;
    }

    template <class N>
    constexpr N maxof() {
        return is_unsigned<N>() ? static_cast<N>(~0) : ~msb_on<N>();
    }

    template <class N>
    constexpr N minof() {
        return is_unsigned<N>() ? 0 : msb_on<N>();
    }

    constexpr unsigned long long maxvalue(int size, bool sign = false) {
        switch (size) {
            case 1:
                return sign ? maxof<char>() : maxof<unsigned char>();
            case 2:
                return sign ? maxof<short>() : maxof<unsigned short>();
            case 4:
                return sign ? maxof<int>() : maxof<unsigned int>();
            case 8:
                return sign ? maxof<long long>() : maxof<unsigned long long>();
        }
        return static_cast<unsigned long long>(-1);
    }

    constexpr int need_bytes(unsigned long long s, bool sign) {
        if (sign) {
            if (s > (unsigned long long)maxof<long long>()) {
                return -1;
            }
            else if (s > (unsigned long long)maxof<int>()) {
                return 8;
            }
            else if (s > (unsigned long long)maxof<short>()) {
                return 4;
            }
            else if (s > (unsigned long long)maxof<char>()) {
                return 2;
            }
            else {
                return 1;
            }
        }
        else {
            if (s > maxof<unsigned int>()) {
                return 8;
            }
            else if (s > maxof<unsigned short>()) {
                return 4;
            }
            else if (s > maxof<unsigned char>()) {
                return 2;
            }
            else {
                return 1;
            }
        }
    }

    template <class N>
    bool parse_int(const char *be, const char *ed, N &n, int radix = 10) {
#if __cplusplus < 201703L
        try {
            size_t idx = 0;
            unsigned long long t = stoull(std::string(be, ed), &idx, radix);
            if (static_cast<size_t>(ed - be) != idx) {
                return false;
            }
            if (need_bytes(t, !is_unsigned<N>()) > sizeof(N))
                return false;
            n = (N)t;
        } catch (...) {
            return false;
        }
#else
        auto res = std::from_chars(be, ed, n, radix);
        if (res.ec != std::errc{}) {
            return false;
        }
        if (res.ptr != ed) {
            return false;
        }
#endif
        return true;
    }

    template <class N>
    bool parse_float(const char *be, const char *ed, N &n, bool hex = false) {
#if (__cplusplus < 201703L) || (defined(__GNUC__) && __GNUC__ < 11)
        try {
            size_t idx = 0;
            n = (N)std::stod(std::string(be, ed), &idx);
            if (static_cast<size_t>(ed - be) != idx) {
                return false;
            }
        } catch (...) {
            return false;
        }
#else
        auto res = std::from_chars(be, ed, n, hex ? std::chars_format::hex : std::chars_format::general);
        if (res.ec != std::errc{}) {
            return false;
        }
        if (res.ptr != ed) {
            return false;
        }
#endif
        return true;
    }

    template <class Str, class N>
    bool parse_int(const Str &str, N &n, int radix = 10) {
        return parse_int(str.data(), &str.data()[str.size()], n, radix);
    }

    template <class Str, class N>
    bool parse_float(const Str &str, N &n, bool hex = false) {
        return parse_float(str.data(), &str.data()[str.size()], n, hex);
    }

}  // namespace PROJECT_NAME
