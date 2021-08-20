cmake_minimum_required(VERSION 3.16)
project(socklib)

if(WIN32)
include_directories("src/commonlib" "D:/OpenSSL/build-x64/include")
link_directories("D:/OpenSSL/build-x64-debug/lib")
endif()

set(CMAKE_CXX_STANDARD 20)

find_package(Threads REQUIRED)

add_executable(sock "src/main.cpp")

if(WIN32)
target_link_libraries(sock libssl libcrypto ws2_32)
elseif(LINUX)
find_package(OPENSSL REQUIRED)
target_link_libraries(sock libssl libcrypto Thread::Threads)
endif()




