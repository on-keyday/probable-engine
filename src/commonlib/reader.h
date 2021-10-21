/*
    commonlib - common utility library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <cstddef>
#include <utility>

#include "project_name.h"

#ifdef COMMONLIB2_HAS_CONCEPTS
#include <concepts>
#endif

namespace PROJECT_NAME {

    template <class T, class C = char>
    constexpr T translate_byte_as_is(const C* s) {
        T res = T();
        res.~T();
        char* res_p = (char*)&res;
        for (auto i = 0u; i < sizeof(T); i++) {
            res_p[i] = ((unsigned char*)(s))[i];
        }
        return res;
    }

    template <class T, class C = char>
    constexpr T translate_byte_reverse(const C* s) {
        T res = T();
        res.~T();
        char* res_p = (char*)&res;
        auto k = 0ull;
        for (auto i = sizeof(T) - 1;; i--) {
            res_p[i] = ((unsigned char*)(s))[k];
            if (i == 0) break;
            k++;
        }
        return res;
    }

    constexpr bool is_big_endian() {
#if defined(__BIG_ENDIAN__)
        return true;
#elif defined(__LITTLE_ENDIAN__)
        return false;
#else
        const int i = 1;
        const bool b = (const bool)*(char*)&i;
        return b;
#endif
    }

    template <class T, class C>
    constexpr T translate_byte_net_and_host(const C* s) {
#if defined(__BIG_ENDIAN__)
        return translate_byte_as_is<T>(s);
#elif defined(__LITTLE_ENDIAN__)
        return translate_byte_reverse<T>(s);
#else
        return is_big_endian() ? translate_byte_reverse<T>(s) : translate_byte_as_is<T>(s);
#endif
    }

    template <class T>
    using remove_cv_ref = std::remove_cv_t<std::remove_reference_t<T>>;

#if __cplusplus > 201703L && __has_include(<concepts>)
    template <class T>
    concept Is_integral = std::is_integral_v<remove_cv_ref<T>>;

    template <class Buf>
    concept Readable = requires(const Buf& x) {
        { x[0] } -> Is_integral;
        x.size();
    };

    template <class Buf>
    concept CharArray = std::is_pointer_v<Buf> && std::is_integral_v<std::remove_pointer_t<Buf>>;
/*#define READABLE Readable
#else
#define READABLE class*/
#endif

    template <class Buf>
#if __cplusplus > 201703L && __has_include(<concepts>)
    requires Readable<Buf> || CharArray<Buf>
#endif
    struct Reader {
       private:
        Buf buf;
        using RBuf = std::remove_reference_t<Buf>;
        using Char = remove_cv_ref<decltype(buf[0])>;

       public:
        using IgnoreHandler = bool (*)(RBuf&, size_t&, size_t bufsize);
        using not_expect_default = bool (*)(Char);
        using cmp_default = bool (*)(Char, Char);

        static bool default_cmp(Char c1, Char c2) {
            return c1 == c2;
        }

       private:
        size_t pos = 0;
        IgnoreHandler ignore_cb = nullptr;
        //bool stop=false;
        //bool refed=false;

        template <class Str>
        inline size_t strlen(Str str) const {
            size_t i = 0;
            while (str[i] != 0) {
                if (str[i + 1] == 0) return i + 1;
                if (str[i + 2] == 0) return i + 2;
                if (str[i + 3] == 0) return i + 3;
                if (str[i + 4] == 0) return i + 4;
                if (str[i + 5] == 0) return i + 5;
                if (str[i + 6] == 0) return i + 6;
                if (str[i + 7] == 0) return i + 7;
                if (str[i + 8] == 0) return i + 8;
                if (str[i + 9] == 0) return i + 9;
                if (str[i + 10] == 0) return i + 10;
                if (str[i + 11] == 0) return i + 11;
                if (str[i + 12] == 0) return i + 12;
                if (str[i + 13] == 0) return i + 13;
                if (str[i + 14] == 0) return i + 14;
                if (str[i + 15] == 0) return i + 15;
                i += 16;
            }
            return i;
        }

        template <class C>
        size_t buf_size(C* str) const {
            if (!str) return 0;
            return strlen(str);
        }

        template <class C>
        size_t buf_size(const C* str) const {
            if (!str) return 0;
            return strlen(str);
        }

        template <class Str>
        size_t buf_size(Str& str) const {
            return str.size();
        }

        template <class C>
        bool on_end(const C* s, size_t pos) {
            return s[pos] == 0;
        }

        template <class C>
        bool on_end(C* s, size_t pos) {
            return s[pos] == 0;
        }

        template <class Str>
        bool on_end(Str& s, size_t pos) {
            return s.size() <= pos;
        }

        bool endbuf(size_t bufsize, int ofs = 0) const {
            return (bufsize <= (size_t)(pos + ofs));
        }

        /*
        bool refcheck(){
            if(refed){
                auto s=buf_size(buf);
                if(pos>=s){
                    pos=s;
                    stop=true;
                }
                else{
                    stop=false;
                }
                refed=false;
                return true;
            }
            return false;
        }*/

        bool ignore() {
            auto bs = buf_size(buf);
            if (endbuf(bs)) return false;
            //refcheck();
            //if(stop)return false;
            if (ignore_cb) {
                //stop=!ignore_cb(buf,pos,buf_size(buf));
                //return stop==false;
                return ignore_cb(buf, pos, bs);
            }
            return true;
        }

        template <class T>
        bool invoke_not_expect(bool (*not_expect)(T t), Buf& b, size_t ofs) {
            if (!not_expect) return false;
            return not_expect(b[ofs]);
        }

        template <class Func>
        bool invoke_not_expect(Func&& not_expect, Buf& b, size_t ofs) {
            return not_expect(b[ofs]);
        }

        bool invoke_not_expect(std::nullptr_t, Buf&, size_t) {
            return false;
        }

        template <class Str, class NotExpect, class Cmp>
        size_t ahead_detail(Str& str, NotExpect&& not_expect, Cmp&& cmp) {
            //if(!cmp)return 0;
            if (!ignore()) return 0;
            size_t i = 0;
            size_t bufsize = buf_size(buf);
            for (; !on_end(str, i); i++) {
                if (endbuf(bufsize, (int)i)) return 0;
                if (!cmp(buf[pos + i], str[i])) return 0;
            }
            if (i == 0) return 0;
            if (invoke_not_expect(std::forward<NotExpect>(not_expect), buf, pos + i)) return 0;
            return i;
        }

        template <class Str, class NotExpect, class Cmp>
        inline size_t ahead_check(Str& str, NotExpect&& nexp, Cmp&& cmp) {
            return ahead_detail(str, std::forward<NotExpect>(nexp), std::forward<Cmp>(cmp));
        }

        /*
        template<class C,class NotExpect,class Cmp>
        inline size_t ahead_check(const C* str,NotExpect&& nexp,Cmp&& cmp){
            if(!str)return 0;
            return ahead_detail(str,std::forward<NotExpect>(nexp),std::forward<Cmp>(cmp)); 
        }*/

        template <class C, class NotExpect, class Cmp>
        inline size_t ahead_check(C* str, NotExpect&& nexp, Cmp&& cmp) {
            if (!str) return 0;
            return ahead_detail(str, std::forward<NotExpect>(nexp), std::forward<Cmp>(cmp));
        }

        void copy(const Reader& from) {
            //refed=from.refed;
            pos = from.pos;
            ignore_cb = from.ignore_cb;
            //stop=from.stop;
        }

        void move(Reader&& from) {
            pos = from.pos;
            from.pos = 0;
            ignore_cb = from.ignore_cb;
            from.ignore_cb = nullptr;
            /*stop=from.stop;
            from.stop=false;
            refed=from.refed;
            from.refed=false;*/
        }

       public:
        using char_type = Char;
        using buf_type = Buf;

        Reader(const Reader& from)
            : buf(from.buf) {
            copy(from);
        }

        Reader(Reader&& from) noexcept
            : buf(std::forward<Buf>(from.buf)) {
            move(std::forward<Reader>(from));
        }

        Reader(IgnoreHandler cb = nullptr)
            : buf(Buf()) {
            ignore_cb = cb;
        }

        Reader(std::remove_cv_t<RBuf>& in, IgnoreHandler cb = nullptr)
            : buf(in) {
            ignore_cb = cb;
        }

        Reader(const RBuf& in, IgnoreHandler cb = nullptr)
            : buf(in) {
            //buf=in;
            ignore_cb = cb;
        }

        Reader(RBuf&& in, IgnoreHandler cb = nullptr) noexcept
            : buf(std::forward<RBuf>(in)) {
            //buf=std::move(in);
            ignore_cb = cb;
        }

        Reader& operator=(const Reader& from) {
            buf = from.buf;
            copy(from);
            return *this;
        }

        Reader& operator=(Reader&& from) {
            buf = std::forward<RBuf>(from.buf);
            move(std::forward<Reader>(from));
            return *this;
        }
#ifdef COMMONLIB2_IS_MSVC
       private:
#define ahead ahead_internal
#define expect expect_internal
#define expectp expectp_internal
#endif

        template <class Str, class NotExpect = not_expect_default, class Cmp = cmp_default>
        size_t ahead(Str&& str, NotExpect&& not_expect = NotExpect(), Cmp&& cmp = std::move(default_cmp)) {
            return ahead_check(str, std::forward<NotExpect>(not_expect), std::forward<Cmp>(cmp));
        }

        template <class Str, class NotExpect = not_expect_default, class Cmp = cmp_default>
        bool expect(Str&& str, NotExpect&& not_expect = NotExpect(), Cmp&& cmp = std::move(default_cmp)) {
            auto size = ahead(str, std::forward<NotExpect>(not_expect), std::forward<Cmp>(cmp));
            if (size == 0) return false;
            pos += size;
            ignore();
            return true;
        }

        template <class Str, class Ctx, class NotExpect = not_expect_default, class Cmp = cmp_default>
        bool expectp(Str&& str, Ctx& ctx, NotExpect&& not_expect = NotExpect(), Cmp&& cmp = std::move(default_cmp)) {
            if (expect<Str, NotExpect, Cmp>(str, std::forward<NotExpect>(not_expect), std::forward<Cmp>(cmp))) {
                ctx = (Ctx)str;
                return true;
            }
            return false;
        }

#ifdef COMMONLIB2_IS_MSVC
#undef ahead
#undef expect
#undef expectp
       public:
        template <class Str, class NotExpect = not_expect_default, class Cmp = cmp_default>
        size_t ahead(Str& str, NotExpect not_expect = NotExpect(), Cmp cmp = default_cmp) {
            return ahead_internal(str, std::forward<NotExpect>(not_expect), std::forward<Cmp>(cmp));
        }

        template <class Str, class NotExpect = not_expect_default, class Cmp = cmp_default>
        bool expect(Str& str, NotExpect not_expect = NotExpect(), Cmp cmp = default_cmp) {
            return expect_internal(str, std::forward<NotExpect>(not_expect), std::forward<Cmp>(cmp));
        }

        template <class Str, class Ctx, class NotExpect = not_expect_default, class Cmp = cmp_default>
        bool expectp(Str& str, Ctx& ctx, NotExpect not_expect = NotExpect(), Cmp cmp = default_cmp) {
            return expectp_internal(str, ctx, std::forward<NotExpect>(not_expect), std::forward<Cmp>(cmp));
        }
#endif

        Char achar() const {
            return offset(0);
        }

        bool seek(size_t p, bool strict = false) {
            if (p == 0) {
                this->pos = 0;
                return true;
            }
            auto sz = buf_size(buf);
            if (sz < p) {
                if (strict) return false;
                p = sz;
            }
            this->pos = p;
            //if(!endbuf())stop=false;
            return true;
        }

        bool increment() {
            return seek(pos + 1, true);
        }

        bool decrement() {
            return seek(pos - 1, true);
        }

        bool seekend() {
            return seek(buf_size(buf));
        }

        Char offset(int ofs) const {
            if (endbuf(buf_size(buf), ofs)) return Char();
            return buf[pos + ofs];
        }

        bool ceof(int ofs = 0) const {
            return  //stop||
                endbuf(buf_size(buf), ofs);
        }

        bool eof() {
            ignore();
            return ceof();
        }

        /*
        bool release(){
            if(endbuf())return false;
            stop=false;
            return true;
        }*/

        size_t readpos() const {
            return pos;
        }

        size_t readable() {
            if (eof()) return 0;
            return buf_size(buf) - pos;
        }

        IgnoreHandler set_ignore(IgnoreHandler hander) {
            auto ret = this->ignore_cb;
            this->ignore_cb = hander;
            return ret;
        }

        template <class T>
        size_t read_byte(T* res = nullptr, size_t size = sizeof(T),
                         T (*endian_handler)(const char*) = translate_byte_as_is, bool strict = false) {
            static_assert(sizeof(Char) == 1, "");
            if (strict && readable() < size) return 0;
            auto bs = buf_size(buf);
            size_t beginpos = pos;
            if (res) {
                if (!endian_handler) return 0;
                size_t willread = size / sizeof(T) + (size % sizeof(T) ? 1 : 0);
                size_t ofs = 0;
                while (!eof() && ofs < willread) {
                    char pass[sizeof(T)] = {0};
                    size_t remain = size - sizeof(T) * ofs;
                    size_t max = remain < sizeof(T) ? remain : sizeof(T);
                    for (auto i = 0; i < max; i++) {
                        pass[i] = buf[pos];
                        pos++;
                        if (endbuf(bs)) {
                            break;
                        }
                    }
                    res[ofs] = endian_handler(pass);
                    ofs++;
                }
            }
            else {
                pos += size;
            }
            if (endbuf(bs)) {
                pos = bs;
            }
            return pos - beginpos;
        }

        RBuf& ref() { /*refed=true;*/
            return buf;
        }

       private:
        template <class Ret>
        static bool read_string(Reader* self, Ret& buf, bool*& noline, bool begin) {
            if (!self) return false;
            if (begin) {
                if (self->achar() != (Char)'\'' && self->achar() != (Char)'"' && self->achar() != (Char)'`') return false;
                buf.push_back(self->achar());
                self->increment();
                //ctx=self->set_ignore(nullptr);
                return true;
            }
            else {
                if (self->achar() == *buf.begin()) {
                    if (*(buf.end() - 1) != '\\') {
                        buf.push_back(self->achar());
                        self->increment();
                        //self->set_ignore(ctx);
                        return true;
                    }
                }
                if (*noline) {
                    if (self->achar() == (Char)'\n' || self->achar() == (Char)'\r') {
                        *noline = false;
                        return true;
                    }
                }
                buf.push_back(self->achar());
                return false;
            }
        }

        template <class Func>
        inline bool readwhile_impl_detail(Func& func, bool usecheckers) {
            ignore();
            if (!func(this, true)) return false;
            auto ig = set_ignore(nullptr);
            bool ok = false;
            auto bs = buf_size(buf);
            while (!endbuf(bs)) {
                if (func(this, false)) {
                    ok = true;
                    break;
                }
                pos++;
            }
            if (endbuf(bs)) {
                auto t = func(nullptr, false);
                if (usecheckers) ok = t;
            }
            set_ignore(ig);
            return ok;
        }

        template <class Ctx, class Ret, class Func>
        bool readwhile_impl1(Ret& ret, Func check, Ctx& ctx, bool usecheckers) {
            if (!check) return false;
            auto func = [&](Reader* self, bool flag) {
                return check(self, ret, ctx, flag);
            };
            return readwhile_impl_detail(func, usecheckers);
        }

        template <class Func, class Ctx>
        bool readwhile_impl2(Func check, Ctx& ctx, bool usecheckers) {
            if (!check) return false;
            auto func = [&](Reader* self, bool flag) {
                return check(self, ctx, flag);
            };
            return readwhile_impl_detail(func, usecheckers);
        }

       public:
        template <class Ctx = void*, class Ret>
        bool readwhile(Ret& ret, bool (*check)(Reader*, Ret&, Ctx&, bool), Ctx ctx = 0, bool usecheckers = false) {
            return readwhile_impl1(ret, check, ctx, usecheckers);
        }

        template <class Ctx>
        bool readwhile(bool (*check)(Reader*, Ctx&, bool), Ctx& ctx, bool usecheckers = false) {
            return readwhile_impl2(check, ctx, usecheckers);
        }

        template <class Ctx>
        bool readwhile(bool (*check)(Reader*, Ctx, bool), Ctx ctx, bool usecheckers = false) {
            return readwhile_impl2(check, ctx, usecheckers);
        }

        template <class Ret>
        bool string(Ret& ret, bool noline = true) {
            auto first = noline;
            auto res = readwhile(ret, read_string, &noline);
            if (res) {
                if (noline != first) return false;
            }
            return res;
        }
    };

    template <class Buf>
    bool ignore_space(Buf& buf, size_t& pos, size_t bufsize) {
        using Char = remove_cv_ref<decltype(buf[0])>;
        while (bufsize > pos) {
            if (buf[pos] == (Char)' ' || buf[pos] == (Char)'\t') {
                pos++;
                continue;
            }
            break;
        }
        return bufsize > pos;
    }

    template <class Buf>
    bool ignore_space_line(Buf& buf, size_t& pos, size_t bufsize) {
        using Char = remove_cv_ref<decltype(buf[0])>;
        while (bufsize > pos) {
            auto c = buf[pos];
            if (c == (Char)' ' || c == (Char)'\t' || c == (Char)'\n' || c == (Char)'\r') {
                pos++;
                continue;
            }
            break;
        }
        return bufsize > pos;
    }

    template <class Buf>
    bool ignore_c_comments(Buf& buf, size_t& pos, size_t bufsize) {
        using Char = remove_cv_ref<decltype(buf[0])>;
        auto size = [&](int ofs = 0) { return bufsize > (pos + ofs); };
        while (size()) {
            auto c = buf[pos];
            if (c == (Char)' ' || c == (Char)'\t' || c == (Char)'\n' || c == (Char)'\r') {
                pos++;
                continue;
            }
            else if (c == (Char)'/' && size(1) && buf[pos + 1] == (Char)'*') {
                pos += 2;
                bool ok = false;
                while (size()) {
                    if (buf[pos] == (Char)'*' && size(1) && buf[pos + 1] == (Char)'/') {
                        ok = true;
                        pos += 2;
                        break;
                    }
                    pos++;
                }
                if (!ok) return false;
                continue;
            }
            else if (c == (Char)'/' && size(1) && buf[pos + 1] == (Char)'/') {
                pos += 2;
                while (size()) {
                    if (buf[pos] == (Char)'\n' || buf[pos] == (Char)'\r') {
                        pos++;
                        break;
                    }
                    pos++;
                }
                continue;
            }
            break;
        }
        return size();
    }

#if __cplusplus >= 201703L
    template <class Buf>
    Reader(const Buf&) -> Reader<Buf>;

    template <class Buf>
    Reader(Buf&&) -> Reader<Buf>;
#endif

    template <class Buf>
    using cmpf_t = typename Reader<Buf>::cmp_default;

    template <class Buf>
    using nexpf_t = typename Reader<Buf>::not_expect_default;

    template <class Buf>
    using b_char_type = typename Reader<Buf>::char_type;

    template <class Buf>
    constexpr int b_char_size_v = sizeof(b_char_type<Buf>);

    template <class Buf, size_t size>
    constexpr bool bcsizeeq = size == b_char_size_v<Buf>;
}  // namespace PROJECT_NAME