/*
    Copyright (c) 2021 on-keyday
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <cstring>
#include <memory>

#include "reader.h"

namespace PROJECT_NAME {

#define DEFINE_ENUMOP(TYPE)                              \
    constexpr TYPE operator&(TYPE a, TYPE b) {           \
        using basety = std::underlying_type_t<TYPE>;     \
        return static_cast<TYPE>((basety)a & (basety)b); \
    }                                                    \
    constexpr TYPE operator|(TYPE a, TYPE b) {           \
        using basety = std::underlying_type_t<TYPE>;     \
        return static_cast<TYPE>((basety)a | (basety)b); \
    }                                                    \
    constexpr TYPE operator~(TYPE a) {                   \
        using basety = std::underlying_type_t<TYPE>;     \
        return static_cast<TYPE>(~(basety)a);            \
    }                                                    \
    constexpr TYPE operator^(TYPE a, TYPE b) {           \
        using basety = std::underlying_type_t<TYPE>;     \
        return static_cast<TYPE>((basety)a ^ (basety)b); \
    }                                                    \
    constexpr TYPE &operator&=(TYPE &a, TYPE b) {        \
        a = a & b;                                       \
        return a;                                        \
    }                                                    \
    constexpr TYPE &operator|=(TYPE &a, TYPE b) {        \
        a = a | b;                                       \
        return a;                                        \
    }                                                    \
    constexpr TYPE &operator^=(TYPE &a, TYPE b) {        \
        a = a ^ b;                                       \
        return a;                                        \
    }                                                    \
    constexpr bool any(TYPE a) {                         \
        using basety = std::underlying_type_t<TYPE>;     \
        return static_cast<basety>(a) != 0;              \
    }

    template <class Buf>
    struct Refer {
       private:
        Buf &buf;

       public:
        Buf *operator->() {
            return std::addressof(buf);
        }

        decltype(buf[0]) operator[](size_t s) const {
            return buf[s];
        }

        decltype(buf[0]) operator[](size_t s) {
            return buf[s];
        }

        size_t size() const {
            return buf.size();
        }
        Refer(const Refer &in)
            : buf(in.buf) {}
        Refer(Refer &&in) noexcept
            : buf(in.buf) {}
        Refer(Buf &in)
            : buf(in) {}
    };

    template <class C>
    struct Sized {
        C *ptr = nullptr;
        size_t _size = 0;
        constexpr Sized(C *inptr, size_t sz)
            : ptr(inptr), _size(sz) {}

        template <size_t sz>
        constexpr Sized(C (&inptr)[sz])
            : ptr((C *)inptr), _size(sz) {}

        constexpr C operator[](size_t pos) const {
            if (pos >= _size)
                return C();
            return ptr[pos];
        }

        constexpr size_t size() const {
            return _size;
        }
    };

    template <class T>
    struct Ref {
       private:
        T &t;

       public:
        T *operator->() {
            return std::addressof(t);
        }
        T &operator*() {
            return t;
        }
        Ref(const Ref &in)
            : t(in.t) {}
        Ref(Ref &&in) noexcept
            : t(in.t) {}
        Ref(T &in)
            : t(in) {}
    };

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

    struct LinePosContext {
        size_t line = 0;
        size_t pos = 0;
        size_t nowpos = 0;
        bool crlf = false;
        bool cr = false;
    };

    template <class Char>
    bool is_digit(Char c) {
        return c >= (Char)'0' && c <= (Char)'9';
    }

    template <class Char>
    bool is_oct(Char c) {
        return c >= (Char)'0' && c <= (Char)'7';
    }

    template <class Char>
    bool is_hex(Char c) {
        return c >= (Char)'a' && c <= (Char)'f' ||
               c >= (Char)'A' && c <= (Char)'F' ||
               is_digit(c);
    }

    template <class Char>
    bool is_bin(Char c) {
        return c == (Char)'0' || c == (Char)'1';
    }

    template <class Char>
    bool is_alphabet(Char c) {
        return c >= (Char)'a' && c <= (Char)'z' ||
               c >= (Char)'A' && c <= (Char)'Z';
    }

    template <class Char>
    bool is_string_symbol(Char c) {
        return c == (Char)'\"' || c == (Char)'\'' || c == (Char)'`';
    }

    template <class Char>
    bool is_alpha_or_num(Char c) {
        return is_alphabet(c) || is_digit(c);
    }

    template <class Char>
    bool is_c_id_usable(Char c) {
        return is_alpha_or_num(c) || c == (Char)'_';
    }

    template <class Char>
    bool is_c_id_top_usable(Char c) {
        return is_alphabet(c) || c == (Char)'_';
    }

    template <class Char>
    bool is_url_host_usable(Char c) {
        return is_alpha_or_num(c) || c == (Char)'-' || c == (Char)'.';
    }

    template <class Char>
    bool is_url_scheme_usable(Char c) {
        return is_alpha_or_num(c) || c == (Char)'+';
    }

    template <class Char>
    bool do_nothing(Char) {
        return true;
    }

    template <class Buf, class Ctx>
    bool depthblock(Reader<Buf> *self, Ctx &check, bool begin) {
        if (!self || !begin)
            return true;
        if (!check(self, true))
            return false;
        size_t dp = 0;
        bool ok = false;
        while (!self->eof()) {
            if (check(self, true)) {
                dp++;
            }
            if (check(self, false)) {
                if (dp == 0) {
                    ok = true;
                    break;
                }
                dp--;
            }
        }
        return ok;
    }

    template <class Buf, class Ret, class Ctx>
    bool depthblock_withbuf(Reader<Buf> *self, Ret &ret, Ctx &check, bool begin) {
        if (!self || !begin)
            return true;
        if (!check(self, true))
            return false;
        size_t beginpos = self->readpos(), endpos = beginpos;
        size_t dp = 0;
        bool ok = false;
        while (!self->eof()) {
            endpos = self->readpos();
            if (check(self, true)) {
                dp++;
            }
            if (check(self, false)) {
                if (dp == 0) {
                    ok = true;
                    break;
                }
                dp--;
            }
            self->increment();
        }
        if (ok) {
            auto finalpos = self->readpos();
            self->seek(beginpos);
            while (self->readpos() != endpos) {
                ret.push_back(self->achar());
                self->increment();
            }
            self->seek(finalpos);
        }
        return ok;
    }

    template <class Buf>
    struct BasicBlock {
        bool c_block = true;
        bool arr_block = true;
        bool bracket = true;
        bool operator()(Reader<Buf> *self, bool begin) {
            bool ret = false;
            if (c_block) {
                if (begin) {
                    ret = self->expect("{");
                }
                else {
                    ret = self->expect("}");
                }
            }
            if (!ret && arr_block) {
                if (begin) {
                    ret = self->expect("[");
                }
                else {
                    ret = self->expect("]");
                }
            }
            if (!ret && bracket) {
                if (begin) {
                    ret = self->expect("(");
                }
                else {
                    ret = self->expect(")");
                }
            }
            return ret;
        }
        BasicBlock(bool block = true, bool array = true, bool bracket = true)
            : c_block(block), arr_block(array), bracket(bracket) {}
    };

    template <class Buf, class Ret, class Char>
    bool until(Reader<Buf> *self, Ret &ret, Char &ctx, bool begin) {
        if (begin)
            return true;
        if (!self)
            return true;
        if (self->achar() == ctx)
            return true;
        ret.push_back(self->achar());
        return false;
    }

    template <class Buf, class Char>
    bool untilnobuf(Reader<Buf> *self, Char ctx, bool begin) {
        if (begin)
            return true;
        if (!self)
            return true;
        if (self->achar() == ctx)
            return true;
        return false;
    }

    template <class Buf, class Ret, class Ctx>
    bool untilincondition(Reader<Buf> *self, Ret &ret, Ctx &judge, bool begin) {
        if (begin)
            return true;
        if (!self)
            return true;
        if (!judge(self->achar()))
            return true;
        ret.push_back(self->achar());
        return false;
    }

    template <class Buf, class Ctx>
    bool untilincondition_nobuf(Reader<Buf> *self, Ctx &judge, bool begin) {
        if (begin)
            return true;
        if (!self)
            return true;
        if (!judge(self->achar()))
            return true;
        return false;
    }

    template <class Buf, class Ret, class Ctx>
    bool addifmatch(Reader<Buf> *self, Ret &ret, Ctx &judge, bool begin) {
        if (begin)
            return true;
        if (!self)
            return true;
        if (judge(self->achar())) {
            ret.push_back(self->achar());
        }
        return false;
    }

    template <class Buf, class Ctx>
    bool linepos(Reader<Buf> *self, Ctx &ctx, bool begin) {
        if (!self)
            return true;
        if (begin) {
            ctx.nowpos = self->readpos();
            ctx.line = 0;
            ctx.pos = 0;
            self->seek(0);
            return true;
        }
        if (self->readpos() >= ctx.nowpos) {
            self->seek(ctx.nowpos);
            return true;
        }
        ctx.pos++;
        if (ctx.crlf) {
            while (self->expect("\r\n")) {
                ctx.line++;
                ctx.pos = 0;
            }
        }
        else if (ctx.cr) {
            while (self->expect("\r")) {
                ctx.line++;
                ctx.pos = 0;
            }
        }
        else {
            while (self->expect("\n")) {
                ctx.line++;
                ctx.pos = 0;
            }
        }
        return false;
    }

    template <class Buf, class Ret>
    bool cmdline_read(Reader<Buf> *self, Ret &ret, int *&res, bool begin) {
        if (begin) {
            if (!res)
                return false;
            return true;
        }
        if (!self)
            return true;
        auto c = self->achar();
        if (is_string_symbol(c)) {
            if (!self->string(ret, !(*res))) {
                *res = 0;
            }
            else {
                *res = -1;
            }
        }
        else {
            auto prev = ret.size();
            self->readwhile(ret, untilincondition, [](char c) { return c != ' ' && c != '\t'; });
            if (ret.size() == prev) {
                *res = 0;
            }
            else {
                *res = 1;
            }
        }
        return true;
    }

    template <class T>
    void defcmdlineproc_impl(T &p) {
        if (is_string_symbol(p[0])) {
            p.pop_back();
            p.erase(0, 1);
        }
    }

    template <class T>
    bool defcmdlineproc(T &p) {
        defcmdlineproc_impl(p);
        return true;
    }

    template <class Buf>
    bool get_cmdline(Reader<Buf> &, int mincount, bool, bool) {
        return mincount == 0;
    }

    template <class Buf, class Now, class... Args>
    bool get_cmdline(Reader<Buf> &r, int mincount, bool fit, bool lineok, Now &now, Args &...arg) {
        int i = lineok ? 1 : 0;
        r.ahead("'");
        r.readwhile(now, cmdline_read, &i);
        if (i == 0) {
            return mincount == 0;
        }
        if (fit && mincount == 0) {
            return true;
        }
        return get_cmdline(r, mincount == 0 ? 0 : mincount - 1, fit, lineok, arg...);
    }

    template <class Func, class... Args>
    bool proc_cmdline(Func proc, Args &...args) {
        [](auto...) {}(proc(args)...);
        return true;
    }

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

#if (__cplusplus < 201703L) || (defined(__GNUC__) && __GNUC__ < 11)
#include <string>
#endif
#if __cplusplus >= 201703L
#include <charconv>
#endif

namespace PROJECT_NAME {
    template <class N>
    constexpr N msb_on() {
        return static_cast<N>(~(static_cast<N>(~0)>>1));
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

    template <class T, class Buf, class Func>
    T get_position_translate(size_t pos, Func func, const Buf &buf) {
        constexpr int sof = sizeof(T);
        if (buf.size() % sof)
            return T();
        if (sof * (pos + 1) > buf.size()) {
            return T();
        }
        char tmp[sof] = {0};
        for (auto i = 0; i < sof; i++) {
            tmp[i] = buf[sof * pos + i];
        }
        return func(tmp);
    }

    constexpr size_t OrderedSize(size_t size, size_t order) {
        return size % order ? 0 : size / order;
    }

    template <class Buf, class T>
    struct ByteTo {
       private:
        static_assert(sizeof(b_char_type<Buf>) == 1, "");
        Buf buf;

       public:
        T operator[](size_t pos) const {
            return get_position_translate<T>(pos, translate_byte_as_is<T>, buf);
        }

        size_t size() const {
            return OrderedSize(buf.size(), sizeof(T));
        }

        ByteTo() {}
        ByteTo(const Buf &in)
            : buf(in) {}
        ByteTo(Buf &&in)
            : buf(std::forward<Buf>(in)) {}
    };

    template <class Buf, class T>
    struct ReverseByte {
       private:
        static_assert(sizeof(b_char_type<Buf>) == 1, "");
        Buf buf;

       public:
        T operator[](size_t pos) const {
            return get_position_translate<T>(pos, translate_byte_reverse<T>, buf);
        }

        size_t size() const {
            return OrderedSize(buf.size(), sizeof(T));
        }

        ReverseByte() {}
        ReverseByte(const Buf &in)
            : buf(in) {}
        ReverseByte(Buf &&in)
            : buf(std::forward<Buf>(in)) {}
    };

    template <class Buf, class T>
    struct Order {
       private:
        static_assert(sizeof(b_char_type<Buf>) == 1, "");
        Buf buf;
        bool reverse = false;

       public:
        T operator[](size_t pos) const {
            return reverse ? get_position_translate<T>(pos, translate_byte_reverse<T>, buf) : get_position_translate<T>(pos, translate_byte_as_is<T>, buf);
        }

        size_t size() const {
            return OrderedSize(buf.size(), sizeof(T));
        }

        Order() {}
        Order(const Buf &in)
            : buf(in) {}
        Order(Buf &&in)
            : buf(std::forward<Buf>(in)) {}

        void set_reverse(bool rev) {
            reverse = rev;
        }

        Buf &ref() {
            return buf;
        }
    };

    template <class Buf>
    struct Reverse {
        Buf buf;
        size_t size() const {
            return buf.size();
        }

        b_char_type<Buf> operator[](size_t pos) const {
            if (pos >= size())
                return b_char_type<Buf>();
            return buf[size() - 1 - pos];
        }

        b_char_type<Buf> operator[](size_t pos) {
            if (pos >= size())
                return b_char_type<Buf>();
            return buf[size() - 1 - pos];
        }

        Reverse() {}
        Reverse(const Buf &in)
            : buf(in) {}
        Reverse(Buf &&in)
            : buf(std::forward<Buf>(in)) {}
    };
}  // namespace PROJECT_NAME