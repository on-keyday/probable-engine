
#include <iostream>

#include "protocol.h"

#include <reader.h>
#include <net_helper.h>

using namespace commonlib2;

int main(int, char**) {
    auto conn = socklib::Http::open("https://httpbin.org/post", false, "D:/CommonLib/netsoft/cacert.pem");
    if (!conn) {
        std::cout << "connection failed\n";
        return -1;
    }
    const char payload[] = "Hello World";
    conn->send("POST", {{"content-type", "text/plain"}}, payload, sizeof(payload) - 1);

    conn->recv_async([](auto& conn, bool result) {
        std::cout << conn.response().find(":body")->second;
    });

    while (conn->wait()) Sleep(5);
}