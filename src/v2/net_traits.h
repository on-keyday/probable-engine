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
#endif
    }  // namespace v2
}  // namespace socklib