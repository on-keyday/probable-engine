/*
    commonlib - common utility library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <string>
#include <type_traits>

#include "basic_helper.h"
#include "utf_helper.h"
#include "utfreader.h"

namespace PROJECT_NAME {

    template <class Buf, class T>
    auto& output_number(Reader<Buf>&& r, T& out) {
        std::string buf;
        bool minus = false;
        if CONSTEXPRIF (std::is_signed<T>::value) {
            if (r.expect("-")) {
                minus = true;
            }
        }
        NumberContext<b_char_type<Buf>> ctx;
        if CONSTEXPRIF (std::is_integral<T>::value) {
            ctx.flag |= NumberFlag::only_int;
        }
        r.readwhile(buf, number, &ctx);
        if (ctx.failed || buf.size() == 0) {
            return r;
        }
        auto ofs = ctx.radix == 10 ? 0 : ctx.radix == 16 || ctx.radix == 2 ? 2
                                                                           : 1;
        if (ofs) buf.erase(0, ofs);
        if (minus) {
            buf = "-" + buf;
        }
        if CONSTEXPRIF (std::is_integral<T>::value) {
            parse_int(buf, out, ctx.radix);
        }
        else if CONSTEXPRIF (std::is_floating_point<T>::value) {
            parse_float(buf, out, ctx.radix == 16);
        }
        return r;
    }
#define DEFINE_OPERATOR_RSHIFT_NUM(TYPE)                                    \
    template <class Buf>                                                    \
    auto& operator>>(Reader<Buf>& r, TYPE& out) {                           \
        return output_number<Buf, TYPE>(std::forward<Reader<Buf>>(r), out); \
    }                                                                       \
                                                                            \
    template <class Buf>                                                    \
    auto& operator>>(Reader<Buf>&& r, TYPE& out) {                          \
        return output_number<Buf, TYPE>(std::forward<Reader<Buf>>(r), out); \
    }

    DEFINE_OPERATOR_RSHIFT_NUM(signed short)
    DEFINE_OPERATOR_RSHIFT_NUM(unsigned short)
    DEFINE_OPERATOR_RSHIFT_NUM(signed int)
    DEFINE_OPERATOR_RSHIFT_NUM(unsigned int)
    DEFINE_OPERATOR_RSHIFT_NUM(signed long)
    DEFINE_OPERATOR_RSHIFT_NUM(unsigned long)
    DEFINE_OPERATOR_RSHIFT_NUM(signed long long)
    DEFINE_OPERATOR_RSHIFT_NUM(unsigned long long)

    DEFINE_OPERATOR_RSHIFT_NUM(float)
    DEFINE_OPERATOR_RSHIFT_NUM(double)
#undef DEFINE_OPERATOR_RSHIFT_NUM

    /*template<class Buf>
    constexpr int b_char_size_v=sizeof(b_char_type<Buf>);

    template<class Buf,size_t size>
    inline constexpr bool bcsizeeq=size==b_char_size_v<Buf>;*/

    template <class Buf, class Str>
    auto& output_str(Reader<std::enable_if_t<bcsizeeq<Buf, 1>, Buf>>&& r, std::enable_if_t<bcsizeeq<Str, 1>, Str>& str) {
        //if CONSTEXPRIF(bcsizeeq<Str,1>){
        str.reserve(r.readable());
        r.readwhile(str, untilincondition, do_nothing<b_char_type<Buf>>);
        //}
        //else if CONSTEXPRIF(bcsizeeq<Str,2>){
        //    int ctx=0;
        //    r.readwhile(str,utf8toutf16,&ctx);
        //}
        //else if CONSTEXPRIF(bcsizeeq<Str,4>){
        //    int ctx=0;
        //    r.readwhile(str,utf8toutf32,&ctx);
        //}
        return r;
    }

    template <class Buf, class Str>
    auto& output_str(Reader<std::enable_if_t<bcsizeeq<Buf, 1>, Buf>>&& r, std::enable_if_t<bcsizeeq<Str, 2>, Str>& str) {
        int ctx = 0;
        r.readwhile(str, utf8toutf16, &ctx);
        return r;
    }

    template <class Buf, class Str>
    auto& output_str(Reader<std::enable_if_t<bcsizeeq<Buf, 1>, Buf>>&& r, std::enable_if_t<bcsizeeq<Str, 4>, Str>& str) {
        int ctx = 0;
        r.readwhile(str, utf8toutf32, &ctx);
        return r;
    }

    template <class Buf, class Str>
    auto& output_str(Reader<std::enable_if_t<bcsizeeq<Buf, 2>, Buf>>&& r, std::enable_if_t<bcsizeeq<Str, 1>, Str>& str) {
        //if CONSTEXPRIF(bcsizeeq<Str,1>){
        int ctx = 0;
        r.readwhile(str, utf16toutf8, &ctx);
        //}
        //else if CONSTEXPRIF(bcsizeeq<Str,2>){
        //    str.reserve(r.readable());
        //    r.readwhile(str,untilincondition,do_nothing<b_char_type<Buf>>);
        //}
        //else if CONSTEXPRIF(bcsizeeq<Str,4>){
        //    int ctx=0;
        //    r.readwhile(str,utf16toutf32,&ctx);
        //}
        return r;
    }

    template <class Buf, class Str>
    auto& output_str(Reader<std::enable_if_t<bcsizeeq<Buf, 2>, Buf>>&& r, std::enable_if_t<bcsizeeq<Str, 2>, Str>& str) {
        str.reserve(r.readable());
        r.readwhile(str, untilincondition, do_nothing<b_char_type<Buf>>);
        return r;
    }

    template <class Buf, class Str>
    auto& output_str(Reader<std::enable_if_t<bcsizeeq<Buf, 2>, Buf>>&& r, std::enable_if_t<bcsizeeq<Str, 4>, Str>& str) {
        int ctx = 0;
        r.readwhile(str, utf16toutf32, &ctx);
        return r;
    }

    template <class Buf, class Str>
    auto& output_str(Reader<std::enable_if_t<bcsizeeq<Buf, 4>, Buf>>&& r, std::enable_if_t<bcsizeeq<Str, 1>, Str>& str) {
        //if CONSTEXPRIF(bcsizeeq<Str,1>){
        int ctx = 0;
        r.readwhile(str, utf32toutf8, &ctx);
        /*}
        else if CONSTEXPRIF(bcsizeeq<Str,2>){
            int ctx=0;
            r.readwhile(str,utf32toutf16,&ctx);
        }
        else if CONSTEXPRIF(bcsizeeq<Str,4>){
            str.reserve(r.readable());
            r.readwhile(str,untilincondition,do_nothing<b_char_type<Buf>>);
        }*/
        return r;
    }

    template <class Buf, class Str>
    auto& output_str(Reader<std::enable_if_t<bcsizeeq<Buf, 4>, Buf>>&& r, std::enable_if_t<bcsizeeq<Str, 2>, Str>& str) {
        int ctx = 0;
        r.readwhile(str, utf32toutf16, &ctx);
        return r;
    }

    template <class Buf, class Str>
    auto& output_str(Reader<std::enable_if_t<bcsizeeq<Buf, 4>, Buf>>&& r, std::enable_if_t<bcsizeeq<Str, 4>, Str>& str) {
        str.reserve(r.readable());
        r.readwhile(str, untilincondition, do_nothing<b_char_type<Buf>>);
        return r;
    }

    template <class Buf, class Str,
              class = std::enable_if_t<!std::is_function<Str>::value && !std::is_arithmetic<Str>::value, void>>
    decltype(auto) operator>>(Reader<Buf>& r, Str& str) {
        return output_str<Buf, Str>(std::forward<Reader<Buf>>(r), str);
    }

    template <class Buf, class Str,
              class = std::enable_if_t<!std::is_function<Str>::value && !std::is_arithmetic<Str>::value, void>>
    decltype(auto) operator>>(Reader<Buf>&& r, Str& str) {
        return output_str<Buf, Str>(std::forward<Reader<Buf>>(r), str);
    }

    struct U8Filter_Type {};
    struct U16Filter_Type {};
    struct U32Filter_Type {};

    inline U8Filter_Type u8filter() {
        return U8Filter_Type();
    }
    inline U16Filter_Type u16filter() {
        return U16Filter_Type();
    }
    inline U32Filter_Type u32filter() {
        return U32Filter_Type();
    }

    template <class IStream>
    auto filter_char_size(IStream&& in, U8Filter_Type (*)()) {
        Reader<std::string> r;
        constexpr int size = sizeof(typename IStream::char_type);
        int C = 0;
        if CONSTEXPRIF (size == 1) {
            in >> r.ref();
        }
        else if CONSTEXPRIF (size == 2) {
            Reader<std::conditional_t<sizeof(wchar_t) == 2, std::wstring, std::u16string>> str;
            in >> str.ref();
            str.readwhile(r.ref(), utf16toutf8, &C);
        }
        else if CONSTEXPRIF (size == 4) {
            Reader<std::conditional_t<sizeof(wchar_t) == 4, std::wstring, std::u32string>> str;
            in >> str.ref();
            str.readwhile(r.ref(), utf32toutf8, &C);
        }
        return r;
    }

    template <class IStream>
    auto filter_char_size(IStream&& in, U16Filter_Type (*)()) {
        Reader<std::conditional_t<sizeof(wchar_t) == 2, std::wstring, std::u16string>> r;
        constexpr int size = sizeof(typename IStream::char_type);
        int C = 0;
        if CONSTEXPRIF (size == 1) {
            Reader<std::string> str;
            in >> str.ref();
            str.readwhile(r.ref(), utf8toutf16, &C);
        }
        else if CONSTEXPRIF (size == 2) {
            in >> r.ref();
        }
        else if CONSTEXPRIF (size == 4) {
            Reader<std::conditional_t<sizeof(wchar_t) == 4, std::wstring, std::u32string>> str;
            in >> str.ref();
            str.readwhile(r.ref(), utf32toutf16, &C);
        }
        return r;
    }

    template <class IStream>
    auto filter_char_size(IStream&& in, U32Filter_Type (*)()) {
        Reader<std::conditional_t<sizeof(wchar_t) == 4, std::wstring, std::u32string>> r;
        constexpr int size = sizeof(typename IStream::char_type);
        int C = 0;
        if CONSTEXPRIF (size == 1) {
            Reader<std::string> str;
            in >> str.ref();
            str.readwhile(r.ref(), utf8toutf32, &C);
        }
        else if CONSTEXPRIF (size == 2) {
            Reader<std::conditional_t<sizeof(wchar_t) == 2, std::wstring, std::u16string>> str;
            in >> str.ref();
            str.readwhile(r.ref(), utf16toutf32, &C);
        }
        else if CONSTEXPRIF (size == 4) {
            in >> r.ref();
        }
        return r;
    }

#ifdef _WIN32 /*for windows in Japanese-lang*/
    struct CP932Filter_Type {};
    struct TOCP932Filter_Type {};
    inline CP932Filter_Type nativefilter() {
        return CP932Filter_Type();
    }
    inline TOCP932Filter_Type utffilter() {
        return TOCP932Filter_Type();
    }

    template <class IStream>
    auto filter_char_size(IStream&& in, CP932Filter_Type (*)()) {
        Reader<std::wstring> r;
        static_assert(sizeof(typename IStream::char_type) == 1, "");
        Reader<std::string> str;
        in >> str.ref();
        int ctx = 0;
        str.readwhile(r.ref(), sjistoutf16, &ctx);
        return r;
    }

    template <class IStream>
    auto filter_char_size(IStream&& in, TOCP932Filter_Type (*)()) {
        Reader<std::string> r;
        static_assert(sizeof(typename IStream::char_type) == 2, "");
        Reader<std::wstring> str;
        in >> str.ref();
        int ctx = 0;
        str.readwhile(r.ref(), utf16tosjis, &ctx);
        return r;
    }
#else
    inline U16Filter_Type nativefilter() {
        return U16Filter_Type();
    }
    inline U8Filter_Type utffilter() {
        return U8Filter_Type();
    }
#endif

    template <class Char, class Traits, class Filter>
    decltype(auto) operator>>(std::basic_istream<Char, Traits>& in, Filter (*filter)()) {
        return filter_char_size(std::forward<std::basic_istream<Char, Traits>>(in), filter);
    }

    template <class Char, class Traits, class Filter>
    decltype(auto) operator>>(std::basic_istream<Char, Traits>&& in, Filter (*filter)()) {
        return filter_char_size(std::forward<std::basic_istream<Char, Traits>>(in), filter);
    }

    template <class Str = std::string>
    struct StrStream {
        Str& s;

        StrStream(Str& str)
            : s(str) {}
        StrStream(Str&& str)
            : s(str) {}

        using char_type = b_char_type<Str>;

       private:
        template <class T, size_t sz>
        size_t size(T (&i)[sz]) {
            return sz;
        }

        template <class T>
        size_t size(T& in) {
            return in.size();
        }

       public:
        template <class OtherStr, class = std::enable_if_t<!std::is_function<OtherStr>::value, void>>
        StrStream& operator>>(OtherStr& in) {
            in.reserve(size(s));
            for (auto& o : s) {
                in.push_back(o);
            }
            return *this;
        }
    };

    template <class Str, class Filter>
    decltype(auto) operator>>(StrStream<Str>&& in, Filter (*filter)()) {
        return filter_char_size(std::forward<StrStream<Str>>(in), filter);
    }

    template <class Buf>
    decltype(auto) operator>>(Reader<Buf>&& r, U8Filter_Type (*)()) {
        Reader<ToUTF8<Buf>> ret;
        ret.ref().base_reader() = std::move(r);
        ret.ref().reset();
        return ret;
    }

    template <class Buf>
    decltype(auto) operator>>(Reader<Buf>&& r, U16Filter_Type (*)()) {
        Reader<ToUTF16<Buf>> ret;
        ret.ref().base_reader() = std::move(r);
        ret.ref().reset();
        return ret;
    }

    template <class Buf>
    decltype(auto) operator>>(Reader<Buf>&& r, U32Filter_Type (*)()) {
        Reader<ToUTF32<Buf>> ret;
        ret.ref().base_reader() = std::move(r);
        ret.ref().reset();
        return ret;
    }

#ifdef _WIN32
    template <class Buf>
    decltype(auto) operator>>(Reader<Buf>&& r, TOCP932Filter_Type (*)()) {
        Reader<std::string> ret;
        StrStream<Buf>(r.ref()) >> utffilter >> ret.ref();
        return ret;
    }

    template <class Buf>
    decltype(auto) operator>>(Reader<Buf>&& r, CP932Filter_Type (*)()) {
        Reader<std::wstring> ret;
        StrStream<Buf>(r.ref()) >> nativefilter >> ret.ref();
        return ret;
    }
#endif

    struct Resetpos_Type {};

    template <class Buf>
    void resetpos(Reader<Buf>& r) {
        r.seek(0);
    }

    template <class Buf>
    void igws(Reader<Buf>& r) {
        r.set_ignore(ignore_space);
    }

    template <class Buf>
    void igno(Reader<Buf>& r) {
        r.set_ignore(nullptr);
    }

    template <class Buf>
    Reader<Buf>& operator>>(Reader<Buf>& r, void (*ctrl)(Reader<Buf>&)) {
        ctrl(r);
        return r;
    }

    template <class Buf>
    Reader<Buf>& operator>>(Reader<Buf>&& r, void (*ctrl)(Reader<Buf>&)) {
        ctrl(r);
        return r;
    }

    template <class Buf, class C>
    using Pairing = std::pair<Ref<Reader<Buf>>, bool (*)(C)>;

    template <class Buf, class C>
    Pairing<Buf, C> operator>>(Reader<Buf>& r, bool (*f)(C)) {
        return {r, f};
    }

    template <class Buf, class C>
    Pairing<Buf, C> operator>>(Reader<Buf>&& r, bool (*f)(C)) {
        return {r, f};
    }

    template <class Buf, class C, class Str>
    auto& operator>>(Pairing<Buf, C>&& p, Str& str) {
        if CONSTEXPRIF (b_char_size_v<Buf> == b_char_size_v<Str>) {
            (*p.first).readwhile(str, untilincondition, p.second);
        }
        return *p.first;
    }
    /*
    struct Cut_Type{};

    inline Cut_Type cut(){return Cut_Type();}

    template<class Buf>
    auto& operator>>(Reader<Buf>& r,Cut_Type(*)()){
        return r;
    }

    template<class Buf>
    auto& operator>>(Reader<Buf>&& r,Cut_Type(*)()){
        return r;
    }*/
}  // namespace PROJECT_NAME