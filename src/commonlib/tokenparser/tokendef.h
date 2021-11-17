/*
    commonlib - common utility library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once

#include "../reader.h"
#include "../utf_helper.h"
#include "../enumext.h"

#include <memory>
namespace PROJECT_NAME {
    namespace tokenparser {
        enum class TokenKind {
            unknown,
            root,          //ルートノード
            spaces,        //スペース
            line,          //ライン
            identifiers,   //識別子
            comments,      //コメント
            symbols,       //記号
            keyword,       //キーワード
            weak_keyword,  //弱キーワード
        };

        BEGIN_ENUM_STRING_MSG(TokenKind, token_kind)
        ENUM_STRING_MSG(TokenKind::root, "root");
        ENUM_STRING_MSG(TokenKind::spaces, "space")
        ENUM_STRING_MSG(TokenKind::line, "line")
        ENUM_STRING_MSG(TokenKind::identifiers, "identifier")
        ENUM_STRING_MSG(TokenKind::comments, "comment")
        ENUM_STRING_MSG(TokenKind::symbols, "symbol")
        ENUM_STRING_MSG(TokenKind::keyword, "keyword")
        ENUM_STRING_MSG(TokenKind::weak_keyword, "weak_keyword")
        END_ENUM_STRING_MSG("unknown")

        template <class Base>
        struct Registry {
            Base reg;

            Registry() {}

            Registry(Base&& b)
                : reg(std::move(b)) {}

            Registry(Registry&& r)
                : reg(std::move(r.reg)) {}

            Registry(const Registry& r)
                : reg(r.reg) {}

            Registry& operator=(Registry&& m) {
                reg = std::move(m.reg);
                return *this;
            }

            Registry& operator=(const Registry& m) {
                reg = m.reg;
                return *this;
            }

            Registry(std::initializer_list<const char*> list) {
                for (auto&& i : list) {
                    Register(i);
                }
            }

            template <class String>
            void Register(const String& keyword) {
                reg.push_back(keyword);
            }

            template <class String>
            bool Include(String& expects) const {
                for (auto& e : reg) {
                    if (e == expects) {
                        return true;
                    }
                }
                return false;
            }

            template <class Buf, class String>
            bool Expect(commonlib2::Reader<Buf>& r, String& expected) const {
                for (auto& e : reg) {
                    if (r.expectp(e, expected)) {
                        return true;
                    }
                }
                return false;
            }

            template <class Buf>
            bool Ahead(commonlib2::Reader<Buf>& r) const {
                for (auto& e : reg) {
                    if (r.ahead(e)) {
                        return true;
                    }
                }
                return false;
            }
        };

        template <class String>
        struct Spaces;
        template <class String>
        struct Line;
        template <class String>
        struct Comment;
        template <class String>
        struct RegistryRead;
        template <class String>
        struct Identifier;

        template <class String>
        struct Token {
           private:
            TokenKind kind = TokenKind::unknown;
            std::shared_ptr<Token> next = nullptr;
            Token* prev = nullptr;

           protected:
            constexpr Token(TokenKind k)
                : kind(k) {}

            void set_kind(TokenKind k) {
                kind = k;
            }

           public:
            constexpr Token()
                : kind(TokenKind::root) {}
            TokenKind get_kind() const {
                return kind;
            }

            bool is_(TokenKind e) const {
                return kind == e;
            }

            bool is_nodisplay() const {
                return kind == TokenKind::unknown || kind == TokenKind::spaces || kind == TokenKind::line || kind == TokenKind::root;
            }

            bool is_identifier() const {
                return kind == TokenKind::identifiers || kind == TokenKind::weak_keyword;
            }

            bool is_keyword() const {
                return kind == TokenKind::keyword || kind == TokenKind::weak_keyword;
            }

            virtual bool has_(const String&) const {
                return false;
            }

            bool set_next(std::shared_ptr<Token> p) {
                if (this->next || !p || p->prev) {
                    return false;
                }
                this->next = p;
                p->prev = this;
                return true;
            }

            bool force_set_next(std::shared_ptr<Token> p) {
                if (!p) {
                    return false;
                }
                if (p->prev) {
                    p->prev->next = nullptr;
                }
                if (this->next) {
                    this->next->prev = nullptr;
                }
                this->next = p;
                p->prev = this;
                return true;
            }

            std::shared_ptr<Token>& get_next() {
                return next;
            }

            Token* get_prev() {
                return prev;
            }

            void remove() {
                if (next) {
                    next->prev = nullptr;
                    next = nullptr;
                }
                if (prev) {
                    prev->next = nullptr;
                    prev = nullptr;
                }
            }

            ~Token() {
                remove();
            }

            virtual String to_string() const {
                return String();
            }

            virtual Spaces<String>* space() {
                return nullptr;
            }

            virtual Line<String>* line() {
                return nullptr;
            }

            virtual Comment<String>* comment() {
                return nullptr;
            }

            virtual RegistryRead<String>* registry() {
                return nullptr;
            }

            virtual Identifier<String>* identifier() {
                return nullptr;
            }
        };

        template <class String>
        struct Spaces : Token<String> {
           private:
            size_t numsp = 0;
            char16_t spchar = 0;

           public:
            constexpr Spaces(size_t nsp, uint16_t c)
                : numsp(nsp), spchar(c), Token<String>(TokenKind::spaces) {}

            char16_t get_spacechar() const {
                return spchar;
            }

            size_t get_spacecount() const {
                return numsp;
            }

            constexpr static char16_t getspace(size_t idx) {
                constexpr std::uint16_t spaces[] = {
                    0x00a0,
                    0x2002,
                    0x2003,
                    0x2004,
                    0x2005,
                    0x2006,
                    0x2007,
                    0x2008,
                    0x2009,
                    0x200a,
                    0x200b,
                    0x3000,
                    0xfeff,
                    0,
                };
                return spaces[idx];
            }

            template <class Buf>
            static bool Ahead(commonlib2::Reader<Buf>& r) {
                if (r.achar() == ' ' || r.achar() == '\t') {
                    return true;
                }
                for (auto i = 0; getspace(i); i++) {
                    if CONSTEXPRIF (commonlib2::bcsizeeq<Buf, 1>) {
                        commonlib2::U8MiniBuffer buf;
                        commonlib2::make_utf8_from_utf32(getspace(i), buf);
                        if (r.ahead(buf)) {
                            return true;
                        }
                    }
                    else {
                        if (r.achar() == getspace(i)) {
                            return true;
                        }
                    }
                }
                return false;
            }

            template <class Buf>
            static std::shared_ptr<Spaces> ReadSpace(commonlib2::Reader<Buf>& r) {
                size_t numsp = 0;
                char16_t spchar = 0;
                if (ReadSpace(r, numsp, spchar)) {
                    return std::make_shared<Spaces>(numsp, spchar);
                }
                return nullptr;
            }

            template <class Buf>
            static bool ReadSpace(commonlib2::Reader<Buf>& r, size_t& numsp, char16_t& spchar) {
                if (r.achar() == ' ' || r.achar() == '\t') {
                    spchar = r.achar();
                    while (r.achar() == spchar) {
                        numsp++;
                        r.increment();
                    }
                    return true;
                }
                for (auto i = 0; getspace(i); i++) {
                    if CONSTEXPRIF (commonlib2::bcsizeeq<Buf, 1>) {
                        commonlib2::U8MiniBuffer buf;
                        commonlib2::make_utf8_from_utf32(getspace(i), buf);
                        if (r.ahead(buf)) {
                            spchar = getspace(i);
                            while (r.expect(buf)) {
                                numsp++;
                            }
                            return true;
                        }
                    }
                    else {
                        if (r.achar() == getspace(i)) {
                            spchar = getspace(i);
                            while (r.achar() == spchar) {
                                numsp++;
                            }
                            return true;
                        }
                    }
                }
                return false;
            }

            bool has_(const String& other) const override {
                return to_string() == other;
            }

            String to_string() const override {
                String ret;
                if (spchar == ' ' || spchar == '\t') {
                    for (size_t i = 0; i < numsp; i++) {
                        ret.push_back(spchar);
                    }
                    return ret;
                }
                if CONSTEXPRIF (commonlib2::bcsizeeq<String, 1>) {
                    commonlib2::U8MiniBuffer buf;
                    commonlib2::make_utf8_from_utf32(spchar, buf);
                    for (size_t i = 0; i < numsp; i++) {
                        for (auto k = 0; k < buf.size(); k++) {
                            ret.push_back(buf[k]);
                        }
                    }
                    return ret;
                }
                else {
                    for (size_t i = 0; i < numsp; i++) {
                        ret.push_back(spchar);
                    }
                    return ret;
                }
            }

            Spaces<String>* space() override {
                return this;
            }
        };

        enum class LineKind {
            none = 0,
            lf = 1,
            cr = 2,
            crlf = cr | lf,
        };

        DEFINE_ENUMOP(LineKind);

        template <class String>
        struct Line : Token<String> {
           private:
            LineKind linekind = LineKind::none;
            size_t numline = 0;

           public:
            constexpr Line(LineKind kind, size_t num)
                : linekind(kind), numline(num), Token<String>(TokenKind::line) {}

            size_t get_linecount() const {
                return numline;
            }

            LineKind get_linekind() const {
                return linekind;
            }

            template <class Buf>
            static bool Ahead(commonlib2::Reader<Buf>& r) {
                return r.achar() == '\n' || r.achar() == '\r';
            }

            template <class Buf>
            static std::shared_ptr<Line> ReadLine(commonlib2::Reader<Buf>& r) {
                LineKind kind = LineKind::none;
                size_t numline = 0;
                if (ReadLine(r, kind, numline)) {
                    return std::make_shared<Line>(kind, numline);
                }
                return nullptr;
            }

            template <class Buf>
            static bool ReadLine(commonlib2::Reader<Buf>& r, LineKind& kind, size_t& numline) {
                if (r.ahead("\r\n")) {
                    kind = LineKind::crlf;
                    while (r.expect("\r\n")) {
                        numline++;
                    }
                }
                else if (r.achar() == '\r' || r.achar() == '\n') {
                    auto exp = r.achar();
                    kind = exp == '\n' ? LineKind::lf : LineKind::cr;
                    while (r.achar() == exp) {
                        numline++;
                        r.increment();
                    }
                }
                else {
                    return false;
                }
                return true;
            }

            bool has_(const String& other) const override {
                return to_string() == other;
            }

            String to_string() const override {
                String ret;
                for (auto i = 0; i < numline; i++) {
                    if (any(linekind & LineKind::cr)) {
                        ret.push_back('\r');
                    }
                    if (any(linekind & LineKind::lf)) {
                        ret.push_back('\n');
                    }
                }
                return ret;
            }

            Line<String>* line() override {
                return this;
            }
        };

        template <class String>
        struct Comment : Token<String> {
           private:
            String comments;
            bool oneline = false;

           public:
            Comment(String&& com, bool ol)
                : comments(std::move(com)), oneline(ol), Token<String>(TokenKind::comments) {}

            const String& get_comment() const {
                return comments;
            }

            bool is_oneline() const {
                return oneline;
            }

            template <class Buf, class... Args>
            static std::shared_ptr<Comment<String>> ReadComment(commonlib2::Reader<Buf>& r, bool& error, Args&&... args) {
                String comment;
                bool oneline = false;
                if (ReadComment(error, oneline, r, comment, std::forward<Args>(args)...)) {
                    return std::make_shared<Comment<String>>(std::move(comment), oneline);
                }
                return nullptr;
            }

            template <class Buf, class Begin>
            static bool ReadComment(bool& eof, bool& oneline, commonlib2::Reader<Buf>& r, String& comment, Begin&& begin) {
                eof = false;
                oneline = true;
                if (r.expect(begin)) {
                    while (true) {
                        if (r.ceof()) {
                            break;
                        }
                        if (r.expect("\r\n") || r.expect("\r") || r.expect("\n")) {
                            break;
                        }
                        comment += r.achar();
                        r.increment();
                    }
                    return true;
                }
                return false;
            }

            template <class Buf, class Begin, class End>
            static bool ReadComment(bool& eof, bool& oneline, commonlib2::Reader<Buf>& r, String& comment, Begin&& begin, End&& end, bool nest = false) {
                eof = false;
                oneline = false;
                if (r.expect(begin)) {
                    size_t depth = 1;
                    while (depth) {
                        if (r.ceof()) {
                            eof = true;
                            return false;
                        }
                        if (r.expect(end)) {
                            depth--;
                            if (depth) {
                                comment += end;
                            }
                        }
                        else if (nest && r.expect(begin)) {
                            comment += begin;
                            depth++;
                        }
                        else {
                            comment += r.achar();
                            r.increment();
                        }
                    }
                    return true;
                }
                return false;
            }

            bool has_(const String& other) const override {
                return comments == other;
            }

            String to_string() const override {
                return comments;
            }

            Comment<String>* comment() override {
                return this;
            }
        };

        template <class String>
        struct RegistryRead : Token<String> {
           private:
            String token;

           public:
            RegistryRead(String&& str, TokenKind kind)
                : token(std::move(str)), Token<String>(kind) {}
            bool keyword_to_weak() {
                if (this->get_kind() != TokenKind::keyword) {
                    return false;
                }
                set_kind(TokenKind::weak_keyword);
                return true;
            }

            const String& get_token() const {
                return token;
            }

            template <class Buf, class Registry>
            static std::shared_ptr<RegistryRead<String>> ReadToken(TokenKind kind, commonlib2::Reader<Buf>& r, Registry& rep) {
                String tok;
                if (ReadToken(r, tok, rep)) {
                    return std::make_shared<RegistryRead>(std::move(tok), kind);
                }
                return nullptr;
            }

            template <class Buf, class Registry>
            static bool ReadToken(commonlib2::Reader<Buf>& r, String& read, Registry& reg) {
                return reg.Expect(r, read);
            }

            bool has_(const String& other) const override {
                return token == other;
            }

            String to_string() const override {
                return token;
            }

            RegistryRead<String>* registry() override {
                return this;
            }
        };

        template <class Registry1, class Registry2>
        struct MultiRegistry {
            Registry1& reg1;
            Registry2& reg2;

            MultiRegistry(Registry1& r1, Registry2& r2)
                : reg1(r1), reg2(r2) {}

            template <class Buf>
            bool Ahead(commonlib2::Reader<Buf>& r) {
                return reg1.Ahead(r) || reg2.Ahead(r);
            }

            template <class Buf>
            bool Expect(commonlib2::Reader<Buf>& r) {
                return reg1.Expect(r) || reg2.Expect(r);
            }
        };

        template <class String, class Registry>
        struct NonIdentifierRegistry {
            Registry& reg;

            NonIdentifierRegistry(Registry& rg)
                : reg(rg) {}

            template <class Buf>
            bool Ahead(commonlib2::Reader<Buf>& r) {
                return Spaces<String>::Ahead(r) || Line<String>::Ahead(r) || reg.Ahead(r);
            }
        };

        template <class String>
        struct Identifier : Token<String> {
           private:
            String id;

           public:
            Identifier(String&& s)
                : id(std::move(s)), Token<String>(TokenKind::identifiers) {}

            void set_identifier(const String& str) {
                id = str;
            }

            const String& get_identifier() const {
                return id;
            }

            template <class Buf, class Registry>
            static std::shared_ptr<Identifier<String>> ReadIdentifier(commonlib2::Reader<Buf>& r, Registry& reg) {
                String id;
                if (ReadIdentifier(r, reg, id)) {
                    return std::make_shared<Identifier<String>>(std::move(id));
                }
                return nullptr;
            }

            template <class Buf, class Registry>
            static bool ReadIdentifier(commonlib2::Reader<Buf>& r, Registry& reg, String& id) {
                if (reg.Ahead(r)) {
                    return false;
                }
                while (true) {
                    if (r.ceof()) {
                        break;
                    }
                    if (reg.Ahead(r)) {
                        break;
                    }
                    id.push_back(r.achar());
                    r.increment();
                }
                return true;
            }

            bool has_(const String& other) const override {
                return id == other;
            }

            String to_string() const override {
                return id;
            }

            Identifier<String>* identifier() override {
                return this;
            }
        };
    }  // namespace tokenparser
}  // namespace PROJECT_NAME