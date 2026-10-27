/* The CE SDK has no signal.h. lua.c only uses signal() for SIGINT, and luace.h turns signal() into nothing. */
#ifndef _LUA_CE_SIGNAL_H_
#define _LUA_CE_SIGNAL_H_

#define SIGINT 2
#define SIG_DFL ((void (*)(int))0)
#define SIG_IGN ((void (*)(int))1)

typedef int sig_atomic_t;

#endif
