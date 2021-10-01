#include "v2/http1.h"
#include <map>
using namespace socklib::v2;

int main() {
    Http1Request<std::string, std::multimap<std::string, std::string>, std::vector<char>> request;
    decltype(request)::request_t request;
}