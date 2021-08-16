/*
    Copyright (c) 2021 on-keyday
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "reader.h"
#ifdef _WIN32
#include <Windows.h>
#endif
namespace PROJECT_NAME {

    struct U8MiniBuffer {
        unsigned char minbuf[4] = {0};
        size_t pos = 0;

        void push_back(unsigned char c) {
            if (pos >= 4) return;
            minbuf[pos] = c;
            pos++;
        }

        size_t size() const {
            return pos;
        }

        unsigned char operator[](size_t p) {
            if (p >= 4) return char();
            return minbuf[p];
        }

        void reset() {
            pos = 0;
            minbuf[0] = 0;
            minbuf[1] = 0;
            minbuf[2] = 0;
            minbuf[3] = 0;
        }

        U8MiniBuffer& operator=(const U8MiniBuffer& in) {
            reset();
            pos = in.pos;
            for (auto i = 0; i < pos; i++) {
                minbuf[i] = in.minbuf[i];
            }
            return *this;
        }

        U8MiniBuffer& operator=(U8MiniBuffer&& in) {
            reset();
            pos = in.pos;
            for (auto i = 0; i < pos; i++) {
                minbuf[i] = in.minbuf[i];
            }
            in.reset();
            return *this;
        }
    };

    struct U16MiniBuffer {
        unsigned short minbuf[2] = {0};
        size_t pos = 0;

        void push_back(unsigned short c) {
            if (pos >= 2) return;
            minbuf[pos] = c;
            pos++;
        }

        size_t size() const {
            return pos;
        }

        char16_t operator[](size_t p) {
            if (p >= 2) return char();
            return minbuf[p];
        }

        void reset() {
            pos = 0;
            minbuf[0] = 0;
            minbuf[1] = 0;
        }

        U16MiniBuffer& operator=(const U16MiniBuffer& in) {
            reset();
            pos = in.pos;
            for (auto i = 0; i < pos; i++) {
                minbuf[i] = in.minbuf[i];
            }
            return *this;
        }

        U16MiniBuffer& operator=(U16MiniBuffer&& in) {
            reset();
            pos = in.pos;
            for (auto i = 0; i < pos; i++) {
                minbuf[i] = in.minbuf[i];
            }
            in.reset();
            return *this;
        }
    };

    inline unsigned char utf8bits(int i) {
        const unsigned char maskbits[] = {
            //first byte mask
            0b10000000,
            0b11000000,
            0b11100000,
            0b11110000,
            0b11111000,

            //i>=5,second byte later mask
            0b00011110,              //two byte must
            0b00001111, 0b00100000,  //three byte must
            0b00000111, 0b00110000,  //four byte must
        };
        return i >= 0 && i < sizeof(maskbits) ? maskbits[i] : 0;
    }

    inline unsigned char utf8mask(unsigned char c, int i) {
        if (i < 1 || i > 4) return 0;
        return (utf8bits(i) & c) == utf8bits(i - 1) ? utf8bits(i - 1) : 0;
    }

    template <class Buf>
    bool check_utf8bits(Reader<Buf>* self, int& ctx, int ofsdef = 0) {
        auto r = [&](int ofs = 0) {
            return (unsigned char)self->offset(ofs + ofsdef);
        };
        auto mask = [&r](auto i, int ofs = 0) {
            return utf8mask(r(ofs), i);
        };
        auto needbits = [&r](int ofs, int masknum) {
            return r(ofs) & utf8bits(masknum);
        };
        int range = 0;
        if (r() < 0x80) {
            range = 1;
        }
        else if (mask(2)) {
            range = 2;
            if (!needbits(0, 5)) {
                ctx = 1;
                return false;
            }
        }
        else if (mask(3)) {
            range = 3;
            if (!needbits(0, 6) && !needbits(1, 7)) {
                ctx = 12;
                return false;
            }
        }
        else if (mask(4)) {
            range = 4;
            if (!needbits(0, 8) && !needbits(1, 9)) {
                ctx = 12;
                return false;
            }
        }
        else {
            ctx = 1;
            return false;
        }
        ctx = range;
        return true;
    }

    template <class Buf, class Ret>
    bool utf8_read(Reader<Buf>* self, Ret& ret, int*& ctx, bool begin) {
        static_assert(sizeof(typename Reader<Buf>::char_type) == 1, "");
        if (begin) {
            if (!ctx) return false;
            return true;
        }
        if (!self) return true;
        /*auto r=[&](int ofs=0){
            return (unsigned char)self->offset(ofs);
        };*/
        auto mask = [&self](auto i, int ofs = 0) {
            return utf8mask((unsigned char)self->offset(ofs), i);
        };
        /*auto needbits=[&r](int ofs,int masknum){
            return r(ofs)&utf8bits(masknum);
        };*/
        int range = 0;
        /*if(r()<0x80){
            range=1;
        }
        else if(mask(2)){
            range=2;
            if(!needbits(0,5)){
                *ctx=1;
                return true;
            }
        }
        else if(mask(3)){
            range=3;
            if(!needbits(0,6)&&!needbits(1,7)){
                *ctx=12;
                return true;
            }
        }
        else if(mask(4)){
            range=4;
            if(!needbits(0,8)&&!needbits(1,9)){
                *ctx=12;
            }
        }
        else{
            *ctx=1;
            return true;
        }*/
        if (!check_utf8bits(self, range)) {
            *ctx = range;
            return true;
        }
        for (auto i = 0; i < range; i++) {
            if (i) {
                self->increment();
                if (!mask(1)) {
                    *ctx = i + 1;
                    return true;
                }
            }
            ret.push_back(self->achar());
        }
        return false;
    }

    template <class Buf> /*,class Str>*/
    char32_t utf8toutf32_impl(Reader<Buf>* self, int* ctx) {
        //Str buf;
        U8MiniBuffer buf;
        utf8_read(self, buf, ctx, false);
        if (*ctx) return 0;
        auto len = (int)buf.size();
        if (len == 1) {
            return (char32_t)buf[0];
        }
        auto maskbit = [](int i) {
            return ~utf8bits(i);
        };
        auto make = [&maskbit, &buf, &len]() {
            char32_t ret = 0;
            for (int i = 0; i < len; i++) {
                auto mul = (len - i - 1);
                auto shift = 6 * mul;
                unsigned char masking = 0;
                if (i == 0) {
                    masking = buf[i] & maskbit(len - 1);
                }
                else {
                    masking = buf[i] & maskbit(1);
                }
                ret |= masking << shift;
            }
            return ret;
        };
        return (char32_t)make();
    }

    template <class Buf, class Ret>
    bool utf8toutf32(Reader<Buf>* self, Ret& ret, int*& ctx, bool begin) {
        static_assert(sizeof(typename Reader<Buf>::char_type) == 1, "");
        static_assert(sizeof(ret[0]) == 4, "");
        if (begin) {
            if (!ctx) return false;
            return true;
        }
        if (!self) return true;
        auto c = utf8toutf32_impl(self, ctx);
        if (*ctx) return true;
        ret.push_back(c);
        return false;
    }

    template <class Buf, class Ret>
    bool utf32toutf8(Reader<Buf>* self, Ret& ret, int*& ctx, bool begin) {
        static_assert(sizeof(typename Reader<Buf>::char_type) == 4, "");
        static_assert(sizeof(ret[0]) == 1, "");
        using Char8 = remove_cv_ref<decltype(ret[0])>;
        if (begin) {
            if (!ctx) return false;
            return true;
        }
        if (!self) return true;
        unsigned int C = self->achar();
        auto push = [&ret, C](int len) {
            Char8 mask = ~utf8bits(1);
            for (auto i = 0; i < len; i++) {
                auto mul = (len - 1 - i);
                auto shift = 6 * mul;
                Char8 abyte = 0, shiftC = (Char8)(C >> shift);
                if (i == 0) {
                    abyte = utf8bits(len - 1) | (shiftC & mask);
                }
                else {
                    abyte = utf8bits(0) | (shiftC & mask);
                }
                ret.push_back(abyte);
            }
        };
        if (C < 0x80) {
            ret.push_back((Char8)C);
        }
        else if (C < 0x800) {
            push(2);
        }
        else if (C < 0x10000) {
            push(3);
        }
        else if (C < 0x110000) {
            push(4);
        }
        else {
            *ctx = C;
            return true;
        }
        return false;
    }

    inline bool is_utf16_surrogate_high(unsigned short C) {
        return 0xD800 <= C && C <= 0xDC00;
    }

    inline bool is_utf16_surrogate_low(unsigned short C) {
        return 0xDC00 <= C && C < 0xE000;
    }

    inline char32_t make_surrogate_char(unsigned short first, unsigned short second) {
        return 0x10000 + (first - 0xD800) * 0x400 + (second - 0xDC00);
    }

    template <class Buf>
    char32_t utf16toutf32_impl(Reader<Buf>* self, int* ctx) {
        auto C = self->achar();
        auto hi = [&C] {
            return is_utf16_surrogate_high(C);
        };
        auto low = [&C] {
            return is_utf16_surrogate_low(C);
        };
        if (hi()) {
            auto first = C;
            self->increment();
            C = self->achar();
            if (!low()) {
                *ctx = 2;
                return true;
            }
            return make_surrogate_char(first, C);
        }
        else {
            return C;
        }
    }

    template <class Buf, class Ret>
    bool utf16toutf32(Reader<Buf>* self, Ret& ret, int*& ctx, bool begin) {
        static_assert(sizeof(typename Reader<Buf>::char_type) == 2, "");
        static_assert(sizeof(ret[0]) == 4, "");
        if (begin) {
            if (!ctx) return false;
            return true;
        }
        if (!self) return true;
        auto C = utf16toutf32_impl(self, ctx);
        if (*ctx) return true;
        ret.push_back(C);
        return false;
    }

    template <class Buf, class Ret>
    bool utf32toutf16(Reader<Buf>* self, Ret& ret, int*& ctx, bool begin) {
        static_assert(sizeof(typename Reader<Buf>::char_type) == 4, "");
        static_assert(sizeof(ret[0]) == 2, "");
        if (begin) {
            if (!ctx) return false;
            return true;
        }
        if (!self) return true;
        auto C = self->achar();
        if (C < 0 || C > 0x10FFFF) {
            *ctx = 1;
            return true;
        }
        if (C < 0x10000) {
            ret.push_back((char16_t)C);
        }
        else {
            auto first = char16_t((C - 0x10000) / 0x400 + 0xD800);
            auto second = char16_t((C - 0x10000) % 0x400 + 0xDC00);
            ret.push_back(first);
            ret.push_back(second);
        }
        return false;
    }

    template <class Buf, class Ret>
    bool utf8toutf16(Reader<Buf>* self, Ret& ret, int*& ctx, bool begin) {
        static_assert(sizeof(typename Reader<Buf>::char_type) == 1, "");
        static_assert(sizeof(ret[0]) == 2, "");
        if (begin) {
            if (!ctx) return false;
            return true;
        }
        if (!self) return true;
        auto C = utf8toutf32_impl(self, ctx);
        if (*ctx)
            return true;
        if (C == 0) {
            ret.push_back(C);
            return false;
        }
        char32_t buf[] = {C, 0};
        Reader<char32_t*> read((char32_t*)buf);
        read.readwhile(ret, utf32toutf16, ctx);
        if (*ctx) return true;
        return false;
    }

    template <class Buf, class Ret>
    bool utf16toutf8(Reader<Buf>* self, Ret& ret, int*& ctx, bool begin) {
        static_assert(sizeof(typename Reader<Buf>::char_type) == 2, "");
        static_assert(sizeof(ret[0]) == 1, "");
        if (begin) {
            if (!ctx) return false;
            return true;
        }
        if (!self) return true;
        auto C = utf16toutf32_impl(self, ctx);
        if (*ctx) return true;
        if (C == 0) {
            ret.push_back(C);
            return false;
        }
        char32_t buf[] = {C, 0};
        Reader<char32_t*> read(buf);
        read.readwhile(ret, utf32toutf8, ctx);
        if (*ctx) return true;
        return false;
    }

#ifdef _WIN32 /*for windows in Japanese-lang*/

    template <class Buf, class Ret>
    bool sjistoutf16(Reader<Buf>* self, Ret& ret, int*& ctx, bool begin) {
        static_assert(sizeof(typename Reader<Buf>::char_type) == 1);
        static_assert(sizeof(ret[0]) == 2);
        if (begin) {
            if (!ctx) return false;
            return true;
        }
        if (!self) return true;
        unsigned char C = self->achar();
        wchar_t result[] = {0, 0, 0};
        int res = 0;
        if (C < 0x80 || (0xa0 < C && C < 0xe0)) {
            unsigned char buf[] = {C};
            res = MultiByteToWideChar(932, MB_PRECOMPOSED, (char*)buf, 1, result, sizeof(result));
            if (res == 0) {
                *ctx = 1;
                return true;
            }
        }
        else if (0x80 < C && C < 0xa0 || 0xe0 <= C && C < 0xfd) {
            auto first = C;
            self->increment();
            C = self->achar();
            if (!(0x40 <= C && C < 0x7f || 0x80 <= C && C < 0xfd)) {
                *ctx = 2;
                return true;
            }
            unsigned char buf[] = {first, C};
            res = MultiByteToWideChar(932, MB_PRECOMPOSED, (char*)buf, 2, result, sizeof(result));
            if (res == 0) {
                *ctx = 2;
                return true;
            }
        }
        for (auto i = 0; i < res; i++) {
            ret.push_back(result[i]);
        }
        return false;
    }

    template <class Buf, class Ret>
    bool utf16tosjis(Reader<Buf>* self, Ret& ret, int*& ctx, bool begin) {
        static_assert(sizeof(typename Reader<Buf>::char_type) == 2);
        static_assert(sizeof(ret[0]) == 1);
        if (begin) {
            if (!ctx) return false;
            return true;
        }
        if (!self) return true;
        wchar_t C = self->achar();
        char result[] = {0, 0, 0};
        int res = 0;
        if (is_utf16_surrogate_high(C)) {
            self->increment();
            wchar_t S = self->achar();
            wchar_t buf[] = {C, S};
            res = WideCharToMultiByte(932, 0, buf, 2, result, sizeof(result), NULL, NULL);
            if (res == 0) {
                *ctx = 1;
                return true;
            }
        }
        else {
            wchar_t buf[] = {C};
            res = WideCharToMultiByte(932, 0, buf, 1, result, sizeof(result), NULL, NULL);
            if (res == 0) {
                *ctx = 1;
                return true;
            }
        }
        for (auto i = 0; i < res; i++) {
            ret.push_back(result[i]);
        }
        return false;
    }
#endif

    template <class Buf>
    bool utf8seek_minus(Reader<Buf>* self) {
        if (!self) return false;
        if (self->readpos() < 1) return false;
        if (static_cast<unsigned char>(self->offset(-1)) < 0x7f) {
            self->decrement();
            return true;
        }
        if (self->readpos() < 2) return false;
        int range = 0;
        auto mask = [&self](auto i, int ofs = 0) {
            return utf8mask((unsigned char)self->offset(ofs), i);
        };
        auto check = [&](auto count) {
            if (check_utf8bits(self, range, -count)) {
                if (range == count) {
                    for (auto i = 0; i < count - 1; i++) {
                        if (!mask(1, -i - 1)) {
                            return false;
                        }
                    }
                    for (auto i = 0; i < count; i++) {
                        self->decrement();
                    }
                    return true;
                }
            }
            return false;
        };
        if (check(2)) return true;
        if (self->readpos() < 3) return false;
        if (check(3)) return true;
        if (self->readpos() < 4) return false;
        if (check(4)) return true;
        return false;
    }

    template <class Buf>
    bool utf16seek_minus(Reader<Buf>* self) {
        if (!self) return false;
        if (self->readpos() == 0) {
            return false;
        }
        if (self->readpos() >= 2) {
            if (is_utf16_surrogate_high(self->offset(-2)) &&
                is_utf16_surrogate_low(self->offset(-1))) {
                self->decrement();
                self->decrement();
                return true;
            }
        }
        self->decrement();
        return true;
    }
}  // namespace PROJECT_NAME