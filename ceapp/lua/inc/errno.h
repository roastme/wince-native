/* A plain errno variable. The toolchain errno.h makes GCC emit a TLS reference, and this CE image has no TLS. */
#ifndef _LUA_CE_ERRNO_H_
#define _LUA_CE_ERRNO_H_

extern int errno;

#define EINTR 4
#define ENOENT 2
#define EACCES 13
#define EEXIST 17
#define EINVAL 22

#endif
