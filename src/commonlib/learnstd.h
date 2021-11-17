/*
    commonlib - common utility library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "project_name.h"

namespace PROJECT_NAME {
    //以下stdの実装模倣

    struct TypeIDBase {
       protected:
        static size_t& get_counter() {
            static size_t typeid_counter = 0;
            return typeid_counter;
        }

        static size_t get_count() {
            return ++get_counter();
        }
    };

    //template<class PlaceHolder>
    //size_t TypeIDBase<PlaceHolder>::typeid_counter=0;

    struct TypeId {
        virtual size_t get_id() const = 0;
        virtual size_t get_size() const = 0;
        virtual bool operator==(const TypeId&) const = 0;
    };

    template <class T>
    struct TypeId_size {
        constexpr static size_t size = sizeof(T);
    };

    template <>
    struct TypeId_size<void> {
        constexpr static size_t size = 0;
    };

    template <class T>
    struct TypeId_impl : TypeId, TypeIDBase {
        static size_t get_id_impl() {
            static size_t id = TypeIDBase::get_count();
            return id;
        }

        size_t get_id() const override {
            return get_id_impl();
        }

        size_t get_size() const override {
            return TypeId_size<T>::size;
        }

        bool operator==(const TypeId& in) const override {
            return get_id() == in.get_id();
        }

        template <class U>
        friend const TypeId& typeinfo();

       private:
        constexpr TypeId_impl() {
            get_id();
        }
        constexpr TypeId_impl(const TypeId_impl&) = delete;
        constexpr TypeId_impl(TypeId_impl&&) = delete;
    };

    //template<class T>
    //size_t TypeId<T>::id=TypeIDBase<void>::get_count();

    template <class U>
    const TypeId& typeinfo() {
        static TypeId_impl<U> info;
        return info;
    }

    template <class T>
    const TypeId& typeinfo(const T&) {
        return typeinfo<T>();
    }

    template <class T>
    const TypeId& typeinfo(const T*) {
        return typeinfo<const T*>();
    }

#if __cplusplus >= 201703L
    inline auto& typeid_type = typeinfo<TypeId>();
    inline auto& void_type = typeinfo<void>();
    inline auto& bool_type = typeinfo<bool>();
    inline auto& int8_type = typeinfo<int8_t>();
    inline auto& int16_type = typeinfo<int16_t>();
    inline auto& int32_type = typeinfo<int32_t>();
    inline auto& int64_type = typeinfo<int64_t>();
    inline auto& uint8_type = typeinfo<uint8_t>();
    inline auto& uint16_type = typeinfo<uint16_t>();
    inline auto& uint32_type = typeinfo<uint32_t>();
    inline auto& uint64_type = typeinfo<uint64_t>();
#endif
    struct Anything {
       private:
        struct BaseTy {
            virtual void* get() const = 0;
            virtual const TypeId& type() const = 0;
            virtual ~BaseTy() {}
        };

        template <class T>
        struct HolderTy : public BaseTy {
            T t;
            const TypeId& id;
            HolderTy(T&& in)
                : t(std::forward<T>(in)), id(typeinfo<T>()) {}
            HolderTy(const T& in)
                : t(in), id(typeinfo<T>()) {}
            void* get() const override {
                return (void*)std::addressof(t);
            }
            const TypeId& type() const override {
                return id;
            }
            ~HolderTy() {}
        };

        BaseTy* hold = nullptr;
        //TypeId& id;
        //size_t refsize=0;
        //size_t Tsize=0;
       public:
        Anything() {}

        template <class T>
        Anything(const T& in) {
            hold = new HolderTy<T>(in);
            //id=TypeID<T>::get_id();
            //refsize=sizeof(HolderTy<T>);
            //Tsize=sizeof(T);
        }

        template <class T>
        Anything(T* in) {
            hold = new HolderTy<T*>(in);
        }
        /*
        template<class T>
        Anything(T* in){
            hold=new HolderTy<T*>(in);
        }*/

        template <class T>
        Anything(T&& in) {
            hold = new HolderTy<T>(std::forward<T>(in));
        }

        //Anything(Anything*)=delete;

        Anything& operator=(Anything&& in) {
            delete hold;
            hold = in.hold;
            in.hold = nullptr;
            return *this;
        }

        Anything& operator=(const Anything& in) = delete;

        void reset() {
            delete hold;
            hold = nullptr;
        }

        const TypeId& type() const {
            if (!hold) return typeinfo<void>();
            return hold->type();
        }

       private:
        template <class T>
        friend T cast_to(const Anything&);
        template <class T>
        friend T cast_to(Anything&&);
        template <class T>
        friend const T* cast_to(const Anything*);
        template <class T>
        friend T* cast_to(Anything*);

        template <class T>
        T* get() const {
            if (hold && hold->type() == typeinfo<T>()) {
                return reinterpret_cast<T*>(hold->get());
            }
            return nullptr;
        }
    };

    template <class T>
    const T* cast_to(const Anything* v) {
        using U = std::remove_reference_t<T>;
        return v->get<U>();
    }

    template <class T>
    T* cast_to(Anything* v) {
        using U = std::remove_reference_t<T>;
        return v->get<U>();
    }

    template <class T>
    T cast_to(const Anything& v) {
        using U = std::remove_reference_t<T>;
        if (auto t = v.get<U>()) {
            return *t;
        }
        throw "type not matched";
    }

    template <class T>
    T cast_to(Anything&& v) {
        using U = std::remove_reference_t<T>;
        if (auto t = v.get<U>()) {
            T res = std::move(*t);
            return res;
        }
        throw "type not matched";
    }

    template <class T>
    struct TupleValue {
        T val;

        template <class V>
        constexpr TupleValue(V&& v)
            : val(std::forward<V>(v)) {}
        T& get() {
            return val;
        }
        const T& get() const {
            return val;
        }
        bool equal(const TupleValue& in) const {
            return val == in;
        }
    };

    template <>
    struct TupleValue<void> {
        constexpr TupleValue(void) {}
        void get() {}
        void get() const {}
        bool equal(const TupleValue& in) const {
            return true;
        }
    };

    template <class... Args>
    struct Tuple {
        constexpr Tuple(Args&&...) {}
        constexpr Tuple(const Tuple& arg) {}

        constexpr static size_t tuple_size = 0;
        static constexpr size_t size() {
            return 0;
        }
        template <class Func>
        void for_each(Func func) {
        }

        template <class Func, class = std::enable_if_t<true>>
        auto put(Func func) {
        }
    };

    template <class T, class... Arg>
    struct Tuple<T, Arg...> {
       private:
        TupleValue<T> val;
        using next_t = Tuple<Arg...>;
        next_t next;

       public:
        constexpr static size_t tuple_size = sizeof...(Arg);
        constexpr Tuple() {}
        constexpr Tuple(T&& in, Arg&&... arg)
            : val(std::forward<T>(in)), next(std::forward<Arg>(arg)...) {}

        constexpr Tuple(const Tuple& arg)
            : val(arg.val), next(arg.next) {}

        template <size_t pos, class = std::enable_if_t<pos == 0, T>>
        constexpr decltype(val.get()) get() const {
            return val.get();
        }

        template <size_t pos, class = std::enable_if_t<pos == 0, T>>
        constexpr decltype(val.get()) get() {
            return val.get();
        }

        template <size_t pos, class = std::enable_if_t<pos != 0, T>>
        constexpr decltype(auto) get() {
            return next.template get<pos - 1>();
        }

        template <size_t pos, class = std::enable_if_t<pos != 0, T>>
        constexpr decltype(auto) get() const {
            return next.template get<pos - 1>();
        }

        static constexpr size_t size() {
            return tuple_size;
        }

        template <class Func>
        void for_each(Func&& func) {
            func(val);
            next.for_each(std::forward<Func>(func));
        }
    };

    template <class... Args>
    Tuple(Args...) -> Tuple<std::decay_t<Args>...>;

    /*
    template <class T>
    struct Tuple<T> {
       private:
        T val;

        constexpr static size_t _size = 1;

       public:
        constexpr Tuple(T&& in)
            : val(std::forward<T>(in)) {}

        constexpr Tuple(const Tuple& arg)
            : val(arg.val) {}

        constexpr size_t size() const {
            return _size;
        }

        constexpr bool operator==(const Tuple& in) const {
            return val == in.val;
        }

        template <size_t pos, class Ret = std::enable_if_t<pos == 0, T>>
        constexpr Ret& get() {
            return val;
        }

        template <class Func>
        void for_each(Func func) {
            func(val);
        }

        template <class Func>
        auto put(Func func) {
            return val;
        }
    };

    template <class... Arg>
    struct Tuple<void, Arg...>;

    template <>
    struct Tuple<void>;*/

    template <class Func, class C = void*, class... Arg1, class... Arg2>
    void tuple_op(Func func, const Tuple<Arg1...>& t1, const Tuple<Arg2...>& t2, C ctx = nullptr) {
        func(t1.val, t2.val, ctx);
        tuple_op(func, t1.next, t2.next, ctx);
    }

    template <class Func, class C = void*, class Arg1, class Arg2>
    void tuple_op(Func func, const Tuple<Arg1>& t1, const Tuple<Arg2>& t2, C ctx = nullptr) {
        func(ctx, t1.val, t2.val, ctx);
    }

    template <class F>
    struct Function;

    enum class FType {
        none,
        obj,
        ptr,
    };

    template <class Ret, class... Args>
    struct Function<Ret(Args...)> {
        struct FuncBase {
            virtual Ret operator()(Args&&... args) = 0;
        };

        using FuncPtrType = Ret (*)(Args...);
        template <class T>
        using MemberPtrType = Ret (T::*)(Args...);

        template <class T>
        struct FuncHolder : FuncBase {
            T t;
            FuncHolder(T&& in)
                : t(std::move(in)) {}
            Ret operator()(Args&&... args) override {
                return t(std::forward<Args>(args)...);
            }
        };

        template <class T>
        struct MemberHolder : FuncBase {
            T* self;
            MemberPtrType<T> ptr;

            MemberHolder(T* s, MemberPtrType<T> p)
                : self(s), ptr(p) {}
            Ret operator()(Args&&... args) override {
                return (self->*ptr)(std::forward<Args>(args)...);
            }
        };

        union {
            FuncBase* base = nullptr;
            FuncPtrType funcp;
        };

        FType type = FType::none;

        constexpr Function()
            : type(FType::none) {}

        Function(FuncPtrType in)
            : funcp(in), type(FType::ptr) {}

        template <class T>
        Function(T* self, MemberPtrType<T> ptr)
            : type(FType::obj) {
            base = new MemberHolder<T>(self, ptr);
        }

        template <class Obj>
        Function(Obj&& obj)
            : type(FType::obj) {
            base = new FuncHolder<Obj>(std::move(obj));
        }

        void destruct() {
            if (type == FType::obj) {
                delete base;
            }
            base = nullptr;
        }

        Function& operator=(Function&& in) {
            base = in.base;
            in.base = nullptr;
            type = in.type;
            in.type = FType::none;
            return *this;
        }

        Ret operator()(Args... args) const {
            if (type == FType::ptr) {
                return funcp(std::forward<Args>(args)...);
            }
            else if (type == FType::obj) {
                return (*base)(std::forward<Args>(args)...);
            }
            throw "no function holds";
        }

        operator bool() const {
            return is_callable();
        }

        bool is_callable() const {
            return type != FType::none;
        }
    };

#if __cplusplus >= 201703L
    template <class Ret, class... Args>
    Function(Ret (*)(Args...)) -> Function<Ret(Args...)>;

    template <class T, class Ret, class... Args>
    Function(T*, Ret (T::*)(Args...)) -> Function<Ret(Args...)>;

    template <class F>
    Function(F) -> Function<F>;
#endif

}  // namespace PROJECT_NAME