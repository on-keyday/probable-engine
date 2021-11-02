/*
    socklib - simple socket library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "string_buffer.h"
#include <type_traits>
#include <algorithm>
#include <iterator>
#include <extutil.h>
#include "http_reason_phrase.h"

namespace socklib {
    namespace v3 {
        struct HeaderContext {
            virtual const char* get(const char*) = 0;
            virtual const char* get(const char*, size_t) = 0;
            virtual bool get(size_t, const char*&, const char*&) = 0;
            virtual bool set(const char*, const char*) = 0;
            virtual bool remove(const char*) = 0;
            virtual bool remove(const char*, size_t) = 0;
            virtual void clear() = 0;
        };

        template <class MultiMap>
        struct HeaderContext_impl : HeaderContext {
            MultiMap map;
            using buffer_t = StringBuffer_impl<std::remove_cvref_t<decltype(map[""])>>;

            virtual const char* get(const char* key) override {
                if (!key) return nullptr;
                if (auto found = map.find(key); found != map.end()) {
                    return buffer_t::get_data(found->second);
                }
                return nullptr;
            }

            virtual const char* get(const char* key, size_t index) override {
                if (!key) return nullptr;
                if (auto found = map.equal_range(key); found != map.end()) {
                    auto it = found->first;
                    auto end = found->second;
                    for (auto i = 0; it != end && i < index; i++) {
                        it++;
                    }
                    if (it == end) {
                        return nullptr;
                    }
                    return buffer_t::get_data((*it).second);
                }
                return nullptr;
            }

            virtual bool get(size_t index, const char*& key, const char*& value) override {
                if (index >= map.size()) {
                    return false;
                }
                auto it = map.begin();
                for (auto i = 0; i < index; i++) {
                    it++;
                }
                key = buffer_t::get_data(it.first);
                value = buffer_t::get_data(it.second);
                return true;
            }

            virtual bool set(const char* key, const char* value) override {
                if (!key || !value) {
                    return false;
                }
                map.emplace(key, value);
                return true;
            }

            virtual bool remove(const char* key) override {
                if (!key) return false;
                map.erase(key);
                return true;
            }

            virtual bool remove(const char* key, size_t index) override {
                if (!key) {
                    return false;
                }
                if (auto found = map.equal_range(key); found != map.end()) {
                    auto it = found->first;
                    auto end = found->second;
                    for (auto i = 0; it != end && i < index; i++) {
                        it++;
                    }
                    if (it == end) {
                        return false;
                    }
                    map.erase(it);
                    return true;
                }
                return false;
            }

            virtual void clear() {
                map.clear();
            }
        };

        enum class HeaderFlag {
            small_letter = 0x1,
            verify_status = 0x2,
            verify_header = 0x4,
            ignore_invalid = 0x8,
            only_lf = 0x10,
        };

        DEFINE_ENUMOP(HeaderFlag);

        enum class BodySizeHeader {
            noinfo = 0,
            chunked = 0x1,
            content_length = 0x2,
            content_type = 0x4,
        };

        DEFINE_ENUMOP(BodySizeHeader)

        struct BodyInfo {
            BodySizeHeader state;
            size_t length = 0;
        };

        struct ImportantHeader {
            const char* host = nullptr;
            size_t content_length = 0;
            bool needlen = false;
        };

        struct HeaderIO {
            static bool endline(char c) {
                return c != '\r' && c != '\n';
            }

            template <class Buf>
            static bool expect_endline(commonlib2::Reader<Buf>& r) {
                return r.expect("\r\n") || r.expect("\r") || r.expect("\n");
            }

            static bool header_cmp(unsigned char c1, unsigned char c2) {
                return std::toupper(c1) == std::toupper(c2);
            }

            template <class Buf, class String>
            static bool parse_common(commonlib2::Reader<Buf>& r, HeaderContext& header,
                                     String& tmpbuf1, String& tmpbuf2, BodyInfo* info) {
                while (!r.ceof()) {
                    tmpbuf1.clear();
                    tmpbuf2.clear();
                    if (expect_endline(r)) {
                        return true;
                    }
                    r.readwhile(tmpbuf1, commonlib2::until, ':');
                    if (!r.expect(":")) {
                        return false;
                    }
                    while (r.achar() == ' ' || r.achar() == '\t') {
                        r.increment();
                    }
                    r.readwhile(tmpbuf2, commonlib2::untilincondition, endline);
                    if (!expect_endline(r)) {
                        return false;
                    }
                    header.set(tmpbuf1.c_str(), tmpbuf2.c_str());
                    if (info) {
                        if (commonlib2::str_eq(tmpbuf1.c_str(), "content-length", header_cmp)) {
                            commonlib2::Reader(tmpbuf2.c_str()) >> info->length;
                            info->state |= BodySizeHeader::content_length;
                        }
                        else if (commonlib2::str_eq(tmpbuf1.c_str(), "transfer-encoding", header_cmp)) {
                            if (std::string_view(tmpbuf2.c_str()).find("chunked")) {
                                info->state |= BodySizeHeader::chunked;
                            }
                        }
                        else if (commonlib2::str_eq(tmpbuf1.c_str(), "content-type", header_cmp)) {
                            info->state |= BodySizeHeader::content_type;
                        }
                    }
                }
                return false;
            }

            template <class Buf, class String>
            static bool parse_request(commonlib2::Reader<Buf>& r, String& method, String& path,
                                      HeaderContext& header, String& tmpbuf1, String& tmpbuf2, String* version = nullptr, BodyInfo* bodyinfo = nullptr) {
                r.readwhile(method, commonlib2::until, ' ');
                if (!r.expect(" ")) {
                    return false;
                }
                r.readwhile(path, commonlib2::until, ' ');
                if (!r.expect(" ")) {
                    return false;
                }
                r.readwhile(version ? *version : tmpbuf1, commonlib2::untilincondition, endline);
                if (!expect_endline(r)) {
                    return false;
                }
                return parse_common(r, header, tmpbuf1, tmpbuf2, bodyinfo);
            }

            template <class Buf, class String>
            static bool parse_response(commonlib2::Reader<Buf>& r, int& status,
                                       HeaderContext& header,
                                       String& tmpbuf1, String& tmpbuf2,
                                       String* version = nullptr, String* reason = nullptr, BodyInfo* bodyinfo = nullptr) {
                r.readwhile(version ? *version : tmpbuf1, commonlib2::until, ' ');
                if (!r.expect(" ")) {
                    return false;
                }
                r.readwhile(tmpbuf2, commonlib2::until, ' ');
                if (!r.expect(" ")) {
                    return false;
                }
                if (tmpbuf2.size() != 3) {
                    return false;
                }
                commonlib2::Reader(commonlib2::Sized(tmpbuf2.c_str(), tmpbuf2.size())) >> status;
                if (status < 100) {
                    return false;
                }
                r.readwhile(reason ? *reason : tmpbuf1, commonlib2::untilincondition, endline);
                if (!expect_endline(r)) {
                    return false;
                }
                return parse_common(r, header, tmpbuf1, tmpbuf2, bodyinfo)
            }

            static bool is_valid_statusline(const char* val) {
                commonlib2::Reader vr(val);
                if (vr.ceof()) {
                    return false;
                }
                while (!vr.ceof()) {
                    if (vr.achar() == ' ' || vr.achar() == '\r' || vr.achar() == '\n') {
                        return false;
                    }
                    vr.increment();
                }
                return true;
            }

            static bool is_valid_header(const char* key, const char* value) {
                commonlib2::Reader kr(key), vr(value);
                while (!kr.ceof() && !vr.ceof()) {
                    auto c = (std::uint8_t)kr.achar();
                    auto d = (std::uint8_t)vr.achar();
                    if (c == ':' || c > 0x7f || (c != 0 && c < 0x20) || (d != 0 && d < 0x20)) {
                        return false;
                    }
                    if (!kr.ceof()) {
                        kr.increment();
                    }
                    if (!vr.ceof()) {
                        vr.increment();
                    }
                }
                return true;
            }

            template <class Key, class String>
            static bool to_smaller(Key begin, Key end, String& result) {
                std::transform(
                    begin, end,
                    std::back_inserter(result)
                        [](unsigned char c) { return std::tolower(c); });
                return true;
            }

            template <class Const, class String>
            static bool write_keyval(String& raw, Const&& key, Const&& value, String* tmpbuf, HeaderFlag flag) {
                if (any(flag & HeaderFlag::verify_header)) {
                    if (!is_valid_header(key, value)) {
                        if (any(flag & HeaderFlag::ignore_invalid)) {
                            continue;
                        }
                        return false;
                    }
                }
                if (any(flag & HeaderFlag::small_letter)) {
                    if (!tmpbuf) {
                        return false;
                    }
                    tmpbuf->clear();
                    to_smaller(key, key + ::strlen(key), *tmpbuf);
                    raw.append(tmpbuf->c_str());
                }
                else {
                    raw.append(key);
                }
                raw.append(": ");
                raw.append(value);
                if (any(flag & HeaderFlag::only_lf)) {
                    raw.append("\n");
                }
                else {
                    raw.append("\r\n");
                }
                return true;
            }

            template <class String>
            static bool write_common(String& raw, HeaderContext& ctx, String* tmpbuf, HeaderFlag flag, ImportantHeader* imh) {
                if (imh) {
                    if (imh->host) {
                        if(!write_keyval(raw,"Host", imh->host,tmpbuf,flag){
                            return false;
                        }
                    }
                }
                const char *key = nullptr, *value = nullptr;
                for (size_t i = 0; ctx.get(i, key, value); i++) {
                    if (!write_keyval(raw, key, value, tmpbuf, flag)) {
                        return false;
                    }
                }
                if (any(flag & HeaderFlag::only_lf)) {
                    raw.append("\n");
                }
                else {
                    raw.append("\r\n");
                }
                return true;
            }

            template <class String, class Const = const char*>
            static bool write_request(String& raw, const Const& method, const Const& path, HeaderContext& header,
                                      int version = 1, HeaderFlag flag = HeaderFlag::verify_header | HeaderFlag::verify_status, const Const& host = nullptr, String* tmpbuf = nullptr) {
                using buffer_t = StringBuffer_impl<std::remove_cvref<Const>>;
                if (any(flag & HeaderFlag::verify_status)) {
                    if (!is_valid_statusline(buffer_t::get_data(method)) &&
                        !is_valid_statusline(buffer_t::get_data(path))) {
                        return false;
                    }
                }
                raw.append(buffer_t::get_data(method));
                raw.push_back(' ');
                raw.append(buffer_t::get_data(path));
                raw.push_back(' ');
                if (version == 0) {
                    raw.append("HTTP/1.0");
                }
                else {
                    raw.append("HTTP/1.1");
                }
                if (any(flag & HeaderFlag::only_lf)) {
                    raw.append("\n");
                }
                else {
                    raw.append("\r\n");
                }
                return write_common(raw, header, tmpbuf, flag);
            }

            template <class String, class Const = const char*>
            static bool write_response(String& raw, int status, HeaderContext& header, int version = 1, HeaderFlag flag = HeaderFlag::verify_header | HeaderFlag::verify_status, String* tmpbuf = nullptr, const Const& reason = nullptr) {
                using buffer_t = StringBuffer_impl<std::remove_cvref<Const>>;
                if (status < 100 || status > 999) {
                    return false;
                }
                if (version == 0) {
                    raw.append("HTTP/1.0 ");
                }
                else {
                    raw.append("HTTP/1.1 ")
                }
                char status[5] = "000 ";
                status[0] += status / 100;
                status[1] += (status % 100) / 10;
                status[2] += (status % 10);
                raw.append(status);
                if (buffer_t::get_data(reason)) {
                    if (any(flag & HeaderFlag::verify_status)) {
                        if(!is_valid_statusline(buffer_t::get_data(reason)){
                            return false;
                        }
                    }
                    raw.append(buffer_t::get_data(reason));
                }
                else {
                    raw.append(reason_phrase(status, true));
                }
                if (any(flag & HeaderFlag::only_lf)) {
                    raw.append("\n");
                }
                else {
                    raw.append("\r\n");
                }
                return write_common(raw, header, tmpbuf, flag);
            }
        };
    }  // namespace v3
}  // namespace socklib
