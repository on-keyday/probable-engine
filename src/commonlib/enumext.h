/*
    Copyright (c) 2021 on-keyday
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once

#include"project_name.h"

namespace PROJECT_NAME{
    #define DEFINE_ENUMOP(TYPE)                              \
    constexpr TYPE operator&(TYPE a, TYPE b) {           \
        using basety = std::underlying_type_t<TYPE>;     \
        return static_cast<TYPE>((basety)a & (basety)b); \
    }                                                    \
    constexpr TYPE operator|(TYPE a, TYPE b) {           \
        using basety = std::underlying_type_t<TYPE>;     \
        return static_cast<TYPE>((basety)a | (basety)b); \
    }                                                    \
    constexpr TYPE operator~(TYPE a) {                   \
        using basety = std::underlying_type_t<TYPE>;     \
        return static_cast<TYPE>(~(basety)a);            \
    }                                                    \
    constexpr TYPE operator^(TYPE a, TYPE b) {           \
        using basety = std::underlying_type_t<TYPE>;     \
        return static_cast<TYPE>((basety)a ^ (basety)b); \
    }                                                    \
    constexpr TYPE &operator&=(TYPE &a, TYPE b) {        \
        a = a & b;                                       \
        return a;                                        \
    }                                                    \
    constexpr TYPE &operator|=(TYPE &a, TYPE b) {        \
        a = a | b;                                       \
        return a;                                        \
    }                                                    \
    constexpr TYPE &operator^=(TYPE &a, TYPE b) {        \
        a = a ^ b;                                       \
        return a;                                        \
    }                                                    \
    constexpr bool any(TYPE a) {                         \
        using basety = std::underlying_type_t<TYPE>;     \
        return static_cast<basety>(a) != 0;              \
    }


    template<class E,E trueval,E falseval>
    struct EnumWrap{
        E e;
        constexpr EnumWrap():e(falseval){}
        constexpr EnumWrap(bool i):e(i?trueval:falseval){}
        constexpr EnumWrap(E i):e(i){}
        operator bool()const{
            return e==trueval;
        }

        operator E()const{
            return e;
        }

        EnumWrap operator|(const EnumWrap& i)const{
            return e|i.e;  
        }

        EnumWrap operator&(const EnumWrap& i)const{
            return e&i.e;  
        }

        EnumWrap operator^(const EnumWrap& i)const{
            return e^i.e;  
        }

        EnumWrap& operator|=(const EnumWrap& i){
            e|=i.e;
            return *this;  
        }

        EnumWrap& operator&=(const EnumWrap& i){
            e&=i.e;
            return *this;  
        }

        EnumWrap& operator^=(const EnumWrap& i){
            e^=i.e;
            return *this;
        }

        EnumWrap operator~()const{
            return ~e;
        }
    };
}