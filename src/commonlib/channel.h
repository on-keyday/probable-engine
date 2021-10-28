/*
    commonlib - common utility library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <enumext.h>
#include <atomic>
#include <memory>
#include <tuple>
#include <deque>

namespace PROJECT_NAME {

    enum class ChanError {
        none,
        closed,
        limited,
        empty,
    };

    using ChanErr = commonlib2::EnumWrap<ChanError, ChanError::none, ChanError::closed>;

    enum class ChanDisposeFlag {
        remove_front,
        remove_back,
        remove_new,
    };

    template <class T, template <class...> class Queue>
    struct Channel;

    template <class T, template <class...> class Que>
    struct SendChan {
        using base_chan = Channel<T, Que>;

       private:
        std::shared_ptr<base_chan> chan;

       public:
        SendChan(std::shared_ptr<base_chan>& chan)
            : chan(chan) {}

        ChanErr operator<<(T&& value) {
            return chan->store(std::move(value));
        }

        bool close() {
            return chan->close();
        }

        bool closed() const {
            return chan->closed();
        }
    };

    template <class T, template <class...> class Que>
    struct RecvChan {
        using base_chan = Channel<T, Que>;

       private:
        std::shared_ptr<base_chan> chan;

       public:
        RecvChan(std::shared_ptr<base_chan>& chan)
            : chan(chan) {}

        ChanErr operator>>(T& value) {
            return chan->load(value);
        }

        bool close() {
            return chan->close();
        }

        bool closed() const {
            return chan->closed();
        }
    };

    template <class T, template <class...> class Queue>
    struct ChanBuf {
        Queue<T> impl;
        T& front() {
            return impl.front();
        }
        void pop_front() {
            impl.pop_front();
        }

        void pop_back() {
            impl.pop_back();
        }

        void push_back(T&& t) {
            impl.push_back(std::move(t));
        }

        size_t size() const {
            return impl.size();
        }
    };

    template <class T, template <class...> class Queue>
    struct Channel {
        using queue_type = ChanBuf<T, Queue>;
        using value_type = T;

       private:
        ChanDisposeFlag dflag = ChanDisposeFlag::remove_new;
        size_t quelimit = ~0;
        queue_type que;
        std::atomic_flag lock_;
        std::atomic_flag closed_;

       public:
        Channel(size_t quelimit = ~0, ChanDisposeFlag dflag = ChanDisposeFlag::remove_new) {
            lock_.clear();
            closed_.clear();
            this->quelimit = quelimit;
            this->dflag = dflag;
        }

       private:
        bool dispose() {
            if (que.size() == quelimit) {
                switch (dflag) {
                    case ChanDisposeFlag::remove_new:
                        return false;
                    case ChanDisposeFlag::remove_front:
                        que.pop_front();
                        return true;
                    case ChanDisposeFlag::remove_back:
                        que.pop_back();
                        return true;
                    default:
                        return false;
                }
            }
            return true;
        }

        bool lock() {
            while (lock_.test_and_set()) {
                lock_.wait(true);
            }
            if (closed_.test()) {
                unlock();
                return false;
            }
            return true;
        }

        void unlock() {
            lock_.clear();
            lock_.notify_one();
        }

       public:
        ChanErr store(T&& t) {
            if (!lock()) {
                return ChanError::closed;
            }
            if (!dispose()) {
                unlock();
                return ChanError::limited;
            }
            que.push_back(std::move(t));
            unlock();
            return true;
        }

        ChanErr load(T& t) {
            if (!lock()) {
                return ChanError::closed;
            }
            if (que.size() == 0) {
                unlock();
                return ChanError::empty;
            }
            t = std::move(que.front());
            que.pop_front();
            unlock();
            return true;
        }

        bool close() {
            lock();
            bool res = closed_.test_and_set();
            unlock();
            return res;
        }

        bool closed() const {
            return closed_.test();
        }
    };

    template <class T, template <class...> class Que = std::deque>
    std::tuple<SendChan<T, Que>, RecvChan<T, Que>> make_chan(size_t limit = ~0, ChanDisposeFlag dflag = ChanDisposeFlag::remove_new) {
        auto base = std::make_shared<Channel<T, Que>>(limit, dflag);
        return {base, base};
    }
}  // namespace PROJECT_NAME
