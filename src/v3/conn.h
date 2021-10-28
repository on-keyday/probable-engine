/*
    socklib - simple socket library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "../common/platform.h"
#include <enumext.h>

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
        };

        using State = commonlib2::EnumWrap<StateValue, StateValue::succeed, StateValue::failed>;

        struct Setting {
            virtual ContextType get_type() const = 0;
        };

        struct InputSetting : Setting {
            constexpr static bool is_output() {
                return false;
            }
        };

        struct OutputSetting : Setting {
            constexpr static bool is_output() {
                return true;
            }
        };

        template <ContextType v>
        struct IInputSetting : InputSetting {
            constexpr static ContextType setting_type() {
                return v;
            }

            virtual ContextType get_type() const override {
                return v;
            }
        };

        template <ContextType v>
        struct IOutputSetting : OutputSetting {
            constexpr static ContextType setting_type() {
                return v;
            }

            virtual ContextType get_type() const override {
                return v;
            }
        };

        struct ErrorInfo {
            ContextType type = ContextType::unknown;
            const char* errmsg = nullptr;
            std::int64_t numerr = 0;
        };

        struct Context {
            virtual ContextType get_type() const {
                return ContextType::unknown;
            }
            virtual Context* get_base() {
                return nullptr;
            }
            virtual State open() {
                return false;
            }
            virtual State write(const char*, size_t) {
                return false;
            }
            virtual State read(const char*, size_t) {
                return false;
            }
            virtual bool set_setting(const InputSetting&) {
                return false;
            }
            virtual bool get_setting(OutputSetting&) {
                return false;
            }
            virtual State close() {
                return false;
            }

            virtual bool has_error(ErrorInfo* info) {
                return false;
            }
        };

        template <ContextType v>
        struct IContext : Context {
            constexpr static ContextType context_type() {
                return v;
            }

            virtual ContextType get_type() const override {
                return v;
            }
        };

        template <class T>
        T* cast_(Context* ctx) {
            if (!ctx) return nullptr;
            if (ctx->get_type() == T::context_type()) {
                return static_cast<T*>(ctx);
            }
            return nullptr;
        }

        template <class T>
        T* cast_(OutputSetting* ctx) {
            if (!ctx) return nullptr;
            if (ctx->get_type() == T::setting_type()) {
                return static_cast<T*>(ctx);
            }
            return nullptr;
        }

        template <class T>
        const T* cast_(const InputSetting* ctx) {
            if (!ctx) return nullptr;
            if (ctx->get_type() == T::setting_type()) {
                return static_cast<const T*>(ctx);
            }
            return nullptr;
        }
    }  // namespace v3
}  // namespace socklib