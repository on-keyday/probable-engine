/*
    commonlib - common utility library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "project_name.h"

#include "reader.h"
#include "extutil.h"
#include "callback_invoker.h"
#include <map>

namespace PROJECT_NAME {

    enum class OptError {
        none,
        unknown,
        invalid_argument,
        already_exists,
        no_option,
        option_suspended,
        ignore_after,
        invalid_format,
        not_found,
        need_more_argument,
        option_already_set,
    };

    BEGIN_ENUM_ERROR_MSG(OptError)
    ENUM_ERROR_MSG(OptError::none, "no error")
    ENUM_ERROR_MSG(OptError::invalid_argument, "invalid argument")
    ENUM_ERROR_MSG(OptError::already_exists, "option already exists")
    ENUM_ERROR_MSG(OptError::no_option, "no option exists")
    ENUM_ERROR_MSG(OptError::option_suspended, "suspend analyze option")
    ENUM_ERROR_MSG(OptError::ignore_after, "ignore after option")
    ENUM_ERROR_MSG(OptError::not_found, "unknown option")
    ENUM_ERROR_MSG(OptError::need_more_argument, "need more argument")
    ENUM_ERROR_MSG(OptError::option_already_set, "option already set")
    END_ENUM_ERROR_MSG

    /*
    constexpr const char* error_message(OptError e) {
        switch (e) {
            case OptError::none:
                return "no error";
            case OptError::invalid_argument:
                return "invalid argument";
            case OptError::already_exists:
                return "option already exists";
            case OptError::no_option:
                return "no option exists";
            case OptError::option_suspended:
                return "suspend analyze option";
            case OptError::ignore_after:
                return "ignore after option";
            case OptError::invalid_format:
                return "expect long name but not";
            case OptError::not_found:
                return "unknown option";
            case OptError::need_more_argument:
                return "need more argument";
            case OptError::option_already_set:
                return "option already set";
            default:
                return "unknown error";
        }
    }*/

    using OptErr = EnumWrap<OptError, OptError::none, OptError::unknown>;

    enum class OptOption {
        none = 0,
        two_prefix_igopt = 0x1,
        two_prefix_longname = 0x2,
        ignore_when_not_found = 0x4,
        two_same_opt_denied = 0x8,
        parse_all_arg = 0x10,
        default_mode = two_prefix_igopt | ignore_when_not_found | two_prefix_longname | parse_all_arg
    };

    DEFINE_ENUMOP(OptOption)

    template <class Char = char, class String = std::string>
    struct Option {
        String optname;
        Char alias[3] = {0};
        String help;
        size_t argcount = 0;
        bool same_denied = false;
        bool needless_cut = false;

        Option& operator=(const Option& other) {
            optname = other.optname;
            help = other.help;
            alias[0] = other.alias[0];
            alias[1] = other.alias[1];
            alias[2] = other.alias[2];
            argcount = other.argcount;
            needless_cut = other.needless_cut;
            return *this;
        }
    };

    template <class Char = char, class String = std::string, template <class...> class Vec = std::vector, template <class...> class Map = std::map>
    struct OptMap {
        using Opt = Option<Char, String>;

       private:
        Map<Char, Opt*> char_opt;
        Map<String, Opt> str_opt;

        Char optprefix = (Char)'-';
        String usage;

       public:
        struct OptResult {
           private:
            friend struct OptMap;
            Opt* base = nullptr;
            Vec<Vec<String>> arg_;

           public:
            const Opt* info() const {
                return base;
            }

            const Vec<Vec<String>>& args() const {
                return arg_;
            }

            const Vec<String>* arg() {
                if (!arg_.size()) {
                    return nullptr;
                }
                return &arg_[0];
            }
        };

        struct OptResMap {
           private:
            friend struct OptMap;
            Map<String, OptResult> mapping;

           public:
            OptResult* has_(const String& opt) {
                if (auto found = mapping.find(opt); found != mapping.end()) {
                    return &found->second;
                }
                return nullptr;
            }

            void clear() {
                mapping.clear();
            }
        };

        void set_optprefix(Char c) {
            optprefix = c;
        }

        Char get_optprefix() const {
            return optprefix;
        }

        OptErr set_option(std::initializer_list<Opt> opt) {
            for (auto&& o : opt) {
                if (auto e = set_option(o); !e) {
                    return e;
                }
            }
            return true;
        }

        OptErr set_option(const Opt& option) {
            if (str_opt.find(option.optname) != str_opt.end()) {
                return OptError::already_exists;
            }
            for (auto& c : option.alias) {
                if (c == 0) break;
                if (char_opt.find(c) != char_opt.end()) {
                    return OptError::already_exists;
                }
            }
            //auto& opt = optbase.back();
            auto opt = str_opt.emplace(option.optname, option);
            for (auto& i : option.alias) {
                if (i == 0) break;
                char_opt.emplace(i, &opt.first->second);
            }
            return true;
        }

        OptErr unset_option(const String& optname) {
            if (auto found = str_opt.find(optname); found != str_opt.end()) {
                for (auto a : found->second.alias) {
                    if (a == 0) break;
                    char_opt.erase(a);
                }
                str_opt.erase(optname);
                return true;
            }
            return OptError::not_found;
        }

       private:
        static void fullargkey(const char* key, String& tmp) {
            Reader<const char*>(key) >> tmp;
        }

        void setfullarg(String& tmp) {
            fullargkey(":arg", tmp);
            str_opt[tmp] = Opt{
                .optname = tmp,
                .argcount = 1,
                .needless_cut = true,
            };
        }

       public:
        void set_usage(const String& use) {
            usage = use;
        }

        String help(size_t preoffset = 0, size_t currentoffset = 2, bool noUsage = false) const {
            String ret;
            String fullarg;
            size_t two = currentoffset << 1;
            fullargkey(":arg", fullarg);
            auto add_space = [&](size_t count) {
                for (size_t i = 0; i < count; i++) {
                    ret += (Char)' ';
                }
            };
            if (usage.size()) {
                if (!noUsage) {
                    add_space(preoffset);
                    fullargkey("Usage:\n", ret);
                }
                add_space(preoffset + currentoffset);
                ret += "";
                ret += usage;
                ret += (Char)'\n';
            }
            size_t maxlen = 0;
            auto make_str = [&](auto& op, size_t* onlysize) {
                if (onlysize) {
                    *onlysize += preoffset + two;
                }
                else {
                    add_space(preoffset + two);
                }
                size_t sz = preoffset + two;
                for (auto c : op.alias) {
                    if (c == 0) break;
                    if (onlysize) {
                        *onlysize += 4;
                    }
                    else {
                        ret += (Char)'-';
                        ret += (Char)c;
                        ret += (Char)',';
                        ret += (Char)' ';
                        sz += 4;
                    }
                }
                if (onlysize) {
                    *onlysize += 3;
                    *onlysize += op.optname.size();
                }
                else {
                    ret += (Char)'-';
                    ret += (Char)'-';
                    ret += op.optname;
                    ret += (Char)' ';
                    sz += 3;
                    sz += op.optname.size();
                    while (maxlen > sz) {
                        ret += (Char)' ';
                        sz++;
                    }
                    ret += (Char)':';
                    ret += op.help;
                    ret += '\n';
                }
            };
            for (auto& opd : str_opt) {
                auto& op = opd.second;
                if (!op.help.size() || op.optname == fullarg) continue;
                size_t sz = 0;
                make_str(op, &sz);
                if (sz > maxlen) {
                    maxlen = sz;
                }
            }
            for (auto& opd : str_opt) {
                auto& op = opd.second;
                if (!op.help.size() || op.optname == fullarg) continue;
                make_str(op, nullptr);
            }
            return ret;
        }

        template <class C, class Ignore = bool (*)(const String&)>
        OptErr parse_opt(int& index, int& col, int argc, C** argv, OptResMap& optres, OptOption op = OptOption::default_mode, Ignore&& cb = Ignore()) {
            if (!argv || argc < 0 || index < 0 || col < 0) {
                return OptError::invalid_argument;
            }
            String fullarg;
            if (any(op & OptOption::parse_all_arg)) {
                setfullarg(fullarg);
            }
            auto set_optarg = [&](Opt* opt, bool fullarg = false) -> OptErr {
                OptResult* res = nullptr;
                if (auto found = optres.mapping.find(opt->optname); found != optres.mapping.end()) {
                    if (!fullarg && (any(op & OptOption::two_same_opt_denied) || opt->same_denied)) {
                        return OptError::option_already_set;
                    }
                    res = &found->second;
                }
                else {
                    res = &optres.mapping[opt->optname];
                    res->base = opt;
                }
                if (opt->argcount) {
                    Vec<String> arg;
                    for (auto i = 0; i < opt->argcount; i++) {
                        index++;
                        if (index == argc || !argv[index]) {
                            return OptError::need_more_argument;
                        }
                        String str;
                        Reader(argv[index]) >> str;
                        arg.push_back(std::move(str));
                    }
                    if (opt->needless_cut) {
                        if (!res->arg_.size()) {
                            res->arg_.push_back(Vec<String>());
                        }
                        for (auto&& a : arg) {
                            res->arg_[0].push_back(std::move(a));
                        }
                    }
                    else {
                        res->arg_.push_back(std::move(arg));
                    }
                }
                return true;
            };
            auto all_arg_set = [&]() -> OptErr {
                index--;
                if (auto e = set_optarg(&str_opt[fullarg], true); !e) {
                    return e;
                }
                return true;
            };
            auto invoke = [&](String&& str, bool on_error) {
                return invoke_cb<Ignore, bool>::invoke(std::forward<Ignore>(cb), str, on_error);
            };
            bool first = false;
            for (; index < argc; index++) {
                auto arg = argv[index];
                for (; arg[col]; col++) {
                    if (col == 0) {
                        if (arg[0] != (C)optprefix) {
                            if (any(op & OptOption::parse_all_arg)) {
                                if (auto e = all_arg_set(); !e) {
                                    return e;
                                }
                                break;
                            }
                            if (first) {
                                return OptError::no_option;
                            }
                            else {
                                return OptError::option_suspended;
                            }
                        }
                        if (arg[1] == 0) {
                            if (any(op & OptOption::parse_all_arg)) {
                                if (auto e = all_arg_set(); !e) {
                                    return e;
                                }
                                break;
                            }
                            return OptError::no_option;
                        }
                        if (arg[1] == (C)optprefix) {
                            if (any(op & OptOption::two_prefix_igopt)) {
                                if (arg[2] == 0) {
                                    index++;
                                    if (any(op & OptOption::parse_all_arg)) {
                                        for (; index < argc; index++) {
                                            if (auto e = all_arg_set(); !e) {
                                                return e;
                                            }
                                        }
                                        return true;
                                    }
                                    return OptError::ignore_after;
                                }
                            }
                            if (any(op & OptOption::two_prefix_longname)) {
                                if (arg[2] == 0) {
                                    if (any(op & OptOption::parse_all_arg)) {
                                        if (auto e = all_arg_set(); !e) {
                                            return e;
                                        }
                                        break;
                                    }
                                    return OptError::invalid_format;
                                }
                                if (auto found = str_opt.find((const C*)(arg + 2)); found == str_opt.end()) {
                                    if (any(op & OptOption::ignore_when_not_found)) {
                                        if (!invoke(arg + 1, false)) {
                                            return OptError::not_found;
                                        }
                                        break;
                                    }
                                    if (any(op & OptOption::parse_all_arg)) {
                                        if (auto e = all_arg_set(); !e) {
                                            return e;
                                        }
                                        break;
                                    }
                                    invoke(arg + 1, true);
                                    return OptError::not_found;
                                }
                                else {
                                    if (auto e = set_optarg(&found->second); !e) {
                                        return e;
                                    }
                                    break;
                                }
                            }
                        }
                    }
                    else {
                        if (auto found = char_opt.find(arg[col]); found == char_opt.end()) {
                            if (any(op & OptOption::ignore_when_not_found)) {
                                if (!invoke(String(arg + col, 1), false)) {
                                    return OptError::not_found;
                                }
                                continue;
                            }
                            invoke(String(arg + col, 1), true);
                            return OptError::not_found;
                        }
                        else {
                            if (auto e = set_optarg(found->second); !e) {
                                invoke(String(arg + col, 1), true);
                                return e;
                            }
                        }
                    }
                }
                col = 0;
            }
            return true;
        }
    };
}  // namespace PROJECT_NAME
