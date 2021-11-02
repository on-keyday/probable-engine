/*
    socklib - simple socket library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <enumext.h>
#include "string_buffer.h"
#include <atomic>
#include <memory>

namespace socklib {
    namespace v3 {
        enum class ContextType {
            unknown,
            dns,
            tcp_socket,
            udp_socket,
            ssl,
            crypt,
            file,
            http1,
            http2,
        };

        enum class StateValue {
            succeed,
            failed,
            inprogress,
            fatal,
        };

        enum class CtxState {
            free,
            opening,
            writing,
            reading,
            closing,
        };

        BEGIN_ENUM_STRING_MSG(CtxState, state_value)
        ENUM_STRING_MSG(CtxState::opening, "opening")
        ENUM_STRING_MSG(CtxState::writing, "writing")
        ENUM_STRING_MSG(CtxState::reading, "reading")
        ENUM_STRING_MSG(CtxState::closing, "closing")
        END_ENUM_STRING_MSG("free")

        using State = commonlib2::EnumWrap<StateValue, StateValue::succeed, StateValue::failed>;

        struct ContextID {
           private:
            static std::atomic_flag flag;
            size_t id = 0;
            static size_t init() {
                flag.clear();
                return 0;
            }
            static size_t get_id() {
                static size_t idholder = init();
                while (flag.test_and_set() == true) {
                    flag.wait(true);
                }
                idholder++;
                auto e = idholder;
                flag.clear();
                flag.notify_one();
                return e;
            }

           public:
            ContextID()
                : id(get_id()) {}
            size_t get() const {
                return id;
            }
        };

        struct ErrorInfo {
            ContextType type = ContextType::unknown;
            const char* errmsg = nullptr;
            std::int64_t numerr = 0;
        };

        struct Context {
           private:
            ContextType type = ContextType::unknown;
            CtxState state = CtxState::free;
            int progress = 0;
            StringBuffer* errmsg = nullptr;
            errno_t errcode = 0;
            ContextID id;

           public:
            bool is_free_context() const {
                return state == CtxState::free;
            }
            static size_t type_id() {
                static ContextID id;
                return id.get();
            }

            constexpr static ContextType get_type() {
                return ContextType::unknown;
            }

            Context(const StringBufferBuilder* b, ContextType type)
                : type(type), errmsg(b->make()) {}

            template <CtxState check>
            bool test_and_set_state() {
                if (state == CtxState::free || state == check) {
                    report("invalid state. now state is", -1);
                    add_report(state_value(state));
                    add_report(", expected free or ");
                    add_report(state_value(check));
                    return false;
                }
                state = check;
                return true;
            }

            void reset_state() {
                state = CtxState::free;
            }

            size_t get_id() const {
                return id.get();
            }

            void report(const char* err, errno_t code = -1) {
                set_progress(0);
                reset_state();
                errmsg->clear();
                errmsg->set(err);
                errcode = code;
            }
            void add_report(const char* err) {
                errmsg->append(err);
            }

            int get_progress() {
                return progress;
            }

            void increment() {
                progress++;
            }

            void set_progress(int p) {
                progress = p;
            }

            virtual size_t get_typeid() const {
                return type_id();
            }

            bool has_error(ErrorInfo& info) const {
                if (errmsg->size()) {
                    info.errmsg = errmsg->c_str();
                    info.numerr = errcode;
                    info.type = type;
                    return true;
                }
                return false;
            }

            virtual ~Context() {
                delete errmsg;
            }
        };

#define OVERRIDE_CONTEXT(TYPE)                   \
    constexpr static ContextType get_type() {    \
        return TYPE;                             \
    }                                            \
    static size_t type_id() {                    \
        static ContextID id;                     \
        return id.get();                         \
    }                                            \
                                                 \
    virtual size_t get_typeid() const override { \
        return type_id();                        \
    }

        struct ContextManager {
           private:
            std::shared_ptr<Context> ctx;
            std::shared_ptr<ContextManager> child;
            StringBufferBuilder* b;

           public:
            ContextManager(StringBufferBuilder* b)
                : b(b) {}

            template <class CtxType>
            State check_id(CtxType*& ctx, bool* is_new = nullptr) {
                auto got = this->get<CtxType>();
                if (!got) {
                    return StateValue::fatal;
                }
                if (ctx) {
                    if (ctx->get_id() != got->get_id()) {
                        got->report("invalid context. id not same");
                        return false;
                    }
                    if (is_new) {
                        *is_new = false;
                    }
                }
                else {
                    ctx = got;
                    if (is_new) {
                        *is_new = true;
                    }
                }
                return true;
            }

            template <class CtxType>
            CtxType* get() {
                if (ctx) {
                    Context* ptr = std::addressof(*ctx);
                    if (ptr->get_typeid() != CtxType::type_id()) {
                        return nullptr;
                    }
                    return static_cast<CtxType*>(ptr);
                }
                return nullptr;
            }

            template <class CtxType>
            CtxType* get_or_make() {
                if (auto e = get<CtxType>()) {
                    return e;
                }
                else {
                    auto tmp = std::make_shared<CtxType>(b, CtxType::get_type());
                    ctx = tmp;
                    return std::addressof(*tmp);
                }
            }

            ContextManager* get_child() {
                if (!child) {
                    child = std::make_shared<ContextManager>();
                }
                return std::addressof(*child);
            }

            template <class CtxType>
            CtxType* get_in_link(size_t* dpresult = nullptr, size_t depth = 0) {
                if (auto t = get<CtxType>()) {
                    if (dpresult) {
                        *dpresult = depth;
                    }
                    return t;
                }
                if (child) {
                    return child->get_in_link<CtxType>(dpresult, depth + 1);
                }
                return nullptr;
            }

            bool get_error_in_link(ErrorInfo& info) const {
                if (ctx && ctx->has_error(info)) {
                    return true;
                }
                if (child) {
                    return child->get_error_in_link(info);
                }
                return true;
            }
        };

    }  // namespace v3
}  // namespace socklib