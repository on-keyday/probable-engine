#include "v2/http.h"
#include <map>
using namespace socklib;
using namespace socklib::v2;

struct TestBase {
    virtual int method() const {
        return 1;
    }
};

struct TestDerived : TestBase {
   private:
    virtual int method() const override {
        return 2;
    }
};

int main() {
    auto p = make_request();
    TimeoutContext timeout(30);
    p->set_httpversion(1);
    p->set_cacert("D:/commonlib/netsoft/cacert.pem");
    auto res = p->request("GET", "google.com", &timeout);
    res = p->response(&timeout);
}