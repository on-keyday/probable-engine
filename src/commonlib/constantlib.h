/*
    commonlib - common utility library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
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

    
    template <class C=char, size_t size_ = 1>
    struct ConstString {
        static_assert(size_!=0,"size must not be 0");
        C buf[size_] = {0};

        constexpr static size_t strsize = (size_t)(size_-1);
        constexpr ConstString() {}

        constexpr ConstString(const ConstString& in) {
            for (size_t i = 0; i < size_; i++) {
                buf[i] = in.buf[i];
            }
        }

        constexpr ConstString(const C (&ptr)[size_]) {
            for (size_t i = 0; i <size_-1; i++) {
                buf[i] = ptr[i];
            }
            buf[size_-1]=0;
        }

        constexpr size_t size() const {
            return strsize;
        }

        template<size_t ofs=0,size_t count=(size_t)~0>
        constexpr auto substr()const{
            static_assert(ofs < strsize,"invalid offset");
            constexpr size_t sz= count < strsize - ofs ? count : strsize - ofs;
            C copy[sz+1];
            for(auto i=0;i<sz;i++){
                copy[i]=buf[ofs+i];
            }
            copy[sz]=0;
            return ConstString<C,sz+1>(copy);
        }

        constexpr ConstString<C, size_ + 1> push_back(C c) const {
            C copy[size_ + 1];
            for (size_t i = 0; i < strsize; i++) {
                copy[i] = buf[i];
            }
            copy[strsize] = c;
            return ConstString<C, size_ + 1>(copy);
        }

        constexpr ConstString<C, size_ + 1> push_front(C c) const {
            C copy[size_ + 1];
            copy[0]=c;
            for (size_t i = 0; i < strsize+1; i++) {
                copy[i] = buf[i];
            }
            return ConstString<C, size_ + 1>(copy);
        }

        constexpr auto pop_back() const {
            static_assert(size_!=1,"pop_back from 0 size string is invalid");
            return substr<0,strsize-1>();
        }

        constexpr auto pop_front() const {
            static_assert(size_!=1,"pop_front from 0 size string is invalid");
            return substr<1>();
        }

    private:

        template<size_t add,class T>
        constexpr auto appending(const T& src)const{
            ConstString<C,strsize+add> copy;
            for(size_t i=0;i<strsize;i++){
                copy[i]=buf[i];
            }
            for(size_t i=0;i<add-1;i++){
                copy[i+strsize]=src[i];
            }
            copy[strsize+add-1]=0;
            return copy;
        }

    public:
        template <size_t add>
        constexpr auto append(const ConstString<C, add>& src) const {
            return appending<add>(src);
        }

        template <size_t add>
        constexpr auto append(const C (&src)[add]) const {
            return appending<add>(src);
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

        constexpr C* data(){
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
  
    };

    template<class T1,class T2>
    constexpr bool equal(const T1& s1,size_t sz1,const T2& s2,size_t sz2){
        if (sz1 != sz2) return false;
        for (size_t i = 0; i < sz1; i++) {
            if (s1[i] != s2[i]) {
                return false;
            }
        }
        return true;
    }

    template<class C>
    constexpr size_t const_strlen(const C* str){
        size_t idx=0;
        while(str&&str[idx]){
            idx++;
        }
        return idx;
    }

    template<size_t sz1,size_t sz2,class T1,class T2>
    constexpr bool equal(const T1& s1,const T2& s2){
        return equal<T1,T2>(s1,sz1,s2,sz2);
    }

    template <class C, size_t sz1, size_t sz2>
    constexpr bool operator==(const ConstString<C, sz1>& s1, const ConstString<C, sz2>& s2) {
        return equal<sz1,sz2>(s1,s2);
    }

    template <class C, size_t sz1, size_t sz2>
    constexpr bool operator==(const C (&s1)[sz1], const ConstString<C, sz2>& s2) {
        return equal<sz1,sz2>(s1,s2);
    }

    template <class C, size_t sz1, size_t sz2>
    constexpr bool operator==(const ConstString<C, sz1>& s1, const C(&s2)[sz2]) {
        return equal<sz1,sz2>(s1,s2);
    }

    template <class C, size_t sz1>
    constexpr bool operator==(const ConstString<C, sz1>& s1, const C* s2) {
        return equal(s1,sz1-1,s2,const_strlen(s2));
    }

    template <class C, size_t sz2>
    constexpr bool operator==(const C* s1,const ConstString<C, sz2>& s2) {
        return equal(s1,const_strlen(s1),s2,sz2-1);
    }

    template<class C,size_t sz1,size_t sz2>
    constexpr auto operator+(const ConstString<C, sz1>& s1, const ConstString<C, sz2>& s2){
        return s1.append(s2);
    }

    template<class C,size_t sz1,size_t sz2>
    constexpr auto operator+(const C (&s1)[sz1],const ConstString<C, sz2>& s2){
        return ConstString<C,sz1>(s1).append(s2);
    }


    template<class C,size_t sz1,size_t sz2>
    constexpr auto operator+(const ConstString<C, sz1>& s1,const C (&s2)[sz2]){
        return s1.append(s2);
    }

}  // namespace PROJECT_NAME
