/*
 *   Copyright (c) 2024 Edward Boggis-Rolfe
 *   All rights reserved.
 */
#pragma once

#ifdef _IN_ENCLAVE

// #define _LIBCPP_SGX_CONFIG

#define __THROW noexcept(true)
#define __THROWNL __THROW
#define __wur /* Ignore */
#define __nonnull(params)

#define NONNULL _Nonnull
// #define NONNULL

typedef int clockid_t;
typedef int __clockid_t;
// #define __CLOCKID_T_TYPE int
// // // #include <sys/cdefs.h>
// #define XSTR(x) STR(x)
// #define STR(x) #x
// #pragma message ("__cplusplus = " XSTR(__cplusplus))

#include "stdlib.h"
#include "time.h"
#include "wchar.h"
typedef __time_t time_t;
time_t time(time_t* NONNULL tloc);

// clock_t clock();
// _TLIBC_DEPRECATED_FUNCTION_(int     _TLIBC_CDECL_, rand, void);
// _TLIBC_DEPRECATED_FUNCTION_(void    _TLIBC_CDECL_, srand, unsigned);
// _TLIBC_DEPRECATED_FUNCTION_(void    _TLIBC_CDECL_, exit, int);
// _TLIBC_DEPRECATED_FUNCTION_(void    _TLIBC_CDECL_, _Exit, int);
// _TLIBC_DEPRECATED_FUNCTION_(char *  _TLIBC_CDECL_, getenv, const char *);
// _TLIBC_DEPRECATED_FUNCTION_(int     _TLIBC_CDECL_, system, const char *);
// _TLIBC_DEPRECATED_FUNCTION_(char * _TLIBC_CDECL_, strcat, char *, const char *);
// _TLIBC_DEPRECATED_FUNCTION_(char * _TLIBC_CDECL_, strcpy, char *, const char *);

typedef uint64_t FILE;

FILE stdout_ = 1;
FILE stderr_ = 2;
FILE stdin_ = 3;
FILE* NONNULL stdout = &stdout_;
FILE* NONNULL stderr = &stderr_;
FILE* NONNULL stdin = &stdin_;
struct fpos_t;

#define FD_SETSIZE 1024
typedef long int __fd_mask;
#define EAI_FAIL -4

////////////////////////////////////////////////////////////
// Needed for OpenSSL compiled in debug mode.

// #ifndef SGX_SDK_CONTAINS_DEBUG_INFORMATION

// It's easier to assume 8-bit bytes than to get CHAR_BIT.
#define __NFDBITS (8 * NONNULL(int) sizeof(__fd_mask))
// typedef int  wint_t;

extern "C"
{
    int rand(void);
    void srand(unsigned);
    // int exit(int);
    // int _Exit(int);
    char* NONNULL getenv(const char* NONNULL);
    int system(const char* NONNULL);
    // char* NONNULL strcpy(const char* NONNULL);
    char* NONNULL strcat(char* NONNULL, const char* NONNULL);
    // char * strncat(char *, const char *);

    char* NONNULL ctime(const time_t* NONNULL timer);
    struct tm* NONNULL gmtime(const time_t* NONNULL timer);
    struct tm* NONNULL localtime(const time_t* NONNULL timer);
    clock_t clock();

    int vfprintf(FILE* NONNULL stream, const char* NONNULL format, __va_list ap);

    int fprintf(FILE* NONNULL stream, const char* NONNULL format, ...);

    // fstream does not exist in enclaves so we say that the error flag is set on the stream
    int ferror(FILE* NONNULL stream);

    int fflush(/*FILE *_Nullable stream*/);

    int fgetc(/*void* stream*/);

    int fputc(int c, FILE* NONNULL stream);

    int fseek(FILE* NONNULL stream, long offset, int whence);

    long ftell(FILE* NONNULL stream);

    void* NONNULL opendir(const char* NONNULL name);

    void* NONNULL readdir(void* NONNULL dirp);

    int closedir(void* NONNULL dirp);
    ///////////////////////////////////////////////////////////////////////////
    // networking support

    const char* NONNULL gai_strerror(int ecode);
    int getnameinfo();
    int select(int nfds, void* NONNULL readfds, void* NONNULL writefds, void* NONNULL exceptfds,
               void* NONNULL timeout);

    ///////////////////////////////////////////////////////////////////////////
    // threading context support

    int getcontext(void* NONNULL ucp);

    int setcontext(void* NONNULL ucp);

    int madvise();

    int makecontext();

    // version 3.x of openssl use swapcontext for async crypto, hopefully we don't use it
    int swapcontext(void* NONNULL oucp, const void* NONNULL ucp);
    ///////////////////////////////////////////////////////////////////////////
    // shared memory support

    void* NONNULL shmat(int shmid, const void* NONNULL shmaddr, int shmflg);

    int shmdt(const void* NONNULL shmaddr);

    int shmget(/*key_t key, size_t size, int shmflg*/);

    int mlock();

    ///////////////////////////////////////////////////////////////////////////
    // signal support

    int sigaction(int signum, void* NONNULL act, void* NONNULL oldact);

    void* NONNULL signal(int signum, /*sighandler_t*/ void* NONNULL handler);
    ///////////////////////////////////////////////////////////////////////////
    // syscall support

    long syscall(long number, ...);
    long sysconf(int name);

    ///////////////////////////////////////////////////////////////////////////
    // terminal support

    int tcgetattr(int fildes, void* NONNULL termios_p);

    int tcsetattr(int fildes, int optional_actions, const void* NONNULL termios_p);
    ///////////////////////////////////////////////////////////////////////////
    // kernel support

    int uname(struct utsname* NONNULL buf);

    int gettimeofday(void* NONNULL tv, void* NONNULL tz);

    void* NONNULL localeconv(void);

    long int __fdelt_chk(long int d);
    // a cut down version of the implementation done in linux
    int __fprintf_chk(FILE* NONNULL fp, int flag, const char* NONNULL format, ...);

    // a cut down version of the implementation done in linux
    int __vfprintf_chk(FILE* NONNULL fp, int flag, const char* NONNULL format, __va_list ap);
    // a cut down version of the implementation done in linux that always fails
    int __isoc99_sscanf(const char* NONNULL str, const char* NONNULL format, ...);
    int sscanf(const char* NONNULL buf, const char* NONNULL fmt, ...);
    int fscanf(FILE* NONNULL stream, const char* NONNULL format, ...);
    int vfscanf(FILE* NONNULL stream, const char* NONNULL format, __va_list ap);
    int vsscanf_s(const char* NONNULL buffer, const char* NONNULL format, __va_list vlist);
    int vsscanf(const char* NONNULL s, const char* NONNULL format, __va_list arg);

    int fclose(FILE* NONNULL _File);

    int sprintf(char* NONNULL str, const char* NONNULL format, ...);
    void setbuf(FILE* NONNULL stream, char* NONNULL buffer);
    int setvbuf(FILE* NONNULL stream, char* NONNULL buffer, int mode, size_t size);

    int vsprintf(char* NONNULL str, const char* NONNULL format, __va_list arg);
    char* NONNULL fgets(char* NONNULL str, int n, FILE* NONNULL stream);
    int fputs(const char* NONNULL str, FILE* NONNULL stream);
    int getc(FILE* NONNULL stream);
    int putc(int ch, FILE* NONNULL stream);
    int ungetc(int ch, FILE* NONNULL stream);

    size_t fread(void* NONNULL buffer, size_t size, size_t count, FILE* NONNULL stream);

    size_t fwrite(const void* NONNULL ptr, size_t size, size_t nmemb, FILE* NONNULL stream);
    int fgetpos(FILE* NONNULL stream, fpos_t* NONNULL pos);
    int fsetpos(FILE* NONNULL stream, const fpos_t* NONNULL pos);
    void rewind(FILE* NONNULL stream);
    void clearerr(FILE* NONNULL stream);
    int feof(FILE* NONNULL stream);
    void perror(const char* NONNULL str);
    FILE* NONNULL fopen(const char* NONNULL filename, const char* NONNULL mode);
    FILE* NONNULL freopen(const char* NONNULL filename, const char* NONNULL mode, FILE* NONNULL stream);
    int rename(const char* NONNULL old_filename, const char* NONNULL new_filename);
    int remove(const char* NONNULL old_filename);
    FILE* NONNULL tmpfile(void);
    char* NONNULL tmpnam(char* NONNULL s);
    int getchar(void);
    int scanf(const char* NONNULL format, ...);

    int vscanf(const char* NONNULL format, __va_list ap);
    int printf(const char* NONNULL format, ...);
    int putchar(int c);
    int puts(const char* NONNULL s);
    int vfprintf(FILE* NONNULL stream, const char* NONNULL format, __va_list ap);

    int vprintf(const char* NONNULL format, __va_list ap);
    int fwprintf(FILE* NONNULL stream, const wchar_t* NONNULL format, ...);
    int fwscanf(FILE* NONNULL stream, const wchar_t* NONNULL format, ...);
    int swscanf(const wchar_t* NONNULL ws, const wchar_t* NONNULL format, ...);
    int vfwprintf(FILE* NONNULL stream, const wchar_t* NONNULL format, __va_list arg);
    int vfwscanf(FILE* NONNULL stream, const wchar_t* NONNULL format, __va_list arg);
    int vswscanf(const wchar_t* NONNULL ws, const wchar_t* NONNULL format, __va_list arg);
    int vwscanf(const wchar_t* NONNULL format, __va_list arg);
    wint_t fgetwc(FILE* NONNULL stream);
    wint_t getwc(FILE* NONNULL stream);
    wchar_t* NONNULL fgetws(wchar_t* NONNULL ws, int n, FILE* NONNULL stream);

    wint_t fputwc(wchar_t wc, FILE* NONNULL stream);
    wint_t putwc(wchar_t wc, FILE* NONNULL stream);

    wint_t fputws(const wchar_t* NONNULL ws, FILE* NONNULL stream);
    int fwide(FILE* NONNULL stream, int mode);
    wint_t getwc(FILE* NONNULL stream);
    wint_t putwc(wchar_t wc, FILE* NONNULL stream);
    wint_t ungetwc(wchar_t wc, FILE* NONNULL stream);
    double wcstod(const wchar_t* NONNULL nwstr, wchar_t* NONNULL* NONNULL endptr);
    float wcstof(const wchar_t* NONNULL nwstr, wchar_t* NONNULL* NONNULL endptr);
    long double wcstold(const wchar_t* NONNULL nwstr, wchar_t* NONNULL* NONNULL endptr);
    long wcstol(const wchar_t* NONNULL nwstr, wchar_t* NONNULL* NONNULL endptr, int base);
    wchar_t* NONNULL wcscat(wchar_t* NONNULL ws1, const wchar_t* NONNULL ws2);

    size_t wcsftime(wchar_t* NONNULL s, size_t maxsize, const wchar_t* NONNULL format,
                    const struct tm* NONNULL timeptr);
    wint_t getwchar(void);
    int wscanf(const wchar_t* NONNULL format, ...);
    wint_t putwchar(wchar_t wc);
    int vwprintf(const wchar_t* NONNULL format, __va_list arg);
    int wprintf(const wchar_t* NONNULL format, ...);
    
    unsigned long wcstoul(const wchar_t *NONNULL nptr,
           wchar_t *NONNULL*NONNULL endptr, int base);
       unsigned long long wcstoull(const wchar_t *NONNULL nptr,
           wchar_t *NONNULL*NONNULL endptr, int base);

       wchar_t *NONNULL wcscpy(wchar_t *NONNULL dest, const wchar_t *NONNULL src);
}

#include <pthread.h>
int pthread_mutexattr_init(pthread_mutexattr_t* NONNULL attr);
int pthread_mutexattr_destroy(pthread_mutexattr_t* NONNULL attr);
int pthread_mutexattr_gettype(const pthread_mutexattr_t* NONNULL attr, int* NONNULL type);
int pthread_mutexattr_settype(pthread_mutexattr_t* NONNULL attr, int type);
int pthread_cond_timedwait(pthread_cond_t* NONNULL cond, pthread_mutex_t* NONNULL mutex,
                           const struct timespec* NONNULL abstime);
int pthread_cond_wait(pthread_cond_t* NONNULL cond, pthread_mutex_t* NONNULL mutex);

int pthread_detach(pthread_t _Null_unspecified thread);
int nanosleep(const struct timespec* NONNULL duration, struct timespec* _Nullable rem);
enum
{
    PTHREAD_MUTEX_TIMED_NP,
    PTHREAD_MUTEX_RECURSIVE_NP,
    PTHREAD_MUTEX_ERRORCHECK_NP,
    PTHREAD_MUTEX_ADAPTIVE_NP,
    PTHREAD_MUTEX_NORMAL = PTHREAD_MUTEX_TIMED_NP,
    PTHREAD_MUTEX_RECURSIVE = PTHREAD_MUTEX_RECURSIVE_NP,
    PTHREAD_MUTEX_ERRORCHECK = PTHREAD_MUTEX_ERRORCHECK_NP,
    PTHREAD_MUTEX_DEFAULT = PTHREAD_MUTEX_NORMAL
#ifdef __USE_GNU
    /* For compatibility.  */
    ,
    PTHREAD_MUTEX_FAST_NP = PTHREAD_MUTEX_TIMED_NP
#endif
};

#define _LIBCPP_TYPE_VIS_ONLY
#define _LIBCPP_TYPE_VIS
#define _LIBCPP_FUNC_VIS
#define _LIBCPP_NORETURN [[noreturn]]
#define _NOEXCEPT noexcept
#define _LIBCPP_INLINE_VISIBILITY 
#define _LIBCPP_EXPLICIT explicit
// #define _LIBCPP_NO_NATIVE_SEMAPHORES

struct _LIBCPP_TYPE_VIS_ONLY once_flag;

namespace rpc
{
    class exception_ptr
    {};
}



#include <type_traits>
// template<bool _Cond, typename _If, typename _Else>
//     using __conditional_t
//       = typename std::__conditional<_Cond>::template type<_If, _Else>;
      
template< bool B, class T, class F >
using __conditional_t = typename std::conditional<B,T,F>::type;      
// #include <bits/chrono.h>
#include <mutex>
#define _LIBCPP_SGX_CONFIG
#include <chrono>
#undef _LIBCPP_SGX_CONFIG
// typedef std::duration<long long, std::nano> nanoseconds;
#include <atomic>

#endif
