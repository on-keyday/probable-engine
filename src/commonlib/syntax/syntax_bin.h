/*
    commonlib - common utility library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once

#include "syntax.h"
#include "../tokenparser/token_bin.h"

namespace PROJECT_NAME {
    namespace syntax {
        struct SyntaxIO {
            template <class Buf>
            static bool write_syntax(Serializer<Buf>& target, const std::shared_ptr<Syntax>& stx, std::map<std::shared_ptr<token_t>, size_t>& stxtok) {
                if (!tkpsr::BinaryIO::write_num(target, size_t(stx->type))) {
                    return false;
                }
                switch (stx->type) {
                    case SyntaxType::or_: {
                        auto or_ = std::static_pointer_cast<OrSyntax>(stx);
                        if (!tkpsr::BinaryIO::write_num(target, or_->syntax.size())) {
                            return false;
                        }
                        for (auto& v : or_->syntax) {
                            if (!tkpsr::BinaryIO::write_num(target, v.size())) {
                                return false;
                            }
                            for (auto& c : v) {
                                if (!write_syntax(target, c, stxtok)) {
                                    return false;
                                }
                            }
                        }
                        [[fallthrough]];
                    }
                    default: {
                        target.write(std::uint8_t(stx->flag));
                        auto found = stxtok.find(stx->token);
                        if (found == stxtok.end()) {
                            return false;
                        }
                        if (!tkpsr::BinaryIO::write_num(target, found->second)) {
                            return false;
                        }
                        return true;
                    }
                }
            }

            template <class Buf>
            static bool read_syntax(Deserializer<Buf>& target, std::shared_ptr<Syntax>& stx, std::vector<std::shared_ptr<token_t>>& stxtok) {
                size_t tmpnum = 0;
                if (!tkpsr::BinaryIO::read_num(target, tmpnum)) {
                    return false;
                }
                auto type = SyntaxType(tmpnum);
                switch (type) {
                    case SyntaxType::or_: {
                        auto or_ = std::make_shared<OrSyntax>(SyntaxType::or_);
                        if (!tkpsr::BinaryIO::read_num(target, tmpnum)) {
                            return false;
                        }
                        auto count = tmpnum;
                        for (size_t i = 0; i < count; i++) {
                            if (!tkpsr::BinaryIO::read_num(target, tmpnum)) {
                                return false;
                            }
                            or_->syntax.push_back(std::vector<std::shared_ptr<Syntax>>());
                            auto& back = or_->syntax.back();
                            for (size_t k = 0; k < tmpnum; k++) {
                                if (!read_syntax(target, stx, stxtok)) {
                                    return false;
                                }
                                back.push_back(std::move(stx));
                                stx = nullptr;
                            }
                        }
                        stx = std::move(or_);
                        [[fallthrough]];
                    }
                    default: {
                        if (!stx) {
                            stx = std::make_shared<Syntax>(type);
                        }
                        target.template read_as<std::uint8_t>(stx->flag);
                        if (!tkpsr::BinaryIO::read_num(target, tmpnum)) {
                            return false;
                        }
                        if (stxtok.size() <= tmpnum) {
                            return false;
                        }
                        stx->token = stxtok[tmpnum];
                        return true;
                    }
                }
            }

            template <class Buf>
            static bool save(Serializer<Buf>& target, SyntaxCompiler& syntaxc, bool minimum = false) {
                target.write_byte("StD0", 4);
                size_t count = 0;
                std::map<std::shared_ptr<token_t>, size_t> stxtok;
                auto cb = [&](auto& v, bool before) {
                    if (!before) {
                        stxtok.insert({v, count});
                        count++;
                    }
                    else if (minimum) {
                        if (v->is_(tkpsr::TokenKind::comments)) {
                            if (auto p = v->get_prev()) {
                                if (p->is_(tkpsr::TokenKind::symbols) && p->has_("#")) {
                                    return false;
                                }
                            }
                        }
                        else if (v->is_(tkpsr::TokenKind::symbols) && v->has_("#")) {
                            return false;
                        }
                        else if (v->is_(tkpsr::TokenKind::line)) {
                            if (auto p = v->get_prev()) {
                                if (p->is_(tkpsr::TokenKind::comments)) {
                                    return false;
                                }
                            }
                        }
                        else if (v->is_(tkpsr::TokenKind::spaces)) {
                            if (auto p = v->get_next()) {
                                if (p->is_(tkpsr::TokenKind::line)) {
                                    return false;
                                }
                            }
                        }
                    }
                    return true;
                };
                auto result = tkpsr::TokensIO::write_parsed<std::map<std::string, size_t>>(target, syntaxc.pm.parser, std::move(cb));
                if (!result) {
                    return false;
                }
                {
                    std::map<std::string, size_t> tmpmap;
                    if (!tkpsr::TokenIO::write_mapping(target, syntaxc.match.p.parser.GetKeyWords(), tmpmap)) {
                        return false;
                    }
                    tmpmap.clear();
                    if (!tkpsr::TokenIO::write_mapping(target, syntaxc.match.p.parser.GetSymbols(), tmpmap)) {
                        return false;
                    }
                }
                for (auto& stx : syntaxc.match.p.syntax) {
                    if (!tkpsr::BinaryIO::write_string(target, stx.first)) {
                        return false;
                    }
                    if (!tkpsr::BinaryIO::write_num(target, stx.second.size())) {
                        return false;
                    }
                    for (auto& v : stx.second) {
                        if (!write_syntax(target, v, stxtok)) {
                            return false;
                        }
                    }
                }
                if (!tkpsr::BinaryIO::write_num(target, 0)) {
                    return false;
                }
                return true;
            }

            template <class Buf>
            static bool load(Deserializer<Buf>& target, SyntaxCompiler& syntaxc) {
                if (!target.base_reader().expect("StD0")) {
                    return false;
                }
                size_t count = 0;
                std::vector<std::shared_ptr<token_t>> stxtok;
                auto cb = [&](auto& v) {
                    stxtok.push_back(v);
                };
                syntaxc.pm.parser.GetKeyWords().reg.clear();
                syntaxc.pm.parser.GetSymbols().reg.clear();
                auto result = tkpsr::TokensIO::read_parsed<std::map<size_t, std::string>>(target, syntaxc.pm.parser, std::move(cb));
                if (!result) {
                    return false;
                }
                {
                    syntaxc.match.p.parser.GetKeyWords().reg.clear();
                    syntaxc.match.p.parser.GetSymbols().reg.clear();
                    std::map<size_t, std::string> tmpmap;
                    if (!tkpsr::TokenIO::read_mapping<std::string>(target, syntaxc.match.p.parser.GetKeyWords(), tmpmap)) {
                        return false;
                    }
                    tmpmap.clear();
                    if (!tkpsr::TokenIO::read_mapping<std::string>(target, syntaxc.match.p.parser.GetSymbols(), tmpmap)) {
                        return false;
                    }
                }
                auto& elms = syntaxc.match.p.syntax;
                elms.clear();
                while (true) {
                    std::string name;
                    if (!tkpsr::BinaryIO::read_string(target, name)) {
                        return false;
                    }
                    if (name.size() == 0) {
                        break;
                    }
                    auto got = elms.insert({name, std::vector<std::shared_ptr<Syntax>>()});
                    if (!got.second) {
                        return false;
                    }
                    size_t count = 0;
                    if (!tkpsr::BinaryIO::read_num(target, count)) {
                        return false;
                    }
                    for (size_t i = 0; i < count; i++) {
                        std::shared_ptr<Syntax> stx;
                        if (!read_syntax(target, stx, stxtok)) {
                            return false;
                        }
                        got.first->second.push_back(stx);
                    }
                }
                return true;
            }
        };
    }  // namespace syntax
}  // namespace PROJECT_NAME
