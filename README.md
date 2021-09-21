# socklib
simple socket library(c++)

**warning:**  this library is still in development that destructive changes will be able to occur
## How to Build
0. Before build, install software below and add to PATH.
   - cmake(https://cmake.org/download/)
   - ninja(https://github.com/ninja-build/ninja)
   -  clang++(https://github.com/llvm/llvm-project)
2. edit CMakeLists.txt if you need (see below **info**) 
3. run ```build.bat```(windows) or ```. build``` (linux). After built, you can find built executable at root directory.
## info
This library is denpended on OpenSSL(https://github.com/openssl/openssl).

Before you build,edit CMakeLists.txt for your environment.
