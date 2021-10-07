# socklib
simple socket library(c++)

**warning:**  this library is still in development and not released, so destructive changes will be able to occur.also, no documentation and no support exists.

## Before Download
If you want to download this libary for **practical purpose**, I don't recommend this now.

This library is made only to satisfy my hobby. 

So not tested completely, maybe many bugs not fixed, and maybe insecure.

If you want practical library right now, see other.


## Version
now avaliable version is version 1 (on src/v1) and version 2 (on src/v2). 

now active development is version 2 (on src/v2) support http/1.1, http2 (over TLS), websocket, and will support more protocols.

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
