/*
    Copyright (c) 2021 on-keyday
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#define PROJECT_NAME commonlib2

#if __cplusplus >= 201703L
#define CONSTEXPRIF constexpr
#else
#define CONSTEXPRIF
#endif
#if defined(_MSC_VER) && !defined(__clang__) && !defined(__GNUC__)
#define COMMONLIB2_IS_MSVC
#endif
#if defined(__linux__) || defined(__APPLE__) || defined(__ANDROID__) || defined(__EMSCRIPTEN__)
#define COMMONLIB2_IS_UNIX_LIKE
#endif

#if __cplusplus > 201703L && __has_include(<concepts>)
#define COMMONLIB2_HAS_CONCEPTS
#endif