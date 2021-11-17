/*
    commonlib - common utility library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once

#include "tokendef.h"
namespace PROJECT_NAME {
    namespace tokenparser {

        template <class String>
        struct MergeRule {
            String begin_comment;
            String end_comment;
            bool nest = false;
            String oneline_comment;
            String escape;
            struct {
                String symbol;
                bool noescape = false;
                bool allowline = false;
            } string_symbol[3];
        };

        enum class MergeError {
            none,
            read_error,
            unexpected_eof_on_block_comment,
            unexpected_line_on_string_disallow_line,
            unexpected_eof_on_string_escape,
            unexpected_eof_on_string,
            unknown,
        };

        using MergeErr = EnumWrap<MergeError, MergeError::none, MergeError::unknown>;

        template <class Vector, class String>
        struct TokenParser {
           private:
            using reg_t = Registry<Vector>;
            using token_t = Token<String>;
            reg_t symbols;
            reg_t keywords;
            token_t roottoken;
            token_t* current = &roottoken;

           public:
            TokenParser() {}
            TokenParser(reg_t&& sym, reg_t&& key)
                : symbols(std::move(sym)), keywords(std::move(key)) {}

            void SetSymbolAndKeyWord(reg_t&& sym, reg_t&& key) {
                symbols = std::move(sym);
                keywords = std::move(key);
            }

            template <class Buf>
            bool Read(commonlib2::Reader<Buf>& r) {
                auto set_next = [this](auto& v) {
                    current->set_next(v);
                    current = std::addressof(*v);
                };
                auto middleroot = std::make_shared<token_t>();
                roottoken.remove();
                current = &roottoken;
                set_next(middleroot);
                while (!r.ceof()) {
                    auto sp = Spaces<String>::ReadSpace(r);
                    if (sp) {
                        set_next(sp);
                        continue;
                    }
                    auto ln = Line<String>::ReadLine(r);
                    if (ln) {
                        set_next(ln);
                        continue;
                    }
                    auto sym = RegistryRead<String>::ReadToken(TokenKind::symbols, r, symbols);
                    if (sym) {
                        set_next(sym);
                        continue;
                    }
                    auto key = RegistryRead<String>::ReadToken(TokenKind::keyword, r, keywords);
                    if (key) {
                        set_next(key);
                        continue;
                    }
                    NonIdentifierRegistry<String, reg_t> nonid(symbols);
                    auto id = Identifier<String>::ReadIdentifier(r, nonid);
                    if (!id) {
                        return false;
                    }
                    set_next(id);
                }
                return true;
            }

            bool MergeKeyWord(std::shared_ptr<Token<String>>& node) {
                if (node->get_kind() == TokenKind::identifiers) {
                    while (auto next = node->get_next()) {
                        if (next->get_kind() == TokenKind::keyword) {
                            auto id = node->identifier();
                            id->set_identifier(id->get_identifier() + next->to_string());
                            auto nextnext = next->get_next();
                            next->remove();
                            node->set_next(nextnext);
                            continue;
                        }
                        break;
                    }
                }
                else if (node->get_kind() == TokenKind::keyword) {
                    if (auto next = node->get_next()) {
                        auto keyword = node->registry();
                        if (next->get_kind() == TokenKind::identifiers) {
                            auto id = next->identifier();
                            id->set_identifier(keyword->get_token() + id->get_identifier());
                            auto prev = node->get_prev();
                            node->remove();
                            prev->set_next(next);
                            node = next;
                            return true;
                        }
                        else if (next->get_kind() == TokenKind::keyword) {
                            auto nextkey = next->registry();
                            auto tmp = std::make_shared<Identifier<String>>(keyword->get_token() + nextkey->get_token());
                            auto nextnext = next->get_next();
                            auto prev = node->get_prev();
                            node->remove();
                            next->remove();
                            prev->set_next(tmp);
                            if (nextnext) {
                                tmp->set_next(nextnext);
                            }
                            if (auto prevprev = prev->get_prev()) {
                                node = prevprev->get_next();
                            }
                            else {
                                node = tmp;
                            }
                            return true;
                        }
                    }
                }
                return false;
            }
            template <class RuleStr>
            bool MergeComment(MergeError& err, const MergeRule<RuleStr>& rule, std::shared_ptr<Token<String>>& node) {
                if (node->get_kind() == TokenKind::symbols) {
                    auto symbol = node->registry();
                    auto& token = symbol->get_token();
                    if (token == rule.begin_comment) {
                        size_t depth = 1;
                        String result;
                        auto first = node->get_next();
                        auto com = first;
                        for (; com && depth; com = com->get_next()) {
                            auto tmp = com->to_string();
                            if (tmp == rule.end_comment) {
                                depth--;
                                if (!depth) {
                                    break;
                                }
                            }
                            else if (rule.nest && tmp == rule.begin_comment) {
                                depth++;
                            }
                            result += tmp;
                        }
                        if (depth) {
                            err = MergeError::unexpected_eof_on_block_comment;
                            return true;
                        }
                        if (com != first) {
                            auto comment = std::make_shared<Comment<String>>(std::move(result), false);
                            //auto last = com->get_prev();
                            first->remove();
                            //last->remove();
                            node->set_next(comment);
                            comment->set_next(com);
                        }
                        node = com;
                        return true;
                    }
                    else if (token == rule.oneline_comment) {
                        String result;
                        auto first = node->get_next();
                        auto com = first;
                        for (; com; com = com->get_next()) {
                            if (com->get_kind() == TokenKind::line) {
                                break;
                            }
                            result += com->to_string();
                        }
                        if (com != first) {
                            auto comment = std::make_shared<Comment<String>>(std::move(result), true);
                            first->remove();
                            node->set_next(comment);
                            comment->set_next(com);
                            node = com ? com : comment;
                        }
                        else {
                            node = com;
                        }
                        return true;
                    }
                    else {
                        for (auto& str : rule.string_symbol) {
                            if (token == str.symbol) {
                                String result;
                                auto first = node->get_next();
                                auto com = first;
                                for (; com; com = com->get_next()) {
                                    if (com->get_kind() == TokenKind::line) {
                                        if (!str.allowline) {
                                            err = MergeError::unexpected_line_on_string_disallow_line;
                                            return true;
                                        }
                                    }
                                    auto tmp = com->to_string();
                                    if (tmp == rule.escape) {
                                        if (!str.noescape) {
                                            result += tmp;
                                            com = com->get_next();
                                            if (!com) {
                                                err = MergeError::unexpected_eof_on_string_escape;
                                                return true;
                                            }
                                            if (auto l = com->line()) {
                                                if (l->get_linecount() != 1) {
                                                    if (!str.allowline) {
                                                        err = MergeError::unexpected_line_on_string_disallow_line;
                                                        return true;
                                                    }
                                                }
                                            }
                                            result += com->to_string();
                                            continue;
                                        }
                                    }
                                    if (tmp == str.symbol) {
                                        break;
                                    }
                                    result += tmp;
                                }
                                if (!com) {
                                    err = MergeError::unexpected_eof_on_string;
                                    return true;
                                }
                                if (com != first) {
                                    auto comment = std::make_shared<Comment<String>>(std::move(result), false);
                                    first->remove();
                                    node->set_next(comment);
                                    comment->set_next(com);
                                }
                                node = com;
                                return true;
                            }
                        }
                    }
                }
                return false;
            }

            template <class RuleStr>
            MergeErr Merge(const MergeRule<RuleStr>& rule) {
                for (std::shared_ptr<Token<String>> node = roottoken.get_next(); node; node = node->get_next()) {
                    if (MergeKeyWord(node)) {
                        continue;
                    }
                    MergeError err = MergeError::none;
                    if (MergeComment(err, rule, node)) {
                        if (err != MergeError::none) {
                            return err;
                        }
                    }
                }
                return MergeError::none;
            }

            template <class Buf, class RuleStr>
            MergeErr ReadAndMerge(commonlib2::Reader<Buf>& r, const MergeRule<RuleStr>& rule) {
                if (!Read(r)) {
                    return MergeError::read_error;
                }
                return Merge(rule);
            }

            bool ConvertWeakKeyWord(const reg_t& weaks) {
                auto first = roottoken.get_next();
                if (!first) {
                    return false;
                }
                while (first) {
                    if (first->get_type() == TokenKind::keyword) {
                        auto reg = first->registry();
                        if (weaks.Include(reg->get_token())) {
                            reg->keyword_to_weak();
                        }
                    }
                    first = first->get_next();
                }
                return true;
            }

            std::shared_ptr<token_t> GetParsed() {
                return roottoken.get_next();
            }
        };
    }  // namespace tokenparser
}  // namespace PROJECT_NAME