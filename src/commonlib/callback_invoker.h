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
    struct has_bool_impl {
        template <class F>
        static std::true_type has(decltype((bool)std::declval<F>(), (void)0)*);

        template <class F>
        static std::false_type has(...);
    };

    template <class F>
    struct has_bool : decltype(has_bool_impl::has<F>(nullptr)) {};

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

}  // namespace PROJECT_NAME