#include "v2/socklib.h"
#include "v2/http_cookie.h"

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

void test_http() {
    constexpr auto cacert = "D:/commonlib/netsoft/cacert.pem";
    auto p = make_request();
    TimeoutContext timeout(30);
    p->set_httpversion(2);
    p->requestHeader() = {{"Accept", "text/html"}};
    p->set_cacert(cacert);
    auto res = p->request("GET", "https://www.google.com", &timeout);
    res = p->response(&timeout);

    res = p->request("GET", "https://www.google.com/teapot", &timeout);
    res = p->response(&timeout);

    p->close(&timeout);

    HttpAcceptContext<String> accept;
    accept.tcp.port = 8090;
    SleeperContext sleeper;
    bool e = false;
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
        rec->set_requestflag(RequestFlag::header_is_small);
        rec->response(503);
    }
}

int main() {
    Date date;
    String str;

    DateParser<String, std::vector>::parse("Wed, 21 Oct 2015 07:28:00 GMT", date);
    time_t now = time(nullptr), decode = false;
    DateParser<String, std::vector>::from_time_t(now, date);
    DateWriter<String>::write(str, date);
    DateWriter<String>::to_time_t(decode, date);
    time_t diff = now - decode;
}