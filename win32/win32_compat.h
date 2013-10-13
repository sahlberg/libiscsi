/*
Copyright (c) 2006 by Dan Kennedy.
Copyright (c) 2006 by Juliusz Chroboczek.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/
/*Adaptions by memphiz@xbmc.org*/

#ifdef WIN32
#ifndef win32_COMPAT_H_
#define win32_COMPAT_H_
#define NO_IPv6 1

#include <winsock2.h>
#include <ws2tcpip.h>
#include <Ws2ipdef.h>
#include <basetsd.h>
#include <io.h>
#include <malloc.h>
#include <sys/stat.h>

#define SOL_TCP IPPROTO_TCP

typedef int ssize_t;
typedef int uid_t;
typedef int gid_t;
typedef int socklen_t;

/* Wrapper macros to call misc. functions win32 is missing */
#define writev win32_writev
#define readv win32_readv
#define poll(x, y, z)        win32_poll(x, y, z)
#define inet_pton(x,y,z)     win32_inet_pton(x,y,z)
#define sleep(x)             Sleep(x * 1000)
#define snprintf(a, b, c, ...) _snprintf_s(a, b, b, c, ## __VA_ARGS__)
int     win32_inet_pton(int af, const char * src, void * dst);
int     win32_poll(struct pollfd *fds, unsigned int nfsd, int timeout);
int     win32_gettimeofday(struct timeval *tv, struct timezone *tz);

struct iovec {
    void *iov_base;
    size_t iov_len;
};

#define inline

#endif//win32_COMPAT_H_
#endif//WIN32
