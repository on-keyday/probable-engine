/*
    commonlib - common utility library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once

#include "basic_helper.h"
#include "json_util.h"
#include "extutil.h"

#include <string>
#include <map>

namespace PROJECT_NAME {

    template <class Buf>
    struct HTTP2Frame {
        Buf buf;
        bool succeed = false;
        int id = 0;
        int len = 0;
        unsigned char type = 0;
        unsigned char flag = 0;
        int maxlen = 16384;
        bool continues = false;
    };

    template <class Buf>
    struct URLContext {
        bool needend = false;
        Buf scheme;
        Buf username;
        Buf password;
        Buf host;
        Buf port;
        Buf path;
        Buf query;
        Buf tag;
        bool succeed = false;
    };

    struct Base64Context {
        char c62 = '+';
        char c63 = '/';
        bool strict = false;
        bool succeed = false;
        bool nopadding = false;
    };

    template <class Buf>
    struct URLEncodingContext {
        bool path = false;
        bool space_to_plus = false;
        bool query = false;
        bool big = false;
        bool failed = false;
        Buf no_escape;
        bool special(char c, bool decode) {
            if (space_to_plus) {
                if (decode) {
                    return c == ' ';
                }
                else {
                    return c == '+';
                }
            }
            return false;
        }
        bool special_add(Buf& ret, char c, bool decode) {
            if (decode) {
                if (c == '+') {
                    ret.push_back(' ');
                }
                else {
                    ret.push_back(c);
                }
            }
            else {
                if (c == ' ') {
                    ret.push_back('+');
                }
                else {
                    ret.push_back(c);
                }
            }
            return true;
        }
        bool noescape(char c) {
            if (is_alpha_or_num(c)) return true;
            if (path) {
                if (c == '/' || c == '.' || c == '-' || c == '_') return true;
            }
            if (query) {
                if (c == '?' || c == '&' || c == '=' || c == '_') return true;
            }
            for (auto i : no_escape) {
                if (i == c) return true;
            }
            return false;
        }
    };

    template <class Ctx, class Buf>
    bool http2frame(Reader<Buf>* self, Ctx& ctx, bool begin) {
        using Char = typename Reader<Buf>::char_type;
        static_assert(sizeof(Char) == 1);
        if (begin) {
            ctx.succeed = false;
            ctx.continues = false;
            return true;
        }
        if (!self) return true;
        if (self->readable() < 9) {
            ctx.continues = true;
            return true;
        }
        if (self->read_byte(&ctx.len, 3, translate_byte_net_and_host, true) < 3) return true;
        ctx.len >>= 8;
        if (ctx.len >= 0x00800000 || ctx.len < 0) return true;
        if (self->read_byte(&ctx.type, 1, translate_byte_net_and_host, true) < 1) return true;
        if (self->read_byte(&ctx.flag, 1, translate_byte_net_and_host, true) < 1) return true;
        if (self->read_byte(&ctx.id, 4, translate_byte_net_and_host, true) < 4) return true;
        if (ctx.id < 0) return true;
        if (self->readable() < ctx.len) {
            ctx.continues = true;
            return true;
        }
        else if (ctx.len > ctx.maxlen) {
            return true;
        }
        ctx.buf.resize(ctx.len);
        if (self->read_byte(ctx.buf.data(), ctx.len, translate_byte_as_is, true) < ctx.len) {
            return true;
        }
        /*
        memmove(ctx.buf.data(), &self->ref().data()[self->readpos()], ctx.len);
        self->seek(self->readpos() + ctx.len);*/
        ctx.succeed = true;
        return true;
    }

    template <class Ctx, class Buf>
    bool http2frame_reader(Reader<Buf>* self, Ctx& ctx, bool begin) {
        if (!self) return true;
        if (!begin) return true;
        auto beginpos = self->readpos();
        self->readwhile(http2frame, ctx);
        /*if (ctx.succeed) {
            //self->ref().erase(beginpos, ctx.len + 9);
            //self->seek(beginpos);
        }*/
        if (ctx.continues) {
            self->seek(beginpos);
        }
        return true;
    }

    template <class Ctx, class Buf>
    bool parse_url(Reader<Buf>* self, Ctx& ctx, bool begin) {
        using Char = typename Reader<Buf>::char_type;
        static_assert(sizeof(Char) == 1);
        if (begin) return true;
        if (!self) return true;
        auto beginpos = self->readpos();
        bool has_scheme = false, has_userandpass = false;
        if (self->readwhile(untilnobuf, (Char)'/')) {
            if (self->offset(-1) == (Char)':') {
                has_scheme = true;
            }
        }
        self->seek(beginpos);
        if (self->readwhile(untilnobuf, (Char)'@')) {
            has_userandpass = true;
        }
        self->seek(beginpos);
        if (has_scheme) {
            self->readwhile(ctx.scheme, untilincondition, is_url_scheme_usable<Char>);
            if (!self->expect(":")) return true;
        }
        self->expect("//");
        if (has_userandpass) {
            if (!self->readwhile(
                    ctx.username, untilincondition,
                    +[](Char c) { return c != ':' && c != '@'; })) return true;
            if (self->achar() == ':') {
                if (!self->expect(":")) return true;
                if (!self->readwhile(ctx.password, until, (Char)'@')) return true;
            }
            if (!self->expect("@")) return true;
        }
        if (self->expect("[")) {
            ctx.host.push_back('[');
            if (!self->readwhile(
                    ctx.host, untilincondition, [](Char c) {
                        return c == (Char)':' || is_hex(c);
                    },
                    true)) return false;
            if (ctx.host.size() <= 1) return false;
            if (!self->expect("]")) return false;
            ctx.host.push_back(']');
        }
        else {
            if (!self->readwhile(ctx.host, untilincondition, is_url_host_usable<Char>, true)) return true;
        }
        if (self->expect(":")) {
            if (!self->readwhile(
                    ctx.port, untilincondition, +[](Char c) { return is_digit(c); }, true)) return true;
            //if(!self->ahead("/"))return true;
        }
        if (self->ahead("/")) {
            if (!self->readwhile(
                    ctx.path, untilincondition, +[](Char c) { return c != '?' && c != '#'; }, true)) return true;
            if (self->ahead("?")) {
                if (!self->readwhile(
                        ctx.query, untilincondition, +[](Char c) { return c != '#'; }, true)) return true;
            }
            if (self->ahead("#")) {
                if (!self->readwhile(
                        ctx.tag, untilincondition, +[](Char c) { return c != '\0'; }, true)) return true;
            }
        }
        if (ctx.needend && !self->ceof()) return true;
        ctx.succeed = true;
        return true;
    }

    template <class Ret, class Ctx, class Buf>
    bool base64_encode(Reader<Buf>* self, Ret& ret, Ctx& ctx, bool begin) {
        static_assert(sizeof(typename Reader<Buf>::char_type) == 1);
        if (!ctx) return true;
        if (begin) return true;
        if (!self) {
            return true;
        }
        auto translate = [&ctx](unsigned char c) -> char {
            if (c >= 64) return 0;
            if (c < 26) {
                return 'A' + c;
            }
            else if (c >= 26 && c < 52) {
                return 'a' + (c - 26);
            }
            else if (c >= 52 && c < 62) {
                return '0' + (c - 52);
            }
            else if (c == 62) {
                return ctx->c62;
            }
            else if (c == 63) {
                return ctx->c63;
            }
            return 0;
        };
        while (!self->ceof()) {
            int num = 0;
            auto read_size = self->read_byte(&num, 3, translate_byte_net_and_host);
            if (!read_size) break;
            num >>= 8;
            char buf[] = {(char)((num >> 18) & 0x3f), (char)((num >> 12) & 0x3f), (char)((num >> 6) & 0x3f), (char)(num & 0x3f)};
            for (auto i = 0; i < read_size + 1; i++) {
                ret.push_back(translate(buf[i]));
            }
        }
        while (!ctx->nopadding && ret.size() % 4) {
            ret.push_back('=');
        }
        return true;
    }

    template <class Ret, class Ctx, class Buf>
    bool base64_decode(Reader<Buf>* self, Ret& ret, Ctx& ctx, bool begin) {
        static_assert(sizeof(typename Reader<Buf>::char_type) == 1);
        if (!ctx) return true;
        if (begin) return true;
        if (!self) {
            return true;
        }
        auto firstsize = self->readable();
        if (ctx->strict == true && firstsize % 4) return true;
        auto translate = [&ctx](char c) -> unsigned char {
            if (c >= 'A' && c <= 'Z') {
                return c - 'A';
            }
            else if (c >= 'a' && c <= 'z') {
                return c - 'a' + 26;
            }
            else if (c >= '0' && c <= '9') {
                return c + -'0' + 52;
            }
            else if (c == ctx->c62) {
                return 62;
            }
            else if (c == ctx->c63) {
                return 63;
            }
            return ~0;
        };
        Reader<Ret> tmp;
        self->readwhile(tmp.ref(), addifmatch, [&ctx](char c) { return is_alpha_or_num(c) || ctx->c62 == c || ctx->c63 == c || c == '='; });
        if (ctx->strict) {
            if (tmp.readable() != firstsize) return true;
        }
        while (!tmp.ceof()) {
            int shift = 0;
            char bytes[4] = {0}, *shift_p = (char*)&shift;
            auto readsize = tmp.read_byte(bytes, 4);
            auto no_ignore = 0;
            for (auto i = 0; i < readsize; i++) {
                auto bit = translate(bytes[i]);
                if (bit == 0xff)
                    break;
                no_ignore++;
                shift += ((bit << 6) * (3 - i));
            }
            shift = translate_byte_net_and_host<int>(shift_p);
            shift >>= 8;
            for (auto i = 0; i < no_ignore - 1; i++) {
                ret.push_back(shift_p[i]);
            }
        }
        ctx->succeed = true;
        return true;
    }

    template <class Ret, class Ctx, class Buf>
    bool url_encode(Reader<Buf>* self, Ret& ret, Ctx& ctx, bool begin) {
        static_assert(sizeof(typename Reader<Buf>::char_type) == 1);
        if (begin) return true;
        if (!self) return true;
        if (!ctx->noescape(self->achar())) {
            if (ctx->special(self->achar(), false)) {
                ctx->special_add(ret, self->achar(), false);
            }
            else {
                auto c = self->achar();
                auto msb = (c & 0xf0) >> 4;
                auto lsb = c & 0x0f;
                auto translate = [&ctx](unsigned char c) -> unsigned char {
                    if (c > 15) return 0;
                    if (c < 10) {
                        return c + '0';
                    }
                    else {
                        if (ctx->big) {
                            return c - 10 + 'A';
                        }
                        else {
                            return c - 10 + 'a';
                        }
                    }
                    return 0;
                };
                ret.push_back('%');
                ret.push_back(translate(msb));
                ret.push_back(translate(lsb));
            }
        }
        else {
            ret.push_back(self->achar());
        }
        return false;
    }

    template <class Ret, class Ctx, class Buf>
    bool url_decode(Reader<Buf>* self, Ret& ret, Ctx& ctx, bool begin) {
        static_assert(sizeof(typename Reader<Buf>::char_type) == 1);
        if (begin) return true;
        if (!self) {
            return true;
        }
        if (ctx->special(self->achar(), true)) {
            ctx->special_add(ret, self->achar(), true);
        }
        else if (self->achar() == '%') {
            auto first = 0;
            auto second = 0;
            self->increment();
            first = self->achar();
            self->increment();
            second = self->achar();
            auto translate = [](unsigned char c) -> unsigned char {
                if (c >= '0' && c <= '9') {
                    return c - '0';
                }
                else if (c >= 'A' && c <= 'F') {
                    return c - 'A' + 10;
                }
                else if (c >= 'a' && c <= 'f') {
                    return c - 'a' + 10;
                }
                return ~0;
            };
            first = translate(first);
            second = translate(second);
            if (first == 0xff || second == 0xff) {
                ctx->failed = true;
                return true;
            }
            ret.push_back((first << 4) + second);
        }
        else {
            ret.push_back(self->achar());
        }
        return false;
    }

    template <class Str = std::string, class Buf, class Map>
    auto parse_httpbody(Map& data, Reader<Buf>& r) {
        Str result;
        if (auto f = data.find("transfer-encoding"); f != data.end() && (*f).second.find("chunked") != ~0) {
            while (true) {
                size_t size = 0;
                std::string num = "0x";
                bool notext;
                do {
                    notext = false;
                    getline(r, num, false, &notext);
                } while (notext);
                Reader<std::string>(num) >>
                    size;
                if (size == 0) {
                    r.expect("\r\n");
                    break;
                }
                auto prev = result.size();
                result.resize(result.size() + size);
                while (r.read_byte(result.data() + prev, size, translate_byte_as_is, true) < size) {
                    r.offset(1);
                    if (!r.ref().reading()) {
                        return false;
                    }
                }
                while (!r.expect("\r\n")) {
                    if (r.readable() < 2) {
                        if (!r.ref().reading()) {
                            return false;
                        }
                    }
                }
            }
        }
        else if (f = data.find("content-length"); f != data.end()) {
            size_t size = 0;
            Reader((*f).second) >> size;
            result.resize(size);
            while (r.read_byte(result.data(), size, translate_byte_as_is, true) < size) {
                if (r.eof()) break;
                if (!r.ref().reading()) {
                    return false;
                }
            }
        }
        else if (f = data.find("content-type"); f != data.end() && f->second.find("json") != ~0) {
            size_t first = r.readpos();
            const char* err;
            do {
                err = nullptr;
                JSON<std::unordered_map, std::vector>::parse(r, &err);
            } while (err);
            size_t last = r.readpos();
            r.seek(first);
            result.resize(last - first);
            r.read_byte(result.data(), last - first);
        }
        else {
            if (r.readpos() + 1 == r.ref().size()) {
                return true;
            }
            while (r.ref().reading())
                ;
            r >> result;
            if (result.size()) result.pop_back();  //for socklib::SockReader
        }
        data.emplace(":body", std::move(result));
        return true;
    }

    template <class Str = std::string, class Buf, class Map>
    bool parse_httpheader(Reader<Buf>& r, Map& ret) {
        while (true) {
            auto tmp = getline<Str>(r, false);
            if (tmp.size() == 0) break;
            auto f = split(tmp, ":", 1);
            if (f.size() < 2) return false;
            while (f[1].size() && (f[1][0] == ' ' || f[1][0] == '\t')) f[1].erase(0, 1);
            std::transform(f[0].begin(), f[0].end(), f[0].begin(), [](char c) { return std::tolower((unsigned char)c); });
            ret.emplace(f[0], f[1]);
        }
        return true;
    }

    template <class Buf, class Str = std::string, class Map>
    bool parse_httprequest(Reader<Buf>& r, Map& ret, bool ignore_body = false) {
        auto status = split(getline(r, false), " ");
        if (status.size() < 2) return false;
        ret.emplace(":method", status[0]);
        ret.emplace(":path", status[1]);
        if (status.size() >= 3) ret.emplace(":version", status[2]);
        if (!parse_httpheader<Str>(r, ret)) return false;
        if (!ignore_body) {
            if (!parse_httpbody<Str>(ret, r)) {
                return false;
            }
        }
        return true;
    }

    template <class Buf, class Str = std::string, template <class...> class Map = std::multimap>
    auto parse_httprequest(Reader<Buf>& r, bool ignore_body = false) {
        Map<Str, Str> ret;
        if (!parse_httprequest(r, ret, ignore_body)) {
            return decltype(ret)();
        }
        return ret;
    }

    template <class Buf, class Str = std::string, class Map>
    bool parse_httpresponse(Reader<Buf>& r, Map& ret, bool ignore_body = false) {
        auto status = split(getline(r, false), " ");
        if (status.size() < 3) return false;
        ret.emplace(":version", status[0]);
        ret.emplace(":status", status[1]);
        std::string phrase = status[2];
        for (auto i = 3; i < status.size(); i++) {
            phrase += " " + status[i];
        }
        ret.emplace(":phrase", phrase);
        if (!parse_httpheader<Str>(r, ret)) return false;
        if (!ignore_body) {
            parse_httpbody<Str>(ret, r);
        }
        return true;
    }

    template <class Buf, class Str = std::string, template <class...> class Map = std::multimap>
    auto parse_httpresponse(Reader<Buf>& r, bool ignore_body = false) {
        Map<Str, Str> ret;
        if (!parse_httpresponse(r, ret, ignore_body)) {
            return decltype(ret)();
        }
        return ret;
    }

    struct SHA1Context {
        unsigned int h[5] = {0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0};
        unsigned char result[sizeof(h)] = {0};
        unsigned long long total = 0;
        bool intotal = false;

        void calc(const char* bits) {
            auto rol = [](unsigned int word, auto shift) {
                return (word << shift) | (word >> (sizeof(word) * 8 - shift));
            };
            unsigned int w[80] = {0};
            unsigned int a = h[0], b = h[1], c = h[2], d = h[3], e = h[4];
            for (auto i = 0; i < 80; i++) {
                if (i < 16) {
                    w[i] = translate_byte_net_and_host<unsigned int>(&bits[i * 4]);
                }
                else {
                    w[i] = rol(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
                }
                unsigned int f = 0, k = 0;
                if (i <= 19) {
                    f = (b & c) | ((~b) & d);
                    k = 0x5A827999;
                }
                else if (i <= 39) {
                    f = b ^ c ^ d;
                    k = 0x6ED9EBA1;
                }
                else if (i <= 59) {
                    f = (b & c) | (b & d) | (c & d);
                    k = 0x8F1BBCDC;
                }
                else {
                    f = b ^ c ^ d;
                    k = 0xCA62C1D6;
                }
                unsigned int tmp = rol(a, 5) + f + e + k + w[i];
                e = d;
                d = c;
                c = rol(b, 30);
                b = a;
                a = tmp;
            }
            h[0] += a;
            h[1] += b;
            h[2] += c;
            h[3] += d;
            h[4] += e;
        }
    };

    template <class Buf>
    bool sha1(Reader<Buf>* r, SHA1Context& ctx, bool begin) {
        if (begin) {
            constexpr unsigned int init[] = {0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0};
            ctx.h[0] = init[0];
            ctx.h[1] = init[1];
            ctx.h[2] = init[2];
            ctx.h[3] = init[3];
            ctx.h[4] = init[4];
            ctx.total = 0;
            ctx.intotal = false;
        }
        union {
            char bits[64] = {0};
            unsigned long long ints[8];
        } c;
        if (!r) {
            if (!ctx.intotal) {
                c.ints[7] = translate_byte_net_and_host<unsigned long long>(&ctx.total);
                ctx.calc(c.bits);
                ctx.intotal = true;
            }
            int count = 0;
            for (auto& i : ctx.h) {
                unsigned int be = translate_byte_net_and_host<unsigned int>(&i);
                for (auto k = 0; k < 4; k++) {
                    ctx.result[count] = reinterpret_cast<char*>(&be)[k];
                    count++;
                }
            }
            return true;
        }
        size_t sz = 0;
        sz = r->read_byte(c.bits, 64);
        ctx.total += sz * 8;
        if (sz < 64) {
            c.bits[sz] = 0x80;
            ctx.intotal = true;
            if (sz < 56) {
                c.ints[7] = translate_byte_net_and_host<unsigned long long>(&ctx.total);
                ctx.calc(c.bits);
            }
            else {
                ctx.calc(c.bits);
                memset(c.bits, 0, 64);
                c.ints[7] = translate_byte_net_and_host<unsigned long long>(&ctx.total);
                ctx.calc(c.bits);
            }
        }
        else {
            ctx.calc(c.bits);
        }
        return begin;
    }

}  // namespace PROJECT_NAME