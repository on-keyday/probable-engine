/*
    commonlib - common utility library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "project_name.h"
#define POSIX_SOURCE 200809L
#include <stddef.h>
#include <stdio.h>
#include <sys/stat.h>

#include <mutex>

#ifdef __EMSCRIPTEN__
#include <iostream>
#endif

#ifdef _WIN32
#include <Windows.h>
#include <io.h>
#elif defined(COMMONLIB2_IS_UNIX_LIKE)
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#else
#error "unsurpported platform"
#endif

#define COMMONLIB_MSB_FLAG(x) ((x)1 << (sizeof(x) * 8 - 1))

#if defined(_WIN32)
#if defined(__GNUC__)
#define fopen_s(pfp, filename, mode) ((*pfp) = ::fopen(filename, mode))
#define COMMONLIB_FILEIO_STRUCT_STAT struct stat
#define COMMONLIB_FILEIO_FUNC_FSTAT ::fstat
#else
#define COMMONLIB_FILEIO_FUNC_FSTAT ::_fstat64
#define COMMONLIB_FILEIO_STRUCT_STAT struct _stat64
#endif
#define COMMONLIB_FILEIO_FUNC_FSEEK ::_fseeki64
#define COMMONLIB_FILEIO_FSEEK_CAST(x) (long long)(x)
#else
#define fopen_s(pfp, filename, mode) ((*pfp) = fopen(filename, mode))
#define COMMONLIB_FILEIO_STRUCT_STAT struct stat
#define COMMONLIB_FILEIO_FUNC_FSTAT ::fstat
#define COMMONLIB_FILEIO_FUNC_FSEEK ::fseek
#define COMMONLIB_FILEIO_FSEEK_CAST(x) (long)(x)
#define _fileno fileno
#endif

namespace PROJECT_NAME {
    inline bool getfilesizebystat(int fd, size_t& size) {
        COMMONLIB_FILEIO_STRUCT_STAT status;
        if (COMMONLIB_FILEIO_FUNC_FSTAT(fd, &status) == -1) {
            return false;
        }
        size = (size_t)status.st_size;
        return true;
    }

    struct FileInput {
        friend struct FileMap;

       private:
        ::FILE* file = nullptr;
        size_t size_cache = 0;
        mutable size_t prevpos = 0;
        mutable char samepos_cache = 0;

        void open_file(FILE** pfp, const char* name) {
            fopen_s(pfp, name, "rb");
        }
#ifdef _WIN32
        void open_file(FILE** pfp, const wchar_t* name) {
            _wfopen_s(pfp, name, L"rb");
        }
#endif
       public:
        FileInput() {}

        FileInput(const FileInput&) = delete;

        FileInput(FileInput&& in) noexcept {
            file = in.file;
            in.file = nullptr;
            size_cache = in.size_cache;
            in.samepos_cache = 0;
            //got_size=in.got_size;
            //in.got_size=false;
            prevpos = in.prevpos;
            in.prevpos = 0;
            samepos_cache = in.samepos_cache;
            in.samepos_cache = 0;
        }

        template <class C>
        FileInput(C* filename) {
            open(filename);
        }

        ~FileInput() {
            close();
        }

        bool set_fp(FILE* fp) {
            if (file) {
                close();
            }
            file = fp;
            return true;
        }

        template <class C>
        bool open(C* filename) {
            if (!filename) return false;
            FILE* tmp = nullptr;
            open_file(&tmp, filename);
            if (!tmp) {
                return false;
            }
            auto fd = _fileno(tmp);
            if (!getfilesizebystat(fd, size_cache)) {
                fclose(tmp);
                return 0;
            }
            close();
            file = tmp;
            return true;
        }

        bool close() {
            if (!file) return false;
            ::fclose(file);
            file = nullptr;
            size_cache = 0;
            //got_size=false;
            prevpos = 0;
            samepos_cache = 0;
            return true;
        }

        size_t size() const {
            if (!file) return 0;
            return size_cache;
        }

        char operator[](size_t p) const {
            if (!file) return 0;
            if (size_cache <= p) return 0;
            if (COMMONLIB_MSB_FLAG(size_t) & p) return 0;
            if (prevpos < p) {
                if (COMMONLIB_FILEIO_FUNC_FSEEK(file, COMMONLIB_FILEIO_FSEEK_CAST(p - prevpos),
                                                SEEK_CUR) != 0) {
                    return 0;
                }
            }
            else if (prevpos - 1 == p) {
                return samepos_cache;
            }
            else if (prevpos > p) {
                if (COMMONLIB_FILEIO_FUNC_FSEEK(file, COMMONLIB_FILEIO_FSEEK_CAST(p),
                                                SEEK_SET) != 0) {
                    return 0;
                }
            }
            prevpos = p + 1;
            auto c = fgetc(file);
            if (c == EOF) {
                return 0;
            }
            samepos_cache = (char)c;
            return c;
        }

        bool is_open() {
            return file != nullptr;
        }
    };

    template <class Buf, class Mutex = std::mutex>
    struct ThreadSafe {
       private:
        Buf buf;
        mutable Mutex lock;

       public:
        ThreadSafe()
            : buf(Buf()) {}
        ThreadSafe(ThreadSafe&& in)
            : buf(std::forward<Buf>(in.buf)) {}
        ThreadSafe(const Buf&) = delete;
        ThreadSafe(Buf&& in)
            : buf(std::forward<Buf>(in)) {}

        Buf& get() {
            lock.lock();
            return buf;
        }

        bool release(Buf& ref) {
            if (std::addressof(buf) != std::addressof(ref)) return false;
            lock.unlock();
            return true;
        }

        auto operator[](size_t p) const
            -> decltype(buf[p]) {
            std::scoped_lock<std::mutex> locker(lock);
            return buf[p];
        }

        auto operator[](size_t p)
            -> decltype(buf[p]) {
            std::scoped_lock<std::mutex> locker(lock);
            return buf[p];
        }

        size_t size() const {
            std::scoped_lock<std::mutex> locker(lock);
            return buf.size();
        }
    };

    template <class Buf, class Mutex>
    struct ThreadSafe<Buf&, Mutex> {
       private:
        using INBuf = std::remove_reference_t<Buf>;
        INBuf& buf;
        mutable Mutex lock;

       public:
        ThreadSafe(ThreadSafe&& in)
            : buf(in.buf) {}
        ThreadSafe(INBuf& in)
            : buf(in) {}

        Buf& get() {
            lock.lock();
            return buf;
        }

        bool release(Buf& ref) {
            if (std::addressof(buf) != std::addressof(ref)) return false;
            lock.unlock();
            return true;
        }

        auto operator[](size_t p) const
            -> decltype(buf[p]) {
            std::scoped_lock<std::mutex> locker(lock);
            return buf[p];
        }

        auto operator[](size_t p)
            -> decltype(buf[p]) {
            std::scoped_lock<std::mutex> locker(lock);
            return buf[p];
        }

        size_t size() const {
            std::scoped_lock<std::mutex> locker(lock);
            return buf.size();
        }
    };

    struct FileMap {
       private:
        FILE* fp = nullptr;

#if defined(_WIN32)

        HANDLE file = INVALID_HANDLE_VALUE;
        HANDLE maph = nullptr;
        int err = 0;

        HANDLE fp_to_native(FILE* fp) {
            int fd = _fileno(fp);
            auto fileh = _get_osfhandle(fd);
            return (HANDLE)fileh;
        }

        HANDLE get_handle(const char* name) {
            return CreateFileA(
                name,
                GENERIC_READ,
                FILE_SHARE_READ,
                NULL,
                OPEN_EXISTING,
                FILE_ATTRIBUTE_NORMAL,
                NULL);
        }

        HANDLE get_handle(const wchar_t* name) {
            return CreateFileW(
                name,
                GENERIC_READ,
                FILE_SHARE_READ,
                NULL,
                OPEN_EXISTING,
                FILE_ATTRIBUTE_NORMAL,
                NULL);
        }

        bool get_map(HANDLE tmp, size_t tmpsize) {
            HANDLE tmpm = CreateFileMappingA(tmp, NULL, PAGE_READONLY, 0, 0, NULL);
            if (tmpm == NULL) {
                err = GetLastError();
                return false;
            }
            char* rep = (char*)MapViewOfFile(tmpm, FILE_MAP_READ, 0, 0, 0);
            if (!rep) {
                err = GetLastError();
                CloseHandle(tmpm);
                return false;
            }
            close_detail();
            file = tmp;
            maph = tmpm;
            place = rep;
            _size = tmpsize;
            return true;
        }

        template <class C>
        bool open_detail(const C* in) {
            auto tmp = get_handle(in);
            if (tmp == INVALID_HANDLE_VALUE) {
                return false;
            }
            DWORD high = 0;
            DWORD low = GetFileSize(tmp, &high);
            uint64_t tmpsize = ((uint64_t)high << 32) + low;
            if (!get_map(tmp, (size_t)tmpsize)) {
                CloseHandle(tmp);
                return false;
            }
            return true;
        }

        bool from_fileinput(FILE* fp, size_t size) {
            if (!fp) return false;
            auto tmp = fp_to_native(fp);
            if (tmp == INVALID_HANDLE_VALUE) {
                return false;
            }
            if (!get_map(tmp, size)) {
                return false;
            }
            this->fp = fp;
            return true;
        }

        bool close_detail() {
            if (file == INVALID_HANDLE_VALUE) return false;
            UnmapViewOfFile((LPCVOID)place);
            CloseHandle(maph);
            if (fp) {
                ::fclose(fp);
                fp = nullptr;
            }
            else {
                CloseHandle(file);
            }
            file = INVALID_HANDLE_VALUE;
            maph = nullptr;
            place = nullptr;
            _size = 0;
            return true;
        }

        bool move_detail(FileMap&& in) {
            file = in.file;
            in.file = INVALID_HANDLE_VALUE;
            maph = in.maph;
            in.maph = nullptr;
            return true;
        }

#elif defined(COMMONLIB2_IS_UNIX_LIKE)
        int fd = -1;
        long maplen = 0;

        int fp_to_native(FILE* fp) {
            return _fileno(fp);
        }

        int get_handle(const char* in) {
            return ::open(in, O_RDONLY);
        }

        bool get_map(int tmpfd, long tmpsize) {
            long pagesize = ::getpagesize(), mapsize = 0;
            mapsize = (tmpsize / pagesize + 1) * pagesize;
            char* tmpmap = (char*)mmap(nullptr, mapsize, PROT_READ, MAP_SHARED, tmpfd, 0);
            if (tmpmap == MAP_FAILED) {
                return false;
            }
            close_detail();
            fd = tmpfd;
            _size = tmpsize;
            maplen = mapsize;
            place = tmpmap;
            return true;
        }

        bool open_detail(const char* in) {
            int tmpfd = get_handle(in);
            if (tmpfd == -1) {
                return false;
            }
            size_t tmpsize = 0;
            if (!getfilesizebystat(tmpfd, tmpsize)) {
                ::close(fd);
                return false;
            }
            if (!get_map(tmpfd, (long)tmpsize)) {
                ::close(fd);
                return false;
            }
            return true;
        }

        bool from_fileinput(FILE* fp, size_t size) {
            if (!fp) return false;
            auto tmp = fp_to_native(fp);
            if (tmp == -1) {
                return false;
            }
            if (!get_map(tmp, size)) {
                return false;
            }
            this->fp = fp;
            return true;
        }

        bool close_detail() {
            if (fd == -1) return false;
            munmap(place, maplen);
            if (fp) {
                ::fclose(fp);
                fp = nullptr;
            }
            else {
                ::close(fd);
            }
            fd = -1;
            maplen = 0;
            place = nullptr;
            _size = 0;
            return true;
        }

        bool move_detail(FileMap&& in) {
            fd = in.fd;
            in.fd = -1;
            return true;
        }
#endif
        char* place = nullptr;
        size_t _size = 0;

        bool move_common(FileMap&& in) {
            move_detail(std::forward<FileMap>(in));
            fp = in.fp;
            in.fp = nullptr;
            place = in.place;
            in.place = nullptr;
            _size = in._size;
            in._size = 0;
            return true;
        }

       public:
        FileMap(const char* name) {
            open(name);
        }

#ifdef _WIN32
        FileMap(const wchar_t* name) {
            open(name);
        }
#endif

        ~FileMap() {
            close();
        }

        FileMap(FileMap&& in) noexcept {
            move_common(std::forward<FileMap>(in));
        }

        FileMap(FileInput&& in) {
            if (!in.file) {
                close();
                return;
            }
            if (!from_fileinput(in.file, in.size_cache)) {
                ::fclose(in.file);
            }
            in.file = nullptr;
            in.size_cache = 0;
            in.samepos_cache = 0;
            in.prevpos = 0;
        }

        bool open(const char* name) {
            if (!name) return false;
            return open_detail(name);
        }
#ifdef _WIN32
        bool open(const wchar_t* name) {
            if (!name) return false;
            return open_detail(name);
        }
#endif
        bool close() {
            return close_detail();
        }

        size_t size() const {
            return _size;
        }

        char operator[](size_t pos) const {
            if (!place || _size <= pos) return char();
            return place[pos];
        }

        const char* c_str() const {
            return place;
        }

        bool is_open() const {
            return place != nullptr;
        }
    };
#ifdef _fileno
#undef _fileno
#endif

    struct FileWriter {
       private:
        FILE* fp = nullptr;

       public:
        template <class C>
        FileWriter(C* path, bool add = false) {
            open(path, add);
        }
#ifdef _WIN32
        bool open(const wchar_t* path, bool add = false) {
            FILE* tmp = nullptr;
            _wfopen_s(&tmp, path, add ? L"ab" : L"wb");
            if (!tmp) {
                return false;
            }
            close();
            fp = tmp;
            return true;
        }
#endif

        bool open(const char* path, bool add = false) {
            FILE* tmp = nullptr;
            fopen_s(&tmp, path, add ? "ab" : "wb");
            if (!tmp) {
                return false;
            }
            close();
            fp = tmp;
            return true;
        }

        bool close() {
            if (fp) {
                ::fclose(fp);
                fp = nullptr;
            }
            return true;
        }

        template <class C>
        bool write(C* byte, size_t size) {
            if (!is_open()) return false;
            if (::fwrite(byte, sizeof(C), size, fp) < size) {
                return false;
            }
            return true;
        }

        template <class T>
        bool write(const T& t) {
            return write((const char*)&t, sizeof(T));
        }

        template <class T>
        void push_back(const T& t) {
            write(t);
        }

        bool is_open() const {
            return fp != nullptr;
        }
    };

    struct FileReader {
        FileInput* input = nullptr;
        FileMap* map = nullptr;

        template <class C>
        FileReader(C* in) {
            open(in);
        }

        ~FileReader() noexcept {
            close();
        }

       private:
        void move(FileReader&& in) {
            close();
            input = in.input;
            in.input = nullptr;
            map = in.map;
            in.map = nullptr;
        }

       public:
        FileReader(FileReader&& in) noexcept {
            move(std::forward<FileReader>(in));
        }

        FileReader& operator=(FileReader&& in) {
            move(std::forward<FileReader>(in));
            return *this;
        }

        template <class C>
        bool open_input(C* name) {
            FileInput in = name;
            if (!in.is_open()) {
                return false;
            }
            close();
            input = new FileInput(std::move(in));
            return true;
        }

        template <class C>
        bool open_map(C* name) {
            FileMap in = name;
            if (!in.is_open()) {
                return false;
            }
            close();
            map = new FileMap(std::move(in));
            return true;
        }

        template <class C>
        bool open(C* name) {
            return open_map(name) || open_input(name);
        }

        bool close() {
            if (input) {
                input->close();
                delete input;
                input = nullptr;
            }
            else if (map) {
                map->close();
                delete map;
                map = nullptr;
            }
            return true;
        }

        char operator[](size_t pos) const {
            if (input) {
                return input->operator[](pos);
            }
            else if (map) {
                return map->operator[](pos);
            }
            return char();
        }

        size_t size() const {
            if (input) {
                return input->size();
            }
            else if (map) {
                return map->size();
            }
            return 0;
        }

        bool is_open() const {
            return input || map;
        }

        bool use_filemap() const {
            return map != nullptr;
        }
    };
#ifdef _WIN32
    using path_string = std::wstring;
#else
    using path_string = std::string;
#endif

}  // namespace PROJECT_NAME