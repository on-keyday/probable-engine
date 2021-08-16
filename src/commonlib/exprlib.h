/*
    Copyright (c) 2021 on-keyday
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <project_name.h>

#include <utility>
namespace PROJECT_NAME {
    struct CharObject {
        unsigned char c = 0;

        constexpr CharObject(char in) {
            c = (unsigned char)in;
        }

        constexpr CharObject(CharObject&& in)
            : c(in.c) {}

        constexpr bool operator()(const CharObject& obj) {
            return c == obj.c;
        }
    };

    struct CharRangeObject {
        unsigned char begin = 0;
        unsigned char end = 0;

        constexpr CharRangeObject(CharObject&& f, CharObject&& e) {
            begin = f.c;
            end = e.c;
        }

        constexpr bool operator()(const CharObject& obj) {
            if (begin > end) return false;
            return obj.c >= begin && obj.c <= end;
        }

        template <class Op>
        constexpr bool operator()(const Op& obj) {
            auto o = obj.achar();
            return o >= begin && o <= end;
        }
    };

    constexpr CharRangeObject operator-(CharObject&& l, CharObject&& r) {
        return CharRangeObject(std::forward<CharObject>(l),
                               std::forward<CharObject>(r));
    }

    template <class T, class U>
    struct OrObject {
        T t;
        U u;

        constexpr OrObject(T&& t, U&& u)
            : t(std::forward<T>(t)), u(std::forward<U>(u)) {}

        template <class Op>
        constexpr bool operator()(const Op& obj) {
            return t(obj) || u(obj);
        }
    };

    template <class T>
    constexpr OrObject<T, CharRangeObject> operator|(T&& l, CharRangeObject&& r) {
        return OrObject<T, CharRangeObject>(
            std::forward<T>(l),
            std::forward<CharRangeObject>(r));
    }

    template <class T>
    constexpr OrObject<T, CharObject> operator|(T&& l, CharObject&& r) {
        return OrObject<T, CharObject>(
            std::forward<T>(l),
            std::forward<CharObject>(r));
    }

    template <class Obj>
    struct RepeatObject {
        Obj o;
        constexpr RepeatObject(Obj&& in)
            : o(std::forward<Obj>(in)) {}

        template <class Op>
        constexpr bool operator()(Op&& in) {
            while (o(in)) {
                in.increment();
            }
            return true;
        }
    };

    template <class T, class U>
    struct AndObject {
        T t;
        U u;

        constexpr AndObject(T&& t, U&& u)
            : t(std::forward<T>(t)), u(std::forward<U>(u)) {}

        template <class Op>
        constexpr bool operator()(const Op& obj) {
            return t(obj) && u(obj);
        }
    };

    template <class T, class U>
    constexpr AndObject<T, U> operator&(T&& t, U&& u) {
        return AndObject<T, U>(std::forward<T>(t), std::forward<U>(u));
    }

    template <class T, class U>
    constexpr RepeatObject<OrObject<T, U>> operator*(OrObject<T, U>&& in) {
        return RepeatObject<OrObject<T, U>>(std::forward<OrObject<T, U>>(in));
    }

    struct PlaceHolder {
        constexpr CharRangeObject operator[](CharRangeObject in) const {
            return in;
        }
        constexpr CharObject operator()(char c) const {
            return CharObject(c);
        }
    };

    const PlaceHolder _p = {};

    constexpr auto identifier = (_p[_p('a') - 'z'] | _p[_p('A') - 'Z'] | _p('_')) & *(_p[_p('a') - 'z'] | _p[_p('A') - 'Z'] | _p[_p('0') - '9'] | _p('_'));
}  // namespace PROJECT_NAME