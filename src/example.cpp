#include "v2/http.h"
#include "v2/hpack.h"
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
    p->set_httpversion(1);
    p->set_cacert("D:/commonlib/netsoft/cacert.pem");
    auto res = p->request("GET", "google.com", &timeout);
    res = p->response(&timeout);

    TestDerived d;

    using hpack = Hpack<std::string, std::deque<std::pair<std::string, std::string>>, std::multimap<std::string, std::string>>;
    std::string dst;
    hpack::table_t table, after;
    hpack::header_t header;
    std::uint32_t v = 9999;
    hpack::encode({{"cf-ray", "69846a37db520aba-KIZ"}}, dst, table, 9999);
    hpack::decode(header, dst, after, v);
}