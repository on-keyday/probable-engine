#pragma once

#include <string>
#include <map>
#include <deque>

#include "http.h"
#include "websocket.h"

namespace socklib {
    namespace v2 {
        using String = std::string;
        using HttpHeader = std::multimap<String, String>;
        using HpackTable = std::deque<std::pair<String, String>>;

        template <class... Arg>
        using Http2MapType = std::map<Arg...>;

        std::shared_ptr<ClientRequestProxy<String, HttpHeader, String, Http2MapType, HpackTable>> make_request() {
            return make_request<String, HttpHeader, String, Http2MapType, HpackTable>();
        }

        std::shared_ptr<ServerRequestProxy<String, HttpHeader, String, Http2MapType, HpackTable>> accept_request(std::shared_ptr<InetConn>&& conn) {
            return accept_request<String, HttpHeader, String, Http2MapType, HpackTable>(std::move(conn));
        }

        using DefWebSocket = WebSocket<String, HttpHeader>;
        using DefWebSocketConn = WebSocketConn<String>;
        using DefWebSocketRequestContext = WebSocektRequestContext<String, HttpHeader>;
    }  // namespace v2
}  // namespace socklib
