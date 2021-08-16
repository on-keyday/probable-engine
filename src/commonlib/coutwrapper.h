/*
    Copyright (c) 2021 on-keyday
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
#endif

namespace PROJECT_NAME {
    struct IOWrapper {
       private:
        static bool& Flag() {
            static bool flag = false;
            return flag;
        }

        static bool Init_impl(bool sync, const char*& err) {
            if (Flag()) return true;
            Flag() = true;
#ifdef _WIN32
            if (_setmode(_fileno(stdin), _O_U8TEXT) == -1) {
                err = "error:text input mode change failed";
                return false;
            }
            if (_setmode(_fileno(stdout), _O_U8TEXT) == -1) {
                err = "error:text output mode change failed\n";
                return false;
            }
            if (_setmode(_fileno(stderr), _O_U8TEXT) == -1) {
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
            ss.str("");
            ss << in;
            if (fout != base) {
                std::string tmp = ss.str();
                fwrite(tmp.c_str(), 1, tmp.size(), fout);
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
            if (!Able_continue()) throw std::runtime_error("not called IOWrapper::Init() before io function");
            size_t ret = 0;
            /*if (get_initial(out)) {
                return out.size();
            }*/

            while (in.rdbuf()->in_avail() > 0) {
#ifdef _WIN32
                wchar_t str[100] = {0};
#else
                char str[100] = {0};
#endif
                auto red = in.readsome(str, 100);
                if (red > 0) {
#ifdef _WIN32
                    std::string tmp;
                    Reader(Sized(str, red)) >> tmp;
                    out += tmp;
#else
                    out += std::string(str, red);
#endif
                    ret += red;
                }
            }
            return ret;
        }
    };

    struct CoutWrapper : IOWrapper {
       private:
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
            if (onlybuffer) {
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

}  // namespace PROJECT_NAME
