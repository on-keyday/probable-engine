/*
    commonlib - common utility library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once

#include "project_name.h"

#include "optmap.h"

namespace PROJECT_NAME {
    template <class Cmd, class Char = char, class String = std::string, template <class...> class Vec = std::vector, template <class...> class Map = std::map>
    struct SubCommand_base {
        using option_t = OptMap<Char, String, Vec, Map>;
        using optset_t = typename option_t::Opt;
        using optres_t = typename option_t::OptResMap;

       protected:
        String cmdname;
        option_t opt;
        Map<String, Cmd> subcmd;
        Cmd* parent = nullptr;
        String helpstr;

       public:
        SubCommand_base() {}
        SubCommand_base(const String& name)
            : cmdname(name) {}

        struct SubCmdResult {
            friend Cmd;
            friend SubCommand_base;

           private:
            Vec<std::pair<String, optres_t>> result;
            Cmd* current = nullptr;

           public:
            const Cmd* get_current() const {
                return current;
            }

            size_t get_layersize() const {
                return result.size();
            }
            optres_t* get_layer(const String& name) {
                for (auto& v : result) {
                    if (v.first == name) {
                        return &v.second;
                    }
                }
                return nullptr;
            }

            optres_t* get_layer(size_t index) {
                if (index >= result.size()) {
                    return nullptr;
                }
                return &result[index].second;
            }

            String fmt(const String& msg, Char sep = ':') {
                if (current) {
                    return current->get_currentcmdname(sep) + msg;
                }
                return msg;
            }

            String fmtln(const String& msg, Char sep = ':') {
                return fmt(msg, sep) + '\n';
            }

            String help(const Cmd* p = nullptr, const char* usagemsg = "Usage:", const char* subcmdmsg = "Subcommand:", size_t currentoffset = 2, size_t preoffset = 0, bool noUsage = false) {
                if (!p) {
                    p = current;
                }
                if (p) {
                    String cmdname = p->get_currentcmdname(0);
                    cmdname.pop_back();
                    return p->help(preoffset, currentoffset, noUsage, subcmdmsg, usagemsg, cmdname.c_str());
                }
                return String();
            }
        };

        void set_helpstr(const String& str) {
            helpstr = str;
        }

        String help(size_t preoffset = 0, size_t currentoffset = 2, bool noUsage = false, const char* subcmdmsg = "Subcommand:", const char* usagemsg = "Usage:", const char* cmdnamealias = nullptr) const {
            String ret;
            auto add_space = [&](auto count) {
                for (size_t i = 0; i < count; i++) {
                    ret += (Char)' ';
                }
            };
            auto add = 0;
            if (!noUsage) {
                add_space(preoffset);
                if (cmdnamealias) {
                    ret += cmdnamealias;
                }
                else {
                    ret += cmdname;
                }
                if (helpstr.size()) {
                    ret += ' ';
                    ret += '-';
                    ret += ' ';
                    ret += helpstr;
                }
                ret += '\n';
                add = currentoffset;
            }
            auto two = (currentoffset << 1);
            ret += opt.help(preoffset + add, currentoffset, noUsage, usagemsg);
            if (subcmd.size()) {
                ret += '\n';
                add_space(preoffset + add);
                Reader<const char*>(subcmdmsg) >> ret;
                ret += '\n';
                size_t maxlen = 0;
                for (auto& sub : subcmd) {
                    auto sz = sub.second.cmdname.size();
                    if (sz > maxlen) {
                        maxlen = sz;
                    }
                }
                for (auto& sub : subcmd) {
                    add_space(preoffset + two + add);
                    ret += sub.second.cmdname;
                    auto sz = sub.second.cmdname.size();
                    while (sz < maxlen) {
                        ret += ' ';
                        sz++;
                    }
                    ret += ' ';
                    ret += ':';
                    ret += sub.second.helpstr;
                    ret += '\n';
                }
            }
            return ret;
        }

        String get_currentcmdname(Char sep = ':') const {
            String ret;
            if (parent) {
                ret = parent->get_currentcmdname(sep);
            }
            ret += cmdname;
            if (sep != 0) {
                ret += sep;
            }
            ret += ' ';
            return ret;
        }

        Cmd* set_usage(const String& str) {
            opt.set_usage(str);
            return static_cast<Cmd*>(this);
        }

        const String& get_cmdname() const {
            return cmdname;
        }

        const Cmd* get_parent() const {
            return parent;
        }

        const Cmd* get_subcmd(const String& name) const {
            if (auto found = subcmd.find(name); found != subcmd.end()) {
                return &found->second;
            }
            return nullptr;
        }

        option_t& get_option() {
            return opt;
        }

        OptErr set_option(std::initializer_list<optset_t> list) {
            return opt.set_option(list);
        }

        Cmd* set_subcommand(const String& name, const String& help, std::initializer_list<optset_t> list = {}) {
            if (subcmd.count(name)) {
                return nullptr;
            }
            Cmd ret(name);
            if (!ret.opt.set_option(list)) {
                return nullptr;
            }
            ret.helpstr = help;
            ret.parent = static_cast<Cmd*>(this);
            return &(subcmd[name] = std::move(ret));
        }

        template <class C, class Ignore = bool (*)(const String&, bool)>
        OptErr parse_opt(int argc, C** argv, SubCmdResult& optres, OptOption op = OptOption::default_mode, Ignore&& cb = Ignore()) {
            int index = 1, col = 0;
            return parse_opt(index, col, argc, argv, optres, op, cb);
        }

        template <class C, class Ignore = bool (*)(const String&, bool)>
        OptErr parse_opt(int& index, int& col, int argc, C** argv, SubCmdResult& optres, OptOption op = OptOption::default_mode, Ignore&& cb = Ignore()) {
            auto flag = op;
            if (subcmd.size()) {
                flag &= ~OptOption::parse_all_arg;
            }
            optres.result.push_back(std::pair{cmdname, optres_t()});
            optres.current = static_cast<Cmd*>(this);
            while (true) {
                if (auto e = opt.parse_opt(index, col, argc, argv, optres.result.back().second, flag, cb)) {
                    return true;
                }
                else if (e == OptError::option_suspended && subcmd.size()) {
                    if (auto found = subcmd.find(argv[index]); found != subcmd.end()) {
                        index++;
                        return found->second.parse_opt(index, col, argc, argv, optres, op, cb);
                    }
                    if (any(op & OptOption::parse_all_arg)) {
                        flag = op;
                        continue;
                    }
                    return e;
                }
                else {
                    return e;
                }
            }
        }
    };

    template <class Char = char, class String = std::string, template <class...> class Vec = std::vector, template <class...> class Map = std::map>
    struct SubCommand : SubCommand_base<SubCommand<Char, String, Vec, Map>, Char, String, Vec, Map> {
        using base_t = SubCommand_base<SubCommand, Char, String, Vec, Map>;
        using result_t = typename base_t::SubCmdResult;
        SubCommand() {}
        SubCommand(const String& name)
            : SubCommand_base<SubCommand, Char, String, Vec, Map>(name) {}
    };

    template <class... Args>
    struct FuncHolder {
        using result_t = std::pair<int, bool>;

       private:
        struct Base {
            virtual result_t operator()(Args&&... args) const = 0;
            virtual ~Base() {}
        };
        Base* base = nullptr;

        DEFINE_ENABLE_IF_EXPR_VALID(return_int, (int)std::declval<T>()(std::declval<Args>()...));
        DEFINE_ENABLE_IF_EXPR_VALID(return_pair, (result_t)std::declval<T>()(std::declval<Args>()...));

        template <class F, bool is_int = return_int<F>::value, bool is_pair = return_pair<F>::value>
        struct Impl : Base {
            F f;
            Impl(F&& in)
                : f(std::forward<F>(in)) {}
            result_t operator()(Args&&... args) const override {
                return {(int)f(std::forward<Args>(args)...), true};
            }
            virtual ~Impl() {}
        };

        template <class F>
        struct Impl<F, false, false> : Base {
            F f;
            Impl(F&& in)
                : f(std::forward<F>(in)) {}
            result_t operator()(Args&&... args) const override {
                f(std::forward<Args>(args)...);
                return {0, true};
            }
            virtual ~Impl() {}
        };

        template <class F>
        struct Impl<F, false, true> : Base {
            F f;
            Impl(F&& in)
                : f(std::forward<F>(in)) {}
            result_t operator()(Args&&... args) const override {
                return f(std::forward<Args>(args)...);
            }
            virtual ~Impl() {}
        };

       public:
        constexpr FuncHolder() {}

        operator bool() const {
            return base != nullptr;
        }

        result_t operator()(Args&&... args) const {
            return (*base)(std::forward<Args>(args)...);
        }

        FuncHolder& operator=(FuncHolder&& in) noexcept {
            delete base;
            base = in.base;
            in.base = nullptr;
            return *this;
        }

        template <class F>
        FuncHolder(F&& in) {
            base = new Impl<F>(std::forward<F>(in));
        }

        FuncHolder(const FuncHolder&) = delete;

        FuncHolder(FuncHolder&& in) noexcept {
            delete base;
            base = in.base;
            in.base = nullptr;
        }

        ~FuncHolder() {
            delete base;
        }
    };

    template <class Char = char, class String = std::string, template <class...> class Vec = std::vector, template <class...> class Map = std::map>
    struct SubCmdDispatch : SubCommand_base<SubCmdDispatch<Char, String, Vec, Map>, Char, String, Vec, Map> {
        using base_t = SubCommand_base<SubCmdDispatch<Char, String, Vec, Map>, Char, String, Vec, Map>;
        using result_t = typename base_t::SubCmdResult;
        using optset_t = typename base_t::optset_t;

       private:
        using holder_t = FuncHolder<result_t&>;
        holder_t func;

       public:
        SubCmdDispatch() {}
        SubCmdDispatch(const String& name)
            : SubCommand_base<SubCmdDispatch<Char, String, Vec, Map>, Char, String, Vec, Map>(name) {}

        template <class F>
        SubCmdDispatch(const String& name, F&& f)
            : func(std::decay_t<F>(f)), SubCommand_base<SubCmdDispatch<Char, String, Vec, Map>, Char, String, Vec, Map>(name) {}

        template <class F>
        void set_callback(F&& f) {
            func = std::forward<F>(f);
        }

        const holder_t& get_callback() const {
            return func;
        }

        template <class F = holder_t>
        SubCmdDispatch* set_subcommand(const String& name, const String& help, std::initializer_list<optset_t> list = {}, F&& in = holder_t()) {
            auto ret = base_t::set_subcommand(name, help, list);
            if (ret) {
                ret->func = std::forward<F>(in);
            }
            return ret;
        }

        template <class C, class Ignore = bool (*)(const String&, bool)>
        std::pair<OptErr, int> run(int argc, C** argv, OptOption op = OptOption::default_mode, Ignore&& cb = Ignore()) {
            result_t result;
            op |= OptOption::parse_all_arg;
            if (auto e = this->parse_opt(argc, argv, result, op, cb); !e) {
                return {e, (int)e.e};
            }
            auto ptr = result.get_current();
            while (ptr) {
                if (ptr->func) {
                    auto e = ptr->func(result);
                    if (e.second || !ptr->get_parent()) {
                        return {true, e.first};
                    }
                }
                ptr = ptr->get_parent();
            }
            return {false, -1};
        }
    };
}  // namespace PROJECT_NAME
