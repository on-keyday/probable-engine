#include "v2/socklib.h"

#include <map>
#include <deque>
using namespace socklib;
using namespace socklib::v2;

template <class>
struct TestDerived;

template <class Int>
struct TestBase {
    TestBase(Int i) {}

    using base_t = TestBase<Int>;

    virtual Int method() const {
        return 1;
    }

    virtual TestDerived<Int>* is_derived() {
        return nullptr;
    }
};

template <class Int = int>
struct TestDerived : TestBase<Int> {
   private:
    virtual int method() const override {
        return 2;
    }

   public:
    TestDerived<Int>* is_derived() override {
        return this;
    }

    TestDerived()
        : TestBase<Int>(1) {}
};

int main() {
    auto p = make_request();
    TimeoutContext timeout(30);
    p->set_httpversion(2);
    p->requestHeader() = {{"Accept", "text/html"}};
    p->set_cacert("D:/commonlib/netsoft/cacert.pem");
    auto res = p->request("GET", "https://www.google.com", &timeout);
    res = p->response(&timeout);

    res = p->request("GET", "https://www.google.com/teapot", &timeout);
    res = p->response(&timeout);

    p->close(&timeout);
    /*
    DefWebSocketRequestContext ctx;
    ctx.url = "localhost:8090/ws";
    std::shared_ptr<DefWebSocketConn> conn;
    DefWebSocket::open(conn, ctx, &timeout);
    conn->write("cast client hello");
    Sleep(1000);
    conn->write("close");
    ReadContext<String> read;
    conn->read(read, &timeout);*/

    HttpAcceptContext<String> accept;
    accept.tcplayer.port = 8090;
    SleeperContext sleeper;
    bool e;
    InterruptContext it(e, &sleeper);
    auto rec = accept_request(accept, &it);
    auto ac = DefWebSocket::accept(rec);
    if (ac) {
        ac->write("goodbye");
        ac->close();
    }
    else {
        rec->request();
        rec->responseHeader() = {{"connection", "close"}};
        rec->responseBody() = "<html><h1>Service Unavailable</h1></html>";
        rec->response(503);
    }
}