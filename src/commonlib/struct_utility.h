/*
    commonlib - common utility library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once

#include "project_name.h"

namespace PROJECT_NAME {
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
