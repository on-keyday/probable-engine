/*
    socklib - simple socket library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once

#include <enumext.h>
#include <string.h>

namespace socklib {
    namespace v3 {

        struct StringBuffer {
            virtual const char* c_str() const = 0;
            virtual char* data() = 0;
            virtual bool resize(size_t) = 0;
            virtual size_t size() const = 0;
            virtual bool set(const char*, size_t) = 0;
            virtual bool set(const char*) = 0;
            virtual bool clear() = 0;
            virtual bool append_front(const char*, size_t) = 0;
            virtual bool append_front(const char*) = 0;
            virtual bool append_back(const char*, size_t) = 0;
            virtual bool append_back(const char*) = 0;
            virtual bool is_readonly() const = 0;
            virtual bool equal(const char* str, size_t size) = 0;
            virtual bool equal(const char* str) = 0;
            virtual void push_back() = 0;
            virtual ~StringBuffer() {}
        };

        struct StringBufferBuilder {
            virtual StringBuffer* make() const = 0;
        };

        template <class Buf>
        struct StringBuffer_impl : StringBuffer {
           private:
            Buf buf;

           public:
            template <class C>
            static auto get_data(C* c) {
                return (C*)c;
            }

            template <class Str>
            static auto get_data(Str& c) {
                return c.data();
            }

            template <class C>
            static size_t get_size(C* c) {
                if (!c) return 0;
                return ::strlen(c);
            }

            template <class Str>
            static size_t get_size(Str& c) {
                return c.size();
            }

            template <class Str>
            static bool call_resize(Str& c, size_t sz) {
                c.size(sz);
                return true;
            }

            template <class C>
            static bool call_resize(C* c, size_t sz) {
                return false;
            }

            template <bool is_append, bool front, class Str>
            static bool set_or_append(Str& buf, const char* input, size_t size) {
                if (!input) return false;
                if CONSTEXPRIF (is_append) {
                    if CONSTEXPRIF (front) {
                        buf = Str(input, size) + buf;
                    }
                    else {
                        buf.append(input, size);
                    }
                }
                else {
                    buf = Str(input, size);
                }
                return true;
            }

            template <bool is_append, bool front, class C>
            static bool set_or_append(C* c, const char*, size_t) {
                return false;
            }

            static size_t len(const char* in) {
                if (!in) {
                    return 0;
                }
                return ::strlen(in);
            }

            template <class C>
            constexpr static bool is_ptr(C*) {
                return true;
            }

            template <class C>
            constexpr static bool is_ptr(C&) {
                return false;
            }

            template <class C>
            constexpr static bool call_clear(C*) {
                return false;
            }

            template <class C>
            constexpr static bool call_clear(C& c) {
                c.clear();
                return true;
            }

            virtual const char* c_str() const override {
                return get_data(buf);
            }
            virtual char* data() override {
                return get_data(buf);
            }

            virtual size_t size() const override {
                return get_size(buf);
            }

            virtual bool resize(size_t sz) override {
                return call_resize(buf, sz);
            }

            virtual bool is_readonly() const override {
                return is_ptr(buf);
            }

            virtual bool clear() override {
                return call_clear(buf);
            }

            virtual bool set(const char* set, size_t size) override {
                return set_or_append<false, false>(buf, set, size);
            }
            virtual bool set(const char* toset) {
                return set_or_append<false, false>(buf, toset, len(toset));
            }

            virtual bool append_front(const char* toset, size_t size) override {
                return set_or_append<true, true>(buf, toset, size);
            }

            virtual bool append_front(const char* toset) override {
                return set_or_append<true, true>(buf, toset, len(toset));
            }

            virtual bool append(const char* toset, size_t size) {
                return set_or_append<true, false>(buf, toset, size);
            }
            virtual bool append_back(const char* toset) {
                return set_or_append<true, false>(buf, toset, len(toset));
            }

            virtual bool append_back(const char* toset, size_t size) override {
                return set_or_append<true, false>(buf, toset, size);
            }

            virtual bool append_back(const char* toset) override {
                return set_or_append<true, false>(buf, toset, len(toset));
            }

            virtual bool equal(const char* str, size_t strsize) {
                if (!str || !data() || size() != strsize) {
                    return false;
                }
                return ::memcmp(data(), str) == 0;
            }
            virtual bool equal(const char* str) {
                return equal(str, len(str));
            }

            virtual void push_back(char c) {
                append_back(&c, 1);
            }
        };

        template <class T>
        struct StringBufferBuilder_impl : StringBufferBuilder {
            virtual StringBuffer* make() const override {
                return new StringBuffer_impl<Buf>();
            }
        };

    }  // namespace v3
}  // namespace socklib
