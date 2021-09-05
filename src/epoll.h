#pragma once
#include "sockbase.h"
#include <learnstd.h>
#include <memory>
#include <map>
namespace socklib {
    struct EpollCtx {
       private:
        friend struct Epoller;
        std::map<int, std::shared_ptr<Conn>> fds;

       public:
        bool add(std::shared_ptr<Conn>& fd) {
            if (fds.find(fd->sock) != fds.end()) return false;
            fds.emplace(fd->sock, fd);
            return true;
        }

        bool del(std::shared_ptr<Conn>& fd) {
            if (fds.find(fd->sock) == fds.end()) return false;
            fds.erase(fd->sock);
            return true;
        }
    };

    struct Epoller {
        static bool epoll(EpollCtx& ctx) {
            //unimplemeted
            return false;
        }
    };
}  // namespace socklib