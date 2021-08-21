/*
    Copyright (c) 2021 on-keyday
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <basic_helper.h>
#include <learnstd.h>
#include <project_name.h>
#include <reader.h>

#include <vector>

namespace PROJECT_NAME {

    template <class Buf>
    struct Serializer {
        using char_type = unsigned char;

       private:
        Buf serialized;

        using RBuf = std::remove_reference_t<Buf>;

       public:
        Serializer() {}

        Serializer(RBuf& in)
            : serialized(in) {}

        Serializer(const RBuf& in)
            : serialized(in) {}

        Serializer(RBuf&& in)
            : serialized(std::forward<RBuf>(in)) {}

        template <class C>
        void write_byte(C byte, size_t size) {
            for (size_t i = 0; i < size; i++) {
                serialized.push_back(*(byte+i));
            }
        }

        template <class T, class = std::enable_if_t<bcsizeeq<T, 1>, void>>
        void write_byte(T&& seq) {
            for (auto&& i : seq) {
                serialized.push_back(i);
            }
        }

        template <class T>
        void write(const T& t) {
            write_byte((const unsigned char*)&t, sizeof(T));
        }

        template <class To, class From>
        void write_as(const From& t) {
            write((To)t);
        }

        template <class T>
        void write(const T* t, size_t size) {
            if (!t || !size) return;
            write_byte((const unsigned char*)t, size * sizeof(T));
        }

        template <class T>
        void write_reverse(const T& t) {
            write(translate_byte_reverse<T>((const char*)&t));
        }

        template <class T>
        void write_hton(const T& t) {
            write(translate_byte_net_and_host<T>((const char*)&t));
        }

        template <class C>
        void write_full_reverse(C* t, size_t size) {
            if (!t || !size) return;
            for (auto i = size; i != 0; i--) {
                write_reverse(t[i - 1]);
            }
        }

        template <class C>
        void write_reverse(C* t, size_t size) {
            if (!t || !size) return;
            for (size_t i = 0; i < size; i++) {
                write_reverse(t[i]);
            }
        }

        template <class C>
        void write_hton(C* t, size_t size) {
            if (!t || !size) return;
            for (size_t i = 0; i < size; i++) {
                write_hton(t[i]);
            }
        }

        RBuf& get() {
            return serialized;
        }
    };

    template <class Buf>
    struct Deserializer {
       private:
        using RBuf = std::remove_reference_t<Buf>;
        //using Hold=std::conditional_t<std::is_reference_v<Buf>,Refer<RBuf>,Buf>;
        Reader<Buf> r;

       public:
        Deserializer(){}

        Deserializer(RBuf& in)
            : r(in) {}

        Deserializer(const RBuf& in)
            : r(in) {}

        Deserializer(RBuf&& in)
            : r(std::forward<RBuf>(in)) {}

        template <class C>
        bool read_byte(C* byte, size_t size) {
            if (r.read_byte(byte, size, translate_byte_as_is, true) < size) {
                return false;
            }
            return true;
        }

        template <class T>
        bool read_byte(T& t, size_t size) {
            b_char_type<T> rd;
            for (size_t i = 0; i < size; i++) {
                if (!read(rd)) {
                    return false;
                }
                t.push_back(rd);
            }
            return true;
        }

        template <class C>
        bool read_byte_reverse(C* byte, size_t size) {
            if (r.read_byte(byte, size, translate_byte_reverse, true) < size) {
                return false;
            }
            return true;
        }

        template <class T>
        bool read_byte_reverse(T& t, size_t size) {
            b_char_type<T> rd;
            for (size_t i = 0; i < size; i++) {
                if (!read_reverse(rd)) {
                    return false;
                }
                t.push_back(rd);
            }
            return true;
        }

        template <class C>
        bool read_byte_ntoh(C* byte, size_t size) {
            if (r.read_byte(byte, size, translate_byte_net_and_host, true) < size) {
                return false;
            }
            return true;
        }

        template <class T>
        bool read_byte_ntoh(T& t, size_t size) {
            b_char_type<T> rd;
            for (size_t i = 0; i < size; i++) {
                if (!read_ntoh(rd)) {
                    return false;
                }
                t.push_back(rd);
            }
            return true;
        }

        template <class T>
        bool read(T& t) {
            return read_byte(&t, sizeof(T));
        }

        template <class From, class To>
        bool read_as(To& to) {
            From from;
            if (!read(from)) {
                return false;
            }
            to = (To)from;
            return true;
        }

        template <class T>
        bool read_reverse(T& t) {
            return read_byte_reverse(&t, sizeof(T));
        }

        template <class T>
        bool read_ntoh(T& t) {
            return read_byte_ntoh(&t, sizeof(T));
        }

        bool eof() const {
            return r.ceof();
        }

        Reader<Buf>& base_reader() {
            return r;
        }

        /*private:
        RBuf& get_impl(RBuf& ref){
            return ref;
        }

        RBuf& get_impl(Ref<RBuf>& ref){
            return *ref;
        }
    public:*/

        RBuf& get() {
            //return get_impl(r.ref());
            return r.ref();
        }
    };

    template <class T>
    struct NoDestruct {
       private:
        union {
            T t;
            char buf[sizeof(T)] = {0};
        };

       public:
        NoDestruct() {
            t = T();
            //new(&t) T();
        }

        /*NoDestruct(const T& in){
            t=in;
        }
        
        NoDestruct(T&& in){
            t=std::forward<T>(in);
            //new(&t) T(make_T(std::forward<I>(in)));
        }*/

        ~NoDestruct() noexcept {}

        T& operator()() {
            return t;
        }

        operator T&() {
            return t;
        }
    };

    template <class Buf, template <class...> class Vec = std::vector>
    struct Stack {
       private:
        Serializer<Buf&> writer;
        Deserializer<Reverse<Buf>> reader;
        Buf& bufref;
        Vec<Ref<const TypeId>> types;

        template <class T>
        void assign(NoDestruct<T>& obj, T& in) {
            obj() = in;
        }

        template <class T>
        void assign(NoDestruct<T>& obj, T&& in) {
            obj() = std::forward<T>(in);
        }

       public:
        Stack()
            : reader(Buf()), writer(reader.get().buf), bufref(reader.get().buf) {}

       private:
        template <class T>
        size_t push_detail(T&& in) {
            size_t ret = stack_ptr();
            using RT = std::remove_reference_t<T>;
            NoDestruct<RT> obj;
            assign(obj, std::forward<T>(in));
            writer.write(obj());
            types.push_back(typeinfo<T>());
            return ret;
        }

       public:
        template <class T>
        size_t push(T&& in) {
            return push_detail(std::forward<T>(in));
        }

        template <class T>
        size_t push(T* in) {
            return push_detail(std::forward<T*>(in));
        }

        size_t stack_ptr() const {
            return bufref.size();
        }

       private:
        void remove_ext(size_t count) {
            for (size_t i = 0; i < count; i++) {
                bufref.pop_back();
            }
        }

        template <class T>
        bool pop_detail(T& out) {
            if (!reader.read_reverse(out)) {
                return false;
            }
            remove_ext(sizeof(T));
            reader.base_reader().seek(0);
            types.pop_back();
            return true;
        }

       public:
        template <class T>
        bool pop(T& out) {
            if (!types.size()) return false;
            if ((*types.back()).get_size() != typeinfo<T>().get_size()) return false;
            return pop_detail(out);
        }

        template <class T>
        bool pop_strict(T& out) {
            if (!types.size()) return false;
            if ((*types.back()) != typeinfo<T>()) return false;
            return pop_detail(out);
        }

        Buf& raw() {
            return bufref;
        }
    };
}  // namespace PROJECT_NAME