/*
    commonlib - common utility library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "syntax_parser.h"
#include "../callback.h"
#include "../enumext.h"
namespace PROJECT_NAME {
    namespace syntax {
        struct FloatReadPoint {
            std::string str;
            std::shared_ptr<token_t> beforedot;
            std::shared_ptr<token_t> dot;
            std::shared_ptr<token_t> afterdot;
            std::shared_ptr<token_t> sign;
            std::shared_ptr<token_t> aftersign;
            size_t stack = 0;

            std::shared_ptr<token_t>& exists() {
                if (beforedot) {
                    return beforedot;
                }
                else if (dot) {
                    return dot;
                }
                else if (afterdot) {
                    return afterdot;
                }
                else if (sign) {
                    return sign;
                }
                else {
                    return aftersign;
                }
            }
        };

        struct MostReachInfo {
            size_t pos = 0;
            std::string errmsg;
            std::weak_ptr<token_t> token;
            std::weak_ptr<Syntax> syntax;
            void clear() {
                pos = 0;
                errmsg.clear();
                token.reset();
                syntax.reset();
            }
        };

        struct MatchingStackInfo {
            size_t stackptr = 0;
            size_t rootpos = 0;
        };

        enum class MatchingType {
            number,
            integer,
            identifier,
            eof,
            eol,
            string,
            symbol,
            keyword,
            error,
            eos,
            bos,
        };

        BEGIN_ENUM_STRING_MSG(MatchingType, type_str)
        ENUM_STRING_MSG(MatchingType::number, "NUMBER")
        ENUM_STRING_MSG(MatchingType::integer, "INTEGER")
        ENUM_STRING_MSG(MatchingType::identifier, "ID")
        ENUM_STRING_MSG(MatchingType::eof, "EOF")
        ENUM_STRING_MSG(MatchingType::eol, "EOL")
        ENUM_STRING_MSG(MatchingType::eos, "EOS")
        ENUM_STRING_MSG(MatchingType::bos, "BOS")
        ENUM_STRING_MSG(MatchingType::string, "STRING")
        ENUM_STRING_MSG(MatchingType::symbol, "SYMBOL")
        ENUM_STRING_MSG(MatchingType::keyword, "KEYWORD")
        END_ENUM_STRING_MSG("ERROR");

        struct MatchingContext {
            friend struct SyntaxMatching;

           private:
            std::vector<std::string> scope;
            std::string token;
            MatchingType type;
            TokenReader* r;
            std::weak_ptr<token_t> node;
            std::string* err;
            MostReachInfo reach;

            bool match_(const std::string& n) {
                return false;
            }

            template <class Str, class... Args>
            bool match_(const std::string& n, const Str& str, const Args&... a) {
                if (n == str) {
                    return true;
                }
                return match_(n, a...);
            }

           public:
            const MostReachInfo& get_mostreach() const {
                return reach;
            }

            size_t get_tokpos() const {
                return r->count;
            }

            const std::weak_ptr<token_t>& get_tokloc() const {
                return node;
            }

            void set_errmsg(const std::string& v) const {
                *err = v;
            }

            const std::string& current() const {
                return scope[get_stackptr()];
            }

            size_t get_stackptr() const {
                return scope.size() - 1;
            }

            MatchingStackInfo get_stack() const {
                return MatchingStackInfo{
                    get_stackptr(),
                    get_tokpos(),
                };
            }

            bool is_rollbacked(const MatchingStackInfo& st) const {
                return st.rootpos >= get_tokpos();
            }

            bool is_current(const MatchingStackInfo& st) const {
                return st.stackptr == get_stackptr();
            }

            bool is_current(const std::string& n) const {
                return current() == n;
            }

            template <class String, class... Str>
            bool is_under_and_not(const String& n, const Str&... a) {
                for (auto i = scope.size() - 1; i < scope.size(); i--) {
                    if (scope[i] == n) {
                        return true;
                    }
                    if (match_(n, a...)) {
                        return false;
                    }
                }
                return false;
            }

            bool is_under(const std::string& n) const {
                for (auto i = scope.size() - 1; i < scope.size(); i--) {
                    if (scope[i] == n) {
                        return true;
                    }
                }
                return false;
            }

            bool is_type(MatchingType ty) const {
                return ty == type;
            }

            bool is_invisible_type() const {
                return type == MatchingType::bos || type == MatchingType::eos ||
                       type == MatchingType::eol || type == MatchingType::eof;
            }

            bool is_token(const std::string& n) const {
                return n == token;
            }

            const TokenReader& get_reader() const {
                return *r;
            }

            const std::string& get_token() const {
                return token;
            }

            MatchingType get_type() const {
                return type;
            }
        };

        enum class MatchingError {
            none,
            error,
        };

        using MatchingErr = commonlib2::EnumWrap<MatchingError, MatchingError::none, MatchingError::error, MatchingError::none>;

        struct SyntaxMatching {
            using holder_t = std::vector<std::shared_ptr<Syntax>>;
            SyntaxParser p;
            Callback<MatchingErr, const MatchingContext&> cb;

           private:
            MatchingContext ctx;

           public:
            const std::string& mosterr() const {
                return ctx.reach.errmsg;
            }

            void report(TokenReader* r, const std::shared_ptr<token_t>& e, const std::shared_ptr<Syntax>& v, const std::string& msg) {
                p.errmsg = msg;
                if (r && ctx.reach.pos < r->count) {
                    ctx.reach.pos = r->count;
                    ctx.reach.errmsg = msg;
                    ctx.reach.token = e;
                    ctx.reach.syntax = v;
                    //callback(e, *r, e ? e->to_string() : "", MatchingType::error);
                }
            }

            bool callback(const std::shared_ptr<token_t>& relnode, TokenReader& r, const std::string& token, MatchingType type) {
                if (cb) {
                    ctx.token = token;
                    ctx.type = type;
                    ctx.r = &r;
                    ctx.node = relnode;
                    ctx.err = &p.errmsg;
                    if (!cb(ctx)) {
                        return false;
                    }
                }
                return true;
            }

            int parse_literal(TokenReader& r, std::shared_ptr<Syntax>& v) {
                auto e = r.GetorEOF();
                if (!any(v->flag & SyntaxFlag::adjacent)) {
                    e = r.ReadorEOF();
                }
                if (!e) {
                    report(&r, e, v, "unexpected EOF. expect " + v->token->to_string());
                    return 0;
                }
                auto value = v->token->to_string();
                if (!e->has_(value)) {
                    report(&r, e, v, "expect " + value + " but token is " + e->to_string());
                    return 0;
                }
                if (!callback(e, r, value, e->is_(tkpsr::TokenKind::symbols) ? MatchingType::symbol : MatchingType::keyword)) {
                    return -1;
                }
                r.Consume();
                return 1;
            }

            int parse_or(TokenReader& r, std::shared_ptr<OrSyntax>& v, int& idx) {
                std::string errs;
                for (auto i = 0; i < v->syntax.size(); i++) {
                    auto cr = r.FromCurrent();
                    auto res = parse_on_vec(cr, v->syntax[i]);
                    if (res > 0) {
                        idx = i;
                        r.SeekTo(cr);
                        return 1;
                    }
                    else if (res < 0) {
                        return res;
                    }
                    errs += p.errmsg + "\n";
                }
                report(&r, nullptr, v, errs);
                return 0;
            }

            int parse_ref(TokenReader& r, std::shared_ptr<Syntax>& v) {
                auto found = p.syntax.find(v->token->to_string());
                if (found == p.syntax.end()) {
                    report(&r, nullptr, v, "syntax " + v->token->to_string() + " is not defined");
                    return -1;
                }
                ctx.scope.push_back(found->first);
                auto cr = r.FromCurrent();
                auto res = parse_on_vec(cr, found->second);
                if (res > 0) {
                    if (r.current == cr.current) {
                        report(&r, nullptr, v, "infinity loop detected. please fix definitions especially around ? or *");
                        return -1;
                    }
                    r.SeekTo(cr);
                }
                ctx.scope.pop_back();
                return res;
            }

            bool check_int_str(auto& idv, int i, int base, int& allowed) {
                for (; i < idv.size(); i++) {
                    if (base == 16) {
                        if (!commonlib2::is_hex(idv[i])) {
                            allowed = i;
                            return true;
                        }
                    }
                    else {
                        if (!commonlib2::is_digit(idv[i])) {
                            allowed = i;
                            return true;
                        }
                    }
                }
                allowed = idv.size();
                return true;
            }

            void get_floatpoint(TokenReader& cr, std::shared_ptr<Syntax>& v, FloatReadPoint& pt) {
                if (!any(v->flag & SyntaxFlag::adjacent)) {
                    cr.Read();
                }
                while (true) {
                    auto e = cr.Get();
                    if (!e) {
                        break;
                    }
                    if (!pt.dot && !pt.sign && e->has_(".")) {
                        pt.dot = e;
                        pt.str += e->to_string();
                        cr.Consume();
                        continue;
                    }
                    if (!pt.sign && (e->has_("+") || e->has_("-"))) {
                        pt.sign = e;
                        pt.str += e->to_string();
                        cr.Consume();
                        continue;
                    }
                    if (!e->is_(tkpsr::TokenKind::identifiers)) {
                        break;
                    }
                    if (!pt.dot && !pt.sign && !pt.beforedot) {
                        pt.beforedot = e;
                    }
                    else if (pt.dot && !pt.afterdot) {
                        pt.afterdot = e;
                    }
                    else if (pt.sign && !pt.aftersign) {
                        pt.aftersign = e;
                    }
                    else {
                        break;
                    }
                    pt.str += e->to_string();
                    cr.Consume();
                }
                pt.stack = cr.count;
            }

            int parse_float(TokenReader& r, std::shared_ptr<Syntax>& v) {
                auto cr = r.FromCurrent();
                FloatReadPoint pt;
                get_floatpoint(cr, v, pt);
                if (pt.str.size() == 0) {
                    report(&r, pt.exists(), v, "expect number but not");
                    return 0;
                }
                int base = 10;
                int i = 0;
                if (pt.dot && !pt.beforedot && !pt.afterdot) {
                    report(&r, pt.exists(), v, "invalid float number format");
                    return 0;
                }
                if (pt.str.starts_with("0x") || pt.str.starts_with("0X")) {
                    if (pt.str.size() >= 3 && pt.str[2] == '.' && !pt.afterdot) {
                        report(&r, pt.exists(), v, "invalid hex float fromat. token is " + pt.str);
                        return 0;
                    }
                    base = 16;
                    i = 2;
                }
                int allowed = false;
                check_int_str(pt.str, i, base, allowed);
                if (pt.str.size() == allowed) {
                    if (!pt.beforedot || pt.dot || pt.sign) {
                        report(&r, pt.exists(), v, "parser is broken");
                        return -1;
                    }
                    r.current = pt.beforedot->get_next();
                    r.count = pt.stack;
                    if (!callback(pt.exists(), r, pt.str, MatchingType::integer)) {
                        return -1;
                    }
                    return 1;
                }
                if (pt.str[allowed] == '.') {
                    if (!pt.dot) {
                        report(&r, pt.exists(), v, "parser is broken");
                        return -1;
                    }
                    i = allowed + 1;
                }
                check_int_str(pt.str, i, base, allowed);
                if (pt.str.size() == allowed) {
                    if (pt.sign) {
                        report(&r, pt.exists(), v, "parser is broken");
                        return -1;
                    }
                    if (pt.afterdot) {
                        r.current = pt.afterdot->get_next();
                    }
                    else if (pt.dot) {
                        r.current = pt.dot->get_next();
                    }
                    else {
                        report(&r, pt.exists(), v, "parser is broken");
                        return -1;
                    }
                    r.count = pt.stack;
                    if (!callback(pt.exists(), r, pt.str, MatchingType::number)) {
                        return -1;
                    }
                    return 1;
                }
                if (base == 16) {
                    if (pt.str[allowed] != 'p' && pt.str[allowed] != 'P') {
                        report(&r, pt.exists(), v, "invalid hex float format. token is " + pt.str);
                        return 0;
                    }
                }
                else {
                    if (pt.str[allowed] != 'e' && pt.str[allowed] != 'E') {
                        report(&r, pt.exists(), v, "invalid float format. token is " + pt.str);
                        return 0;
                    }
                }
                i = allowed + 1;
                if (pt.str.size() > i) {
                    if (pt.str[i] == '+' || pt.str[i] == '-') {
                        if (!pt.sign) {
                            report(&r, pt.exists(), v, "parser is broken");
                            return -1;
                        }
                        i++;
                    }
                }
                if (pt.str.size() <= i) {
                    report(&r, pt.exists(), v, "invalid float format. token is " + pt.str);
                    return 0;
                }
                check_int_str(pt.str, i, 10, allowed);
                if (pt.str.size() != allowed) {
                    report(&r, pt.exists(), v, "invalid float format. token is " + pt.str);
                    return 0;
                }
                if (pt.aftersign) {
                    r.current = pt.aftersign->get_next();
                }
                else if (pt.afterdot) {
                    r.current = pt.afterdot->get_next();
                }
                else if (pt.beforedot) {
                    r.current = pt.beforedot->get_next();
                }
                else {
                    report(&r, pt.exists(), v, "parser is broken");
                    return -1;
                }
                r.count = pt.stack;
                if (!callback(pt.exists(), r, pt.str, MatchingType::number)) {
                    return -1;
                }
                return true;
            }

            int parse_keyword(TokenReader& r, std::shared_ptr<Syntax>& v) {
                auto cr = r.FromCurrent();
                auto check_integer = [&](auto& e) {
                    auto id = e->identifier();
                    auto& idv = id->get_identifier();
                    int i = 0;
                    int base = 10;
                    if (idv.size() >= 3 && idv.starts_with("0x") || idv.starts_with("0X")) {
                        i = 2;
                        base = 16;
                    }
                    int allowed = 0;
                    check_int_str(idv, i, base, allowed);
                    if (idv.size() != allowed) {
                        report(&r, e, v, "invalid integer." + idv);
                        return false;
                    }
                    return true;
                };
                if (v->token->has_("EOF")) {
                    auto e = cr.Get();
                    if (!any(v->flag & SyntaxFlag::adjacent)) {
                        e = cr.Read();
                    }
                    if (e) {
                        report(&r, e, v, "expect EOF but token is " + e->to_string());
                        return 0;
                    }
                    if (!callback(e, cr, "", MatchingType::eof)) {
                        return -1;
                    }
                }
                else if (v->token->has_("EOL")) {
                    auto e = cr.GetorEOF();
                    if (!any(v->flag & SyntaxFlag::adjacent)) {
                        cr.SetIgnoreLine(false);
                        e = cr.ReadorEOF();
                        cr.SetIgnoreLine(true);
                    }
                    if (!e) {
                        report(&r, e, v, "unexpected EOF. expect EOL but not");
                        return 0;
                    }
                    if (!e->is_(tkpsr::TokenKind::line)) {
                        report(&r, e, v, "expect EOL but token is " + e->to_string());
                        return 0;
                    }
                    if (!callback(e, cr, e->to_string(), MatchingType::eol)) {
                        return -1;
                    }
                    cr.Consume();
                }
                else if (v->token->has_("ID")) {
                    auto e = cr.GetorEOF();
                    if (!any(v->flag & SyntaxFlag::adjacent)) {
                        e = cr.ReadorEOF();
                    }
                    if (!e) {
                        report(&r, e, v, "unexpected EOF. expect identifier");
                        return 0;
                    }
                    if (!e->is_(tkpsr::TokenKind::identifiers)) {
                        report(&r, e, v, "expect identifier but token is " + e->to_string());
                        return 0;
                    }
                    if (!callback(e, cr, e->to_string(), MatchingType::identifier)) {
                        return -1;
                    }
                    cr.Consume();
                }
                else if (v->token->has_("INTEGER")) {
                    auto e = cr.GetorEOF();
                    if (!any(v->flag & SyntaxFlag::adjacent)) {
                        e = cr.ReadorEOF();
                    }
                    if (!e) {
                        report(&r, e, v, "unexpected EOF. expect integer");
                        return 0;
                    }
                    if (!e->is_(tkpsr::TokenKind::identifiers)) {
                        report(&r, e, v, "expect integer but token is " + e->to_string());
                        return 0;
                    }
                    if (!check_integer(e)) {
                        return 0;
                    }
                    if (!callback(e, cr, e->to_string(), MatchingType::integer)) {
                        return -1;
                    }
                    cr.Consume();
                }
                else if (v->token->has_("NUMBER")) {
                    if (auto res = parse_float(cr, v); res <= 0) {
                        return res;
                    }
                }
                else if (v->token->has_("STRING")) {
                    auto e = cr.GetorEOF();
                    if (!any(v->flag & SyntaxFlag::adjacent)) {
                        e = cr.ReadorEOF();
                    }
                    if (!e) {
                        report(&r, e, v, "unexpected EOF. expect string");
                        return 0;
                    }
                    if (!e->has_("\"") && !e->has_("'") && !e->has_("`")) {
                        report(&r, e, v, "expect string but token is " + e->to_string());
                        return 0;
                    }
                    auto startvalue = e->to_string();
                    auto start = e;
                    e = cr.ConsumeGetorEOF();
                    if (!e) {
                        report(&r, e, v, "unexpected EOF. expect string");
                        return 0;
                    }
                    auto value = e->to_string();
                    e = cr.ConsumeGetorEOF();
                    if (!e) {
                        report(&r, e, v, "unexpected EOF. expect end of string " + startvalue);
                        return 0;
                    }
                    if (!e->has_(startvalue)) {
                        report(&r, e, v, "expect " + startvalue + " but token is " + e->to_string());
                    }
                    if (!callback(start, cr, value, MatchingType::string)) {
                        return -1;
                    }
                    cr.Consume();
                }
                else {
                    report(&r, nullptr, v, "unimplemented " + v->token->to_string());
                    return -1;
                }
                r.SeekTo(cr);
                return 1;
            }

            int parse_on_vec(TokenReader& r, holder_t& vec) {
                auto call_v = [&](auto f, auto& v) {
                    bool repeating = false;
                    while (true) {
                        if (auto res = f(r, v); res == 0) {
                            if (any(v->flag & SyntaxFlag::fatal)) {
                                return -1;
                            }
                            if (!repeating && !any(v->flag & SyntaxFlag::ifexists)) {
                                return 0;
                            }
                            break;
                        }
                        else if (res < 0) {
                            return -1;
                        }
                        if (any(v->flag & SyntaxFlag::repeat)) {
                            repeating = true;
                            continue;
                        }
                        break;
                    }
                    return 1;
                };
                for (size_t i = 0; i < vec.size(); i++) {
                    auto& v = vec[i];
                    switch (v->type) {
                        case SyntaxType::literal: {
                            if (auto e = call_v(
                                    [this](auto& r, auto& v) {
                                        return parse_literal(r, v);
                                    },
                                    v);
                                e <= 0) {
                                return e;
                            }
                            break;
                        }
                        case SyntaxType::ref: {
                            if (auto e = call_v(
                                    [this](auto& r, auto& v) {
                                        return parse_ref(r, v);
                                    },
                                    v);
                                e <= 0) {
                                return e;
                            }
                            break;
                        }
                        case SyntaxType::or_: {
                            auto ptr = std::static_pointer_cast<OrSyntax>(v);
                            if (!any(ptr->flag & SyntaxFlag::once_each)) {
                                if (auto e = call_v(
                                        [&](auto& r, auto& v) {
                                            int index = 0;
                                            return parse_or(r, v, index);
                                        },
                                        ptr);
                                    e <= 0) {
                                    return e;
                                }
                            }
                            else {
                                std::set<int> already_set;
                                if (auto e = call_v(
                                        [&](auto& r, auto& v) {
                                            int index = 0;
                                            auto res = parse_or(r, v, index);
                                            if (res > 0) {
                                                if (!already_set.insert(index).second) {
                                                    report(&r, nullptr, v, "index " + std::to_string(index) + " is already set");
                                                    return -1;
                                                }
                                            }
                                            return res;
                                        },
                                        ptr);
                                    e <= 0) {
                                    return e;
                                }
                            }
                            break;
                        }
                        case SyntaxType::keyword: {
                            if (auto e = call_v(
                                    [this](auto& r, auto& v) {
                                        return parse_keyword(r, v);
                                    },
                                    v);
                                e <= 0) {
                                return e;
                            }
                            break;
                        }
                        case SyntaxType::bos:
                        case SyntaxType::eos: {
                            callback(nullptr, r, "", v->type == SyntaxType::bos ? MatchingType::bos : MatchingType::eos);
                            break;
                        }
                        default:
                            report(&r, nullptr, v, "unimplemented");
                            return -1;
                    }
                }
                return 1;
            }

            int parse_follow_syntax() {
                auto found = p.syntax.find("ROOT");
                if (found == p.syntax.end()) {
                    report(nullptr, nullptr, nullptr, "need ROOT syntax element");
                    return false;
                }
                ctx.scope.clear();
                ctx.scope.push_back("ROOT");
                ctx.reach.clear();
                auto r = p.get_reader();
                auto res = parse_on_vec(r, found->second);
                if (res > 0) {
                    p.errmsg.clear();
                }
                return res;
            }
        };
    }  // namespace syntax
}  // namespace PROJECT_NAME
