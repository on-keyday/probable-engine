#pragma once

#include <array>
#include <vector>
#include <map>
#include <string>

#include <serializer.h>

namespace socklib {
    /*struct SimpleHeader {
        const char* header = "";
        const char* value = "";
        constexpr {const char* h, const char* v)
            : header(h), value(v) {)
    );*/
    using sheader_t = std::pair<const char*, const char*>;

    constexpr std::array<sheader_t, 62> predefined = {
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
    std::array<std::vector<bool>, 257> h2huffman{
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

        void append(const std::vector<bool>& a) {
            for (bool b : a) {
                push_back(b);
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
        size_t pos;
        size_t idx;
        std::string& str;

       public:
        bitvec_reader(std::string& in)
            : str(in) {}

        bool get() const {
            return (bool)(((unsigned char)str[idx]) & (1 << (7 - pos)));
        }

        bool incremant() {
            pos++;
            if (pos == 8) {
                if (str.size() == idx + 1) {
                    pos = 7;
                    return false;
                }
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
    };

    struct h2huffman_tree {
       private:
        friend struct Hpack;
        std::vector<bool> value;
        h2huffman_tree* zero = nullptr;
        h2huffman_tree* one = nullptr;
        unsigned char c = 0;
        bool has_c = false;
        bool eos = false;
        h2huffman_tree(const std::vector<bool>& v)
            : value(v) {}
        ~h2huffman_tree() noexcept {
            delete zero;
            delete one;
        }

        static bool append(h2huffman_tree* tree, bool eos, unsigned char c, std::vector<bool>& v, std::vector<bool>& res, size_t idx = 0) {
            if (!tree) {
                return false;
            }
            if (v == res) {
                tree->c = c;
                tree->has_c = true;
                tree->eos = true;
                if (tree->zero || tree->one) {
                    throw "invalid hpack huffman";
                }
                return true;
            }
            res.push_back(v[idx]);
            if (v[idx]) {
                if (!tree->one) {
                    tree->one = new h2huffman_tree(res);
                }
                return append(tree->one, eos, c, v, res, idx + 1);
            }
            else {
                if (!tree->zero) {
                    tree->zero = new h2huffman_tree(res);
                }
                return append(tree->zero, eos, c, v, res, idx + 1);
            }
        }

        static h2huffman_tree* init_tree() {
            static h2huffman_tree _tree({});
            for (auto i = 0; i < 256; i++) {
                std::vector<bool> res;
                append(&_tree, false, i, h2huffman[i], res);
            }
            std::vector<bool> res;
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
    };

    using HpkErr = commonlib2::EnumWrap<HpackError, HpackError::none, HpackError::internal>;

    struct Hpack {
#define TRY(...) \
    if (auto _H_tryres = (__VA_ARGS__); !_H_tryres) return _H_tryres

       private:
        static size_t gethuffmanlen(const std::string& str) {
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
            vec.append(h2huffman[256]);
            return vec.data();
        }

        static HpkErr huffman_decode_achar(unsigned char& c, bitvec_reader& r, h2huffman_tree* t, h2huffman_tree*& fin) {
            if (!t) return HpackError::invalid_value;
            if (t->has_c) {
                c = t->c;
                fin = t;
                return true;
            }
            h2huffman_tree* next = r.get() ? t->one : t->zero;
            if (!r.incremant()) {
                return HpackError::too_short_number;
            }
            return huffman_decode_achar(c, r, next, fin);
        }

        static HpkErr huffman_decode(std::string& res, std::string& src) {
            bitvec_reader r(src);
            auto tree = h2huffman_tree::tree();
            while (true) {
                unsigned char c = 0;
                h2huffman_tree* fin = nullptr;
                TRY(huffman_decode_achar(c, r, tree, fin));
                if (fin->eos) {
                    break;
                }
                res.push_back(c);
            }
            return true;
        }

        template <unsigned int n>
        static HpkErr decode_integer(commonlib2::Deserializer<std::string&>& se, size_t& sz, unsigned char& firstmask) {
            static_assert(n > 0 && n <= 8, "invalid range");
            constexpr unsigned char msk = static_cast<unsigned char>(~0) >> (8 - n);
            unsigned char tmp = 0;

            TRY(se.template read_as<unsigned char>(tmp));
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
                TRY(se.template read_as<unsigned char>(tmp));
                sz += (tmp & 0x7f) * pow(m);
                m += 7;
                if (m > (sizeof(size_t) * 8 - 1)) {
                    return HpackError::too_large_number;
                }
            } while (tmp & 0x80);
            return true;
        }

        template <unsigned int n>
        static HpkErr encode_integer(commonlib2::Serializer<std::string&> se, size_t sz, unsigned char firstmask) {
            static_assert(n > 0 && n <= 8, "invalid range");
            constexpr unsigned char msk = static_cast<unsigned char>(~0) >> (8 - n);
            if (firstmask & msk) {
                return HpackError::invalid_mask;
            }
            if (sz < (size_t)msk) {
                se.template write_as<unsigned char>(firstmask | sz);
            }
            else {
                se.template write_as<unsigned char>(firstmask | msk);
                sz -= msk;
                while (sz > 0x7f) {
                    se.template write_as<unsigned char>((sz % 0x80) | 0x80);
                    sz /= 0x80;
                }
                se.template write_as<unsigned char>(sz);
            }
            return true;
        }

       public:
        static void encode_str(std::string& str, const std::string& value) {
            commonlib2::Serializer<std::string&> se(str);
            if (value.size() > gethuffmanlen(value)) {
                std::string enc = huffman_encode(value);
                encode_integer<7>(se, value.size(), 0x80);
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

        static HpkErr decode(std::multimap<std::string, std::string>& res, std::string& src) {
            commonlib2::Deserializer<std::string&> se(src);
            unsigned char tmp = se.base_reader().achar();
            if (tmp & 0x80) {
            }
            else if ((tmp & 0xf0) == 0) {
                size_t sz = 0;
                TRY(decode_integer<4>(se, sz, tmp));
                if (sz == 0) {
                    std::string key, value;
                    TRY(decode_str(key, se));
                }
                else {
                }
            }
        }
#undef TRY
    };
}  // namespace socklib