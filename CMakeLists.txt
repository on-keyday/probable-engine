cmake_minimum_required(VERSION 3.16)
project(socklib)

if(WIN32)
include_directories("src/commonlib" "D:/OpenSSL/build-x64/include")
link_directories("D:/OpenSSL/build-x64-debug/lib")
else()
link_directories("/lib/x86_64-linux-gnu")
endif()

include_directories("src/commonlib")

set(CMAKE_CXX_STANDARD 20)

find_package(Threads REQUIRED)

add_executable(sock "src/main.cpp")

if(WIN32)
target_link_libraries(sock libssl libcrypto ws2_32)
elseif(linux)
target_link_libraries(sock /lib/x86_64-linux-gnu/libssl.so /lib/x86_64-linux-gnu/libcrypto.so Threads::Threads)
endif()




