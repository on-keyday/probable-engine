/*
    socklib - simple socket library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "../common/platform.h"
#include <enumext.h>
#include "non_block_context.h"

namespace socklib {
    namespace v3 {
        /*
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
        */

        struct Conn {
           private:
            std::shared_ptr<Conn> base;

           public:
            virtual ContextType get_type() const {
                return ContextType::unknown;
            }
            virtual Conn* get_base() {
                if (base) {
                    return std::addressof(*base);
                }
                return nullptr;
            }
            virtual State open(ContextManager& m) {
                if (auto base = get_base()) {
                    return base->open(*m.get_child());
                }
                return false;
            }
            virtual State write(ContextManager& m, const char* str, size_t size) {
                if (auto base = get_base()) {
                    return base->write(*m.get_child(), str, size);
                }
                return false;
            }
            virtual State read(ContextManager& m, char* str, size_t size, size_t& red) {
                if (auto base = get_base()) {
                    return base->read(*m.get_child(), str, size, red);
                }
                return false;
            }
            /*
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
            */
            virtual State close(ContextManager& m) {
                if (auto base = get_base()) {
                    return base->close(*m.get_child());
                }
                return false;
            }
            /*
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
            }*/
        };

        /*
        template <ContextType v>
        struct IContext : Context {
            IContext(const StringBufferBuilder* b)
                : Context(b) {}

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
        }*/
    }  // namespace v3
}  // namespace socklib