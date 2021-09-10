#pragma once

#ifdef _WIN32
#ifdef __MINGW32__
#define _WIN32_WINNT 0x0501
#endif
#include <WinSock2.h>
#include <WS2tcpip.h>
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#define closesocket close
#define ioctlsocket ioctl
#define SD_SEND SHUT_WR
#define SD_RECEIVE SHUT_RD
#define SD_BOTH SHUT_RDWR
#endif

#include <string>
#include <string.h>
#include <stdexcept>
#include <time.h>

#if !defined(_WIN32) || (defined(__GNUC__) && !defined(__clang__))
#define memcpy_s(dst, dsz, src, ssz) memcpy(dst, src, ssz)
#define strcpy_s(dst, dsz, src) strcpy(dst, src)
#endif

#ifndef _WIN32
#define Sleep(time) usleep(time)
#endif

#ifndef USE_OPENSSL
#define USE_OPENSSL 1
#endif

#if USE_OPENSSL
#include <openssl/ssl.h>
#endif