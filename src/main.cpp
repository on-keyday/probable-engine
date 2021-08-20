
#include <iostream>

#include "http.h"

#include <reader.h>
#include <net_helper.h>

//#include <coroutine>
#include <mutex>

#include <deque>

#include <filesystem>
#include <fileio.h>

#ifdef _WIN32
#include <direct.h>
#endif

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
                conn->recv(httprecv, false, cacert, callback);
                return;
            }
        }
    }
    callback(conn);
}

void client_test() {
    auto cacert = "D:/CommonLib/netsoft/cacert.pem";
    auto conn = socklib::Http::open("gmail.com", false, cacert);
    if (!conn) {
        std::cout << "connection failed\n";
        std::cout << "last error:" << WSAGetLastError();
        return;
    }
    const char payload[] = "Hello World";
    conn->send("GET");

    conn->recv_async(httprecv, false, cacert, [](auto& conn) {
        std::cout << conn->response().find(":body")->second;
    });

    while (conn->wait()) {
        Sleep(5);
    }
    conn->close();
}

std::mutex mut;

std::deque<std::shared_ptr<socklib::HttpServerConn>> que;

void parse_proc(std::shared_ptr<socklib::HttpServerConn>& conn, const std::thread::id& id, auto& print_time, bool& keep_alive) {
    auto rec = std::chrono::system_clock::now();
    auto path = conn->path();
    unsigned short status = 0;
    auto meth = conn->request().find(":method")->second;
    const char* conh = "close";
    if (auto found = conn->request().find("connection"); found != conn->request().end()) {
        if (found->second.find("Keep-Alive")) {
            keep_alive = true;
            conh = "Keep-Alive";
        }
    }
    socklib::HttpConn::Header h = {{"Connection", conh}};
    if (keep_alive) {
        h.emplace("Keep-Alive", "timeout=5, max=1000");
    }
    URLEncodingContext<std::string> ctx;

    if (meth != "GET" && meth != "HEAD") {
        const char c[] = "405 method not allowed";
        conn->send(405, "Method Not Allowed", h, c, sizeof(c) - 1);
        status = 405;
        keep_alive = false;
    }
    if (!status) {
        std::string after;
        Reader(path).readwhile(after, url_decode, &ctx);
        if (ctx.failed) {
            after = path;
        }
        std::filesystem::path pt = "." + after;
        pt = pt.lexically_normal();
        if (pt != ".") {
            auto send404 = [&] {
                const char c[] = "404 not found";
                conn->send(404, "Not Found", h, c, sizeof(c) - 1);
                status = 404;
            };
            if (std::filesystem::exists(pt)) {
                Reader<FileReader> fs(pt.c_str());
                if (!fs.ref().is_open()) {
                    send404();
                }
                else {
                    std::string buf;
                    fs >> do_nothing<char> >> buf;
                    if (pt.extension() == ".json") {
                        h.emplace("content-type", "application/json");
                    }
                    else if (pt.extension() == ".html") {
                        h.emplace("content-type", "text/html");
                    }
                    conn->send(200, "OK", h, buf.data(), buf.size());
                    status = 200;
                }
            }
            else {
                send404();
            }
        }
    }
    if (!status) {
        h.emplace("content-type", "text/plain");
        if (meth == "GET") {
            conn->send(200, "OK", h, "It Works!", 9);
        }
        else {
            conn->send(200, "OK", h);
        }
        status = 200;
    }
    auto end = std::chrono::system_clock::now();
    std::cout << "thread-" << id << "-" << std::this_thread::get_id();
    std::cout << "|" << conn->ipaddress() << "|";

    std::cout << meth << "|";
    std::cout << status << "|";
    print_time(rec);
    std::cout << "|";
    print_time(end);
    std::cout << "|\"";
    std::cout << conn->url() << "\"|\n";
}

int main(int, char**) {
    auto maxth = std::thread::hardware_concurrency();
    if (maxth == 0) {
        maxth = 4;
    }
    bool proc_end = false;
    auto process = [&]() {
        auto id = std::this_thread::get_id();
        size_t got = 0;
        while (!proc_end) {
            std::shared_ptr<socklib::HttpServerConn> conn;
            if (mut.try_lock()) {
                if (que.size()) {
                    conn = std::move(que.front());
                    que.pop_front();
                }
                mut.unlock();
            }
            if (!conn) {
                Sleep(1);
                continue;
            }
            got++;
            auto begin = std::chrono::system_clock::now();
            auto print_time = [&](auto end) {
                std::cout << std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count() << "us";
            };
            if (!conn->recv()) {
                return;
            }
            bool keep_alive = false;
            parse_proc(conn, id, print_time, keep_alive);
            if (keep_alive) {
                try {
                    std::thread(
                        [](std::shared_ptr<socklib::HttpServerConn> conn, std::thread::id id) {
                            while (socklib::Selecter::wait(conn->borrow(), 5)) {
                                auto begin = std::chrono::system_clock::now();
                                if (!conn->recv()) {
                                    std::cout << "thread-" << id << "-" << std::this_thread::get_id();
                                    std::cout << ":keep-alive end\n";
                                    return;
                                }
                                auto print_time = [&](auto end) {
                                    std::cout << std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count() << "us";
                                };
                                bool keep_alive = false;
                                parse_proc(conn, id, print_time, keep_alive);
                                if (keep_alive) {
                                    continue;
                                }
                            }
                            std::cout << "thread-" << id << "-" << std::this_thread::get_id();
                            std::cout << ":keep-alive end\n";
                            conn->close();
                        },
                        std::move(conn), id)
                        .detach();
                } catch (...) {
                    std::cout << "start keep-alive thread failed\n";
                }
            }
        }
        std::cout << "thread-" << id << ":" << got << "\n";
    };
    uint32_t i = 0;
    for (i = 0; i < maxth - 1; i++) {
        try {
            std::thread(process)
                .detach();
        } catch (...) {
            std::cout << "thread make suspended\n";
            break;
        }
    }

    std::cout << "thread count:" << i << "\n";
    socklib::Server sv;
    std::thread(
        [&] {
            while (!proc_end) {
                std::string input;
                std::getline(std::cin, input);
                if (input == "exit") {
                    proc_end = true;
                    sv.set_suspend(true);
                }
                else if (auto sp = split_cmd(input); sp.size() >= 2 && sp[0] == "cd") {
                    std::string dir = sp[1];
                    if (dir[0] == '"') {
                        dir.erase(0, 1);
                        dir.pop_back();
                    }
                    if (_chdir(dir.c_str()) != 0) {
                        std::cout << "cd:change cd failed\n";
                    }
                    else {
                        std::cout << "cd:changed\n";
                    }
                }
            }
        })
        .detach();

    while (!proc_end) {
        auto res = socklib::Http::serve(sv, 8090);
        if (sv.suspended()) {
            if (res) res->close();
            Sleep(1000);
            break;
        }
        if (!res) {
            std::cout << "error occured\n";
            proc_end = true;
            Sleep(1000);
            break;
        }
        mut.lock();
        que.push_back(std::move(res));
        mut.unlock();
    }
}
