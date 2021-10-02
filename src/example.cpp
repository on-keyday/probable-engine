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
}