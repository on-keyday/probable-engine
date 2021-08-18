
#include <iostream>

#include "http.h"

#include <reader.h>
#include <net_helper.h>

#include <coroutine>
/*
template <class T>
struct task {
   private:
    struct promise_type {
        T value;

        auto get_return_object() {
            return task{*this};
        };

        auto initial_suspend() {
            return std::suspend_always{};
        }
        auto final_suspend() {
            return std::suspend_always{};
        }
        auto yield_value(T v) {
            value = v;
            return std::suspend_always{};
        }
        void return_void() {}
        void unhandled_exception() {
            std::terminate();
        }
    };
    using coro_handle = std::coroutine_handle<promise_type>;
    coro_handle co;
    task(promise_type& in)
        : co(coro_handle::from_promise(co)) {}

    task(task&& rhs)
        : coro_(std::exchange(rhs.co, nullptr)) {}

    ~task() {
        if (co) co.destroy();
    }
};
*/
using namespace commonlib2;

void httprecv(std::shared_ptr<socklib::HttpClientConn>& conn, bool res, const char* cacert, void (*callback)(decltype(conn))) {
    if (!res) {
        std::cout << "failed to recv\n";
        return;
    }
    unsigned short statuscode = 0;
    Reader<std::string>(conn->response().find(":status")->second) >> statuscode;
    std::cout << conn->url() << "\n";
    std::cout << conn->ipaddress() << "\n";
    std::cout << statuscode << " " << conn->response().find(":phrase")->second << "\n";
    if (statuscode >= 301 && statuscode <= 308) {
        if (auto found = conn->response().find("location"); found != conn->response().end()) {
            if (socklib::Http::reopen(conn, found->second.c_str(), true, cacert)) {
                std::cout << "redirect\n";
                conn->send(conn->method().c_str());
                conn->recv(httprecv, cacert, callback);
                return;
            }
        }
    }
    callback(conn);
}

int main(int, char**) {
    auto cacert = "D:/CommonLib/netsoft/cacert.pem";
    auto conn = socklib::Http::open("gmail.com", false, cacert);
    if (!conn) {
        std::cout << "connection failed\n";
        std::cout << "last error:" << WSAGetLastError();
        return -1;
    }
    const char payload[] = "Hello World";
    conn->send("GET");

    conn->recv_async(httprecv, cacert, [](auto& conn) {
        std::cout << conn->response().find(":body")->second;
    });

    while (conn->wait()) {
        Sleep(5);
    }
    conn->close();

    socklib::Server sv;

    auto test = socklib::TCP::serve(sv, 8090);

    std::string red;

    test->read(red);

    test->write("HTTP/1.1 200 OK\r\nConteng-Length: 10\r\n\r\nYes we can");

    test->close();

    test = socklib::TCP::serve(sv, 8090);

    test->read(red);

    test->write("HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n");
}