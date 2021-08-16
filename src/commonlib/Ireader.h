/*
    Copyright (c) 2021 on-keyday
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <memory>

#include "basic_helper.h"
#include "learnstd.h"
#include "reader.h"
namespace PROJECT_NAME {

    template <class IReader>
    struct Iproxy {
        IReader& r;
        Iproxy(IReader& in)
            : r(in) {}

        int operator[](size_t pos) const {
            r.seek(pos);
            return r.achar();
        }

        size_t size() const {
            return r.size();
        }
    };

    struct IReader {
#define DEFINE_EXPECT_FUN(RET, FUNC, CHAR)                   \
    virtual RET FUNC(const CHAR*) = 0;                       \
    virtual RET FUNC(const Sized<CHAR>&) = 0;                \
    virtual RET FUNC(const CHAR*, Function<bool(CHAR)>) = 0; \
    virtual RET FUNC(const Sized<CHAR>&, Function<bool(CHAR)>) = 0;

        DEFINE_EXPECT_FUN(bool, expect, char)
        DEFINE_EXPECT_FUN(size_t, ahead, char)
        DEFINE_EXPECT_FUN(bool, expect, char16_t)
        DEFINE_EXPECT_FUN(size_t, ahead, char16_t)
        DEFINE_EXPECT_FUN(bool, expect, char32_t)
        DEFINE_EXPECT_FUN(size_t, ahead, char32_t)
#undef DEFINE_EXPECT_FUN

        virtual int achar() const = 0;
        virtual bool increment() = 0;
        virtual bool decrement() = 0;
        virtual bool seek(size_t pos) = 0;

        virtual bool readwhile(const Function<bool(Reader<Iproxy<IReader>>*, void*, void*, bool)>&, void*, void*) = 0;

        virtual const TypeId& type() const = 0;
        virtual void* get_base() const = 0;
        virtual size_t size() const = 0;
    };

    /*
    struct Iproxy{
        mutable IReader& r;
        
        int operator[](size_t pos)const{
            r.seek(pos);
            return r.achar();
        }

        size_t size()const{
            return r.size();
        }
    };*/

    template <class Buf>
    struct IReader_impl : IReader {
        Reader<Buf>& r;
        size_t sz;
        IReader_impl(Reader<Buf>& in)
            : r(in) {
            auto pos = r.readpos();
            r.seek(static_cast<size_t>(-1));
            sz = r.readpos();
            r.seek(pos);
        }

        size_t size() const override {
            return sz;
        }

        bool readwhile(const Function<bool(Reader<Iproxy<IReader>>*, void*, void*, bool)>& func, void* ctx1, void* ctx2) override {
            r.ahead("");
            Reader<Iproxy<IReader>> proxy(*this);
            proxy.seek(r.readpos());
            struct Tmp {
                void* ctx1;
                void* ctx2;
                const Function<bool(Reader<Iproxy<IReader>>*, void*, void*, bool)>& func;
            } tmp{ctx1, ctx2, func};
            return proxy.readwhile(
                +[](Reader<Iproxy<IReader>>* in, Tmp* tmp, bool first) {
                    return tmp->func(in, tmp->ctx1, tmp->ctx2, first);
                },
                &tmp);
        }

        int achar() const override {
            return (int)r.achar();
        }

        bool seek(size_t pos) override {
            return r.seek(pos);
        }

        bool increment() override {
            return r.increment();
        }

        bool decrement() override {
            return r.decrement();
        }
#define DEFINE_EXPECT_FUN(RET, FUNC, CHAR)                                \
    RET FUNC(const CHAR* in) override {                                   \
        return r.FUNC(in);                                                \
    }                                                                     \
    RET FUNC(const Sized<CHAR>& in) override {                            \
        return r.FUNC(in);                                                \
    }                                                                     \
    RET FUNC(const CHAR* in, Function<bool(CHAR)> nexp) override {        \
        return r.FUNC(in, [&](CHAR c) { return nexp(c); });               \
    }                                                                     \
    RET FUNC(const Sized<CHAR>& in, Function<bool(CHAR)> nexp) override { \
        return r.FUNC(in, [&](CHAR c) { return nexp(c); });               \
    }

        DEFINE_EXPECT_FUN(bool, expect, char)
        DEFINE_EXPECT_FUN(size_t, ahead, char)
        DEFINE_EXPECT_FUN(bool, expect, char16_t)
        DEFINE_EXPECT_FUN(size_t, ahead, char16_t)
        DEFINE_EXPECT_FUN(bool, expect, char32_t)
        DEFINE_EXPECT_FUN(size_t, ahead, char32_t)

#undef DEFINE_EXPECT_FUN

        const TypeId& type() const override {
            return typeinfo<Reader<Buf>>();
        }

        void* get_base() const override {
            return &r;
        }
    };

    /*template<class Buf>
    std::shared_ptr<IReader> make_ireader(Reader<Buf>& in){
        return std::make_shared<IReader_impl<Buf>>(in);
    }*/

    template <class Buf>
    struct IR {
       private:
        IReader& r;
        IReader_impl<Buf> impl;

       public:
        IR(Reader<Buf>& in)
            : impl(in), r(impl) {}
        IReader& operator()() {
            return r;
        }
        operator IReader&() {
            return r;
        }
    };

    template <class Buf>
    IR<Buf> make_ireader(Reader<Buf>& r) {
        return IR(r);
    }
}  // namespace PROJECT_NAME