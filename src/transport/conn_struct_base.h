#pragma once
#include "sockbase.h"

namespace socklib {
    struct SockReader {
       private:
        mutable std::shared_ptr<Conn> base;
        mutable std::string buffer;
        mutable bool on_eof = false;
        void (*callback)(void*, bool) = nullptr;
        void* ctx = nullptr;
        CancelContext* cancel = nullptr;

       public:
        SockReader(std::shared_ptr<Conn> r, CancelContext* cancel = nullptr)
            : base(r), cancel(cancel) {}
        void setcallback(decltype(callback) cb, void* ct) {
            callback = cb;
            ctx = ct;
        }
        size_t size() const {
            if (!base) {
                on_eof = true;
                return 0;
            }
            if (buffer.size() == 0) reading();
            return buffer.size() + 1;
        }

        bool reading() const {
            if (on_eof) return false;
            auto res = base->read(buffer, cancel);
            if (res == ~0 || res == 0) {
                on_eof = true;
            }
            if (callback) callback(ctx, on_eof);
            return !on_eof;
        }

        char operator[](size_t idx) const {
            if (!base) {
                on_eof = true;
                return char();
            }
            if (buffer.size() > idx) {
                return buffer[idx];
            }
            if (!reading()) return char();
            return buffer[idx];
        }

        bool eof() {
            return on_eof;
        }

        std::string& ref() {
            return buffer;
        }

        void set_canceler(CancelContext* ctx) {
            cancel = ctx;
        }
    };

    struct AppLayer {
       protected:
        std::shared_ptr<Conn> conn;

       public:
        AppLayer(std::shared_ptr<Conn>&& in)
            : conn(std::move(in)) {}

        void interrupt(bool f = true) {
            conn->set_suspend(f);
        }

        std::shared_ptr<Conn> hijack() {
            return std::move(conn);
        }

        std::shared_ptr<Conn>& borrow() {
            return conn;
        }

        std::string ipaddress() {
            if (!conn) return std::string();
            return conn->ipaddress();
        }

        virtual void close() {
            if (conn) {
                conn->close();
            }
        }

        virtual ~AppLayer() {}
    };

}  // namespace socklib