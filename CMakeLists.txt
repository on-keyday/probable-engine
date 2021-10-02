cmake_minimum_required(VERSION 3.16)
project(socklib)

if(WIN32)
include_directories("src/commonlib" "D:/OpenSSL/build-x64/include")
link_directories("D:/OpenSSL/build-x64-debug/lib")
else()
# find_package(OPENSSL REQUIRED)
link_directories("/lib/x86_64-linux-gnu")
endif()

include_directories("src/commonlib")

set(CMAKE_CXX_STANDARD 20)

find_package(Threads REQUIRED)

add_executable(sock "src/example.cpp")

if(WIN32)
target_link_libraries(sock libssl libcrypto ws2_32)
else()
target_link_libraries(sock libssl.so libcrypto.so Threads::Threads)
endif()




