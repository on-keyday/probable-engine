/*
    socklib - simple socket library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <reader.h>
namespace socklib {
    namespace v2 {
#ifdef COMMONLIB2_HAS_CONCEPTS
        template <class String>
        concept StringType = requires(String s) {
            { s.size() } -> commonlib2::Is_integral;
            {s.push_back('a')};
            {s.append(std::declval<const char*>(), std::declval<size_t>())};
            { s[0] } -> commonlib2::Is_integral;
            { s.find("string") } -> commonlib2::Is_integral;
            {s = std::declval<const String>()};
            {s = std::declval<const char*>()};
            {s += std::declval<const String>()};
            {s += std::declval<const char*>()};
            {s += 'a'};
            {s.erase(0, 1)};
            {s.clear()};
        };

        template <class T>
        concept Is_pointer_to_const = std::is_pointer_v<T> && !std::is_const_v<std::remove_pointer_t<T>>;

        template <class T>
        concept Is_referece = std::is_reference_v<T>;

        template <class Vector>
        concept VectorType = requires(Vector v) {
            {v.push_back('a')};
            {v.resize(10)};
            { v.data() } -> Is_pointer_to_const;
        };

        template <class Header, class String>
        concept HeaderType = requires(Header h) {
            {h.emplace(std::declval<String>(), std::declval<String>())};
        };

        template <class T>
        concept HpackString = requires(T t) {
            { t[0] } -> Is_referece;
            {t.push_back('a')};
        };

        template <class Table, class String>
        concept HpackDynamycTable = requires(Table t) {
            {t.push_front({std::declval<String>(), std::declval<String>()})};
            {t.begin() != t.end()};
            {(*t.begin()).first};
            {(*t.begin()).second};
            {t.size()};
            {t.back().first};
            {t.back().second};
            {t.pop_back()};
            {t[1]};
        };
#endif
    }  // namespace v2
}  // namespace socklib
