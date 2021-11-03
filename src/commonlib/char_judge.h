/*
    commonlib - common utility library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once

#include "project_name.h"

namespace PROJECT_NAME {
    template <class Char>
    bool is_digit(Char c) {
        return c >= (Char)'0' && c <= (Char)'9';
    }

    template <class Char>
    bool is_oct(Char c) {
        return c >= (Char)'0' && c <= (Char)'7';
    }

    template <class Char>
    bool is_hex(Char c) {
        return c >= (Char)'a' && c <= (Char)'f' ||
               c >= (Char)'A' && c <= (Char)'F' ||
               is_digit(c);
    }

    template <class Char>
    bool is_bin(Char c) {
        return c == (Char)'0' || c == (Char)'1';
    }

    template <class Char>
    bool is_alphabet(Char c) {
        return c >= (Char)'a' && c <= (Char)'z' ||
               c >= (Char)'A' && c <= (Char)'Z';
    }

    template <class Char>
    bool is_string_symbol(Char c) {
        return c == (Char)'\"' || c == (Char)'\'' || c == (Char)'`';
    }

    template <class Char>
    bool is_alpha_or_num(Char c) {
        return is_alphabet(c) || is_digit(c);
    }

    template <class Char>
    bool is_c_id_usable(Char c) {
        return is_alpha_or_num(c) || c == (Char)'_';
    }

    template <class Char>
    bool is_c_id_top_usable(Char c) {
        return is_alphabet(c) || c == (Char)'_';
    }

    template <class Char>
    bool is_url_host_usable(Char c) {
        return is_alpha_or_num(c) || c == (Char)'-' || c == (Char)'.';
    }

    template <class Char>
    bool is_url_scheme_usable(Char c) {
        return is_alpha_or_num(c) || c == (Char)'+';
    }
}  // namespace PROJECT_NAME
