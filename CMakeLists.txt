cmake_minimum_required(VERSION 3.16)
project(socklib)

include_directories("src/commonlib" "D:/OpenSSL/build-x64/include")
link_directories("D:/OpenSSL/build-x64-debug/lib")

set(CMAKE_CXX_STANDARD 20)

add_executable(sock "src/main.cpp")


target_link_libraries(sock libssl libcrypto ws2_32)





