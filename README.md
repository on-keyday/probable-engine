# socklib
simple socket library(c++)

**warning:**  this library is still in development and not released, so destructive changes will be able to occur.also, no documentation and no support exists.

## Version
now avaliable version is version 1 (not indexed) (usable: http.h,http1.h,http2.h,websocket.h)

now developing is version 2 (on src/v2) will support http/1.1, http2, websocket, and so on with c++ template

## License
MIT License

## How to Build
0. Before build, install software below and add to PATH.
   - cmake(https://cmake.org/download/)
   - ninja(https://github.com/ninja-build/ninja)
   -  clang++(https://github.com/llvm/llvm-project)
2. edit CMakeLists.txt if you need (see below **Info**) 
3. run ```build.bat```(windows) or ```. build``` (linux). After built, you can find built example code executable at root directory.


## Info
This library is denpended on OpenSSL(https://github.com/openssl/openssl).

Before you build,edit CMakeLists.txt for your environment.
