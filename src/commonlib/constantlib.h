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

    
    template <class C, size_t size_ = 0>
    struct ConstString {
        C buf[size_] = {0};

        constexpr static size_t strsize = size_ - 1;
        constexpr ConstString() {}

        constexpr ConstString(const ConstString& in) {
            for (size_t i = 0; i < strsize; i++) {
                buf[i] = in.buf[i];
            }
        }

        constexpr ConstString(const C (&ptr)[size_]) {
            for (size_t i = 0; i < strsize; i++) {
                buf[i] = ptr[i];
            }
        }

        constexpr size_t size() const {
            return strsize;
        }

        constexpr ConstString<C, size_ + 1> push_back(C c) const {
            C copy[size_ + 1];
            for (size_t i = 0; i < strsize; i++) {
                copy[i] = buf[i];
            }
            copy[strsize] = c;
            return ConstString<C, size_ + 1>(copy);
        }

    private:
        template <size_t sz, size_t add, size_t srcsize, bool flag = (add == srcsize - 1)>
        struct Appender {
            constexpr static auto appending(const ConstString<C, sz + add>& dst, const ConstString<C, srcsize>& src, size_t idx) {
                auto s = dst.push_back(src[idx]);
                return Appender<sz, add + 1, srcsize>::appending(s, src, idx + 1);
            }
        };

        template <size_t sz, size_t add, size_t srcsize>
        struct Appender<sz, add, srcsize, true> {
            constexpr static auto appending(const ConstString<C, sz + add>& dst, const ConstString<C, srcsize>& src, size_t idx) {
                return ConstString<C, sz + add>(dst);
            }
        };

    public:
        template <size_t add>
        constexpr auto append(const ConstString<C, add>& src) const {
            return Appender<size_, 0, add>::appending(*this, src, 0);
        }

        template <size_t add>
        constexpr auto append(const C (&str)[add]) const {
            ConstString<C, add> src(str);
            return Appender<size_, 0, add>::appending(*this, src, 0);
        }

        constexpr const C& operator[](size_t idx) const {
            if (idx >= size_) {
                throw "out of range";
            }
            return buf[idx];
        }

        constexpr C& operator[](size_t idx) {
            if (idx >= size_) {
                throw "out of range";
            }
            return buf[idx];
        }

        constexpr const C* c_str() const {
            return buf;
        }

        constexpr C* begin() {
            return buf;
        }

        constexpr const C* begin() const {
            return buf;
        }

        constexpr C* end() {
            return buf + strsize;
        }

        constexpr const C* end() const {
            return buf + strsize;
        }

        template<size_t ofs=0,size_t count=(size_t)~0>
        constexpr auto substr()const{
            constexpr size_t sz= count < size_ ? count : size_;
            C copy[sz-ofs];
            for(auto i=0;i<sz-ofs;i++){
                copy[i]=buf[ofs+i];
            }
            return ConstString<C,sz-ofs>(copy);
        }
    };

    template <class C, size_t sz1, size_t sz2>
    constexpr bool operator==(const ConstString<C, sz1>& s1, const ConstString<C, sz2>& s2) {
        if (sz1 != sz2) return false;
        for (size_t i = 0; i < sz1; i++) {
            if (s1[i] != s2[i]) {
                return false;
            }
        }
        return true;
    }


}  // namespace PROJECT_NAME
