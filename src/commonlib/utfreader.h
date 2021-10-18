/*
    commonlib - common utility library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <memory>

#include "utf_helper.h"
namespace PROJECT_NAME {

    template <class Buf>
    struct ToUTF32_impl {
        mutable Reader<Buf> r;
        mutable bool err = false;
        mutable size_t prev = 0;
        mutable char32_t chache = 0;

        template <class Func>
        char32_t get_achar(Func func) const {
            int ierr = 0;
            auto c = func(&r, &ierr);
            r.increment();
            if (ierr) {
                err = true;
            }
            return c;
        }

        template <class Func>
        size_t count(Func func) const {
            r.seek(0);
            size_t c = 0;
            while (!r.ceof()) {
                get_achar(func);
                if (err) {
                    return 0;
                }
                c++;
            }
            //r.seek(pos);
            return c;
        }

        template <class Func>
        bool seekminus(Func func, long long ofs) const {
            if (ofs >= 0) return false;
            for (auto i = -1; i > ofs; i--) {
                if (!func(&r)) {
                    err = true;
                    return false;
                }
            }
            return true;
        }

        template <class Func1, class Func2>
        char32_t get_position(Func1 func1, Func2 func2, size_t pos) const {
            if (prev - 1 == pos) {
                return chache;
            }
            if (pos < prev) {
                seekminus(func1, static_cast<long long>(pos - prev - 1));
            }
            else {
                for (auto i = prev; i < pos - prev; i++) {
                    get_achar(func2);
                    if (err) return char32_t();
                }
            }
            chache = get_achar(func2);
            if (err) return char32_t();
            prev = pos + 1;
            return chache;
        }

        size_t reset() {
            err = false;
            r.seek(0);
            auto ret = count();
            r.seek(0);
            return ret;
        }

        Buf& ref() {
            return r.ref();
        }

        Reader<Buf>& base_reader() {
            return r;
        }

        ToUTF32_impl() {}

        ToUTF32_impl(Buf&& in)
            : r(std::forward<Buf>(in)) {}
        ToUTF32_impl(const Buf& in)
            : r(in) {}

        void copy(const ToUTF32_impl& in) {
            r = in.r;
            err = in.err;
            chache = in.chache;
            prev = in.prev;
        }

        void move(ToUTF32_impl&& in) noexcept {
            r = std::move(in.r);
            err = in.err;
            in.err = false;
            chache = in.chache;
            in.chache = 0;
            prev = in.prev;
            in.prev = 0;
        }
    };

    template <class Buf, class Tmp>
    struct FromUTF32_impl {
        using char_type = std::make_unsigned_t<b_char_type<Buf>>;
        mutable Tmp minbuf;
        using to_type = remove_cv_ref<decltype(minbuf[0])>;
        mutable Reader<Buf> r;
        mutable size_t ofs = 0;
        mutable bool err = false;
        mutable size_t prev = 0;

        mutable size_t readpos = 0;

        template <class Func>
        bool parse(Func func) const {
            minbuf.reset();
            int ctx = 0;
            int* ctxp = &ctx;
            readpos = r.readpos();
            func(&r, minbuf, ctxp, false);
            if (ctx) {
                err = true;
                return false;
            }
            ofs = 0;
            return true;
        }

        template <class Func>
        bool increment(Func func) const {
            ofs++;
            if (ofs >= minbuf.size()) {
                if (r.ceof()) return false;
                size_t prev = r.readpos();
                if (!parse(func)) return false;
                r.increment();
            }
            return true;
        }

        template <class Func, class Func2>
        bool decrement(Func func, Func2 func2) const {
            if (ofs == 0) {
                if (r.readpos() == 0) {
                    err = true;
                    return false;
                }
                r.seek(readpos);
                if (!func2(&r)) {
                    err = true;
                    return false;
                }
                parse(func);
                r.increment();
                if (err) return false;
                ofs = minbuf.size() - 1;
            }
            else {
                ofs--;
            }
            return true;
        }

        template <class Func>
        size_t count(Func func) {
            size_t c = 0;
            while (!r.ceof()) {
                if (!increment(func)) break;
                c += minbuf.size();
                ofs = minbuf.size();
            }
            r.seek(0);
            ofs = 0;
            minbuf.reset();
            increment(func);
            return err ? 0 : c;
        }

        template <class Func, class Func2>
        to_type get_position(Func func, Func2 func2, size_t pos) const {
            if (prev == pos) {
                return minbuf[ofs];
            }
            if (prev < pos) {
                for (auto i = 0; i < pos - prev; i++) {
                    increment(func);
                    if (err) return char();
                }
            }
            else {
                for (auto i = 0; i < prev - pos; i++) {
                    decrement(func, func2);
                    if (err) return char();
                }
            }
            prev = pos;
            return minbuf[ofs];
        }

        size_t ofset_from_head() {
            return ofs;
        }

        size_t offset_to_next() {
            return minbuf.size() - ofs;
        }

        Buf& ref() {
            return r.ref();
        }

        template <class Func>
        size_t reset(Func func) {
            err = false;
            r.seek(0);
            auto ret = count(func);
            //r.seek(pos);
            return ret;
        }

        Reader<Buf>& base_reader() {
            return r;
        }

        FromUTF32_impl() {}

        FromUTF32_impl(Buf&& in)
            : r(std::forward<Buf>(in)) {}
        FromUTF32_impl(const Buf& in)
            : r(in) {}

        void copy(const FromUTF32_impl& in) {
            r = in.r;
            err = in.err;
            ofs = in.ofs;
            prev = in.prev;
            minbuf = in.minbuf;
            readpos = in.readpos;
        }

        void move(FromUTF32_impl&& in) noexcept {
            r = std::move(in.r);
            err = in.err;
            in.err = false;
            prev = in.prev;
            ofs = in.ofs;
            in.ofs = 0;
            in.prev = 0;
            minbuf = std::move(in.minbuf);
            readpos = in.readpos;
            in.readpos = 0;
        }
    };

#define DEFINE_UTF_TEMPLATE_TO32(NAME, SIZE, DECR, INCR)                       \
    template <class Buf>                                                       \
    struct NAME<Buf, SIZE> {                                                   \
       private:                                                                \
        ToUTF32_impl<Buf> impl;                                                \
        size_t _size = 0;                                                      \
                                                                               \
       public:                                                                 \
        NAME() {                                                               \
            _size = impl.count(INCR);                                          \
        }                                                                      \
                                                                               \
        NAME(Buf&& in) : impl(std::forward<Buf>(in)) {                         \
            _size = impl.count(INCR);                                          \
        }                                                                      \
                                                                               \
        NAME(const Buf& in) : impl(in) {                                       \
            _size = impl.count(INCR);                                          \
        }                                                                      \
                                                                               \
        NAME(const ToUTF32& in) {                                              \
            _size = in._size;                                                  \
            impl.copy(in.impl);                                                \
        }                                                                      \
                                                                               \
        NAME(ToUTF32&& in)                                                     \
        noexcept {                                                             \
            _size = in._size;                                                  \
            impl.move(std::forward<ToUTF32_impl<Buf>>(in.impl));               \
            in._size = 0;                                                      \
        }                                                                      \
                                                                               \
        char32_t operator[](size_t s) const {                                  \
            if (impl.err) return char32_t();                                   \
            return _size <= s ? char32_t() : impl.get_position(DECR, INCR, s); \
        }                                                                      \
                                                                               \
        size_t size() const {                                                  \
            return _size;                                                      \
        }                                                                      \
                                                                               \
        Buf* operator->() const {                                              \
            return std::addressof(impl.ref());                                 \
        }                                                                      \
                                                                               \
        bool reset(size_t pos = 0) {                                           \
            _size = impl.reset(pos);                                           \
            return impl.err;                                                   \
        }                                                                      \
                                                                               \
        Reader<Buf>& base_reader() {                                           \
            return impl.base_reader();                                         \
        }                                                                      \
                                                                               \
        NAME& operator=(const NAME& in) {                                      \
            _size = in._size;                                                  \
            impl.copy(in.impl);                                                \
            return *this;                                                      \
        }                                                                      \
                                                                               \
        NAME& operator=(NAME&& in) noexcept {                                  \
            _size = in._size;                                                  \
            in._size = 0;                                                      \
            impl.move(std::forward<ToUTF32_impl>(in.impl));                    \
            return *this;                                                      \
        }                                                                      \
    }

#define DEFINE_UTF_TEMPLATE(NAME, SIZE, INCR, DECR, MINBUF)                     \
    template <class Buf>                                                        \
    struct NAME<Buf, SIZE> {                                                    \
       private:                                                                 \
        FromUTF32_impl<Buf, MINBUF> impl;                                       \
        size_t _size = 0;                                                       \
        using char_type = typename FromUTF32_impl<Buf, MINBUF>::to_type;        \
                                                                                \
       public:                                                                  \
        NAME() {                                                                \
            _size = impl.count(INCR);                                           \
        }                                                                       \
                                                                                \
        NAME(const Buf& in) : impl(in) {                                        \
            _size = impl.count(INCR);                                           \
        }                                                                       \
                                                                                \
        NAME(Buf&& in) : impl(std::forward<Buf>(in)) {                          \
            _size = impl.count(INCR);                                           \
        }                                                                       \
                                                                                \
        NAME(const NAME& in) {                                                  \
            _size = in._size;                                                   \
            impl.copy(in.impl);                                                 \
        }                                                                       \
                                                                                \
        NAME(NAME&& in)                                                         \
        noexcept {                                                              \
            _size = in._size;                                                   \
            impl.move(std::forward<FromUTF32_impl<Buf, MINBUF>>(in.impl));      \
            in._size = 0;                                                       \
        }                                                                       \
                                                                                \
        char_type operator[](size_t s) const {                                  \
            if (impl.err) return char();                                        \
            return s >= _size ? char_type() : impl.get_position(INCR, DECR, s); \
        }                                                                       \
                                                                                \
        size_t size() const {                                                   \
            return _size;                                                       \
        }                                                                       \
                                                                                \
        size_t ofset_from_head() {                                              \
            return impl.ofset_from_head();                                      \
        }                                                                       \
                                                                                \
        size_t offset_to_next() {                                               \
            return impl.offset_to_next();                                       \
        }                                                                       \
                                                                                \
        Buf* operator->() {                                                     \
            return std::addressof(impl.ref());                                  \
        }                                                                       \
                                                                                \
        Reader<Buf>& base_reader() {                                            \
            return impl.base_reader();                                          \
        }                                                                       \
                                                                                \
        bool reset() {                                                          \
            _size = impl.reset(INCR);                                           \
            return impl.err;                                                    \
        }                                                                       \
                                                                                \
        NAME& operator=(const NAME& in) {                                       \
            _size = in._size;                                                   \
            impl.copy(in.impl);                                                 \
            return *this;                                                       \
        }                                                                       \
                                                                                \
        NAME& operator=(NAME&& in) noexcept {                                   \
            _size = in._size;                                                   \
            in._size = 0;                                                       \
            impl.move(std::forward<FromUTF32_impl<Buf, MINBUF>>(in.impl));      \
            return *this;                                                       \
        }                                                                       \
    }

    template <class Buf, int N = sizeof(b_char_type<Buf>)>
    struct ToUTF32;

    DEFINE_UTF_TEMPLATE_TO32(ToUTF32, 1, (utf8seek_minus<Buf>), (utf8toutf32_impl<Buf>));
    DEFINE_UTF_TEMPLATE_TO32(ToUTF32, 2, (utf16seek_minus<Buf>), (utf16toutf32_impl<Buf>));

    template <class Buf, int N = sizeof(b_char_type<Buf>)>
    struct ToUTF8;

    DEFINE_UTF_TEMPLATE(ToUTF8, 4, (utf32toutf8<Buf, U8MiniBuffer>), ([](Reader<Buf>* r) { return r->decrement(); }), U8MiniBuffer);

    DEFINE_UTF_TEMPLATE(ToUTF8, 2, (utf16toutf8<Buf, U8MiniBuffer>), ([](Reader<Buf>* r) { return utf16seek_minus<Buf>(r); }), U8MiniBuffer);

    template <class Buf, int N = sizeof(b_char_type<Buf>)>
    struct ToUTF16;

    DEFINE_UTF_TEMPLATE(ToUTF16, 4, (utf32toutf16<Buf, U16MiniBuffer>), ([](Reader<Buf>* r) { return r->decrement(); }), U16MiniBuffer);

    DEFINE_UTF_TEMPLATE(ToUTF16, 1, (utf8toutf16<Buf, U16MiniBuffer>), ([](Reader<Buf>* r) { return utf8seek_minus<Buf>(r); }), U16MiniBuffer);

#if __cplusplus >= 201703L
    template <class Buf>
    ToUTF8(const Buf&) -> ToUTF8<Buf, b_char_size_v<Buf>>;

    template <class Buf>
    ToUTF16(const Buf&) -> ToUTF16<Buf, b_char_size_v<Buf>>;

    template <class Buf>
    ToUTF32(const Buf&) -> ToUTF32<Buf, b_char_size_v<Buf>>;

    template <class Buf>
    ToUTF8(Buf&&) -> ToUTF8<Buf, b_char_size_v<Buf>>;

    template <class Buf>
    ToUTF16(Buf&&) -> ToUTF16<Buf, b_char_size_v<Buf>>;

    template <class Buf>
    ToUTF32(Buf&&) -> ToUTF32<Buf, b_char_size_v<Buf>>;  //*/
#endif

    template <class Buf>
    using OptUTF8 = std::conditional_t<bcsizeeq<Buf, 1>, Buf, ToUTF8<Buf>>;

    template <class Buf>
    using OptUTF16 = std::conditional_t<bcsizeeq<Buf, 2>, Buf, ToUTF16<Buf>>;

    template <class Buf>
    using OptUTF32 = std::conditional_t<bcsizeeq<Buf, 4>, Buf, ToUTF32<Buf>>;
}  // namespace PROJECT_NAME