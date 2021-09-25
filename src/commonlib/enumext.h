/*
    Copyright (c) 2021 on-keyday
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once

#include "project_name.h"

namespace PROJECT_NAME {
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
    constexpr TYPE& operator&=(TYPE& a, TYPE b) {        \
        a = a & b;                                       \
        return a;                                        \
    }                                                    \
    constexpr TYPE& operator|=(TYPE& a, TYPE b) {        \
        a = a | b;                                       \
        return a;                                        \
    }                                                    \
    constexpr TYPE& operator^=(TYPE& a, TYPE b) {        \
        a = a ^ b;                                       \
        return a;                                        \
    }                                                    \
    constexpr bool any(TYPE a) {                         \
        using basety = std::underlying_type_t<TYPE>;     \
        return static_cast<basety>(a) != 0;              \
    }

    template <class E, E trueval, E falseval>
    struct EnumWrap {
        E e;
        constexpr EnumWrap()
            : e(falseval) {}
        constexpr EnumWrap(bool i)
            : e(i ? trueval : falseval) {}
        constexpr EnumWrap(E i)
            : e(i) {}
        constexpr operator bool() const {
            return e == trueval;
        }

        constexpr operator E() const {
            return e;
        }

        constexpr EnumWrap operator|(const EnumWrap& i) const {
            return e | i.e;
        }

        constexpr EnumWrap operator&(const EnumWrap& i) const {
            return e & i.e;
        }

        constexpr EnumWrap operator^(const EnumWrap& i) const {
            return e ^ i.e;
        }

        constexpr EnumWrap& operator|=(const EnumWrap& i) {
            e |= i.e;
            return *this;
        }

        constexpr EnumWrap& operator&=(const EnumWrap& i) {
            e &= i.e;
            return *this;
        }

        constexpr EnumWrap& operator^=(const EnumWrap& i) {
            e ^= i.e;
            return *this;
        }

        constexpr EnumWrap operator~() const {
            return ~e;
        }
    };

#define BEGIN_ENUM_ERROR_MSG(TYPE)                \
    constexpr const char* error_message(TYPE e) { \
        switch (e) {
#define ENUM_ERROR_MSG(e, word) \
    case e:                     \
        return word;
#define END_ENUM_ERROR_MSG      \
    default:                    \
        return "unknown error"; \
        }                       \
        }
}  // namespace PROJECT_NAME