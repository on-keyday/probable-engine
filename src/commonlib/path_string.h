/*
    commonlib - common utility library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once

#include "project_name.h"
#include <string>
#include "extension_operator.h"

namespace PROJECT_NAME {
#ifdef _WIN32
    using path_string = std::wstring;
#else
    using path_string = std::string;
#endif

    struct ToPath {
        path_string path;
        template <class String>
        ToPath(const String& p) {
            Reader<const String&>(p) >> path;
        }

        operator path_string&() {
            return path;
        }

        auto c_str() const {
            return path.c_str();
        }
    };

}  // namespace PROJECT_NAME
