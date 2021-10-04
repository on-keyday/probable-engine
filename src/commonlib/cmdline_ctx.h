/*
    commonlib - common utility library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <memory>

#include "basic_helper.h"
#include "project_name.h"
#include "reader.h"
#include "utfreader.h"

namespace PROJECT_NAME {

    enum class OptFlag {
        none = 0,
        only_one = 0x1,
        hidden_from_child = 0x2,
        effect_to_parent = 0x4,
    };

    DEFINE_ENUMOP(OptFlag)

    template <class Str>
    struct OptionInfo {
        using string_type = Str;
        string_type name;
        string_type helpstr;
        size_t argcount = 0;
        size_t mustcount = 0;
        //bool only_one=false;
        OptFlag flag = OptFlag::none;
    };

    template <class Buf, class Str, template <class...> class Map, template <class...> class Vec, class User>
    struct CommandCtx;

    template <class Buf, class Str, template <class...> class Map, template <class...> class Vec, class User>
    struct CmdlineCtx;

    enum class LogFlag {
        none = 0,
        program_name = 0x1,
        no_root_name = 0x2,
    };

    DEFINE_ENUMOP(LogFlag);

    enum class CmdFlag {
        none = 0,
        invoke_parent = 0x1,
        use_parentopt = 0x2,
        take_over_prev_run = 0x4,
        no_option = 0x8,
        ignore_opt_after_two_hyphen = 0x10,
        ignore_subc_after_two_hyphen = 0x20,
        ignore_special_after_two_hypen = ignore_opt_after_two_hyphen | ignore_subc_after_two_hyphen,
        use_parentcb = 0x40,
        ignore_invalid_option = 0x80,
    };

    DEFINE_ENUMOP(CmdFlag)

    template <class Buf, class Str, template <class...> class Map, template <class...> class Vec, class User>
    struct ArgCtx {
        friend CommandCtx<Buf, Str, Map, Vec, User>;
        friend CmdlineCtx<Buf, Str, Map, Vec, User>;
        using string_type = Str;
        template <class Value>
        using vec_type = Vec<Value>;
        template <class Key, class Value>
        using map_type = Map<Key, Value>;
        using cmdline_type = CmdlineCtx<Buf, Str, Map, Vec, User>;
        using command_type = CommandCtx<Buf, Str, Map, Vec, User>;
        using arg_option_maptype = map_type<string_type, vec_type<string_type>>;
        //string_type cmdname;
       private:
        vec_type<string_type> arg;
        CommandCtx<Buf, Str, Map, Vec, User>* basectx;
        map_type<string_type, vec_type<string_type>> options;
        std::unique_ptr<ArgCtx> child = nullptr;
        ArgCtx* parent = nullptr;
        LogFlag logmode = LogFlag::program_name;

       public:
        const vec_type<string_type>* exists_option(const string_type& optname, bool all = false) const {
            const ArgCtx* c = this;
            bool on_parent = false;
            do {
                if (on_parent) {
                    auto opt = c->get_cmdline()->get_option(c->basectx, optname);
                    if (opt && any(opt->flag & OptFlag::hidden_from_child)) {
                        return nullptr;
                    }
                }
                if (c->options.count(optname)) {
                    return std::addressof(c->options.at(optname));
                }
                if (!any(c->basectx->flag & CmdFlag::use_parentopt)) {
                    break;
                }
                c = c->parent;
                on_parent = true;
            } while (all && c);
            return nullptr;
        }

        bool get_optarg(string_type& val, const string_type& optname, size_t index, bool all = false) const {
            if (auto opts = exists_option(optname, all)) {
                auto& opt = (*opts);
                if (opt.size() <= index) return false;
                val = opt[index];
                return true;
            }
            return false;
        }

        vec_type<string_type> get_alloptarg(const string_type& optname) const {
            const ArgCtx* c = this;
            vec_type<string_type> ret;
            do {
                if (auto argp = c->exists_option(optname)) {
                    auto arg = (*argp);
                    for (auto& i : arg) {
                        ret.push_back(i);
                    }
                }
                if (!any(c->basectx->flag & CmdFlag::use_parentopt)) {
                    break;
                }
                c = c->parent;
            } while (c);
            return ret;
        }

        bool cmpname(const string_type& str) const {
            if (!basectx) return false;
            return basectx->cmdname == str;
        }

        User* get_user(bool root = false) const {
            if (!basectx) return nullptr;
            //return root?basectx->get_root_user():std::addressof(basectx->user);
            return basectx->get_root_user();
        }

        size_t arglen() const {
            return arg.size();
        }

        bool get_arg(size_t index, string_type& out) const {
            if (index >= arg.size()) return false;
            out = arg[index];
            return true;
        }

        auto begin() const {
            return arg.begin();
        }

        auto end() const {
            return arg.end();
        }

        const ArgCtx* get_parent() const {
            return parent;
        }

        cmdline_type* get_cmdline() const {
            return basectx ? basectx->basectx : nullptr;
        }

        command_type* get_this_command() const {
            return basectx;
        }

        string_type get_command_name() const {
            return basectx->cmdname;
        }

        void set_logmode(LogFlag flag) {
            logmode = flag;
        }

       private:
        template <class Obj>
        void printlog_impl(Obj&) const {}

        template <class Obj, class First, class... Args>
        void printlog_impl(Obj& obj, First first, Args... args) const {
            obj << first;
            return printlog_impl(obj, args...);
        }

        template <class Obj>
        void print_cmdname(LogFlag mode, Obj& obj, command_type* ctx) const {
            if (!ctx) return;
            if (ctx->parent) {
                print_cmdname(mode, obj, ctx->parent);
            }
            else {
                if (!any(mode & LogFlag::no_root_name)) {
                    if (!ctx->basectx) return;
                    ctx->print_program_name(obj);
                }
            }
            obj << ctx->cmdname.c_str();
            obj << ":";
            return;
        }

       public:
        template <class Obj, class... Args>
        void log_with_flag(LogFlag flag, Obj& obj, Args... args) const {
            if (!basectx) return;
            if (any(flag & LogFlag::program_name)) print_cmdname(flag, obj, basectx);
            printlog_impl(obj, args..., "\n");
        }

        template <class Obj, class... Args>
        void log(Obj& obj, Args... args) const {
            log_with_flag(logmode, obj, args...);
        }
    };

    template <class C>
    struct ArgVWrapper {
        C** argv = nullptr;
        int argc = 0;
        uintptr_t operator[](size_t pos) const {
            if (pos >= argc) return 0;
            return (uintptr_t)argv[pos];
        }
        size_t size() const {
            return (size_t)argc;
        }

        ArgVWrapper(int argc, C** argv)
            : argc(argc), argv(argv) {}

        ArgVWrapper() {}

        ArgVWrapper(const ArgVWrapper& in)
            : argv(in.argv), argc(in.argc) {
        }

        ArgVWrapper(ArgVWrapper&& in)
            : argv(in.argv), argc(in.argc) {
            in.argv = nullptr;
            in.argc = 0;
        }

        ArgVWrapper& operator=(const ArgVWrapper& in) {
            argv = in.argv;
            argc = in.argc;
            return *this;
        }

        ArgVWrapper& operator=(ArgVWrapper&& in) noexcept {
            argv = in.argv;
            argc = in.argc;
            in.argv = nullptr;
            in.argc = 0;
            return *this;
        }
    };

    enum class OptCbFlag {
        failed = 0,
        redirect = 0x1,
        succeed = 0x2,
    };

    DEFINE_ENUMOP(OptCbFlag);

    template <class Buf, class Str, template <class...> class Map, template <class...> class Vec, class User>
    struct CommandCtx {
       private:
        friend CmdlineCtx<Buf, Str, Map, Vec, User>;
        friend ArgCtx<Buf, Str, Map, Vec, User>;
        using string_type = Str;
        using reader_type = Reader<Buf>;
        using arg_type = ArgCtx<Buf, Str, Map, Vec, User>;
        using cmdline_type = CmdlineCtx<Buf, Str, Map, Vec, User>;
        //using args_type=Args<string_type>;
        using param_callback = void (*)(string_type&);
        using param_self_callback = bool (*)(reader_type&, string_type&);
        using command_callback = int (*)(const arg_type&, int prev, int pos);

        template <class Key, class Value>
        using map_type = Map<Key, Value>;

        using option_maptype = map_type<string_type, std::shared_ptr<OptionInfo<string_type>>>;
        using arg_option_maptype = typename arg_type::arg_option_maptype;

        using option_callback = OptCbFlag (*)(reader_type&, string_type&, const option_maptype&, arg_option_maptype&);

        cmdline_type* basectx = nullptr;

        CommandCtx* parent = nullptr;
        string_type cmdname;
        string_type help_str;
        param_callback param_proc = nullptr;
        param_self_callback self_proc = nullptr;
        command_callback command_proc = nullptr;
        command_callback prev_run_proc = nullptr;
        option_callback option_proc = nullptr;
        option_maptype options;
        map_type<string_type, CommandCtx> subcommands;
        //User user=User();
        //bool invoke_parent=false;
        //bool use_parentopt=false;
        CmdFlag flag = CmdFlag::none;

        template <class Obj>
        void print_program_name(Obj& obj) const {
            if (!basectx) return;
            if (basectx->program_name.size()) {
                obj << basectx->program_name.c_str();
                obj << ":";
            }
        }

        User* get_root_user() const {
            return basectx ? std::addressof(basectx->user) : nullptr;
        }
    };

    struct Optname {
        const char* longname = nullptr;
        const char* shortname = nullptr;
        bool arg = false;
        OptFlag flag = OptFlag::none;
        const char* help = nullptr;
    };

    enum class ErrorKind {
        input = 0x1,
        user_callback = 0x2,
        option = 0x4,
        registered_config = 0x8,
        system = 0x10,
        param = 0x20,
    };

    DEFINE_ENUMOP(ErrorKind)

    template <class Buf, class Str, template <class...> class Map, template <class...> class Vec, class User = void*>
    struct CmdlineCtx {
        using string_type = Str;
        template <class Key, class Value>
        using map_type = Map<Key, Value>;
        using reader_type = Reader<Buf>;
        using arg_type = ArgCtx<Buf, Str, Map, Vec, User>;
        using command_type = CommandCtx<Buf, Str, Map, Vec, User>;
        template <class Value>
        using vec_type = Vec<Value>;

        using param_callback = typename command_type::param_callback;
        using input_func = bool (*)(reader_type&);
        using command_callback = typename command_type::command_callback;
        using param_self_callback = typename command_type::param_self_callback;
        using option_callback = typename command_type::option_callback;

        using option_maptype = typename command_type::option_maptype;
        using arg_option_maptype = typename arg_type::arg_option_maptype;

        using error_callback = void (*)(ErrorKind, const string_type&, const char*, const string_type&, const char*);

       private:
        friend CommandCtx<Buf, Str, Map, Vec, User>;
        User user;
        param_callback param_proc = nullptr;
        reader_type r;

        map_type<string_type, command_type> _command;
        string_type program_name;

        error_callback errcb = nullptr;

        vec_type<command_callback> rootcb_after;

        vec_type<command_callback> rootcb_prev;

        map_type<string_type, std::shared_ptr<OptionInfo<string_type>>> rootopt;

        void error(ErrorKind kind, const char* err1, const string_type& msg = string_type(), const char* err2 = "") {
            if (errcb) errcb(kind, program_name, err1, msg, err2);
        }

        /*template<class Func>
        bool for_parents(command_type* ctx,Func func){
            for(command_type* c=ctx;c;c=c->parent){
                OptCbFlag flag=func(c);
                if(flag==OptCbFlag::failed){
                    return false;
                }
                if(flag==OptCbFlag::redirect){
                    break;
                }
            }
            return true;
        }*/

#define for_parents(ctx) for (command_type* c = ctx; c; c = c->parent)

        template <class C>
        static bool get_a_line(Reader<ArgVWrapper<C>>& r, string_type& arg) {
            auto c = (const C*)r.achar();
            if (!c) return false;
            r.increment();
            arg = c;
            return true;
        }

        template <class Bufi>
        static bool get_a_line(Reader<Bufi>& r, string_type& arg) {
            return get_cmdline(r, 1, true, true, arg);
        }

        bool get_a_param(string_type& arg, command_type* ctx = nullptr) {
            bool ok = false;
            for_parents(ctx) {
                if (c->self_proc) {
                    if (!c->self_proc(r, arg)) {
                        error(ErrorKind::param | ErrorKind::user_callback, "user defined param callback failed");
                        return false;
                    }
                    ok = true;
                    break;
                }
                if (any(c->flag & CmdFlag::use_parentcb)) {
                    continue;
                }
                break;
            }
            if (!ok && !get_a_line(r, arg)) {
                error(ErrorKind::param, "get param faild");
                return false;
            }
            for_parents(ctx) {
                if (c->param_proc) {
                    c->param_proc(arg);
                    return true;
                }
                if (any(c->flag & CmdFlag::use_parentcb)) {
                    continue;
                }
                break;
            }
            if (param_proc) {
                param_proc(arg);
            }
            return true;
        }

        bool find_option(std::shared_ptr<OptionInfo<string_type>>& opt, const string_type& str,
                         command_type& ctx, arg_type& out, bool* recover, bool show_err) {
            bool ok = false;
            bool on_parent = false;
            for_parents(&ctx) {
                if (c->options.count(str)) {
                    decltype(opt)& ref = c->options[str];
                    if (!on_parent || !any(ref->flag & OptFlag::hidden_from_child)) {
                        opt = ref;
                        ok = true;
                        break;
                    }
                }
                if (any(c->flag & CmdFlag::use_parentopt)) {
                    on_parent = true;
                    continue;
                }
            }
            if (!ok) {
                if (rootopt.count(str)) {
                    opt = rootopt[str];
                }
                else {
                    if (show_err) {
                        const char* msg = recover ? "'(ignored)" : "'";
                        error(ErrorKind::option, "unknown option '", str, msg);
                    }
                    if (recover) {
                        *recover = true;
                    }
                    return false;
                }
            }
            if (any(opt->flag & OptFlag::only_one)) {
                auto p = &out;
                do {
                    if (p->options.count(opt->name)) {
                        error(ErrorKind::option, "option ", str, " is already set");
                        return false;
                    }
                    p = p->parent;
                } while (any(opt->flag & OptFlag::effect_to_parent) && p);
            }
            return true;
        }

        bool parse_a_option(const string_type& str, command_type& ctx, arg_type& out, bool* recover, bool show_err) {
            std::shared_ptr<OptionInfo<string_type>> opt;
            if (!find_option(opt, str, ctx, out, recover, show_err)) return false;
            vec_type<string_type>& optarg = out.options[opt->name];
            size_t count = 0;
            auto opterr = [&] {
                if (count < opt->mustcount) {
                    error(ErrorKind::option, "option ", str, " needs more argument");
                    return false;
                }
                return true;
            };
            while (count < opt->argcount) {
                if (r.ceof()) {
                    return opterr();
                }
                auto prevpos = r.readpos();
                string_type arg;
                if (!get_a_param(arg, &ctx)) {
                    return false;
                }
                if (arg[0] == '-') {
                    if (!opterr()) return false;
                    r.seek(prevpos);
                    break;
                }
                optarg.push_back(std::move(arg));
                count++;
            }
            return true;
        }

        bool option_parse(string_type& str, command_type& ctx, arg_type& out) {
            for_parents(&ctx) {
                if (c->option_proc) {
                    auto res = c->option_proc(r, str, ctx.options, out.options);
                    if (!any(res)) {
                        error(ErrorKind::option | ErrorKind::user_callback, "user defined option callback failed");
                        return false;
                    }
                    if (any(res & OptCbFlag::succeed)) {
                        return true;
                    }
                    break;
                }
                if (any(c->flag & CmdFlag::use_parentcb)) {
                    continue;
                }
                break;
            }
            if (!str.size()) {
                error(ErrorKind::option, "not option value");
                return false;
            }
            auto f = any(ctx.flag & CmdFlag::ignore_invalid_option);
            bool recov = false;
            if (parse_a_option(str, ctx, out, &recov, false)) {
                return true;
            }
            if (!recov) {
                return false;
            }
            for (auto& c : str) {
                recov = false;
                if (!parse_a_option(string_type(1, c), ctx, out, f ? &recov : nullptr, true)) {
                    if (recov) continue;
                    return false;
                }
            }
            return true;
        }

        bool parse_default(command_type& ctx, arg_type& base, arg_type*& fin) {
            bool hypened = false;
            bool igsubc = any(ctx.flag & CmdFlag::ignore_subc_after_two_hyphen);
            bool igopt = any(ctx.flag & CmdFlag::ignore_opt_after_two_hyphen);
            bool noopt = any(ctx.flag & CmdFlag::no_option);
            bool twohypen = igopt || igsubc;
            while (!r.eof()) {
                string_type str;
                if (!get_a_param(str, &ctx)) {
                    //error("param get failed");
                    return false;
                }
                bool subcmd = !(igsubc && hypened);
                bool option = !(igopt && hypened) && !noopt;
                if (subcmd && ctx.subcommands.count(str)) {
                    command_type& sub = ctx.subcommands[str];
                    base.child.reset(new arg_type());
                    base.child->parent = &base;
                    base.child->logmode = base.logmode;
                    return parse_by_context(sub, *base.child, fin);
                }
                else if (twohypen && !hypened && str == "--") {
                    hypened = true;
                    continue;
                }
                else if (option && str[0] == '-') {
                    str.erase(0, 1);
                    if (!option_parse(str, ctx, base)) {
                        return false;
                    }
                }
                else {
                    base.arg.push_back(str);
                }
            }
            fin = &base;
            return true;
        }

        bool parse_by_context(command_type& ctx, arg_type& base, arg_type*& fin) {
            base.basectx = &ctx;
            /*if(ctx.self_proc){
                fin=&base;
                return ctx.self_proc(r,ctx,base);
            }
            else{*/
            return parse_default(ctx, base, fin);
            //}
        }

        bool parse(arg_type& ctx, arg_type*& fin) {
            Str str;
            if (!get_a_param(str)) return false;
            if (param_proc) param_proc(str);
            return parse_with_name(str, ctx, fin);
        }

        bool parse_with_name(const string_type& name, arg_type& ctx, arg_type*& fin) {
            if (!_command.count(name)) return false;
            return parse_by_context(_command[name], ctx, fin);
        }

        bool input() {
            //r=reader_type(string_type(in),ignore_space);
            r.set_ignore(ignore_space);
            return (bool)r.readable();
        }

        int run(const arg_type& arg) {
            if (!arg.basectx) {
                error(ErrorKind::system, "library is broken");
                return -1;
            }
            int res = -1;
            int pos = 0;
            bool called = false;
            for (auto cb : rootcb_prev) {
                cb(arg, res, -1);
            }
            for (command_type* c = arg.basectx; c; c = c->parent) {
                if (c->command_proc) {
                    if (!called && c->prev_run_proc) {
                        //prev_run_proc will be called
                        //when any command_proc run and registered it.
                        //this is supposed to use at common initialize
                        //operation for the command tree
                        //this is able to do pseudo overload by using CmdFlag::take_over_prev_run
                        c->prev_run_proc(arg, res, -1);
                    }
                    res = c->command_proc(arg, res, pos);
                    called = true;
                    pos++;
                    if (any(c->flag & CmdFlag::invoke_parent)) {
                        continue;
                    }
                    break;
                }
            }
            if (!called) {
                error(ErrorKind::registered_config, "any action is not registered");
                return -1;
            }
            for (auto cb : rootcb_after) {
                cb(arg, res, pos);
                pos++;
            }
            return res;
        }

        command_type* register_command_impl(command_type& cmd, const string_type& name,
                                            command_callback proc, const string_type& help, CmdFlag flag, command_callback prev_run) {
            cmd.cmdname = name;
            cmd.command_proc = proc;
            cmd.help_str = help;
            cmd.basectx = this;
            //cmd.use_parentopt=use_parentopt;
            //cmd.invoke_parent=invoke_parent;
            cmd.flag = flag;
            cmd.prev_run_proc = prev_run;
            return &cmd;
        }

        template <class Input>
        bool input_common(Input in) {
            //if(!in)return false;
            if (!in(r)) {
                error(ErrorKind::input & ErrorKind::user_callback, "input failed");
                return false;
            }
            if (!input()) {
                error(ErrorKind::input, "input anything");
                return false;
            }
            return true;
        }

       public:
        void register_error(error_callback cb) {
            errcb = cb;
        }

        //[[nodiscard]]
        command_type* register_command(const string_type& name, command_callback proc,
                                       const string_type& help = string_type(), CmdFlag flag = CmdFlag::none, command_callback prev_run = nullptr) {
            if (!proc || _command.count(name)) return nullptr;
            command_type& cmd = _command[name];
            return register_command_impl(cmd, name, proc, help, flag, prev_run);
        }

        //[[nodiscard]]
        command_type* register_subcommand(command_type* cmd, const string_type& name,
                                          command_callback proc, const string_type& help = string_type(),
                                          CmdFlag flag = CmdFlag::none, command_callback prev_run = nullptr) {
            if (!cmd) return nullptr;
            if (cmd->subcommands.count(name)) return nullptr;
            auto& sub = cmd->subcommands[name];
            sub.parent = cmd;
            if (!prev_run && any(flag & CmdFlag::take_over_prev_run)) {
                prev_run = cmd->prev_run_proc;
            }
            return register_command_impl(sub, name, proc, help, flag, prev_run);
        }

        [[nodiscard]] command_type* get_command(const string_type& name) {
            return _command.count(name) ? &_command[name] : nullptr;
        }

        [[nodiscard]] command_type* get_subcommand(command_type* cmd, const string_type& name) {
            return cmd && cmd->subcommands.count(name) ? &cmd->subcommands[name] : nullptr;
        }

        bool remove_command(const string_type& name) {
            return (bool)_command.erase(name);
        }

        bool remove_subcommand(command_type* cmd, const string_type& name) {
            return cmd ? (bool)cmd->subcommands.erase(name) : false;
        }

        bool register_root_callback(command_callback proc, bool after = false) {
            if (!proc) return false;
            if (after) {
                rootcb_after.push_back(proc);
            }
            else {
                rootcb_prev.push_back(proc);
            }
            return true;
        }

        bool clear_root_callback(bool after = false) {
            if (after) {
                rootcb_after.resize(0);
            }
            else {
                rootcb_prev.resize(0);
            }
            return true;
        }

        bool register_callbacks(command_type* cmd, param_callback param, option_callback option, param_self_callback self) {
            if (!cmd) return false;
            cmd->param_proc = param;
            cmd->self_proc = self;
            cmd->option_proc = option;
            return true;
        }

        bool register_paramproc(param_callback param) {
            param_proc = param;
            return true;
        }

        std::shared_ptr<OptionInfo<string_type>> register_option(command_type* cmd, const string_type& name,
                                                                 size_t arg = 0, size_t must = 0, OptFlag flag = OptFlag::none, const string_type& help = string_type()) {
            auto set_info = [&](auto& info) {
                info.reset(new OptionInfo<string_type>);
                info->name = name;
                info->argcount = arg;
                info->mustcount = must;
                //info->only_one=only_one;
                info->flag = flag;
                info->helpstr = help;
                return info;
            };
            if (!cmd) {
                if (rootopt.count(name)) return nullptr;
                return set_info(rootopt[name]);
            }
            else {
                if (cmd->options.count(name)) return nullptr;
                return set_info(cmd->options[name]);
            }
        }

        std::shared_ptr<OptionInfo<string_type>> get_option(command_type* cmd, const string_type& name) {
            if (!cmd) {
                if (!rootopt.count(name)) return nullptr;
                return rootopt[name];
            }
            if (!cmd->options.count(name)) return nullptr;
            return cmd->options[name];
        }

        bool remove_option(command_type* cmd, const string_type& name) {
            if (!cmd) {
                return (bool)rootopt.erase(name);
            }
            return (bool)cmd->options.erase(name);
        }

        bool register_option_alias(command_type* cmd, std::shared_ptr<OptionInfo<string_type>>& info, const string_type& name) {
            if (!cmd) {
                if (rootopt.count(name)) {
                    return false;
                }
                rootopt[name] = info;
            }
            else {
                if (cmd->options.count(name)) return false;
                cmd->options[name] = info;
            }
            return true;
        }

        bool register_by_optname(command_type* cmd, vec_type<Optname> info) {
            for (Optname& i : info) {
                int arg = i.arg ? 1 : 0;
                if (auto opt = register_option(cmd, i.longname, arg, arg, i.flag, i.help ? i.help : "");
                    !opt || (i.shortname && !register_option_alias(cmd, opt, i.shortname))) {
                    return false;
                }
            }
            return true;
        }

        void register_program_name(const string_type& name) {
            program_name = name;
        }

        template <class input_func = CmdlineCtx::input_func>
        int execute(input_func in, LogFlag logmode = LogFlag::program_name) {
            if (!input_common(in)) return -1;
            arg_type arg, *fin = nullptr;
            arg.set_logmode(logmode);
            if (!parse(arg, fin)) return -1;
            return run(*fin);
        }

        template <class input_func = CmdlineCtx::input_func>
        int execute_with_name(const string_type& name, input_func in, LogFlag logmode = LogFlag::program_name) {
            if (!input_common(in)) return -1;
            arg_type arg, *fin = nullptr;
            arg.set_logmode(logmode);
            if (!parse_with_name(name, arg, fin)) return -1;
            return run(*fin);
        }

        template <class input_func = CmdlineCtx::input_func>
        int execute_with_context(command_type* ctx, input_func in, LogFlag logmode = LogFlag::program_name) {
            if (!ctx) return -1;
            if (!input_common(in)) return -1;
            arg_type arg, *fin = nullptr;
            arg.set_logmode(logmode);
            if (!parse_by_context(*ctx, arg, fin)) return -1;
            return run(*fin);
        }

        User& get_user() {
            return user;
        }
    };

    template <class C, class Str, template <class...> class Map, template <class...> class Vec, class User = void*>
    using ArgVCtx = CmdlineCtx<ArgVWrapper<C>, Str, Map, Vec, User>;

}  // namespace PROJECT_NAME