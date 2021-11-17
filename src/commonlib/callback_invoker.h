/*
    commonlib - common utility library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "project_name.h"
#include <type_traits>

namespace PROJECT_NAME {

#define DEFINE_ENABLE_IF_EXPR_VALID(NAME, EXPR)              \
    struct NAME##_impl {                                     \
        template <class T>                                   \
        static std::true_type has(decltype(EXPR, (void)0)*); \
        template <class T>                                   \
        static std::false_type has(...);                     \
    };                                                       \
    template <class T>                                       \
    struct NAME : decltype(NAME##_impl::has<T>(nullptr)) {}

    DEFINE_ENABLE_IF_EXPR_VALID(has_bool, (bool)std::declval<T>());

    template <class F, class Ret, bool f = has_bool<F>::value>
    struct invoke_cb {
        template <class... Args>
        static Ret invoke(F&& in, Args&&... args) {
            return in(std::forward<Args>(args)...);
        }
    };

    template <class F, class Ret>
    struct invoke_cb<F, Ret, true> {
        template <class... Args>
        static Ret invoke(F&& in, Args&&... args) {
            if (!(bool)in) return (Ret) true;
            return in(std::forward<Args>(args)...);
        }
    };

    template <class F, class Ret, Ret defaultvalue, bool f = has_bool<F>::value>
    struct Invoker {
        template <class... Args>
        static Ret invoke(F&& in, Args&&... args) {
            return in(std::forward<Args>(args)...);
        }
    };

    template <class F, class Ret, Ret defaultvalue>
    struct Invoker<F, Ret, defaultvalue, true> {
        template <class... Args>
        static Ret invoke(F&& in, Args&&... args) {
            if (!(bool)in) return defaultvalue;
            return (Ret)in(std::forward<Args>(args)...);
        }
    };

}  // namespace PROJECT_NAME