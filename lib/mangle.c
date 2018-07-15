#define _GNU_SOURCE
#include <dlfcn.h>
#include <signal.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>

/*
The following MACROS can be defined:
One of:
	SCHEME_XOR
	SCHEME_D_XOR_H
	SCHEME_D_XOR_H_PLUS
*/

#define SCHEME_SUM SCHEME_XOR + SCHEME_D_XOR_H + SCHEME_D_XOR_H_PLUS

#if SCHEME_SUM > 1
	#error MULTIPLE SCHEMES DEFINED
#endif

#if SCHEME_SUM == 0
	#error NO SCHEMES DEFINED! SET ONE OF SCHEME_XOR, SCHEME_D_XOR_H, SCHEME_D_XOR_H_PLUS
#endif

//===============================================
//               UTILITY FUNCTIONS
//===============================================

inline void get_random(uint64_t* ref)
{
	// read 64 bytes from /dev/urandom into the address at ref
	int f_urand = open("/dev/urandom", O_RDONLY);
	if(f_urand < 0)
	{
		fprintf(stderr, "ERROR: Could not open urandom");
		exit(-1);
	}
	read(f_urand, ref, sizeof(*ref));
}

inline void mask_is_mangled(uint64_t* ref)
{
	// We must be able to decide if a pointer is mangled or not
	// For this reason, we make sure, our XOR-mask starts with 0b01
	// Valid canonical user-space-pointer: 0b00... -> 0b01...
	// Error-Values like -1: 0b11... -> 0b10...
	// -> A pointer starting with two different bits is mangled
	*ref |= 1ull << 62;
	*ref &= -1ull >> 1;
}

//===============================================
//                   SCHEMES
//+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// Each scheme must provide definitions for:
uint64_t mangle(uint64_t ptr);
uint64_t demangle(uint64_t ptr);
uint64_t try_demangle(uint64_t ptr);
// It might additionally register a constructor
// for generating a random secret.
//===============================================

#ifdef SCHEME_XOR
	static uint64_t MAGICXOR;

	void __attribute__((constructor)) generate_magic()
	{
		get_random(&MAGICXOR);
		mask_is_mangled(&MAGICXOR);
	}

	uint64_t mangle(uint64_t ptr)
	{
		return ptr ^ MAGICXOR;
	}
	uint64_t demangle(uint64_t ptr)
	{
		return ptr ^ MAGICXOR;
	}
	uint64_t try_demangle(uint64_t ptr)
	{
		if((ptr >> 63) != ((ptr << 1) >> 63))
			return demangle(ptr);
		else
			return ptr;
	}

#elif defined SCHEME_D_XOR_H
	
	uint64_t mangle(uint64_t ptr)
	{
		uint64_t mask = ((ptr << 48) | (1ull << 62)) & (-1ull >> 1);
		return (ptr ^ mask ^ (mask >> 16) ^ (mask >> 32)) ;
	}
	uint64_t demangle(uint64_t ptr)
	{
		uint64_t mask = ((ptr << 48) | (1ull << 62)) & (-1ull >> 1);
		return ptr ^ mask ^ (mask >> 16) ^ (mask >> 32);
	}
	uint64_t try_demangle(uint64_t ptr)
	{
		if((ptr >> 63) != ((ptr << 1) >> 63))
			return demangle(ptr);
		else
			return ptr;
	}

#elif defined SCHEME_D_XOR_H_PLUS
	static uint64_t MAGICXOR;

	void __attribute__((constructor)) generate_magic()
	{
		get_random(&MAGICXOR);
		mask_is_mangled(&MAGICXOR);
	}

	uint64_t mangle(uint64_t ptr)
	{
		uint64_t mask = (ptr << 50) >> 2;
		mask = mask ^ (mask >> 16) ^ (mask >> 32) ^ MAGICXOR;
		return ptr ^ mask;
	}
	uint64_t demangle(uint64_t ptr)
	{
		ptr = ptr ^ MAGICXOR;
		uint64_t mask = (ptr << 50) >> 2;
		mask = mask ^ (mask >> 16) ^ (mask >> 32);
		return ptr ^ mask;
	}
	uint64_t try_demangle(uint64_t ptr)
	{
		if((ptr >> 63) != ((ptr << 1) >> 63))
			return demangle(ptr);
		else
			return ptr;
	}
#endif


//===============================================
//                 END SCHEMES
//===============================================

uint64_t mangle_range(uint64_t *ptr_start, uint64_t *ptr_stop)
{
	for(;ptr_start >= ptr_stop; ptr_start--)
	{
		*ptr_start = mangle(*ptr_start);
	}
	return 0;
}
uint64_t demangle_range(uint64_t *ptr_start, uint64_t *ptr_stop)
{
	for(;ptr_start >= ptr_stop; ptr_start--)
	{
		*ptr_start = demangle(*ptr_start);
	}
	return 0;
}

//===============================================
//           WRAPPED LIBC-FUNCTIONS
//===============================================

typedef char* (*getenv_t)(const char *name);
static getenv_t orig_getenv = NULL;

char *getenv(const char *name)
{
	if(orig_getenv == NULL)
		orig_getenv = (getenv_t)dlsym(RTLD_NEXT, "getenv");

	environ = (char**)try_demangle((uint64_t)environ);
	for(char** e = environ; *e != NULL; e++)
	{
		*e = (char*)try_demangle((uint64_t)*e);
		if(*e == NULL)
			break;
	}
	char* ret = orig_getenv(name);
	return ret;
}

//===============================================
//   WRAPPERS FOR SYSCALL-WRAPPERS FROM LIBC:
//+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// Syscalls receiving structs that have pointers
// need to be wrapped in order to manually
// demangle the pointers
//===============================================

typedef int (*sigaction_t)(int signum, const struct sigaction *act, struct sigaction* oldac);
static sigaction_t orig_sigaction = NULL;

// Wrapper for original sigaction: Demangles sa_handler
int sigaction(int signum, const struct sigaction *act, struct sigaction* oldac)
{
	struct sigaction my_act;

	// find original sigaction
	if(orig_sigaction == NULL)
		orig_sigaction = (sigaction_t)dlsym(RTLD_NEXT, "sigaction");

	memcpy(&my_act, act, sizeof(struct sigaction));
	my_act.sa_handler = (__sighandler_t)try_demangle((uint64_t)my_act.sa_handler);
	return orig_sigaction(signum, &my_act, oldac);
}

#include <sys/uio.h>
typedef ssize_t (*writev_t)(int fd, const struct iovec *iov, int iovcnt);
static writev_t orig_writev = NULL;

ssize_t writev(int fd, const struct iovec *iov, int iovcnt)
{
	struct iovec my_iov[iovcnt];

	// find original writev
	if(orig_writev == NULL)
		orig_writev = (writev_t)dlsym(RTLD_NEXT, "writev");

	for(int i = 0; i < iovcnt; i++)
	{
		memcpy(&my_iov[i], &iov[i], sizeof(struct iovec));
		my_iov[i].iov_base = (void*)try_demangle((uint64_t)iov[i].iov_base);
	}
	return orig_writev(fd, my_iov, iovcnt);
}
