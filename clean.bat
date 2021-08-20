@echo off
ninja clean
rmdir /s /q CMakeFiles
del build.ninja
del cmake_install.cmake
del CMakeCache.txt
