/*
    commonlib - common utility library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <fileio.h>
#include <stdio.h>

#include <fstream>
#include <iostream>
#include <sstream>

#include "extension_operator.h"
#include "project_name.h"

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#ifndef COMMONLIB_IOWRAP_IO_CHARMODE
#define COMMONLIB_IOWRAP_IO_CHARMODE _O_U8TEXT
#endif
#ifndef COMMONLIB_IOWRAP_IN_CHARMODE
#define COMMONLIB_IOWRAP_IN_CHARMODE COMMONLIB_IOWRAP_IO_CHARMODE
#endif
#ifndef COMMONLIB_IOWRAP_OUT_CHARMODE
#define COMMONLIB_IOWRAP_OUT_CHARMODE COMMONLIB_IOWRAP_IO_CHARMODE
#endif
#endif

namespace PROJECT_NAME {
    struct IOWrapper {
        using callback_t = void (*)(const char*, size_t, void*);

       private:
        static bool& Flag() {
            static bool flag = false;
            return flag;
        }

        static bool Init_impl(bool sync, const char*& err) {
            if (Flag()) return true;
            Flag() = true;
#ifdef _WIN32
            if (_setmode(_fileno(stdin), COMMONLIB_IOWRAP_IN_CHARMODE) == -1) {
                err = "error:text input mode change failed";
                return false;
            }
            if (_setmode(_fileno(stdout), COMMONLIB_IOWRAP_OUT_CHARMODE) == -1) {
                err = "error:text output mode change failed\n";
                return false;
            }
            if (_setmode(_fileno(stderr), COMMONLIB_IOWRAP_OUT_CHARMODE) == -1) {
                err = "error:text error mode change failed\n";
                return false;
            }
#endif
            std::ios_base::sync_with_stdio(sync);
            /*            
            std::locale loc("");
#ifdef _WIN32
            std::wcin.imbue(loc);
            std::wcout.imbue(loc);
            std::wcerr.imbue(loc);
            std::wclog.imbue(loc);
#else
            std::cin.imbue(loc);
            std::cout.imbue(loc);
            std::cerr.imbue(loc);
            std::clog.imbue(loc);
#endif*/
            return true;
        }

       public:
        static bool Init(bool sync = false, const char** err = nullptr) {
            static const char* errmsg = nullptr;
            static bool result = Init_impl(sync, errmsg);
            if (err) *err = errmsg;
            return result;
        }

        static bool Initialized() {
            return Flag();
        }

       protected:
        static bool Able_continue() {
#ifdef _WIN32
            return Initialized() && Init(false, nullptr);
#else
            Flag() = true;
            return true;
#endif
        }

        /*static std::string& initial_chahe() {
            static std::string init;
            return init;
        }*/
    };

    //this is maybe faster than cin wrapper
    struct StdInWrapper : IOWrapper {
        //unimplemented
    };

    //this is maybe faster than coutwrapper
    struct StdOutWrapper : IOWrapper {
       private:
        callback_t cb = nullptr;
        void* ctx = nullptr;
        FILE* fout = nullptr;
        FILE* base = nullptr;
        std::ostringstream ss;
        bool onlybuffer;

       public:
        StdOutWrapper(FILE* fp)
            : fout(fp), base(fp) {
        }
        template <class T>
        StdOutWrapper& operator<<(const T& in) {
            if (onlybuffer) {
                ss << in;
                return *this;
            }
            ss.str(std::string());
            ss << in;
            if (fout != base) {
                std::string tmp = ss.str();
                fwrite(tmp.c_str(), 1, tmp.size(), fout);
                return *this;
            }
            if (cb) {
                std::string tmp = ss.str();
                cb(tmp.c_str(), tmp.size(), ctx);
                return *this;
            }

            if (!Able_continue()) throw std::runtime_error("not called IOWrapper::Init() before io function");
#ifdef _WIN32
            std::wstring tmp;
#else
            std::string tmp;
#endif
            Reader(ss.str()) >> tmp;

            fwrite(tmp.c_str(), sizeof(tmp[0]), tmp.size(), fout);
            return *this;
        }

       private:
        bool open_detail(FILE** pfp, const char* filename) {
            fopen_s(pfp, filename, "w");
            return *pfp != nullptr;
        }
#ifdef _WIN32
        bool open_detail(FILE** pfp, const wchar_t* filename) {
            _wfopen_s(pfp, filename, L"w");
            return *pfp != nullptr;
        }
#endif
       public:
        template <class T>
        bool open(T& t) {
            return open(t.c_str());
        }

        template <class C>
        bool open(C* name) {
            FILE* tmp = nullptr;
            if (open_detail(&tmp, name)) {
                if (fout != base) {
                    fclose(fout);
                }
                fout = tmp;
                return true;
            }
            return false;
        }

        bool stop_out(bool stop) {
            auto ret = onlybuffer;
            onlybuffer = stop;
            return ret;
        }

        bool stop_out() {
            return onlybuffer;
        }

        std::string buf_str() {
            return ss.str();
        }

        void reset_buf() {
            ss.str("");
            ss.clear();
        }

        bool is_file() {
            return fout != base;
        }

        void set_callback(callback_t in, void* inctx = nullptr) {
            cb = in;
            ctx = inctx;
        }

        callback_t get_callback() {
            return cb;
        }

        void* get_ctx() {
            return ctx;
        }
    };

    struct CinWrapper : IOWrapper {
       private:
#ifdef _WIN32
        std::wistream& in;
#else
        std::istream& in;
#endif
        /*static bool get_initial_impl(std::string& out, bool& res) {
            out = std::move(initial_chahe());
            res = out.size() != 0;
            return true;
        }

        static bool get_initial(std::string& out) {
            bool res = false;
            static bool init = get_initial_impl(out, res);
            return res;
        }*/

       public:
        using char_type = typename std::remove_reference_t<decltype(in)>::char_type;

        CinWrapper(decltype(in) st)
            : in(st) {}

        template <class T>
        CinWrapper& operator>>(T& out) {
            if (!Able_continue()) throw std::runtime_error("not called IOWrapper::Init() before io function");
            in >> out;
            return *this;
        }

        CinWrapper& operator>>(std::string& out) {
            if (!Able_continue()) throw std::runtime_error("not called IOWrapper::Init() before io function");
#ifdef _WIN32
            std::basic_string<char_type> tmp;
            in >> tmp;
            Reader(tmp) >> out;
#else
            in >> out;
#endif
            return *this;
        }

        CinWrapper& getline(std::string& out) {
            if (!Able_continue()) throw std::runtime_error("not called IOWrapper::Init() before io function");

                /*if (get_initial(out)) {
                return *this;
            }*/
#ifdef _WIN32
            std::basic_string<char_type> tmp;
            std::getline(in, tmp);
            Reader(tmp) >> out;
#else
            std::getline(in, out);
#endif
            return *this;
        }

        size_t readsome(std::string& out) {
            throw std::runtime_error("async io not implemented");
            if (!Able_continue()) throw std::runtime_error("not called IOWrapper::Init() before io function");
            size_t ret = 0;
            /*if (get_initial(out)) {
                return out.size();
            }*/

            while (in.peek()) {
#ifdef _WIN32
                wchar_t c;
#else
                char c;
#endif
                if (!in.get(c)) {
                    in.clear();
                    break;
                }

#ifdef _WIN32
                std::string tmp;
                Reader(Sized(&c, 1)) >> tmp;
                out += tmp;
#else
                out += c;
#endif
                ret++;
            }
            return ret;
        }
    };

    struct CoutWrapper : IOWrapper {
       private:
        callback_t cb = nullptr;
        void* ctx = nullptr;
        std::ofstream file;
        std::ostringstream ss;
        bool onlybuffer = false;
#ifdef _WIN32
        std::wostream& out;
#else
        std::ostream& out;
#endif
       public:
        using char_type = typename std::remove_reference_t<decltype(out)>::char_type;
        CoutWrapper(decltype(out) st)
            : out(st) {}

        template <class T>
        CoutWrapper& operator<<(const T& in) {
            if (onlybuffer) {
                ss << in;
                return *this;
            }
            if (file.is_open()) {
                file << in;
                return *this;
            }
            if (cb) {
                ss.str(std::string());
                ss << in;
                auto tmp = ss.str();
                cb(tmp.c_str(), tmp.size(), ctx);
                return *this;
            }

            if (!Able_continue()) throw std::runtime_error("not called IOWrapper::Init() before io function");

#ifdef _WIN32
            //auto tmp= ss.flags();
            ss.str("");
            //ss.setf(tmp);
            std::wstring str;
            ss << in;
            Reader(ss.str()) >> str;
            out << str;

#else
            out << in;
#endif

            return *this;
        }

        CoutWrapper& operator<<(std::ios_base& (*in)(std::ios_base&)) {
            if (cb || onlybuffer) {
                ss << in;
                return *this;
            }
            if (file.is_open()) {
                file << in;
                return *this;
            }
            if (!Able_continue()) throw std::runtime_error("not called IOWrapper::Init() before io function");
#ifdef _WIN32
            ss << in;
#else
            out << in;
#endif
            return *this;
        }

        bool open(const std::string& in) {
#if _WIN32
            std::wstring tmp;
            Reader(in) >> tmp;
            file.open(tmp.c_str());
#else
            file.open(in);
#endif
            return (bool)file;
        }

        bool stop_out(bool stop) {
            auto ret = onlybuffer;
            onlybuffer = stop;
            return ret;
        }

        bool stop_out() {
            return onlybuffer;
        }

        std::string buf_str() {
            return ss.str();
        }

        void reset_buf() {
            ss.str("");
            ss.clear();
        }

        bool is_file() const {
            return (bool)file;
        }

        void set_callback(callback_t in, void* inctx = nullptr) {
            cb = in;
            ctx = inctx;
        }

        callback_t get_callback() {
            return cb;
        }

        void* get_ctx() {
            return ctx;
        }
    };

    inline CinWrapper& cin_wrapper() {
#ifdef _WIN32
        static CinWrapper Cin(std::wcin);
#else
        static CinWrapper Cin(std::cin);
#endif
        return Cin;
    }

    inline CoutWrapper& cout_wrapper() {
#ifdef _WIN32
        static CoutWrapper Cout(std::wcout);
#else
        static CoutWrapper Cout(std::cout);
#endif
        return Cout;
    }

    inline CoutWrapper& cerr_wrapper() {
#ifdef _WIN32
        static CoutWrapper Cerr(std::wcerr);
#else
        static CoutWrapper Cerr(std::cerr);
#endif
        return Cerr;
    }

    inline CoutWrapper& clog_wrapper() {
#ifdef _WIN32
        static CoutWrapper Clog(std::wclog);
#else
        static CoutWrapper Clog(std::clog);
#endif
        return Clog;
    }

    inline StdOutWrapper& stdout_wrapper() {
        static StdOutWrapper StdOut(stdout);
        return StdOut;
    }

    inline StdOutWrapper& stderr_wrapper() {
        static StdOutWrapper StdErr(stderr);
        return StdErr;
    }

    template <class Out>
    struct ThreadSafeOutPut {
        std::mutex mut;
        Out& out;
        ThreadSafeOutPut(Out& o)
            : out(o) {}
        operator Out&() {
            return out;
        }

        template <class T>
        ThreadSafeOutPut& operator<<(T&& in) {
            mut.lock();
            out << in;
            mut.unlock();
            return *this;
        }

        Out& get() {
            return out;
        }
    };

    inline ThreadSafeOutPut<CoutWrapper>& cout_wrapper_s() {
        static ThreadSafeOutPut<CoutWrapper> safe(cout_wrapper());
        return safe;
    }

    inline ThreadSafeOutPut<CoutWrapper>& cerr_wrapper_s() {
        static ThreadSafeOutPut<CoutWrapper> safe(cerr_wrapper());
        return safe;
    }

    inline ThreadSafeOutPut<CoutWrapper>& clog_wrapper_s() {
        static ThreadSafeOutPut<CoutWrapper> safe(clog_wrapper());
        return safe;
    }

    inline ThreadSafeOutPut<StdOutWrapper>& stdout_wrapper_s() {
        static ThreadSafeOutPut<StdOutWrapper> safe(stdout_wrapper());
        return safe;
    }

    inline ThreadSafeOutPut<StdOutWrapper>& stderr_wrapper_s() {
        static ThreadSafeOutPut<StdOutWrapper> safe(stderr_wrapper());
        return safe;
    }

}  // namespace PROJECT_NAME
