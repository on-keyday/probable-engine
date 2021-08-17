
#include <iostream>

#include "protocol.h"

#include <reader.h>
#include <net_helper.h>

using namespace commonlib2;

auto httprecv(std::shared_ptr<socklib::HttpClientConn>& conn, bool res, const char* cacert) {
    unsigned short statuscode = 0;
    Reader<std::string>(conn->response().find(":status")->second) >> statuscode;
    std::cout << statuscode << "\n";
    std::cout << conn->response().find(":body")->second;
    if (statuscode >= 301 && statuscode <= 308) {
        if (auto found = conn->response().find("location"); found != conn->response().end()) {
            auto tmp = socklib::Http::open(found->second.c_str(), true, cacert);
            if (!tmp) return;
            conn->close();
            tmp->send(conn->method().c_str());
            tmp->recv(httprecv, cacert);
            return;
        }
    }
}

int main(int, char**) {
    auto cacert = "D:/CommonLib/netsoft/cacert.pem";
    auto conn = socklib::Http::open("https://gmail.com", false, cacert);
    if (!conn) {
        std::cout << "connection failed\n";
        std::cout << "last error:" << WSAGetLastError();
        return -1;
    }
    const char payload[] = "Hello World";
    conn->send("GET");

    conn->recv_async(httprecv, cacert);

    while (conn->wait()) Sleep(5);
}