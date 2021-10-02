#include "v2/http1.h"
#include <map>
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
    Http1Request<std::string, std::multimap<std::string, std::string>, std::vector<char>> request;
    decltype(request)::request_t ctx;
    sizeof(ctx);
    TestDerived de;
    TestBase& base = de;
    base.method();
}