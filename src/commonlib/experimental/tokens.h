/*
    commonlib - common utility library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "basic_helper.h"
#include "reader.h"

#include <string>
#include <vector>

namespace PROJECT_NAME {

    enum class ValueKind {
        has_address,
        temporary,
    };

    inline bool is_ng_id(unsigned char c) {
        return c < 0x30 || (c >= 0x3A && c <= 0x40) || (c >= 0x5b && c <= 0x60) || (c >= 0x7b && c <= 0x7f);
    }

    struct Token {
        std::string token;
        std::string notexpect;
        ValueKind kind = ValueKind::temporary;
        template <class Buf>
        bool operator()(Reader<Buf>& r, bool is_id = false) {
            if (is_id) {
                return r.expect(token, [](b_char_type<Buf> c) {
                    return !is_ng_id(c);
                });
            }
            return r.expect(token, [this](b_char_type<Buf> c) {
                for (auto i : notexpect) {
                    if (c == i) {
                        return true;
                    }
                }
                return false;
            });
        }
    };

    enum class TokenBehavior {
        none = 0x0,
        increment = 0x1,
        unary = 0x2,
        three = 0x4,
        identifier = 0x10,
        init = 0x20,
        type = 0x40,
        hide = 0x80,
        prim = 0x100,
    };

    DEFINE_ENUMOP(TokenBehavior)

    struct Tokens {
        std::vector<Token> token;
        //Token* current = nullptr;
        size_t readbegin = 0;
        size_t readend = 0;
        TokenBehavior flag = TokenBehavior::increment;
        template <class Buf>
        bool operator()(Reader<Buf>& r,Token*& current) {
            auto tmp = r.readpos();
            for (auto& i : token) {
                if (i(r, any(flag & TokenBehavior::identifier))) {
                    current = &i;
                    readbegin = tmp;
                    readend = r.readpos();
                    return true;
                }
            }
            return false;
        }

        template <class Buf>
        bool operator()(Reader<Buf>& r, size_t s,Token*& current) {
            if (s >= token.size()) return false;
            auto tmp = r.readpos();
            if (token[s](r)) {
                current = &token[s];
                readbegin = tmp;
                readend = r.readpos();
                return true;
            }
            return false;
        }

        Tokens(std::initializer_list<Token> tok) {
            token = std::vector(tok);
        }
    };

    struct TokenHierarchy {
       private:
        /*size_t loopcount=0;
        
        size_t pos=0;*/
        std::vector<Tokens> token;
        std::vector<std::string> banlist;

       public:
        std::vector<Tokens>& get() {
            return token;
        }

        std::vector<std::string>& ban() {
            return banlist;
        }

        void make_banlist() {
            banlist.clear();
            for (auto& toks : token) {
                if (any(toks.flag & TokenBehavior::identifier)) {
                    for (auto& tok : toks.token) {
                        banlist.push_back(tok.token);
                    }
                }
            }
        }

        TokenHierarchy(std::initializer_list<Tokens> tok) {
            token = tok;
        }

        bool set(size_t pos, TokenBehavior flag) {
            if (pos >= token.size()) return false;
            token[pos].flag = flag;
            return true;
        }

        bool add(size_t pos, TokenBehavior flag) {
            if (pos >= token.size()) return false;
            token[pos].flag |= flag;
            return true;
        }
        void add_ban(std::string&& in) {
            banlist.push_back(std::move(in));
        }

        void remove_ban(std::string&& in) {
            auto _ = std::remove_if(banlist.begin(), banlist.end(), [&](auto& s) { return s == in; });
        }
    };

    template <class Buf>
    struct TokenManager {
       private:
        TokenHierarchy& tokenbase;
        Token* _current=nullptr;
        Reader<Buf>& r;
        size_t _position = 0;
        size_t depth = 0;

        bool incrememt() {
            if (tokenbase.get().size() == 0) return false;
            _position++;
            if (_position == tokenbase.get().size()) {
                depth++;
                _position = 0;
            }
            return true;
        }

        bool decrement() {
            if (tokenbase.get().size() == 0) return false;
            if (_position == 0) {
                if (depth == 0) return false;
                _position = tokenbase.get().size();
                depth--;
            }
            _position--;
            return true;
        }

        Tokens& get() const {
            return tokenbase.get()[_position];
        }

        bool zero() const {
            return tokenbase.get().size() == 0;
        }

        template<class T,class U>
        bool get_pos_and_idx(T pos,U idx,size_t& pos_,size_t& idx_){
            pos_ = (size_t)pos;
            idx_ = (size_t)idx;
            if (pos_ >= tokenbase.get().size()) return false;
            if (idx_ >= tokenbase.get()[pos_].token.size()) return false;
            return true;
        }

       public:
        TokenManager(Reader<Buf>& in_r, TokenHierarchy& h)
            : r(in_r), tokenbase(h) {}

        void posrange(size_t& begin, size_t& end) {
            begin = get().readbegin;
            end = get().readend;
        }

        Token* current() {
            return _current;
        }

        Reader<Buf>& reader() {
            return r;
        }

        size_t getpos() const {
            return _position;
        }

        bool setpos(size_t sz) {
            if (sz >= tokenbase.get().size()) return false;
            _position = sz;
            return true;
        }

        size_t getdepth()const{
            return depth;
        }

        bool setdepth(size_t sz){
            depth=sz;
            return true;
        }

        TokenManager& operator++(int) {
            incrememt();
            return *this;
        }

        TokenManager& operator--(int) {
            decrement();
            return *this;
        }

        TokenManager& operator++() {
            incrememt();
            return *this;
        }

        TokenManager& operator--() {
            decrement();
            return *this;
        }

        void to_begin() {
            _position = 0;
        }

        bool on_begin() const {
            return _position == 0;
        }

        void to_end() {
            if (tokenbase.get().size() == 0) return;
            _position = tokenbase.get().size() - 1;
        }

        bool on_end() const {
            return _position == tokenbase.get().size() - 1;
        }

        bool on_first() const {
            return depth == 0;
        }
        bool operator()() {
            if (zero()) return false;
            return get()(r,_current);
        }

        bool operator()(size_t pos) {
            if (zero()) return false;
            return get()(r, pos,_current);
        }

        template <class T>
        bool operator()(T pos) {
            return (*this)((size_t)pos);
        }

        template <class T,class U>
        bool operator()(T pos,U idx) {
            size_t pos_ ,idx_;
            if(!get_pos_and_idx(pos,idx,pos_,idx_))return false;
            Tokens& c=tokenbase.get()[pos_];
            return c(r,idx,_current);
        }

        bool has(TokenBehavior flag) const {
            if (zero()) return false;
            return any(flag & get().flag);
        }

        bool is_hidden() const {
            return has(TokenBehavior::hide) ||
                   (!on_first() && has(TokenBehavior::init));
        }

        void reset() {
            _position = 0;
            depth = 0;
            r.seek(0);
        }

        bool is_ban_name(std::string& name) {
            for (auto& n : tokenbase.ban()) {
                if (n == name) {
                    return true;
                }
            }
            return false;
        }

        template <class T, class U>
        Token* get_token(T pos, U idx) {
            size_t pos_ ,idx_;
            if(!get_pos_and_idx(pos,idx,pos_,idx_))return nullptr;
            return &tokenbase.get()[pos_].token[idx_];
        }
    };

}  // namespace PROJECT_NAME