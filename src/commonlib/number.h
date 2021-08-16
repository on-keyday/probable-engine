/*
    Copyright (c) 2021 on-keyday
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <limits>
#include <stdexcept>
#include <type_traits>

#include "learnstd.h"
#include "project_name.h"
namespace PROJECT_NAME {

    struct divbyzero_exception : std::logic_error {
        using base = std::logic_error;

        divbyzero_exception(const char* str)
            : base(str) {}
        divbyzero_exception(const std::string& str)
            : base(str) {}
    };

    template <class T, class U>
    struct Get_Int_Type_impl {
        constexpr static bool same = std::is_same_v<T, U>;
        constexpr static bool large = sizeof(T) != sizeof(U);
        using unsignedTy = std::conditional_t<std::is_unsigned_v<T>, T, U>;
        using largerTy = std::conditional_t<(sizeof(T) > sizeof(U)), T, U>;

        using type = std::conditional_t<same, T, std::conditional_t<large, largerTy, unsignedTy>>;
    };

    template <class T, class U>
    using Get_Int_Type = typename Get_Int_Type_impl<T, U>::type;

    template <class T, class = std::enable_if_t<std::is_integral_v<T>>>
    struct Int {
        T value;
        Int(T v)
            : value(v) {}
        Int(const Int& v)
            : value(v.value) {}

        Int<T> operator~() const {
            return ~value;
        }

        operator const T&() const {
            return value;
        }

        template <class U>
        explicit operator Int<U>() {
            if CONSTEXPRIF (sizeof(U) < sizeof(T)) {
#ifdef max
#pragma push_macro("max")
#define DEFINED_MAX
#undef max
#endif
                if (std::numeric_limits<U>::max() < value) {
                    throw std::runtime_error("cast error");
                }
#ifdef DEFINED_MAX
#define max(a, b)
#undef DEFINED_MAX
#pragma pop_macro("max")
#endif
            }
            return Int<U>(value);
        }
    };

    template <>
    struct Int<bool>;

#define DEFINE_INT_OPERATOR(OP, COND, EXC)                                       \
    template <class T, class U>                                                  \
    Int<Get_Int_Type<T, U>> operator OP(const Int<T>& t, const Int<U>& u) {      \
        if (COND) throw EXC;                                                     \
        return t.value OP u.value;                                               \
    }                                                                            \
    template <class T, class U, class = std::enable_if_t<std::is_integral_v<T>>> \
    Int<Get_Int_Type<T, U>> operator OP(T t, const Int<U>& u) {                  \
        return Int<T>(t) OP u;                                                   \
    }                                                                            \
    template <class T, class U, class = std::enable_if_t<std::is_integral_v<U>>> \
    Int<Get_Int_Type<T, U>> operator OP(const Int<T>& t, U u) {                  \
        return t OP Int<U>(u);                                                   \
    }
#define DEFINE_ASSIGN_OPERATOR(OP, COND, EXC)         \
    template <class T>                                \
    Int<T>& operator OP(Int<T>& t, const Int<T>& u) { \
        if (COND) throw EXC;                          \
        t.value OP u.value;                           \
        return t;                                     \
    }                                                 \
    template <class T>                                \
    Int<T>& operator OP(Int<T>& t, T u) {             \
        return t OP Int<T>(u);                        \
    }

    DEFINE_INT_OPERATOR(+, false, "")
    DEFINE_INT_OPERATOR(-, false, "")
    DEFINE_INT_OPERATOR(*, false, "")
    DEFINE_INT_OPERATOR(/, u.value == 0, divbyzero_exception("DIV By ZERO"))
    DEFINE_INT_OPERATOR(%, u.value == 0, divbyzero_exception("MOD By ZERO"))
    DEFINE_INT_OPERATOR(<<, u.value > sizeof(T) * 8, std::invalid_argument("too large number"))
    DEFINE_INT_OPERATOR(>>, u.value > sizeof(T) * 8, std::invalid_argument("too large number"))
    DEFINE_INT_OPERATOR(&, false, "")
    DEFINE_INT_OPERATOR(|, false, "")
    DEFINE_INT_OPERATOR(^, false, "")
    DEFINE_ASSIGN_OPERATOR(+=, false, "")
    DEFINE_ASSIGN_OPERATOR(-=, false, "")
    DEFINE_ASSIGN_OPERATOR(*=, false, "")
    DEFINE_ASSIGN_OPERATOR(/=, u.value == 0, divbyzero_exception("DIV By ZERO"))
    DEFINE_ASSIGN_OPERATOR(%=, u.value == 0, divbyzero_exception("MOD By ZERO"))
    DEFINE_ASSIGN_OPERATOR(<<=, u.value > sizeof(T) * 8, std::invalid_argument("too large number"))
    DEFINE_ASSIGN_OPERATOR(>>=, u.value > sizeof(T) * 8, std::invalid_argument("too large number"))
    DEFINE_ASSIGN_OPERATOR(&=, false, "")
    DEFINE_ASSIGN_OPERATOR(|=, false, "")
    DEFINE_ASSIGN_OPERATOR(^=, false, "")
#undef DEFINE_INT_OPERATOR
#undef DEFINE_ASSIGN_OPERATOR

    enum class CalcType {
        constant,
        var,
    };

    struct Calcable {
        CalcType type;
        Anything obj;

        template <class Ret = double, class... Args>
        Ret operator()(Args&&... d) {
            switch {
                case CalcType::constant:
                    return (Ret)cast_to<double>(obj);
                case CalcType::var:
            }
        }
    };

}  // namespace PROJECT_NAME