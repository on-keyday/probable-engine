#pragma once

#include <array>
#include <vector>
#include <map>
#include <string>
#include <deque>

#include <serializer.h>

#include "http1.h"

namespace socklib {
    /*struct SimpleHeader {
        const char* header = "";
        const char* value = "";
        constexpr {const char* h, const char* v)
            : header(h), value(v) {)
    );*/
    using sheader_t = std::pair<const char*, const char*>;

    constexpr size_t predefined_header_size = 62;

    constexpr std::array<sheader_t, predefined_header_size> predefined_header = {
        sheader_t("INVALIDINDEX", "INVALIDINDEX"),
        sheader_t(":authority", ""),
        sheader_t(":method", "GET"),
        sheader_t(":method", "POST"),
        sheader_t(":path", "/"),
        sheader_t(":path", "/index.html"),
        sheader_t(":scheme", "http"),
        sheader_t(":scheme", "https"),
        sheader_t(":status", "200"),
        sheader_t(":status", "204"),
        sheader_t(":status", "206"),
        sheader_t(":status", "304"),
        sheader_t(":status", "400"),
        sheader_t(":status", "404"),
        sheader_t(":status", "500"),
        sheader_t("accept-charset", ""),
        sheader_t("accept-encoding", "gzip, deflate"),
        sheader_t("accept-language", ""),
        sheader_t("accept-ranges", ""),
        sheader_t("accept", ""),
        sheader_t("access-control-allow-origin", ""),
        sheader_t("age", ""),
        sheader_t("allow", ""),
        sheader_t("authorization", ""),
        sheader_t("cache-control", ""),
        sheader_t("content-disposition", ""),
        sheader_t("content-encoding", ""),
        sheader_t("content-language", ""),
        sheader_t("content-length", ""),
        sheader_t("content-location", ""),
        sheader_t("content-range", ""),
        sheader_t("content-type", ""),
        sheader_t("cookie", ""),
        sheader_t("date", ""),
        sheader_t("etag", ""),
        sheader_t("expect", ""),
        sheader_t("expires", ""),
        sheader_t("from", ""),
        sheader_t("host", ""),
        sheader_t("if-match", ""),
        sheader_t("if-modified-since", ""),
        sheader_t("if-none-match", ""),
        sheader_t("if-range", ""),
        sheader_t("if-unmodified-since", ""),
        sheader_t("last-modified", ""),
        sheader_t("link", ""),
        sheader_t("location", ""),
        sheader_t("max-forwards", ""),
        sheader_t("proxy-authenticate", ""),
        sheader_t("proxy-authorization", ""),
        sheader_t("range", ""),
        sheader_t("referer", ""),
        sheader_t("refresh", ""),
        sheader_t("retry-after", ""),
        sheader_t("server", ""),
        sheader_t("set-cookie", ""),
        sheader_t("strict-transport-security", ""),
        sheader_t("transfer-encoding", ""),
        sheader_t("user-agent", ""),
        sheader_t("vary", ""),
        sheader_t("via", ""),
        sheader_t("www-authenticate", "")};

    struct bitvec_t {
       private:
        std::uint32_t bits = 0;
        std::uint32_t size_ = 0;

        constexpr int shift(int i) const {
            return (sizeof(bits) * 8 - 1 - i);
        }

       public:
        constexpr bitvec_t() {}
        constexpr bitvec_t(std::initializer_list<int> init) noexcept {
            size_ = init.size();
            for (auto i = 0; i < size_; i++) {
                std::uint32_t bit = init.begin()[i] ? 1 : 0;
                bits |= bit << shift(i);
            }
        }

        constexpr bool operator[](size_t idx) const {
            if (idx >= size_) {
                throw "out of range";
            }
            return (bool)(bits & (1 << shift(idx)));
        }

        constexpr size_t size() const {
            return size_;
        }

        template <class T>
        constexpr bool operator==(T& in) const {
            if (size() != in.size()) return false;
            for (auto i = 0; i < in.size(); i++) {
                if ((*this)[i] != in[i]) {
                    return false;
                }
            }
            return true;
        }

        constexpr bool push_back(bool in) {
            if (size_ >= sizeof(bits) * 8) {
                throw "too large vector";
            }
            std::uint32_t bit = in ? 1 : 0;
            bits |= (bit << shift(size_));
            size_++;
            return true;
        }
    };

    constexpr std::array<const bitvec_t, 257> h2huffman{
        {{1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 0, 0, 0},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 0},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 1},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 0, 0},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 0, 1},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1, 0},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1, 1},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 0, 0},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 1, 0},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 0, 1},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 1, 0},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 1, 1},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 0, 0},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 0, 1},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 0},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 0},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 0},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 1},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 0},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1},
         {0, 1, 0, 1, 0, 0},
         {1, 1, 1, 1, 1, 1, 1, 0, 0, 0},
         {1, 1, 1, 1, 1, 1, 1, 0, 0, 1},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1},
         {0, 1, 0, 1, 0, 1},
         {1, 1, 1, 1, 1, 0, 0, 0},
         {1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0},
         {1, 1, 1, 1, 1, 1, 1, 0, 1, 0},
         {1, 1, 1, 1, 1, 1, 1, 0, 1, 1},
         {1, 1, 1, 1, 1, 0, 0, 1},
         {1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1},
         {1, 1, 1, 1, 1, 0, 1, 0},
         {0, 1, 0, 1, 1, 0},
         {0, 1, 0, 1, 1, 1},
         {0, 1, 1, 0, 0, 0},
         {0, 0, 0, 0, 0},
         {0, 0, 0, 0, 1},
         {0, 0, 0, 1, 0},
         {0, 1, 1, 0, 0, 1},
         {0, 1, 1, 0, 1, 0},
         {0, 1, 1, 0, 1, 1},
         {0, 1, 1, 1, 0, 0},
         {0, 1, 1, 1, 0, 1},
         {0, 1, 1, 1, 1, 0},
         {0, 1, 1, 1, 1, 1},
         {1, 0, 1, 1, 1, 0, 0},
         {1, 1, 1, 1, 1, 0, 1, 1},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0},
         {1, 0, 0, 0, 0, 0},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1},
         {1, 1, 1, 1, 1, 1, 1, 1, 0, 0},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0},
         {1, 0, 0, 0, 0, 1},
         {1, 0, 1, 1, 1, 0, 1},
         {1, 0, 1, 1, 1, 1, 0},
         {1, 0, 1, 1, 1, 1, 1},
         {1, 1, 0, 0, 0, 0, 0},
         {1, 1, 0, 0, 0, 0, 1},
         {1, 1, 0, 0, 0, 1, 0},
         {1, 1, 0, 0, 0, 1, 1},
         {1, 1, 0, 0, 1, 0, 0},
         {1, 1, 0, 0, 1, 0, 1},
         {1, 1, 0, 0, 1, 1, 0},
         {1, 1, 0, 0, 1, 1, 1},
         {1, 1, 0, 1, 0, 0, 0},
         {1, 1, 0, 1, 0, 0, 1},
         {1, 1, 0, 1, 0, 1, 0},
         {1, 1, 0, 1, 0, 1, 1},
         {1, 1, 0, 1, 1, 0, 0},
         {1, 1, 0, 1, 1, 0, 1},
         {1, 1, 0, 1, 1, 1, 0},
         {1, 1, 0, 1, 1, 1, 1},
         {1, 1, 1, 0, 0, 0, 0},
         {1, 1, 1, 0, 0, 0, 1},
         {1, 1, 1, 0, 0, 1, 0},
         {1, 1, 1, 1, 1, 1, 0},
         {1, 1, 1, 0, 0, 1, 1},
         {1, 1, 1, 1, 1, 1, 0},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0},
         {1, 0, 0, 0, 1, 0},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1},
         {0, 0, 0, 1, 1},
         {1, 0, 0, 0, 1, 1},
         {0, 0, 1, 0, 0},
         {1, 0, 0, 1, 0, 0},
         {0, 0, 1, 0, 1},
         {1, 0, 0, 1, 0, 1},
         {1, 0, 0, 1, 1, 0},
         {1, 0, 0, 1, 1, 1},
         {0, 0, 1, 1, 0},
         {1, 1, 1, 0, 1, 0, 0},
         {1, 1, 1, 0, 1, 0, 1},
         {1, 0, 1, 0, 0, 0},
         {1, 0, 1, 0, 0, 1},
         {1, 0, 1, 0, 1, 0},
         {0, 0, 1, 1, 1},
         {1, 0, 1, 0, 1, 1},
         {1, 1, 1, 0, 1, 1, 0},
         {1, 0, 1, 1, 0, 0},
         {0, 1, 0, 0, 0},
         {0, 1, 0, 0, 1},
         {1, 0, 1, 1, 0, 1},
         {1, 1, 1, 0, 1, 1, 1},
         {1, 1, 1, 1, 0, 0, 0},
         {1, 1, 1, 1, 0, 0, 1},
         {1, 1, 1, 1, 0, 1, 0},
         {1, 1, 1, 1, 0, 1, 1},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1, 0},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 0, 1, 0},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1, 1},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 0, 0},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 0, 1, 1},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 1, 0, 0},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 1, 0, 1},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 0, 0, 1},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 1, 1, 0},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 0, 1, 0},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 0, 1, 1},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 0, 0},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 0, 1},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 0},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 1, 1},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 0, 0},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 0, 1},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 1, 1, 1},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 0},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 0},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 1},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 0, 0},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 0, 0},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 0, 0, 0},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 0, 1},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 0, 0, 1},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1, 0},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1, 1},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 0, 1, 0},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 0, 1},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 0, 1},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 0, 1, 1},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 0, 0},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 0, 0},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 0, 1},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 0},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 1},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 0, 1},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 0},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 1, 1},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 0, 0},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 0},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 0, 1},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 0},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 1, 0},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 0},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 1},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 0, 0},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 0, 1},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1, 0},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 1, 1},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1, 1},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 0},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 0, 0},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 0, 0},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 0},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 1},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 0, 0},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 0},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 0, 1},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 0, 1},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 0},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 1},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1, 0},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1, 1},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 0},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 0},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 0, 0},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 0, 1},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 0, 0},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 0, 1},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 1},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 0, 0},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 0, 1},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 0, 0},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 0, 1},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1, 0},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 0, 1},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1, 1},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 0, 0},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 1, 0},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 1, 1},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 0},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 0},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 1},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 1, 0},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 0},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 1, 1},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1, 0},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 0, 0},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 0, 1},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1, 1},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 0, 0},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 0, 1},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 1, 0},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 1, 1},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 0, 0},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 0, 1},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 0},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 0},
         {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}}};

    struct bitvec_writer {
       private:
        std::string buf;
        size_t idx = 0;
        size_t pos = 0;

       public:
        bitvec_writer() {
            buf.push_back(0);
        }

        void push_back(bool b) {
            if (pos == 8) {
                buf.push_back(0);
                idx++;
                pos = 0;
            }
            unsigned char v = b ? 1 : 0;
            buf[idx] |= (v << (7 - pos));
            pos++;
        }

        template <class T>
        void append(const T& a) {
            for (auto i = 0; i < a.size(); i++) {
                push_back(a[i]);
            }
        }

        size_t size() const {
            return idx * 8 + pos;
        }

        std::string& data() {
            return buf;
        }
    };

    struct bitvec_reader {
       private:
        size_t pos = 0;
        size_t idx = 0;
        std::string& str;

       public:
        bitvec_reader(std::string& in)
            : str(in) {}

        bool get() const {
            if (idx == str.size()) return true;
            return (bool)(((unsigned char)str[idx]) & (1 << (7 - pos)));
        }

        bool incremant() {
            if (idx == str.size()) return false;
            pos++;
            if (pos == 8) {
                idx++;
                pos = 0;
            }
            return true;
        }

        bool decrement() {
            if (pos == 0) {
                if (idx == 0) {
                    return false;
                }
                idx--;
                pos = 8;
            }
            pos--;
            return true;
        }

        bool eos() const {
            return idx == str.size();
        }
    };

    struct h2huffman_tree {
       private:
        friend struct Hpack;
        //std::vector<bool> value;
        h2huffman_tree* zero = nullptr;
        h2huffman_tree* one = nullptr;
        unsigned char c = 0;
        bool has_c = false;
        bool eos = false;
        constexpr h2huffman_tree(/*const std::vector<bool>& v*/)
        /*: value(v)*/ {}
        ~h2huffman_tree() noexcept {
            delete zero;
            delete one;
        }

        static bool append(h2huffman_tree* tree, bool eos, unsigned char c, const bitvec_t& v, bitvec_t& res, size_t idx = 0) {
            if (!tree) {
                return false;
            }
            if (v == res) {
                tree->c = c;
                tree->has_c = true;
                tree->eos = eos;
                if (tree->zero || tree->one) {
                    throw "invalid hpack huffman";
                }
                return true;
            }
            res.push_back(v[idx]);
            if (v[idx]) {
                if (!tree->one) {
                    tree->one = new h2huffman_tree();
                }
                return append(tree->one, eos, c, v, res, idx + 1);
            }
            else {
                if (!tree->zero) {
                    tree->zero = new h2huffman_tree();
                }
                return append(tree->zero, eos, c, v, res, idx + 1);
            }
        }

        static h2huffman_tree* init_tree() {
            static h2huffman_tree _tree({});
            for (auto i = 0; i < 256; i++) {
                bitvec_t res;
                append(&_tree, false, i, h2huffman[i], res);
            }
            bitvec_t res;
            append(&_tree, true, 0, h2huffman[256], res);
            return &_tree;
        }

        static h2huffman_tree* tree() {
            static h2huffman_tree* ret = init_tree();
            return ret;
        }
    };

    enum class HpackError {
        none,
        too_large_number,
        too_short_number,
        internal,
        invalid_mask,
        invalid_value,
        not_exists,
    };

    using HpkErr = commonlib2::EnumWrap<HpackError, HpackError::none, HpackError::internal>;

    struct Hpack {
//Rust like try
#define TRY(...) \
    if (auto _H_tryres = (__VA_ARGS__); !_H_tryres) return _H_tryres

       private:
        static size_t
        gethuffmanlen(const std::string& str) {
            size_t ret = 0;
            for (auto& c : str) {
                ret += h2huffman[(unsigned char)c].size();
            }
            return (ret + 7) / 8;
        }

        static std::string huffman_encode(const std::string& in) {
            bitvec_writer vec;
            for (auto c : in) {
                vec.append(h2huffman[(unsigned char)c]);
            }
            //vec.append(h2huffman[256]);
            while (vec.size() % 8) {
                vec.push_back(true);
            }
            return vec.data();
        }

        static HpkErr huffman_decode_achar(unsigned char& c, bitvec_reader& r, h2huffman_tree* t, h2huffman_tree*& fin, std::uint32_t& allone) {
            for (;;) {
                if (!t) {
                    return HpackError::invalid_value;
                }
                if (t->has_c) {
                    c = t->c;
                    fin = t;
                    return true;
                }
                bool f = r.get();
                if (!r.incremant()) {
                    return HpackError::too_short_number;
                }
                h2huffman_tree* next = f ? t->one : t->zero;
                allone = (allone && t->one == next) ? allone + 1 : 0;
                t = next;
            }
            //return huffman_decode_achar(c, r, next, fin, allone);
        }

        static HpkErr huffman_decode(std::string& res, std::string& src) {
            bitvec_reader r(src);
            auto tree = h2huffman_tree::tree();
            while (!r.eos()) {
                unsigned char c = 0;
                h2huffman_tree* fin = nullptr;
                std::uint32_t allone = 1;
                auto tmp = huffman_decode_achar(c, r, tree, fin, allone);
                if (!tmp) {
                    if (tmp == HpackError::too_short_number) {
                        if (!r.eos() && allone - 1 > 7) {
                            return HpackError::too_large_number;
                        }
                        break;
                    }
                    return tmp;
                }
                if (fin->eos) {
                    return HpackError::invalid_value;
                }
                res.push_back(c);
            }
            return true;
        }

        template <std::uint32_t n>
        static HpkErr decode_integer(commonlib2::Deserializer<std::string&>& se, size_t& sz, unsigned char& firstmask) {
            static_assert(n > 0 && n <= 8, "invalid range");
            constexpr unsigned char msk = static_cast<std::uint8_t>(~0) >> (8 - n);
            unsigned char tmp = 0;

            TRY(se.template read_as<std::uint8_t>(tmp));
            firstmask = tmp & ~msk;
            tmp &= msk;
            sz = tmp;
            if (tmp < msk) {
                return true;
            }
            size_t m = 0;
            constexpr auto pow = [](size_t x) -> size_t {
                return commonlib2::msb_on<size_t>() >> ((sizeof(size_t) * 8 - 1) - x);
            };
            do {
                TRY(se.template read_as<std::uint8_t>(tmp));
                sz += (tmp & 0x7f) * pow(m);
                m += 7;
                if (m > (sizeof(size_t) * 8 - 1)) {
                    return HpackError::too_large_number;
                }
            } while (tmp & 0x80);
            return true;
        }

        template <std::uint32_t n>
        static HpkErr encode_integer(commonlib2::Serializer<std::string&> se, size_t sz, unsigned char firstmask) {
            static_assert(n > 0 && n <= 8, "invalid range");
            constexpr unsigned char msk = static_cast<std::uint8_t>(~0) >> (8 - n);
            if (firstmask & msk) {
                return HpackError::invalid_mask;
            }
            if (sz < (size_t)msk) {
                se.template write_as<std::uint8_t>(firstmask | sz);
            }
            else {
                se.template write_as<std::uint8_t>(firstmask | msk);
                sz -= msk;
                while (sz > 0x7f) {
                    se.template write_as<std::uint8_t>((sz % 0x80) | 0x80);
                    sz /= 0x80;
                }
                se.template write_as<std::uint8_t>(sz);
            }
            return true;
        }

       public:
        static void encode_str(commonlib2::Serializer<std::string&>& se, const std::string& value) {
            if (value.size() > gethuffmanlen(value)) {
                std::string enc = huffman_encode(value);
                encode_integer<7>(se, enc.size(), 0x80);
                se.write_byte(enc);
            }
            else {
                encode_integer<7>(se, value.size(), 0);
                se.write_byte(value);
            }
        }

        static HpkErr decode_str(std::string& str, commonlib2::Deserializer<std::string&>& se) {
            size_t sz = 0;
            unsigned char mask = 0;
            TRY(decode_integer<7>(se, sz, mask));
            TRY(se.read_byte(str, sz));
            if (mask & 0x80) {
                std::string decoded;
                TRY(huffman_decode(decoded, str));
                str = std::move(decoded);
            }
            return true;
        }

        using DynamicTable = std::deque<std::pair<std::string, std::string>>;

       private:
        template <class F>
        static bool get_idx(F&& f, size_t& idx, DynamicTable& dymap) {
            if (auto found = std::find_if(predefined_header.begin() + 1, predefined_header.end(), [&](auto& c) {
                    return f(c.first, c.second);
                });
                found != predefined_header.end()) {
                idx = std::distance(predefined_header.begin(), found);
            }
            else if (auto found = std::find_if(dymap.begin(), dymap.end(), [&](auto& c) {
                         return f(c.first, c.second);
                     });
                     found != dymap.end()) {
                idx = std::distance(dymap.begin(), found) + predefined_header_size;
            }
            else {
                return false;
            }
            return true;
        }

        static size_t calc_table_size(DynamicTable& table) {
            size_t size = 32;
            for (auto& e : table) {
                size += e.first.size();
                size += e.second.size();
            }
            return size;
        }

       public:
        template <bool adddy = false>
        static HpkErr encode(HttpConn::Header& src, std::string& res,
                             DynamicTable& dymap, std::uint32_t maxtablesize) {
            commonlib2::Serializer<std::string&> se(res);
            for (auto& h : src) {
                size_t idx = 0;
                if (get_idx(
                        [&](const auto& k, const auto& v) {
                            return k == h.first && v == h.second;
                        },
                        idx, dymap)) {
                    TRY(encode_integer<7>(se, idx, 0x80));
                }
                else {
                    if (get_idx(
                            [&](const auto& k, const auto&) {
                                return k == h.first;
                            },
                            idx, dymap)) {
                        if (adddy) {
                            TRY(encode_integer<6>(se, idx, 0x40));
                        }
                        else {
                            TRY(encode_integer<4>(se, idx, 0));
                        }
                    }
                    else {
                        se.template write_as<std::uint8_t>(0);
                        encode_str(se, h.first);
                    }
                    encode_str(se, h.second);
                    if (adddy) {
                        dymap.push_front({h.first, h.second});
                        size_t tablesize = calc_table_size(dymap);
                        while (tablesize > maxtablesize) {
                            if (!dymap.size()) return false;
                            tablesize -= dymap.back().first.size();
                            tablesize -= dymap.back().second.size();
                            dymap.pop_back();
                        }
                    }
                }
            }
            return true;
        }

        static HpkErr decode(HttpConn::Header& res, std::string& src,
                             DynamicTable& dymap, std::uint32_t& maxtablesize) {
            auto update_dymap = [&] {
                size_t tablesize = calc_table_size(dymap);
                while (tablesize > maxtablesize) {
                    if (tablesize == 32) break;  //TODO: check what written in RFC means
                    if (!dymap.size()) {
                        return false;
                    }
                    tablesize -= dymap.back().first.size();
                    tablesize -= dymap.back().second.size();
                    dymap.pop_back();
                }
                return true;
            };
            commonlib2::Deserializer<std::string&> se(src);
            while (!se.base_reader().ceof()) {
                unsigned char tmp = se.base_reader().achar();
                std::string key, value;
                auto read_two_literal = [&]() -> HpkErr {
                    TRY(decode_str(key, se));
                    TRY(decode_str(value, se));
                    res.emplace(key, value);
                    return true;
                };
                auto read_idx_and_literal = [&](size_t idx) -> HpkErr {
                    if (idx < predefined_header_size) {
                        key = predefined_header[idx].first;
                    }
                    else {
                        if (dymap.size() <= idx - predefined_header_size) {
                            return HpackError::not_exists;
                        }
                        key = dymap[idx - predefined_header_size].first;
                    }
                    TRY(decode_str(value, se));
                    res.emplace(key, value);
                    return true;
                };
                if (tmp & 0x80) {
                    size_t idx = 0;
                    TRY(decode_integer<7>(se, idx, tmp));
                    if (idx == 0) {
                        return HpackError::invalid_value;
                    }
                    if (idx < predefined_header_size) {
                        if (!predefined_header[idx].second[0]) {
                            return HpackError::not_exists;
                        }
                        res.emplace(predefined_header[idx].first, predefined_header[idx].second);
                    }
                    else {
                        if (dymap.size() <= idx - predefined_header_size) {
                            return HpackError::not_exists;
                        }
                        res.emplace(dymap[idx - predefined_header_size].first, dymap[idx - predefined_header_size].second);
                    }
                }
                else if (tmp & 0x40) {
                    size_t sz = 0;

                    TRY(decode_integer<6>(se, sz, tmp));

                    if (sz == 0) {
                        TRY(read_two_literal());
                    }
                    else {
                        TRY(read_idx_and_literal(sz));
                    }
                    dymap.push_front({key, value});
                    TRY(update_dymap());
                }
                else if (tmp & 0x20) {  //dynamic table size change
                    //unimplemented
                    size_t sz = 0;
                    TRY(decode_integer<5>(se, sz, tmp));
                    if (maxtablesize > 0x80000000) {
                        return HpackError::too_large_number;
                    }
                    maxtablesize = (std::int32_t)sz;
                    TRY(update_dymap());
                }
                else {
                    size_t sz = 0;
                    TRY(decode_integer<4>(se, sz, tmp));
                    if (sz == 0) {
                        TRY(read_two_literal());
                    }
                    else {
                        TRY(read_idx_and_literal(sz));
                    }
                }
                /*else {
                    return false;
                }*/
            }
            return true;
        }
#undef TRY
    };
}  // namespace socklib
