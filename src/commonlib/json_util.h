/*
    commonlib - common utility library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <stdint.h>

#include <cstring>
#include <regex>
#include <string>
#include <unordered_map>
#include <vector>

#include "basic_helper.h"
#include "reader.h"
#include "extension_operator.h"

namespace PROJECT_NAME {
    enum class JSONType {
        unset,
        null,
        boolean,
        integer,
        unsignedi,
        floats,
        string,
        object,
        array
    };

    enum class JSONFormat {
        space = 0x1,
        tab = 0x2,
        mustline = 0x4,
        elmpush = 0x8,
        afterspace = 0x10,
        mustspace = 0x20,
        endline = 0x40,
        escape = 0x80,
        must = mustline | mustspace,
        defaultf = space | afterspace | endline,
        tabnormal = tab | afterspace | endline,
        shift = space | elmpush | afterspace,
        spacemust = space | mustspace,
        noendline = space | afterspace,
        noendlinetab = tab | afterspace
    };

    /*auto operator|(JSONFormat l,JSONFormat r){
        using basety=std::underlying_type_t<JSONFormat>;
        return static_cast<JSONFormat>((basety)l|(basety)r);
    }*/

    DEFINE_ENUMOP(JSONFormat)

    struct EasyStr {
       private:
        char* str = nullptr;
        size_t len = 0;
        void construct(const char* from, size_t fromlen) {
            try {
                str = new char[fromlen + 1];
                len = fromlen;
            } catch (...) {
                str = nullptr;
                len = 0;
                return;
            }
            memmove(str, from, fromlen);
            str[len] = 0;
        }

       public:
        EasyStr() {
            str = nullptr;
            len = 0;
        }

        EasyStr(const char* from, size_t fromlen) {
            construct(from, fromlen);
        }

        EasyStr(const char* from) {
            construct(from, strlen(from));
        }

        EasyStr(const EasyStr&) = delete;
        EasyStr(EasyStr&& from) noexcept {
            str = from.str;
            len = from.len;
            from.str = nullptr;
            from.len = 0;
        }
        EasyStr& operator=(EasyStr&& from) noexcept {
            if (str != nullptr && str == from.str) return *this;
            delete[] str;
            str = from.str;
            len = from.len;
            from.str = nullptr;
            from.len = 0;
            return *this;
        }

        char& operator[](size_t pos) {
            if (pos > len) return str[len];
            return str[pos];
        }

        const char* const_str() const {
            return str;
        }
        size_t size() const {
            return len;
        }

        ~EasyStr() {
            delete[] str;
            str = nullptr;
            len = 0;
        }
    };

    template <class String>
    bool json_escape(const String& base, String& out, bool escape_utf = false) {
        Reader<ToUTF16<String>> r(base);
        if (base.size() && !r.ref().size()) {
            return false;
        }
        String& s = out;
        while (!r.ceof()) {
            char16_t c = r.achar();
            if (c == '\\') {
                s += "\\\\";
            }
            else if (c == '"') {
                s += "\\\"";
            }
            else if (c == '\n') {
                s += "\\n";
            }
            else if (c == '\r') {
                s += "\\r";
            }
            else if (c == '\t') {
                s += "\\t";
            }
            else if (c < 0x20 || c > 0x7E) {
                if (c < 0x20 || c == 0x7F || escape_utf) {
                    auto encode = translate_byte_net_and_host<std::uint16_t>(&c);
                    const unsigned char* ptr = (const unsigned char*)&encode;
                    auto translate = [](unsigned char c) -> unsigned char {
                        if (c > 15) return 0;
                        if (c < 10) {
                            return c + '0';
                        }
                        else {
                            return c - 10 + 'a';
                        }
                    };
                    s += "\\u";
                    s += translate((ptr[0] & 0xf0) >> 4);
                    s += translate(ptr[0] & 0xf0);
                    s += translate((ptr[1] & 0xf0) >> 4);
                    s += translate(ptr[1] & 0xf0);
                }
                else {
                    U16MiniBuffer buf;
                    buf.push_back(c);
                    if (is_utf16_surrogate_high(c)) {
                        r.increment();
                        c = r.achar();
                        if (!is_utf16_surrogate_low(c)) {
                            return false;
                        }
                        buf.push_back(c);
                    }
                    Reader(buf) >> s;
                }
            }
            else {
                s += (char)c;
            }
            r.increment();
        }
        return true;
    }

    template <class String>
    bool json_unescape(const String& base, String& out) {
        std::string& result = out;
        commonlib2::Reader<std::string> r(base);
        while (!r.ceof()) {
            if (r.achar() == '\\') {
                if (r.expect("\\n")) {
                    result += '\n';
                }
                else if (r.expect("\\a")) {
                    result += '\a';
                }
                else if (r.expect("\\b")) {
                    result += '\b';
                }
                else if (r.expect("\\t")) {
                    result += '\t';
                }
                else if (r.expect("\\r")) {
                    result += '\r';
                }
                else if (r.expect("\\v")) {
                    result += '\v';
                }
                else if (r.expect("\\e")) {
                    result += '\e';
                }
                else if (r.expect("\\f")) {
                    result += '\f';
                }
                else if (r.expect("\\0")) {
                    result += "\0";
                }
                else if (r.expect("\\u")) {
                    auto read_four = [&](unsigned short& e) {
                        char buf[7] = "0x";
                        if (r.read_byte(buf + 2, 4, translate_byte_as_is, true) < 4) {
                            return false;
                        }
                        for (auto i = 0; i < 4; i++) {
                            if (!is_hex(buf[2 + i])) {
                                return false;
                            }
                        }
                        e = 0;
                        Reader<const char*> tmp(buf);
                        tmp >> e;
                        return true;
                    };
                    U16MiniBuffer buf;
                    unsigned short i = 0, k = 0;
                    if (!read_four(i)) {
                        return false;
                    }
                    if (is_utf16_surrogate_high(i)) {
                        if (!r.expect("\\u")) {
                            return false;
                        }
                        if (!read_four(k)) {
                            return false;
                        }
                        buf.push_back(i);
                        buf.push_back(k);
                    }
                    else {
                        buf.push_back(i);
                    }
                    Reader(buf) >> result;
                }
                else {
                    r.increment();
                    if (r.ceof()) {
                        return false;
                    }
                    result += r.achar();
                    r.increment();
                }
            }
            else {
                result += r.achar();
                r.increment();
            }
        }
        return true;
    }

    template <template <class...> class Map = std::unordered_map, template <class...> class Vec = std::vector>
    struct JSON {
        using JSONObjectType = Map<std::string, JSON>;
        using JSONArrayType = Vec<JSON>;

       private:
        union {
            bool boolean;
            int64_t numi;
            uint64_t numu;
            double numf;
            EasyStr value = EasyStr();
            JSONObjectType* obj;
            JSONArrayType* array;
        };
        JSONType type = JSONType::unset;

        inline static bool parse_ull(const std::string& str, uint64_t& n) {
            return parse_int(str, n);
        }

        template <class T>
        T* init() {
            try {
                return new T();
            } catch (...) {
                return nullptr;
            }
        }

        bool init_as_obj(JSONObjectType&& json) {
            obj = init<JSONObjectType>();
            if (!obj) {
                type = JSONType::unset;
                return false;
            }
            type = JSONType::object;
            *obj = std::move(json);
            return true;
        }

        bool init_as_array(JSONArrayType&& json) {
            array = init<JSONArrayType>();
            if (!array) {
                type = JSONType::unset;
                return false;
            }
            type = JSONType::array;
            *array = std::move(json);
            return true;
        }

        template <class Num>
        bool init_as_num(Num num) {
            type = JSONType::integer;
            numi = num;
            return true;
        }

        void destruct() {
            if (type == JSONType::object) {
                delete obj;
                obj = nullptr;
            }
            else if (type == JSONType::array) {
                delete array;
                array = nullptr;
            }
            else if (type == JSONType::string) {
                value.~EasyStr();
            }
            else {
                numu = 0;
            }
        }

        JSON& move_assign(JSON&& from) {
            this->type = from.type;
            if (type == JSONType::object) {
                obj = from.obj;
                from.obj = nullptr;
            }
            else if (type == JSONType::array) {
                array = from.array;
                from.array = nullptr;
            }
            else if (type == JSONType::boolean) {
                boolean = from.boolean;
                from.boolean = false;
            }
            else if (type == JSONType::integer) {
                numi = from.numi;
                from.numi = 0;
            }
            else if (type == JSONType::unsignedi) {
                numu = from.numu;
                from.numu = 0;
            }
            else if (type == JSONType::floats) {
                numf = from.numf;
                from.numi = 0;
            }
            else if (type == JSONType::null || type == JSONType::unset) {
            }
            else if (type == JSONType::string) {
                value = std::move(from.value);
            }
            from.type = JSONType::unset;
            return *this;
        }

        JSON& copy_assign(const JSON& from) {
            this->type = from.type;
            if (type == JSONType::object) {
                *this = JSON("", type);
                auto& m = *from.obj;
                for (auto& i : m) {
                    obj->operator[](i.first) = i.second;
                }
            }
            else if (type == JSONType::array) {
                *this = JSON("", type);
                auto& m = *from.array;
                for (auto& i : m) {
                    array->push_back(i);
                }
            }
            else if (type == JSONType::boolean) {
                boolean = from.boolean;
            }
            else if (type == JSONType::null || type == JSONType::unset) {
            }
            else if (type == JSONType::integer) {
                numi = from.numi;
            }
            else if (type == JSONType::unsignedi) {
                numu = from.numu;
            }
            else if (type == JSONType::floats) {
                numf = from.numf;
            }
            else if (type == JSONType::string) {
                value = EasyStr(from.value.const_str(), from.value.size());
            }
            return *this;
        }

        template <class Buf>
        static JSON parse_json_detail(Reader<Buf>& reader, const char** err) {
            auto error = [&](const char* msg) {
                *err = msg;
                return JSON();
            };
            const char* expected = nullptr;
            if (reader.expect("{")) {
                JSON ret("", JSONType::object);
                if (!reader.expect("}")) {
                    while (true) {
                        std::string key;
                        if (!reader.ahead("\"")) return error("expect \" but not");
                        if (!reader.string(key, true)) return error("unreadable key");
                        if (!reader.expect(":")) return error("expect : but not");
                        key.pop_back();
                        key.erase(0, 1);
                        auto tmp = parse_json_detail(reader, &expected);
                        if (expected) return error(expected);
                        ret[key] = tmp;
                        if (reader.expect(",")) continue;
                        if (!reader.expect("}")) return error("expected , or } but not");
                        break;
                    }
                }
                return ret;
            }
            else if (reader.expect("[")) {
                JSON ret("", JSONType::array);
                if (!reader.expect("]")) {
                    while (true) {
                        auto tmp = parse_json_detail(reader, &expected);
                        if (expected) return error(expected);
                        ret.push_back(std::move(tmp));
                        if (reader.expect(",")) continue;
                        if (!reader.expect("]")) return error("expect , or ] but not");
                        break;
                    }
                }
                return ret;
            }
            else if (reader.ahead("\"")) {
                std::string str, result;
                if (!reader.string(str, true)) return error("unreadable string");
                str.pop_back();
                str.erase(0, 1);
                if (!json_unescape(str, result)) {
                    return error("failed to parse escape");
                }
                return JSON(result, JSONType::string);
            }
            else if (reader.achar() == '-' || is_digit(reader.achar())) {
                bool minus = false;
                if (reader.expect("-")) {
                    minus = true;
                }
                std::string num;
                NumberContext<char> ctx;
                reader.readwhile(num, number, &ctx);
                if (ctx.failed)
                    return error("invalid number");
                if (ctx.radix != 10)
                    return error("radix is not 10");
                auto parse_fl = [&] {
                    double f;
                    if (!parse_float(num, f)) {
                        return error("undecodable number");
                    }
                    return JSON(minus ? -f : f);
                };
                if (any(ctx.flag & NumberFlag::floatf)) {
                    return parse_fl();
                }
                else {
                    uint64_t n = 0;
                    if (parse_ull(num, n)) {
                        if (n & 0x80'00'00'00'00'00'00'00) {
                            if (minus) {
                                double df = (double)n;
                                return JSON(-df);
                            }
                            else {
                                return JSON(n);
                            }
                        }
                        else {
                            auto sn = (long long)n;
                            return JSON(minus ? -sn : sn);
                        }
                    }
                    else {
                        return parse_fl();
                    }
                }
            }
            else if (reader.expectp("true", expected, is_c_id_usable) ||
                     reader.expectp("false", expected, is_c_id_usable)) {
                return JSON(expected, JSONType::boolean);
            }
            else if (reader.expect("null", is_c_id_usable)) {
                return JSON(nullptr);
            }
            return error("not json");
        }

        std::string to_string_detail(JSONFormat format, size_t ofs, size_t indent, size_t base_skip) const {
            auto flag = [&format](auto n) {
                return any(format & n);
            };
            if (type == JSONType::unset)
                return "";
            std::string ret;
            bool first = true;
            auto fmtprint = [&flag, &base_skip, &ret, &indent](size_t ofs) {
                if ((indent == 0) && !flag(JSONFormat::mustline)) return;
                ret += "\n";
                if (flag(JSONFormat::tab)) {
                    for (auto i = 0; i < ofs; i++)
                        for (auto u = 0; u < indent; u++) ret += "\t";
                }
                else if (flag(JSONFormat::space)) {
                    for (auto i = 0; i < ofs; i++)
                        for (auto u = 0; u < indent; u++) ret += " ";
                }
                if (flag(JSONFormat::elmpush))
                    for (auto i = 0; i < base_skip; i++) ret += " ";
            };
            if (type == JSONType::object) {
                ret += "{";
                for (auto& kv : *obj) {
                    if (first) {
                        first = false;
                    }
                    else {
                        ret += ",";
                    }
                    fmtprint(ofs);
                    auto adds = "\"" + kv.first + "\"" + ":";
                    ret += adds;
                    if ((indent && flag(JSONFormat::afterspace)) || flag(JSONFormat::mustspace)) {
                        ret += " ";
                    }
                    auto tmp = kv.second.to_string_detail(format, ofs + 1, indent, base_skip + adds.size() + 1);
                    if (tmp == "") return "";
                    ret += tmp;
                }
                if (obj->size()) fmtprint(ofs - 1);
                ret += "}";
            }
            else if (type == JSONType::array) {
                ret += "[";
                for (auto& kv : *array) {
                    if (first) {
                        first = false;
                    }
                    else {
                        ret += ",";
                    }
                    fmtprint(ofs);
                    auto tmp = kv.to_string_detail(format, ofs + 1, indent, base_skip);
                    if (tmp == "") return "";
                    ret += tmp;
                }
                if (array->size()) fmtprint(ofs - 1);
                ret += "]";
            }
            else if (type == JSONType::null) {
                ret = "null";
            }
            else if (type == JSONType::boolean) {
                ret = boolean ? "true" : "false";
            }
            else if (type == JSONType::string) {
                ret = "\"";
                std::string res;
                json_escape(std::string(value.const_str(), value.size()), res, flag(JSONFormat::escape));
                ret.append(res);
                ret += "\"";
            }
            else if (type == JSONType::integer) {
                ret = std::to_string(numi);
            }
            else if (type == JSONType::unsignedi) {
                ret = std::to_string(numu);
            }
            else if (type == JSONType::floats) {
                ret = std::to_string(numf);
            }
            return ret;
        }

        template <class Struct>
        auto to_json_detail(JSON& to, const Struct& in)
            -> decltype(to_json(to, in)) {
            to_json(to, in);
        }

        template <class Struct>
        auto to_json_detail(JSON& to, const Struct& in)
            -> decltype(in.to_json(to)) {
            in.to_json(to);
        }

        template <class VecT>
        auto to_json_detail(JSON& to, const VecT& vec)
            -> decltype(to_json_detail(to, vec[0])) {
            JSON js;
            for (auto& o : vec) {
                js.push_back(JSON(o));
            }
            to = std::move(js);
        }

        template <class MapT>
        auto to_json_detail(JSON& to, const MapT& m)
            -> decltype(to_json_detail(to, m[std::string()])) {
            JSON js;
            for (auto& o : m) {
                js[o.first] = JSON(o.second);
            }
            to = std::move(js);
        }

        const char* obj_check(bool exist, const std::string& in) const {
            if (type != JSONType::object) {
                return "object kind miss match.";
            }
            if (exist && !obj->count(in)) {
                return "invalid range";
            }
            return nullptr;
        }

        const char* array_check(bool check, size_t pos) const {
            if (type != JSONType::array) {
                return "object kind miss match.";
            }
            if (check && array->size() <= pos) {
                return "invalid index";
            }
            return nullptr;
        }

        void array_if_unset() {
            if (type == JSONType::unset) {
                *this = JSONArrayType{};
            }
        }

        void obj_if_unset() {
            if (type == JSONType::unset) {
                *this = JSONObjectType{};
            }
        }

       public:
        JSON& operator=(const JSON& from) {
            if (this == &from) return *this;
            destruct();
            return copy_assign(from);
        }

        JSON& operator=(JSON&& from) noexcept {
            if (this == &from) return *this;
            destruct();
            return move_assign(std::move(from));
        }

        template <class Struct>
        JSON& not_null_assign(const std::string& name, Struct* in) {
            if (!in) return *this;
            (*this)[name] = in;
            return *this;
        }

        JSON() {
            type = JSONType::unset;
        }

        JSON(const JSON& from) {
            copy_assign(from);
        }

        JSON(JSON&& from)
        noexcept {
            move_assign(std::move(from));
        }

        JSON(const std::string& in, JSONType type) {
            if (type == JSONType::object) {
                init_as_obj({});
            }
            else if (type == JSONType::array) {
                init_as_array({});
            }
            else if (type == JSONType::boolean) {
                if (in == "true") {
                    boolean = true;
                }
                else if (in == "false") {
                    boolean = false;
                }
                else {
                    type = JSONType::unset;
                    return;
                }
            }
            else if (type == JSONType::null) {
            }
            else if (type == JSONType::integer) {
                try {
                    numi = std::stoll(in);
                } catch (...) {
                    type = JSONType::unset;
                    return;
                }
            }
            else if (type == JSONType::unsignedi) {
                try {
                    numu = std::stoull(in);
                } catch (...) {
                    type = JSONType::unset;
                    return;
                }
            }
            else if (type == JSONType::floats) {
                try {
                    numf = std::stod(in);
                } catch (...) {
                    type = JSONType::unset;
                    return;
                }
            }
            else if (type == JSONType::string) {
                value = EasyStr(in.c_str(), in.size());
            }
            this->type = type;
        }

        JSON(std::nullptr_t) {
            type = JSONType::null;
        }

        JSON(bool b) {
            type = JSONType::boolean;
            boolean = b;
        }

        JSON(char num) {
            init_as_num(num);
        }
        JSON(unsigned char num) {
            init_as_num(num);
        }
        JSON(short num) {
            init_as_num(num);
        }
        JSON(unsigned short num) {
            init_as_num(num);
        }
        JSON(int num) {
            init_as_num(num);
        }
        JSON(unsigned int num) {
            init_as_num(num);
        }
        JSON(long num) {
            init_as_num(num);
        }
        JSON(unsigned long num) {
            init_as_num(num);
        }
        JSON(long long num) {
            init_as_num(num);
        }
        JSON(unsigned long long num) {
            type = JSONType::unsignedi;
            numu = num;
        }

        template <class Struct>
        JSON(const Struct& in) {
            try {
                to_json_detail(*this, in);
            } catch (...) {
                destruct();
                type = JSONType::unset;
            }
        }

        template <class Struct>
        JSON(Struct* in) {
            if (!in) {
                type = JSONType::null;
            }
            else {
                try {
                    to_json_detail(*this, *in);
                } catch (...) {
                    destruct();
                    type = JSONType::unset;
                }
            }
        }

        JSON(double num) {
            type = JSONType::floats;
            numf = num;
        }

        JSON(const std::string& in) {
            type = JSONType::string;
            //auto str = escape(in);
            value = EasyStr(in.c_str(), in.size());
        }

        JSON(const char* in) {
            if (!in) {
                type = JSONType::null;
            }
            else {
                type = JSONType::string;
                //auto str = escape(in);
                value = EasyStr(in, ::strlen(in));
            }
        }

        JSON(JSONObjectType&& json) {
            init_as_obj(std::move(json));
        }

        JSON(JSONArrayType&& json) {
            init_as_array(std::move(json));
        }

        JSON& operator[](size_t pos) {
            array_if_unset();
            array_check(true, pos);
            return array->operator[](pos);
        }

        JSON& operator[](const std::string& key) {
            obj_if_unset();
            if (auto e = obj_check(false, key)) {
                throw e;
            }
            return obj->operator[](key);
        }

        JSON& at(const std::string& key) const {
            if (auto e = obj_check(true, key)) {
                throw e;
            }
            return obj->at(key);
        }

        JSON& at(size_t pos) const {
            if (auto e = array_check(true, pos)) {
                throw e;
            }
            return array->at(pos);
        }

        JSON* idx(size_t pos) const {
            //array_if_unset();
            if (array_check(true, pos)) {
                return nullptr;
            }
            return std::addressof(array->at(pos));
        }

        JSON* idx(const std::string& key) const {
            //obj_if_unset();
            if (obj_check(true, key)) {
                return nullptr;
            }
            return std::addressof(obj->at(key));
        }

        bool push_back(JSON&& json) {
            array_if_unset();
            if (auto e = array_check(false, 0)) {
                throw e;
            }
            array->push_back(std::move(json));
            return true;
        }

        bool push_back(const JSON& json) {
            array_if_unset();
            if (auto e = array_check(false, 0)) {
                throw e;
            }
            array->push_back(json);
            return true;
        }

        bool insert(size_t pos, JSON&& json, bool sizecheck = true, bool valid = false) {
            array_if_unset();
            if (auto e = array_check(false, 0)) {
                throw e;
            }
            auto presize = array->size();
            if (sizecheck && presize < pos) return false;
            array->insert(array->begin() + pos, std::move(json));
            if (valid && presize < pos) {
                auto nowsize = array->size();
                for (auto i = presize; i < nowsize; i++) {
                    array->at(i) = nullptr;
                }
            }
            return true;
        }

        bool insert(size_t pos, const JSON& json, bool sizecheck = true, bool valid = false) {
            array_if_unset();
            if (auto e = array_check(false, 0)) {
                throw e;
            }
            auto presize = array->size();
            if (sizecheck && presize < pos) return false;
            array->insert(array->begin() + pos, json);
            if (valid && presize < pos) {
                auto nowsize = array->size();
                for (auto i = presize; i < nowsize; i++) {
                    array->at(i) = nullptr;
                }
            }
            return true;
        }

        bool erase(size_t pos) {
            if (auto e = array_check(false, 0)) {
                throw e;
            }
            if (array->size() <= pos) return false;
            array->erase(array->begin() + pos);
            return true;
        }

        bool erase(const std::string& in) {
            if (auto e = obj_check(false, 0)) {
                throw e;
            }
            return (bool)obj->erase(in);
        }

        size_t size() {
            if (type != JSONType::array) return 0;
            return array->size();
        }

        JSONType gettype() const {
            return type;
        }

        ~JSON() {
            destruct();
        }

        static inline JSON parse(const std::string& in, const char** err = nullptr) {
            JSON ret;
            ret.parse_assign(in, err);
            return ret;
        }

        template <class Buf>
        static inline JSON parse(Reader<Buf>& r, const char** err = nullptr) {
            JSON ret;
            ret.parse_assign(r, err);
            return ret;
        }

        bool parse_assign(const std::string& in, const char** err = nullptr) {
            Reader<PROJECT_NAME::Refer<const std::string>> r(in);
            return parse_assign(r, err);
        }

        template <class Buf>
        bool parse_assign(Reader<Buf>& r, const char** err = nullptr) {
            auto ig = r.set_ignore(ignore_space_line);
            const char* e = nullptr;
            JSON tmp;
            try {
                tmp = parse_json_detail(r, &e);
            } catch (...) {
                e = "exception:unknown error";
            }
            if (err) {
                *err = e;
            }
            if (e == nullptr) {
                *this = std::move(tmp);
            }
            r.set_ignore(ig);
            return e == nullptr;
        }

        std::string to_string(size_t indent = 0, JSONFormat format = JSONFormat::defaultf) const {
            auto ret = to_string_detail(format, 1, indent, 0);
            auto flag = [&format](auto n) {
                return any(format & n);
            };
            if (ret.size() && flag(JSONFormat::endline)) ret += "\n";
            return ret;
        }

        bool operator==(const JSON& right) {
            if (this->type == right.type) {
                if (this->type == JSONType::unset || this->type == JSONType::null) {
                    return true;
                }
                else if (this->type == JSONType::string) {
                    return this->value.size() == right.value.size() &&
                           strcmp(this->value.const_str(), right.value.const_str()) == 0;
                }
                else if (this->type == JSONType::integer) {
                    return this->numi == right.numi;
                }
                else if (this->type == JSONType::floats) {
                    return this->numf == right.numf;
                }
                else if (this->type == JSONType::unsignedi) {
                    return this->numu == right.numu;
                }
                else if (this->type == JSONType::boolean) {
                    return this->boolean == right.boolean;
                }
            }
            return this->to_string() == right.to_string();
        }

       private:
        template <class T>
        const char* signed_num_get(T& res) const {
            if (type != JSONType::integer && type != JSONType::unsignedi) {
                return "object type missmatch";
            }
            auto check = [this](int64_t s, int64_t l) -> const char* {
                if (numi < s || numi > l) {
                    return "out of range";
                }
                res = (T)numi;
                return nullptr;
            };
            switch (sizeof(T)) {
                case 1:
                    return check(msb_on<signed char>(), 0x7f);
                case 2:
                    return check(msb_on<signed short>(), 0x7fff);
                case 4:
                    return check(msb_on<signed int>(), 0x7fffffff);
                case 8: {
                    if (type == JSONType::unsignedi && numi < 0) {
                        return "out of range";
                    }
                    res = (T)numi;
                    return nullptr;
                }
                default:
                    return "too large number size";
            }
            return "unreachable";
        }
        template <class T>
        const char* unsigned_num_get(T& res) const {
            if (type != JSONType::integer && type != JSONType::unsignedi) {
                throw "object type missmatch";
            }
            if (type == JSONType::integer && numi < 0) {
                throw "out of range";
            }
            auto check = [this](uint64_t c) -> const char* {
                if (numu > c) {
                    return "out of range";
                }
                res = (T)numu;
                return nullptr;
            };
            switch (sizeof(T)) {
                case 1:
                    return check(0xff);
                case 2:
                    return check(0xffff);
                case 4:
                    return check(0xffffffff);
                case 8:
                    res = (T)numu;
                    return nullptr;
                default:
                    return "too large number size";
            }
            return "unreachable";
        }

        template <class T>
        T float_num_get() const {
            if (type == JSONType::floats) {
                return (T)numf;
            }
            else if (type == JSONType::integer) {
                return (T)numi;
            }
            else if (type == JSONType::unsignedi) {
                return (T)numu;
            }
            throw "object type missmatch";
        }

       public:
        template <class Struct>
        Struct get() const {
            Struct tmp = Struct();
            get_to(tmp);
            return tmp;
        }

        template <class Struct, class Allocator = Struct* (*)(), class Deleter = void (*)(Struct*)>
        Struct* get_ptr(
            Allocator&& alloc = [] { return new Struct(); },
            Deleter&& del = [](Struct* p) { delete p; }) const {
            if (type == JSONType::null) {
                return nullptr;
            }
            Struct* s = nullptr;
            s = alloc();
            if (!s) {
                throw "memory error";
            }
            try {
                get_to(*s);
            } catch (...) {
                del(s);
                throw;
            }
            return s;
        }

        template <class Struct>
        bool try_get(Struct& s) const {
            try {
                get_to(s);
            } catch (...) {
                return false;
            }
            return true;
        }

        template <class Struct, class Allocator = Struct* (*)(), class Deleter = void (*)(Struct*)>
        bool try_get_ptr(
            Struct*& s, Allocator&& alloc = [] { return new Struct(); },
            Deleter&& del = [](Struct* p) { delete p; }) const {
            try {
                s = get_ptr<Struct>(alloc, del);
            } catch (...) {
                return false;
            }
            return true;
        }

        template <class Struct>
        void get_to(Struct& s) const {
            from_json_detail(s, *this);
        }

        bool is_enable() {
            return type != JSONType::unset;
        }

       private:
#define COMMONLIB_JSON_FROM_JSON_DETAIL_SIG(x)         \
    void from_json_detail(x& s, const JSON& j) const { \
        if (auto e = j.signed_num_get<x>(s)) {         \
            throw e;                                   \
        }                                              \
    }
#define COMMONLIB_JSON_FROM_JSON_DETAIL_UNSIG(x)                \
    void from_json_detail(unsigned x& s, const JSON& j) const { \
        if (auto e = j.unsigned_num_get<unsigned x>(s)) {       \
            throw e;                                            \
        }                                                       \
    }

        COMMONLIB_JSON_FROM_JSON_DETAIL_SIG(char)
        COMMONLIB_JSON_FROM_JSON_DETAIL_SIG(short)
        COMMONLIB_JSON_FROM_JSON_DETAIL_SIG(int)
        COMMONLIB_JSON_FROM_JSON_DETAIL_SIG(long)
        COMMONLIB_JSON_FROM_JSON_DETAIL_SIG(long long)

        COMMONLIB_JSON_FROM_JSON_DETAIL_UNSIG(char)
        COMMONLIB_JSON_FROM_JSON_DETAIL_UNSIG(short)
        COMMONLIB_JSON_FROM_JSON_DETAIL_UNSIG(int)
        COMMONLIB_JSON_FROM_JSON_DETAIL_UNSIG(long)
        COMMONLIB_JSON_FROM_JSON_DETAIL_UNSIG(long long)

#undef COMMONLIB_JSON_FROM_JSON_DETAIL_UNSIG
#undef COMMONLIB_JSON_FROM_JSON_DETAIL_SIG

        void from_json_detail(float& s, const JSON& in) const {
            s = in.float_num_get<float>();
        }

        void from_json_detail(double& s, const JSON& in) const {
            s = in.float_num_get<double>();
        }

        void from_json_detail(bool& s, const JSON& in) const {
            if (in.type != JSONType::boolean) {
                throw "object type missmatch";
            }
            s = in.boolean;
        }

        void from_json_detail(std::string& s, const JSON& in) const {
            if (in.type != JSONType::string) {
                throw "object type missmatch";
            }
            s = std::string(value.const_str(), value.size());
        }

        void from_json_detail(const char*& s, const JSON& in) const {
            if (in.type != JSONType::string) {
                throw "object type missmacth";
            }
            s = value.const_str();
        }

        template <class Struct>
        auto from_json_detail(Struct& to, const JSON& in) const
            -> decltype(from_json(to, in)) {
            from_json(to, in);
        }

        template <class Struct>
        auto from_json_detail(Struct& to, const JSON& in) const
            -> decltype(to.from_json(in)) {
            to.from_json(in);
        }

        template <class VecT>
        auto from_json_detail(VecT& to, const JSON& in) const
            -> decltype(from_json_detail(to[0], in)) {
            if (in.type != JSONType::array) {
                throw "object kind missmatch";
            }
            for (auto& o : *in.array) {
                to.push_back(o.get<remove_cv_ref<decltype(to[0])>>());
            }
        }

        template <class MapT>
        auto from_json_detail(MapT& to, const JSON& in) const
            -> decltype(from_json_detail(to[std::string()], in)) {
            if (in.type != JSONType::object) {
                throw "object kind missmatch";
            }
            for (auto& o : *in.obj) {
                to[o.first] = o.second.get<remove_cv_ref<decltype(to[std::string()])>>();
            }
        }

       public:
        JSON* path(const std::string& p, bool follow_rfc = false, bool make_if_not = false) {
            if (follow_rfc) {
                bool ins;
                std::string s;
                auto ret = pointer(p, s, ins);
                if (ins) return nullptr;
                return ret;
            }
            Reader<const char*> reader(p.c_str());
            bool filepath = false;
            if (reader.ahead("/")) {
                filepath = true;
            }
            JSON *ret = this, *hold;
            while (!reader.ceof()) {
                bool arrayf = false;
                if (filepath) {
                    if (!reader.expect("/")) return nullptr;
                    if (reader.ceof()) break;
                }
                else if (reader.expect("[")) {
                    arrayf = true;
                }
                else if (!reader.expect(".")) {
                    return nullptr;
                }
                std::string key;
                auto path_get = [&](auto& key) {
                    hold = ret->idx(key);
                    if (!hold) {
                        return false;
                    }
                    ret = hold;
                    return true;
                };
                auto read_until_cut = [&]() {
                    reader.readwhile(
                        key, untilincondition, [&filepath, &arrayf](char c) {
                            if (filepath) {
                                if (c == '/') return false;
                            }
                            else {
                                if (c == '.' || c == '[' || c == ']') return false;
                            }
                            return true;
                        },
                        true);
                };
                if (make_if_not) {
                    if (ret->type == JSONType::unset) {
                        if (reader.ahead("\"") || reader.ahead("\'")) {
                            (*ret).init_as_obj({});
                        }
                        else if (arrayf) {
                            (*ret).init_as_array({});
                        }
                        else {
                            (*ret).init_as_obj({});
                        }
                    }
                }
                if (ret->type == JSONType::array) {
                    read_until_cut();
                    uint64_t pos = 0;
                    if (!parse_ull(key, pos)) {
                        return nullptr;
                    }
                    if (!path_get(pos)) {
                        if (make_if_not) {
                            if (!(*ret).insert(pos, nullptr, false, true)) {
                                return nullptr;
                            }
                            ret = &(*ret)[pos];
                        }
                        else {
                            return nullptr;
                        }
                    }
                }
                else if (ret->type == JSONType::object) {
                    if (reader.ahead("\"") || reader.ahead("'")) {
                        if (!reader.string(key, true)) return nullptr;
                        key.pop_back();
                        key.erase(0, 1);
                        if (!path_get(key)) {
                            if (make_if_not) {
                                ret = &(*ret)[key];
                            }
                            else {
                                return nullptr;
                            }
                        }
                    }
                    else if (!arrayf) {
                        read_until_cut();
                        if (!path_get(key)) {
                            if (make_if_not) {
                                ret = &(*ret)[key];
                            }
                            else {
                                return nullptr;
                            }
                        }
                    }
                    else {
                        return nullptr;
                    }
                }
                if (arrayf) {
                    if (!reader.expect("]")) return nullptr;
                }
            }
            return ret;
        }

        JSON* pointer(const std::string& str, std::string& lastelm, bool& insertok, bool parent = false) {
            Reader<const char*> r(str.c_str());
            insertok = false;
            JSON *base = this, *hold = nullptr;
            while (true) {
                if (!r.expect("/")) return nullptr;
                std::string name;
                r.readwhile(name, PROJECT_NAME::untilincondition, [](char c) { return c != '/'; });
                name = std::regex_replace(name, std::regex("~1"), "/");
                name = std::regex_replace(name, std::regex("~0"), "~");
                if (base->type == JSONType::object) {
                    hold = base->idx(name);
                    if (!hold) {
                        if (r.ceof()) {
                            lastelm = name;
                            insertok = true;
                            return base;
                        }
                        return nullptr;
                    }
                }
                else if (base->type == JSONType::array) {
                    size_t pos = 0;
                    if (r.ceof() && name == "-") {
                        lastelm = name;
                        insertok = true;
                        return base;
                    }
                    if (!parse_ull(name, pos)) return nullptr;
                    hold = base->idx(pos);
                    if (!hold) {
                        if (r.ceof()) {
                            lastelm = name;
                            insertok = true;
                            return base;
                        }
                        return nullptr;
                    }
                }
                else {
                    return nullptr;
                }
                if (r.ceof()) {
                    if (parent) {
                        lastelm = name;
                        insertok = true;
                    }
                    break;
                }
                base = hold;
            }
            return parent ? base : hold;
        }

       private:
        static bool patch_add(JSON* base, JSON* op, const std::string& lastelm) {
            auto value = op->idx("value");
            if (!value) return false;
            if (base->type == JSONType::array) {
                if (lastelm == "-") {
                    base->push_back(*value);
                }
                else {
                    size_t pos;
                    if (!parse_ull(lastelm, pos)) return false;
                    if (!base->insert(pos, *value)) return false;
                }
            }
            else if (base->type == JSONType::object) {
                (*base)[lastelm] = *value;
            }
            return true;
        }
        //public:
        JSON patch(const JSON& from) const {
            if (from.type != JSONType::array) return JSON();
            JSON copy = *this;
            size_t i = 0;
            for (auto op = from.idx(i); op; op = from.idx(++i)) {
                auto opkind = op->idx("op");
                auto path = op->idx("path");
                if (!opkind || !path) return JSON();
                std::string opname;
                std::string pathname;
                if (!opkind->try_get(opname)) {
                    return JSON();
                }
                if (!path->try_get(pathname)) {
                    return JSON();
                }
                std::string lastelm;
                bool insertok = false;
                JSON* base = copy.pointer(pathname, lastelm, insertok, true);
                if (!base) return JSON();
                if (opname == "add") {
                    if (!patch_add(base, op, lastelm)) return JSON();
                }
            }
            return JSON();
        }

        JSON merge(const JSON& from) {
            return JSON();
        }
    };

    inline JSON<std::unordered_map, std::vector> operator""_json(const char* in, size_t size) {
        return JSON<std::unordered_map, std::vector>::parse(std::string(in, size));
    }

    using JSONObject = JSON<std::unordered_map, std::vector>::JSONObjectType;
    using JSONArray = JSON<std::unordered_map, std::vector>::JSONArrayType;

    template <class Vec>
    JSON<std::unordered_map, std::vector> atoj(Vec& in) {
        JSON ret = "[]"_json;
        for (auto& i : in) {
            ret.push_back(i);
        }
        return ret;
    }
}  // namespace PROJECT_NAME