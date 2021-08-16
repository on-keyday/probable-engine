#pragma once
#include <algorithm>
#include <string>

#include "basic_helper.h"
#include "project_name.h"

namespace PROJECT_NAME {

    namespace old {
        template <class Map, class Val>
        struct HTTPHeaderContext {
            Map buf;
            bool succeed = false;
            bool synerr = false;
            auto emplace(Val& key, Val& value) -> decltype(buf.emplace(key, value)) {
                std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c) { return tolower(c); });
                return buf.emplace(key, value);
            }
        };

        template <class Map, class Val>
        struct HTTPRequest {
            HTTPHeaderContext<Map, Val> header;
            bool succeed = false;
            int version = 0;
            Val method;
            Val path;
        };

        template <class Map, class Val>
        struct HTTPResponse {
            HTTPHeaderContext<Map, Val> header;
            bool succeed = false;
            int version = 0;
            Val reason;
            int statuscode = 0;
        };

        template <class Buf, class Header, class Recver>
        struct HTTPBodyContext {
            size_t strtosz(Buf& str, bool& res, int base = 10) {
                size_t p = 0;
                res = parse_int(str.data(), &str.data()[str.size()], p, base);
                return p;
            }
            Recver& recv;
            Header& header;
            bool has_header(const char* key, const char* value) {
                auto res = header.count(key);
                if (res && value) {
                    auto it = header.equal_range(key);
                    res = (*it.first).second.find(value) != ~0;
                }
                return res;
            }

            auto header_ref(const char* in) -> decltype((*header.equal_range(in).first).second)& {
                return (*header.equal_range(in).first).second;
            }

            HTTPBodyContext(Header& head, Recver& recver)
                : recv(recver), header(head) {}
            bool succeed = false;
        };

        template <class Val, class Map, class Buf>
        bool httpheader(Reader<Buf>* self, HTTPHeaderContext<Map, Val>& ctx, bool begin) {
            using Char = typename Reader<Buf>::char_type;
            static_assert(sizeof(Char) == 1);
            if (begin) return true;
            if (!self) return true;
            while (!self->ceof()) {
                Val key, value;
                if (!self->readwhile(key, until, ':')) {
                    return true;
                }
                if (!self->expect(":")) {
                    ctx.synerr = true;
                    return true;
                }
                self->expect(" ");
                if (!self->readwhile(value, untilincondition, [](char c) { return c != '\r' && c != '\n'; })) {
                    return true;
                }
                if (!self->expect("\r\n") && !self->expect("\n")) {
                    if (!self->ceof(1)) ctx.synerr = true;
                    return true;
                }
                ctx.emplace(key, value);
                if (self->expect("\r\n") || self->expect("\n")) {
                    ctx.succeed = true;
                    break;
                }
            }
            return true;
        }

        template <class Ctx, class Buf>
        bool httprequest(Reader<Buf>* self, Ctx& ctx, bool begin) {
            using Char = typename Reader<Buf>::char_type;
            static_assert(sizeof(Char) == 1);
            if (begin) return true;
            if (!self) return true;
            if (!self->readwhile(ctx.method, until, ' ')) return true;
            if (!self->expect(" ")) return true;
            if (!self->readwhile(ctx.path, until, ' ')) return true;
            if (!self->expect(" ")) return true;
            if (!self->expect("HTTP/1.0") && !self->expect("HTTP/1.1")) {
                return true;
            }
            ctx.version = ((self->offset(-3) - '0') * 10) + (self->offset(-1) - '0');
            if (!self->expect("\r\n") && !self->expect("\n")) return true;
            if (!self->readwhile(httpheader, ctx.header, true)) return true;
            ctx.succeed = ctx.header.succeed;
            return true;
        }

        template <class Ctx, class Buf>
        bool httpresponse(Reader<Buf>* self, Ctx& ctx, bool begin) {
            using Char = typename Reader<Buf>::char_type;
            static_assert(sizeof(Char) == 1);
            if (begin) return true;
            if (!self) return true;
            char code[3] = {0};
            if (!self->expect("HTTP/1.0") && !self->expect("HTTP/1.1")) {
                return true;
            }
            ctx.version = ((self->offset(-3) - '0') * 10) + (self->offset(-1) - '0');
            if (!self->expect(" ")) return true;
            self->read_byte(code, 3);
            if (!self->expect(" ")) return true;
            ctx.statuscode = (code[0] - '0') * 100 + (code[1] - '0') * 10 + code[2] - '0';
            if (!self->readwhile(ctx.reason, untilincondition, [](char c) { return c != '\r' && c != '\n'; })) return true;
            if (!self->expect("\r\n") && !self->expect("\n")) return true;
            if (!self->readwhile(httpheader, ctx.header, true)) return true;
            ctx.succeed = ctx.header.succeed;
            return true;
        }
        template <class Ret, class Ctx, class Buf>
        bool httpbody(Reader<Buf>* self, Ret& ret, Ctx& ctx, bool begin) {
            static_assert(sizeof(typename Reader<Buf>::char_type) == 1);
            if (!ctx) return false;
            if (!begin) return true;
            if (!self) return true;
            //auto& header=ctx->header;
            bool res = true;
            if (ctx->has_header("transfer-encoding", "chunked")) {
                size_t sum = 0;
                while (true) {
                    while (!is_hex(self->achar())) {
                        if (!ctx->recv(self->ref())) return false;
                    }
                    Buf num;
                    self->readwhile(num, until, '\r');
                    while (!self->expect("\r\n")) {
                        if (!ctx->recv(self->ref())) return false;
                    }
                    auto size = ctx->strtosz(num, res, 16);
                    if (!res) return false;
                    if (size == 0) break;
                    while (self->readable() < size) {
                        if (!ctx->recv(self->ref())) return false;
                    }
                    auto prevsum = sum;
                    sum += size;
                    ret.resize(sum);
                    memmove(&ret.data()[prevsum], &self->ref().data()[self->readpos()], size);
                    self->seek(self->readpos() + size);
                    while (!self->expect("\r\n")) {
                        if (!ctx->recv(self->ref())) return false;
                    }
                }
                ctx->succeed = true;
            }
            else if (ctx->has_header("content-length", nullptr)) {
                size_t size = ctx->strtosz(ctx->header_ref("content-length"), res);
                if (!res) return false;
                while (self->readable() < size) {
                    if (!ctx->recv(self->ref())) return false;
                }
                ret.resize(size);
                memmove(ret.data(), &self->ref().data()[self->readpos()], size);
                ctx->succeed = true;
            }
            return false;
        }
    }  // namespace old
}  // namespace PROJECT_NAME