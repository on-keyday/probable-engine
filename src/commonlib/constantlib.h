/*
    Copyright (c) 2021 on-keyday
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <p2p.h>

#include <utility>

#include "project_name.h"

namespace PROJECT_NAME {
    template <class T, size_t size>
    struct array_base {
        T elm[size];

        constexpr array_base(T (&in)[size]) {
            for (auto i = 0; i < size; i++) {
                elm[i] = in[i];
            }
        }

        constexpr T& operator[](size_t pos) {
            return elm[pos];
        }

        template <size_t first, size_t second>
        constexpr array_base(const array_base<T, first>& f, const array_base<T, second>& s) {
            static_assert(size == first + second);
            auto i = 0;
            for (; i < first; i++) {
                elm[i] = std::move(f[i]);
            }
            for (; i < size; i++) {
                elm[i] = std::move(s[i - first]);
            }
        }

        constexpr T* begin() const {
            return elm;
        }

        constexpr T* end() const {
            return &elm[size];
        }

        constexpr const T* cbegin() const {
            return elm;
        }

        constexpr const T* cend() const {
            return &elm[size];
        }
    };

    template <class T, size_t first, size_t second>
    constexpr array_base<T, first + second> operator+(const array_base<T, first>& f, const array_base<T, second>& s) {
        return array_base<T, first + second>(f, s);
    }

    template <class T, size_t size>
    struct basic_array {
        array_base<T, size> base;
        constexpr basic_array(const array_base<T, size>& in)
            : base(in) {}
        constexpr basic_array(array_base<T, size>&& in)
            : base(in) {}
        constexpr T& operator[](size_t pos) {
            return base[pos];
        }
        constexpr T* begin() {
            return base.begin();
        }
        constexpr T* end() {
            return base.end();
        }
        constexpr const T* cbegin() {
            return base.cbegin();
        }
        constexpr const T* cend() {
            return base.cend();
        }
        constexpr operator array_base<T, size>&() {
            return base;
        }
    };

}  // namespace PROJECT_NAME
