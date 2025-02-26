
#include <iostream>

#include "v1/application/http1.h"
#include "v1/application/websocket.h"

#include <reader.h>
#include <net_helper.h>

//#include <coroutine>
#include <mutex>

#include <deque>

#include <filesystem>
#include <fileio.h>

#ifdef _WIN32
#include <direct.h>
#else
#define _chdir chdir
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

auto cacert = "D:/CommonLib/netsoft/cacert.pem";

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
            socklib::HttpOpenContext ctx;
            ctx.url = found->second.c_str();
            ctx.cacert = cacert;
            ctx.urlencoded = true;
            if (socklib::Http1::reopen(conn, ctx)) {
                std::cout << "redirect\n";
                conn->send(conn->method().c_str());
                conn->recv(httprecv, false, cacert, callback);
                return;
            }
        }
    }
    callback(conn);
}

void client_test(const char* url) {
    socklib::HttpOpenContext ctx;
    ctx.url = url;
    ctx.cacert = cacert;
    auto conn = socklib::Http1::open(ctx);
    if (!conn) {
        std::cout << "connection failed\n";
#ifdef _WIN32
        std::cout << "last error:" << WSAGetLastError() << "\n";
#else
        std::cout << "last error:" << errno << "\n";
#endif
        return;
    }
    conn->send("GET");

    conn->recv(httprecv, false, cacert, [](auto& conn) {
        std::cout << conn->response().find(":body")->second << "\n";
    });

    while (conn->wait()) {
        Sleep(5);
    }
    conn->close();
}

std::mutex mut;

std::deque<std::shared_ptr<socklib::HttpServerConn>> que;

void print_log(std::shared_ptr<socklib::HttpServerConn>& conn, auto& method, unsigned short status,
               auto& print_time, const auto& id, const auto& subid, auto& recvtime) {
    auto end = std::chrono::system_clock::now();
    std::cout << "thread-" << id << "-" << subid;
    std::cout << "|" << conn->ipaddress() << "|";

    std::cout << method << "|";
    std::cout << status << "|";
    print_time(recvtime);
    std::cout << "|";
    print_time(end);
    std::cout << "|\"";
    std::cout << conn->url() << "\"|\n";
}

bool websocket_accept(std::string& method, auto& print_time, auto& recvtime, auto& id, std::shared_ptr<socklib::HttpServerConn>& conn) {
    auto c = socklib::WebSocket::default_hijack_server_proc(conn, [&](auto&, auto&, auto&) {
        print_log(conn, method, 101, print_time, id, std::this_thread::get_id(), recvtime);
        return true;
    });
    if (!c) {
        return false;
    }
    std::thread(
        [](std::shared_ptr<socklib::WebSocketServerConn> conn) {
            while (conn->recvable() || socklib::Selecter::waitone(conn->borrow(), 10)) {
                socklib::WsFrame f;
                if (!conn->recv(f)) {
                    std::cout << "recv failed\n";
                    conn->close();
                    return false;
                }
                std::cout << f.frame_type() << "\n";
                std::cout << f.get_data() << "\n";
                conn->send("Nice to meet you!", 17);
            }
            std::cout << "web socket end\n";
            return true;
        },
        std::move(c))
        .detach();
    return true;
}

void parse_proc(std::shared_ptr<socklib::HttpServerConn>& conn, const std::thread::id& id, auto& print_time, bool& keep_alive, bool& websocket) {
    auto rec = std::chrono::system_clock::now();
    auto path = conn->path();
    unsigned short status = 0;
    auto meth = conn->request().find(":method")->second;
    const char* conh = "Keep-Alive";
    keep_alive = true;
    if (auto found = conn->request().find("connection"); found != conn->request().end()) {
        if (found->second.find("close") != ~0) {
            keep_alive = false;
            conh = "close";
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
        if (pt == "ws") {
            websocket = websocket_accept(meth, print_time, rec, id, conn);
            return;
        }
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
    print_log(conn, meth, status, print_time, id, std::this_thread::get_id(), rec);
}

void websocket_client(const char* url) {
    socklib::HttpOpenContext ctx;
    ctx.url = url;
    ctx.cacert = cacert;
    auto conn = socklib::WebSocket::open(ctx, [](auto& h, bool, const char* event) {
        return true;
    });
    if (!conn) {
        std::cout << "websocket:open failed\n";
        return;
    }
    conn->send("hello", 5);
    socklib::WsFrame frame;
    while (conn->recv(frame)) {
        if (frame.is_close()) {
            std::cout << "webscoket client end\n";
            break;
        }
        std::cout << "websocket:" << frame.get_data() << "\n";
    }
}

void server_proc() {
    auto maxth = std::thread::hardware_concurrency();
    if (maxth == 0) {
        maxth = 4;
    }
    bool proc_end = false;
    auto process = [&]() {
        auto id = std::this_thread::get_id();
        size_t got = 0;
        while (true) {
            try {
                std::shared_ptr<socklib::HttpServerConn> conn;
                if (mut.try_lock()) {
                    if (proc_end) {
                        std::cout << "thread-" << id << ":" << got << "\n";
                        mut.unlock();
                        return;
                    }
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
                    got--;
                    std::cout << "thread-" << id << " recv failed\n";
                    continue;
                }
                bool keep_alive = false, websocket = false;
                parse_proc(conn, id, print_time, keep_alive, websocket);
                if (keep_alive) {
                    try {
                        std::thread(
                            [](std::shared_ptr<socklib::HttpServerConn> conn, std::thread::id id) {
                                while (socklib::Selecter::waitone(conn->borrow(), 5)) {
                                    auto begin = std::chrono::system_clock::now();
                                    if (!conn->recv()) {
                                        std::cout << "thread-" << id << "-" << std::this_thread::get_id();
                                        std::cout << ":keep-alive end\n";
                                        return;
                                    }
                                    auto print_time = [&](auto end) {
                                        std::cout << std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count() << "us";
                                    };
                                    bool keep_alive = false, websocket = false;
                                    parse_proc(conn, id, print_time, keep_alive, websocket);
                                    if (websocket) {
                                        return;
                                    }
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

            } catch (std::exception& e) {
                std::cout << "thread-" << id << ":exception thrown:" << e.what() << "\n";
            } catch (...) {
                std::cout << "thread-" << id << ":exception thrown\n";
            }
        }
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

    std::cout << "thread count:" << i << "\naccessable ipaddress\n";
    socklib::Server sv;
    std::cout << sv.ipaddress_list() << "\n";
    std::thread([&] {
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
            else if (sp.size() >= 2 && sp[0] == "client") {
                client_test(sp[1].c_str());
            }
            else if (sp.size() >= 2 && sp[0] == "websocket") {
                websocket_client(sp[1].c_str());
            }
            else if (sp.size() >= 1 && sp[0] == "clear") {
#ifdef _WIN32
                ::system("cls");
#else
                ::system("clear");
#endif
            }
            else {
                std::cout << "no such command\n";
            }
        }
    }).detach();

    while (!proc_end) {
        auto res = socklib::Http1::serve(sv, 8090);
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

#include "v1/application/http2.h"

void testhttp2() {
    /*std::string str, dec;
    const char* src =
        (const char*)
            u8R"(
        SSLつまりセキュアソケットレイヤーについて、彼は意見を言ったらしいが聞き入れられなかった
        SSLつまりセキュアソケットレイヤーについて、彼は意見を言ったらしいが聞き入れられなかった
        SSLつまりセキュアソケットレイヤーについて、彼は意見を言ったらしいが聞き入れられなかった
        SSLつまりセキュアソケットレイヤーについて、彼は意見を言ったらしいが聞き入れられなかった
    )";
    socklib::Hpack::encode_str(str, src);
    Deserializer<std::string&> de(str);
    socklib::Hpack::decode_str(dec, de);
    if (src != dec) {
        throw "error!";
    }*

    socklib::Hpack::DynamicTable table, other;
    socklib::HttpConn::Header header, decoded;
    const char src[] = {
        0x00,                                                                     // 圧縮情報
        0x07, 0x3a, 0x6d, 0x65, 0x74, 0x68, 0x6f, 0x64,                           // 7 :method
        0x03, 0x47, 0x45, 0x54,                                                   // 3 GET
        0x00,                                                                     // 圧縮情報
        0x05, 0x3a, 0x70, 0x61, 0x74, 0x68,                                       // 5 :path
        0x01, 0x2f,                                                               // 1 /
        0x00,                                                                     // 圧縮情報
        0x07, 0x3a, 0x73, 0x63, 0x68, 0x65, 0x6d, 0x65,                           // 7 :scheme
        0x05, 0x68, 0x74, 0x74, 0x70, 0x73,                                       // 5 https
        0x00,                                                                     // 圧縮情報
        0x0a, 0x3a, 0x61, 0x75, 0x74, 0x68, 0x6f, 0x72, 0x69, 0x74, 0x79,         // 10 :authority
        0x0b, 0x6e, 0x67, 0x68, 0x74, 0x74, 0x70, 0x32, 0x2e, 0x6f, 0x72, 0x67};  // 11 nghttp2.org*
    std::string sc(src, sizeof(src));
    uint32_t maxi = ~0;
    socklib::Hpack::decode(header, sc, table, maxi);
    std::string res;

    socklib::Hpack::encode<true>(h, res, table, maxi);
    socklib::Hpack::decode(decoded, res, other, maxi);
    */

    socklib::HttpConn::Header h = {{":authority", "www.google.com"},
                                   {":scheme", "https"},
                                   {":method", "GET"},
                                   {":path", "/"}};
    socklib::HttpOpenContext ctx;
    ctx.url = "https://www.google.com";
    ctx.cacert = cacert;
    auto conn = socklib::Http2::open(ctx);
    if (!conn) return;
    socklib::H2Stream *st, *c1;
    conn->get_stream(0, st);
    conn->get_stream(1, c1);
    st->send_settings({});
    while (true) {
        if (conn->recvable() || socklib::Selecter::waitone(conn->borrow(), 1, 0)) {
            std::shared_ptr<socklib::H2Frame> frame;
            if (auto e = conn->recv(frame); !e) {
                std::cout << "error\n";
                return;
            }
            if (auto c = frame->settings()) {
                std::cout << "settings";
                if (c->is_set(socklib::H2Flag::ack)) {
                    std::cout << ":ack";
                    c1->send_header(h, false, 0, true);
                }
                else {
                    st->send_settings({}, true);
                }
                std::cout << "\n";
            }
            else if (auto d = frame->data()) {
                //std::cout << "data\n";
                std::cout << d->payload();
                if (d->is_set(socklib::H2Flag::end_stream)) {
                    break;
                }
                if (c1->send_windowupdate((int)d->payload().size())) {
                    //std::cout << "window update\n";
                }
            }
            else if (auto d = frame->header()) {
                std::cout << "header\n";
                for (auto& h : d->header_map()) {
                    if (h.first == ":status") {
                        auto m_0 = [](auto c) {
                            return c - '0';
                        };
                        auto code = m_0(h.second[0]) * 100 + m_0(h.second[1]) * 10 + m_0(h.second[2]);
                        std::cout << "HTTP/2.0 " << h.second << " " << socklib::reason_phrase(code) << "\n";
                    }
                    else {
                        std::cout << h.first << ":" << h.second << "\n";
                    }
                }
                std::cout << "\n";
            }
            else if (auto e = frame->rst_stream()) {
                std::cout << "rst stream\n";
                std::cout << e->code();
                break;
            }
            else if (auto e = frame->goaway()) {
                break;
            }
        }
    }
}

#include "v1/application/http.h"

int main(int, char**) {
    socklib::HttpClient client;
    server_proc();
    sizeof(socklib::HttpOpenContext);
}
