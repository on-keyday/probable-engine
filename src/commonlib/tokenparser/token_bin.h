/*
    commonlib - common utility library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once

#include "tokendef.h"
#include "tokenparser.h"

#include "../serializer.h"
#include "../utfreader.h"

namespace PROJECT_NAME {
    namespace tokenparser {
        struct CodeLimit {  //number codeing like QUIC
            static constexpr size_t limit8 = 0x3f;
            static constexpr size_t limit16 = 0x3fff;
            static constexpr std::uint16_t mask16 = 0x4000;
            static constexpr std::uint8_t mask16_8 = 0x40;
            static constexpr size_t limit32 = 0x3fffffff;
            static constexpr std::uint32_t mask32 = 0x80000000;
            static constexpr std::uint8_t mask32_8 = 0x80;
            static constexpr size_t limit64 = 0x3fffffffffffffff;
            static constexpr std::uint64_t mask64 = 0xC000000000000000;
            static constexpr std::uint8_t mask64_8 = 0xC0;
        };

        struct BinaryIO {
            template <class Buf>
            static bool read_num(Deserializer<Buf>& target, size_t& size) {
                std::uint8_t e = target.base_reader().achar();
                std::uint8_t masked = e & CodeLimit::mask64_8;
                auto set_v = [&](auto& v, auto unmask) {
                    if (!target.read_ntoh(v)) {
                        return false;
                    }
                    size = v & ~unmask;
                    return true;
                };
                if (masked == CodeLimit::mask64_8) {
                    std::uint64_t v;
                    return set_v(v, CodeLimit::mask64);
                }
                else if (masked == CodeLimit::mask32_8) {
                    std::uint32_t v;
                    return set_v(v, CodeLimit::mask32);
                }
                else if (masked == CodeLimit::mask16_8) {
                    std::uint16_t v;
                    return set_v(v, CodeLimit::mask16);
                }
                else {
                    std::uint8_t v;
                    return set_v(v, 0);
                }
            }

            template <class Buf>
            static bool write_num(Serializer<Buf>& target, size_t size) {
                if (size <= CodeLimit::limit8) {
                    target.write_hton(std::uint8_t(size));
                }
                else if (size <= CodeLimit::limit16) {
                    std::uint16_t v = std::uint16_t(size) | CodeLimit::mask16;
                    target.write_hton(v);
                }
                else if (size <= CodeLimit::limit32) {
                    std::uint32_t v = std::uint32_t(size) | CodeLimit::mask32;
                    target.write_hton(v);
                }
                else if (size <= CodeLimit::limit64) {
                    std::uint64_t v = std::uint64_t(size) | CodeLimit::mask64;
                    target.write_hton(v);
                }
                else {
                    return false;
                }
                return true;
            }

            template <class Buf, class String>
            static bool write_string(Serializer<Buf>& target, String& str) {
                OptUTF8<String&> buf(str);
                if (buf.size() == 0) {
                    return false;
                }
                if (!write_num(target, buf.size())) {
                    return false;
                }
                for (size_t i = 0; i < buf.size(); i++) {
                    target.write(buf[i]);
                }
                return true;
            }

            template <class Buf, class String>
            static bool read_string(Deserializer<Buf>& target, String& str) {
                size_t size = 0;
                if (!read_num(target, size)) {
                    return false;
                }
                return target.read_byte(str, size);
            }
        };

        template <class Map>
        struct TokenWriteContext {
            Map keyword;
            Map symbol;
            size_t count = 0;
            Map id;
            template <class String>
            size_t getid(const String& v, bool& already) {
                if (auto found = id.find(v); found != id.end()) {
                    already = true;
                    return found->second;
                }
                count++;
                already = false;
                id[v] = count;
                return count;
            }
        };

        template <class Map>
        struct TokenReadContext {
            Map keyword;
            Map symbol;
            size_t count = 0;
            Map id;
            template <class String>
            size_t setstring(const String& v) {
                count++;
                id[count] = v;
                return count;
            }
        };

        struct TokenIO {
           private:
            template <class T>
            struct MakeToken {
                T t;
                template <class... Args>
                MakeToken(Args&&... args)
                    : t(std::forward<Args>(args)...) {}
                template <class... Args>
                static std::shared_ptr<T> make_shared(Args&&... args) {
                    auto p = std::make_shared<MakeToken<T>>(std::forward<Args>(args)...);
                    return std::shared_ptr<T>(std::move(p), &p->t);
                }
            };

           public:
            template <class String, class Reg, class Map>
            static bool write_mapping(Serializer<String>& target, Registry<Reg>& reg, Map& map) {
                size_t count = 0;
                for (auto& v : reg.reg) {
                    if (map.insert({v, count}).second) {
                        if (!BinaryIO::write_string(target, v)) {
                            return false;
                        }
                        count++;
                    }
                }
                BinaryIO::write_num(target, 0);
                return true;
            }

            template <class String, class Reg, class Map, class Buf>
            static bool read_mapping(Deserializer<Buf>& target, Registry<Reg>& reg, Map& map) {
                size_t i = 0;
                while (true) {
                    String v;
                    if (!BinaryIO::read_string(target, v)) {
                        return false;
                    }
                    if (v.size() == 0) {
                        break;
                    }
                    reg.Register(v);
                    map.insert({i, v});
                    i++;
                }
                return true;
            }

            template <class Buf, class String, class Map>
            static bool read_token(Deserializer<Buf>& target, std::shared_ptr<Token<String>>& token, TokenReadContext<Map>& ctx) {
                TokenKind kind;
                size_t tmpsize = 0;
                if (!BinaryIO::read_num(target, tmpsize)) {
                    return false;
                }
                kind = TokenKind(tmpsize);
                switch (kind) {
                    case TokenKind::line: {
                        auto line = MakeToken<Line<String>>::make_shared();
                        //to type on editor easily
                        Line<String>* ptr = line.get();
                        if (!BinaryIO::read_num(target, tmpsize)) {
                            return false;
                        }
                        ptr->linekind = LineKind(tmpsize);
                        if (!BinaryIO::read_num(target, tmpsize)) {
                            return false;
                        }
                        ptr->numline = tmpsize;
                        token = line;
                        return true;
                    }
                    case TokenKind::spaces: {
                        auto space = MakeToken<Spaces<String>>::make_shared();
                        //to type on editor easily
                        Spaces<String>* ptr = space.get();
                        if (!BinaryIO::read_num(target, tmpsize)) {
                            return false;
                        }
                        ptr->spchar = char32_t(tmpsize);
                        if (!BinaryIO::read_num(target, tmpsize)) {
                            return false;
                        }
                        ptr->numsp = tmpsize;
                        token = space;
                        return true;
                    }
                    case TokenKind::keyword:
                    case TokenKind::weak_keyword: {
                        auto keyword = MakeToken<RegistryRead<String>>::make_shared(kind);
                        //to type on editor easily
                        RegistryRead<String>* ptr = keyword.get();
                        if (!BinaryIO::read_num(target, tmpsize)) {
                            return false;
                        }
                        auto found = ctx.keyword.find(tmpsize);
                        if (found == ctx.keyword.end()) {
                            return false;
                        }
                        ptr->token = found->second;
                        token = keyword;
                        return true;
                    }
                    case TokenKind::symbols: {
                        auto symbol = MakeToken<RegistryRead<String>>::make_shared(kind);
                        //to type on editor easily
                        RegistryRead<String>* ptr = symbol.get();
                        if (!BinaryIO::read_num(target, tmpsize)) {
                            return false;
                        }
                        auto found = ctx.symbol.find(tmpsize);
                        if (found == ctx.symbol.end()) {
                            return false;
                        }
                        ptr->token = found->second;
                        token = symbol;
                        return true;
                    }
                    case TokenKind::identifiers: {
                        auto id = MakeToken<Identifier<String>>::make_shared();
                        //to type on editor easily
                        Identifier<String>* ptr = id.get();
                        if (!BinaryIO::read_num(target, tmpsize)) {
                            return false;
                        }
                        if (tmpsize) {
                            auto found = ctx.id.find(tmpsize);
                            if (found == ctx.id.end()) {
                                return false;
                            }
                            ptr->id = found->second;
                        }
                        else {
                            if (!BinaryIO::read_string(target, ptr->id)) {
                                return false;
                            }
                            ctx.setstring(ptr->id);
                        }
                        token = id;
                        return true;
                    }
                    case TokenKind::comments: {
                        auto comment = MakeToken<Comment<String>>::make_shared();
                        Comment<String>* ptr = comment.get();
                        if (!BinaryIO::read_num(target, tmpsize)) {
                            return false;
                        }
                        ptr->oneline = bool(tmpsize);
                        if (!BinaryIO::read_string(target, ptr->comments)) {
                            return false;
                        }
                        token = comment;
                        return true;
                    }
                    case TokenKind::root: {
                        token = std::make_shared<Token<String>>();
                        return true;
                    }
                    case TokenKind::unknown: {
                        return true;
                    }
                    default: {
                        return false;
                    }
                }
            }

            template <class Buf, class String, class Map>
            static bool write_token(Serializer<Buf>& target, Token<String>& token, TokenWriteContext<Map>& ctx) {
                auto kind = token.get_kind();
                if (!BinaryIO::write_num(target, size_t(kind))) {
                    return false;
                }
                switch (kind) {
                    case TokenKind::line: {
                        Line<String>* line = token.line();
                        if (!BinaryIO::write_num(target, size_t(line->get_linekind()))) {
                            return false;
                        }
                        if (!BinaryIO::write_num(target, size_t(line->get_linecount()))) {
                            return false;
                        }
                        return true;
                    }
                    case TokenKind::spaces: {
                        Spaces<String>* space = token.space();
                        if (!BinaryIO::write_num(target, size_t(space->get_spacechar()))) {
                            return false;
                        }
                        if (!BinaryIO::write_num(target, size_t(space->get_spacecount()))) {
                            return false;
                        }
                        return true;
                    }
                    case TokenKind::keyword:
                    case TokenKind::weak_keyword: {
                        RegistryRead<String>* keyword = token.registry();
                        auto found = ctx.keyword.find(keyword->get_token());
                        if (found == ctx.keyword.end()) {
                            return false;
                        }
                        if (!BinaryIO::write_num(target, size_t(found->second))) {
                            return false;
                        }
                        return true;
                    }
                    case TokenKind::symbols: {
                        RegistryRead<String>* symbol = token.registry();
                        auto found = ctx.symbol.find(symbol->get_token());
                        if (found == ctx.symbol.end()) {
                            return false;
                        }
                        if (!BinaryIO::write_num(target, size_t(found->second))) {
                            return false;
                        }
                        return true;
                    }
                    case TokenKind::identifiers: {
                        Identifier<String>* id = token.identifier();
                        bool already = false;
                        size_t idnum = ctx.getid(id->get_identifier(), already);
                        if (already) {
                            if (!BinaryIO::write_num(target, idnum)) {
                                return false;
                            }
                        }
                        else {
                            if (!BinaryIO::write_num(target, 0)) {
                                return false;
                            }
                            if (!BinaryIO::write_string(target, id->get_identifier())) {
                                return false;
                            }
                        }
                        return true;
                    }
                    case TokenKind::comments: {
                        Comment<String>* comment = token.comment();
                        if (!BinaryIO::write_num(target, std::uint8_t(comment->is_oneline()))) {
                            return false;
                        }
                        if (!BinaryIO::write_string(target, comment->get_comment())) {
                            return false;
                        }
                        return true;
                    }
                    case TokenKind::root: {
                        return true;
                    }
                    default: {
                        return false;
                    }
                }
            }
        };

        struct TokensIO {
            template <class Map, class Vector, class String, class Buf, class Cb = void (*)(std::shared_ptr<Token<String>>&, bool)>
            static bool write_parsed_v1_impl(
                Serializer<Buf>& target, TokenParser<Vector, String>& p, Cb&& cb = [](std::shared_ptr<Token<String>>&, bool before) { return true; }) {
                TokenWriteContext<Map> ctx;
                auto parsed = p.GetParsed();
                if (!parsed) {
                    return false;
                }
                if (!TokenIO::write_mapping(target, p.keywords, ctx.keyword)) {
                    return false;
                }
                if (!TokenIO::write_mapping(target, p.symbols, ctx.symbol)) {
                    return false;
                }
                for (auto tok = parsed; tok; tok = tok->get_next()) {
                    if (!cb(tok, true)) {
                        continue;
                    }
                    if (!TokenIO::write_token(target, *tok, ctx)) {
                        return false;
                    }
                    cb(tok, false);
                }
                if (!BinaryIO::write_num(target, size_t(TokenKind::unknown))) {
                    return false;
                }
                return true;
            }

            template <class Map, class Vector, class String, class Buf, class Cb = void (*)(std::shared_ptr<Token<String>>&, bool)>
            static bool write_parsed(
                Serializer<Buf>& target, TokenParser<Vector, String>& p, Cb&& cb = [](std::shared_ptr<Token<String>>&, bool before) { return true; }) {
                target.write_byte("TkD0", 4);
                return write_parsed_v1_impl<Map>(target, p, std::forward<Cb>(cb));
            }

            template <class Map, class Vector, class String, class Buf, class Cb = void (*)(std::shared_ptr<Token<String>>&)>
            static bool read_parsed_v1_impl(
                Deserializer<Buf>& target, TokenParser<Vector, String>& p, Cb&& cb = [](std::shared_ptr<Token<String>>&) {}) {
                TokenReadContext<Map> ctx;
                if (!TokenIO::read_mapping<String>(target, p.keywords, ctx.keyword)) {
                    return false;
                }
                if (!TokenIO::read_mapping<String>(target, p.symbols, ctx.symbol)) {
                    return false;
                }
                std::shared_ptr<Token<String>> root;
                std::shared_ptr<Token<String>> prev;
                while (true) {
                    std::shared_ptr<Token<String>> tok;
                    if (!TokenIO::read_token(target, tok, ctx)) {
                        return false;
                    }
                    if (!tok) {
                        break;
                    }
                    if (!root) {
                        root = tok;
                        prev = tok;
                        p.roottoken.force_set_next(root);
                    }
                    else {
                        prev->force_set_next(tok);
                        prev = tok;
                    }
                    cb(tok);
                }
                p.current = prev.get();
                return true;
            }

            template <class Map, class Vector, class String, class Buf, class Cb = void (*)(std::shared_ptr<Token<String>>&)>
            static bool read_parsed(
                Deserializer<Buf>& target, TokenParser<Vector, String>& p, Cb&& cb = [](std::shared_ptr<Token<String>>&) {}) {
                if (!target.base_reader().expect("TkD0")) {
                    return false;
                }
                return read_parsed_v1_impl<Map>(target, p, std::forward<Cb>(cb));
            }
        };
    }  // namespace tokenparser
}  // namespace PROJECT_NAME
