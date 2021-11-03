/*
    commonlib - common utility library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once

#include "reader.h"

namespace PROJECT_NAME {
    struct LinePosContext {
        size_t line = 0;
        size_t pos = 0;
        size_t nowpos = 0;
        bool crlf = false;
        bool cr = false;
    };

    template <class Buf>
    struct BasicBlock {
        bool c_block = true;
        bool arr_block = true;
        bool bracket = true;
        bool operator()(Reader<Buf> *self, bool begin) {
            bool ret = false;
            if (c_block) {
                if (begin) {
                    ret = self->expect("{");
                }
                else {
                    ret = self->expect("}");
                }
            }
            if (!ret && arr_block) {
                if (begin) {
                    ret = self->expect("[");
                }
                else {
                    ret = self->expect("]");
                }
            }
            if (!ret && bracket) {
                if (begin) {
                    ret = self->expect("(");
                }
                else {
                    ret = self->expect(")");
                }
            }
            return ret;
        }
        BasicBlock(bool block = true, bool array = true, bool bracket = true)
            : c_block(block), arr_block(array), bracket(bracket) {}
    };

    template <class Char>
    bool do_nothing(Char) {
        return true;
    }

    template <class Buf, class Ctx>
    bool depthblock(Reader<Buf> *self, Ctx &check, bool begin) {
        if (!self || !begin)
            return true;
        if (!check(self, true))
            return false;
        size_t dp = 0;
        bool ok = false;
        while (!self->eof()) {
            if (check(self, true)) {
                dp++;
            }
            if (check(self, false)) {
                if (dp == 0) {
                    ok = true;
                    break;
                }
                dp--;
            }
        }
        return ok;
    }

    template <class Buf, class Ret, class Ctx>
    bool depthblock_withbuf(Reader<Buf> *self, Ret &ret, Ctx &check, bool begin) {
        if (!self || !begin)
            return true;
        if (!check(self, true))
            return false;
        size_t beginpos = self->readpos(), endpos = beginpos;
        size_t dp = 0;
        bool ok = false;
        while (!self->eof()) {
            endpos = self->readpos();
            if (check(self, true)) {
                dp++;
            }
            if (check(self, false)) {
                if (dp == 0) {
                    ok = true;
                    break;
                }
                dp--;
            }
            self->increment();
        }
        if (ok) {
            auto finalpos = self->readpos();
            self->seek(beginpos);
            while (self->readpos() != endpos) {
                ret.push_back(self->achar());
                self->increment();
            }
            self->seek(finalpos);
        }
        return ok;
    }

    template <class Buf, class Ret, class Char>
    bool until(Reader<Buf> *self, Ret &ret, Char &ctx, bool begin) {
        if (begin)
            return true;
        if (!self)
            return true;
        if (self->achar() == ctx)
            return true;
        ret.push_back(self->achar());
        return false;
    }

    template <class Buf, class Char>
    bool untilnobuf(Reader<Buf> *self, Char ctx, bool begin) {
        if (begin)
            return true;
        if (!self)
            return true;
        if (self->achar() == ctx)
            return true;
        return false;
    }

    template <class Buf, class Ret, class Ctx>
    bool untilincondition(Reader<Buf> *self, Ret &ret, Ctx &judge, bool begin) {
        if (begin)
            return true;
        if (!self)
            return true;
        if (!judge(self->achar()))
            return true;
        ret.push_back(self->achar());
        return false;
    }

    template <class Buf, class Ctx>
    bool untilincondition_nobuf(Reader<Buf> *self, Ctx &judge, bool begin) {
        if (begin)
            return true;
        if (!self)
            return true;
        if (!judge(self->achar()))
            return true;
        return false;
    }

    template <class Buf, class Ret, class Ctx>
    bool addifmatch(Reader<Buf> *self, Ret &ret, Ctx &judge, bool begin) {
        if (begin)
            return true;
        if (!self)
            return true;
        if (judge(self->achar())) {
            ret.push_back(self->achar());
        }
        return false;
    }

    template <class Buf, class Ctx>
    bool linepos(Reader<Buf> *self, Ctx &ctx, bool begin) {
        if (!self)
            return true;
        if (begin) {
            ctx.nowpos = self->readpos();
            ctx.line = 0;
            ctx.pos = 0;
            self->seek(0);
            return true;
        }
        if (self->readpos() >= ctx.nowpos) {
            self->seek(ctx.nowpos);
            return true;
        }
        ctx.pos++;
        if (ctx.crlf) {
            while (self->expect("\r\n")) {
                ctx.line++;
                ctx.pos = 0;
            }
        }
        else if (ctx.cr) {
            while (self->expect("\r")) {
                ctx.line++;
                ctx.pos = 0;
            }
        }
        else {
            while (self->expect("\n")) {
                ctx.line++;
                ctx.pos = 0;
            }
        }
        return false;
    }

    template <class Buf, class Ret>
    bool cmdline_read(Reader<Buf> *self, Ret &ret, int *&res, bool begin) {
        if (begin) {
            if (!res)
                return false;
            return true;
        }
        if (!self)
            return true;
        auto c = self->achar();
        if (is_string_symbol(c)) {
            if (!self->string(ret, !(*res))) {
                *res = 0;
            }
            else {
                *res = -1;
            }
        }
        else {
            auto prev = ret.size();
            self->readwhile(ret, untilincondition, [](char c) { return c != ' ' && c != '\t'; });
            if (ret.size() == prev) {
                *res = 0;
            }
            else {
                *res = 1;
            }
        }
        return true;
    }

    template <class T>
    void defcmdlineproc_impl(T &p) {
        if (is_string_symbol(p[0])) {
            p.pop_back();
            p.erase(0, 1);
        }
    }

    template <class T>
    bool defcmdlineproc(T &p) {
        defcmdlineproc_impl(p);
        return true;
    }

    template <class Buf>
    bool get_cmdline(Reader<Buf> &, int mincount, bool, bool) {
        return mincount == 0;
    }

    template <class Buf, class Now, class... Args>
    bool get_cmdline(Reader<Buf> &r, int mincount, bool fit, bool lineok, Now &now, Args &...arg) {
        int i = lineok ? 1 : 0;
        r.ahead("'");
        r.readwhile(now, cmdline_read, &i);
        if (i == 0) {
            return mincount == 0;
        }
        if (fit && mincount == 0) {
            return true;
        }
        return get_cmdline(r, mincount == 0 ? 0 : mincount - 1, fit, lineok, arg...);
    }

    template <class Func, class... Args>
    bool proc_cmdline(Func proc, Args &...args) {
        [](auto...) {}(proc(args)...);
        return true;
    }
}  // namespace PROJECT_NAME
