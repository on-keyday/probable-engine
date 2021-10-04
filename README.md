# socklib
simple socket library(c++)

**warning:**  this library is still in development and not released, so destructive changes will be able to occur. no documentation and no support exists

## License
MIT License

## How to Build
0. Before build, install software below and add to PATH.
   - cmake(https://cmake.org/download/)
   - ninja(https://github.com/ninja-build/ninja)
   -  clang++(https://github.com/llvm/llvm-project)
2. edit CMakeLists.txt if you need (see below **Info**) 
3. run ```build.bat```(windows) or ```. build``` (linux). After built, you can find built executable at root directory.

**warning:** now, example codes are not works.

## Info
This library is denpended on OpenSSL(https://github.com/openssl/openssl).

Before you build,edit CMakeLists.txt for your environment.
