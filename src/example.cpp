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
}