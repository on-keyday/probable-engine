
#include <iostream>

#include "protocol.h"

#include <reader.h>
#include <net_helper.h>

using namespace commonlib2;

auto httprecv(std::shared_ptr<socklib::HttpClientConn>& conn, bool res) {
    unsigned short statuscode = 0;
    Reader<std::string>(conn->response().find(":status")->second) >> statuscode;
    std::cout << statuscode << "\n";
    std::cout << conn->response().find(":body")->second;
    if (statuscode >= 301 && statuscode <= 308) {
        if (auto found = conn->response().find("location"); found != conn->response().end()) {
            auto tmp = socklib::Http::open(found->second.c_str(), true, "D:/CommonLib/netsoft/cacert.pem");
            if (!tmp) return;
            conn->close();
            conn = tmp;
            conn->send("GET");
            conn->recv(httprecv);
        }
    }
}

int main(int, char**) {
    auto conn = socklib::Http::open("https://gmail.com", false, "D:/CommonLib/netsoft/cacert.pem");
    if (!conn) {
        std::cout << "connection failed\n";
        return -1;
    }
    const char payload[] = "Hello World";
    conn->send("GET");

    conn->recv_async(httprecv);

    while (conn->wait()) Sleep(5);
}