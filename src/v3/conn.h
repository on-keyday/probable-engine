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
                if (auto base = get_base()) {
                    return base->open();
                }
                return false;
            }
            virtual State write(const char* str, size_t size) {
                if (auto base = get_base()) {
                    return base->write(str, size);
                }
                return false;
            }
            virtual State read(char* str, size_t size, size_t& red) {
                if (auto base = get_base()) {
                    return base->read(str, size, red);
                }
                return false;
            }
            virtual bool set_setting(const InputSetting& in) {
                if (auto base = get_base()) {
                    return base->set_setting(in);
                }
                return false;
            }
            virtual bool get_setting(OutputSetting& out) {
                if (auto base = get_base()) {
                    return base->get_setting(out);
                }
                return false;
            }
            virtual State close() {
                if (auto base = get_base()) {
                    return base->close();
                }
                return false;
            }

            virtual bool has_error(ErrorInfo* info) {
                if (auto base = get_base()) {
                    return base->has_error(info);
                }
                if (info) {
                    info->type = ContextType::unknown;
                    info->errmsg = "unimplemented";
                    info->numerr = -1;
                }
                return true;
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