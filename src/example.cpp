#include "v2/http.h"
#include "v2/http2.h"

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
    p->requestHeader() = {{"Accept", "text/html"}};
    p->set_cacert("D:/commonlib/netsoft/cacert.pem");
    auto res = p->request("GET", "google.com", &timeout);
    res = p->response(&timeout);

    TestDerived d;
    using table = std::deque<std::pair<std::string, std::string>>;
    using header = std::multimap<std::string, std::string>;
    using dataframe = H2DataFrame<std::string, std::map, header, table>;
    using headerframe = H2HeaderFrame<std::string, std::map, header, table>;
    dataframe data;
    headerframe head;
    commonlib2::Serializer<std::string> se;
    dataframe::h2request_t req;
    data.serialize(100000, se, req);
}