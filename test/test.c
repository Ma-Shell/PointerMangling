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


#include <stdio.h>
#include <signal.h>
#include <string.h>

static void sighdlr(int num)
{
	puts("SIGNAL");
}

void test2()
{
	puts("TEST2");
}

void test()
{
	puts("TEST");
	test2();
}

struct struc {
	int foo;
	int** ipp1;
	int* ip1;
	long ipp2;
	long ip2;
};

void p(int idx, struct struc* s)
{
	long val;
	if(idx==0)
		val = *(s->ipp1) - (s->ip1);
	else
		val = (s->ipp2) - (s->ip2);
	printf("SHOULD BE 0: %ld\n", val);
}

void p2(int** ipp, int* ip)
{
	printf("%ld\n", *ipp -ip);
}

int test_optimization_casts()
{
	struct struc s;
	s.ip1 = (int*)0xdeadbeef;
	s.ipp1 = &s.ip1;
	s.ipp2 = 100;
	s.ip2 = 42;
	p(0, &s);

	int* ip = (int*)0xdeadbeef;
	int** ipp = &ip;
	p2(ipp, ip);
	return 0;
}

#include <errno.h>
int main(int argc, char** argv) {
	environ = argv;
	printf("%s\n", getenv("USER"));
	long int a = 0xdeadbeef42434445;
	long int c = 0xabcdef0102030405;
	puts("INITIALIZING POINTERS");
	long int* b = &a;
	long int* d = &c;
	puts("DONE INITIALIZING");
	printf("b @ %p Should be '%016lx': '%016lx'\n", b, a, *b);
	printf("d @ %p Should be '%016lx': '%016lx'\n", d, c, *d);
	puts("FINAL");
	printf("fileno @ %p, stdout @ %p\n", fileno, stdout);
	char* foo = NULL;
	printf("foo @ %p\n", foo);
	printf("argv @ %p\n", argv);
	printf("argv[0] @ %p\n", argv[0]);
	printf("argv[0] -> %s\n", argv[0]);

	test();

	struct sigaction sigact;
	memset(&sigact, 0, sizeof(sigact));
	sigact.sa_handler = sighdlr;
	printf("sa_handler: %p\n", sigact.sa_handler);

	b = (long*) -1;
	printf("b: %p\n", b);
	if (b == (long*) -1)
		puts("SUCCESS! b == -1");
	else
		puts("ERROR! b != -1");

	test_optimization_casts();

	puts("INSTALLING SIGINT-HANDLER");
	sigaction(SIGINT, &sigact, NULL);
	puts("INSTALLED SIGINT-HANDLER");
	puts("press any char to gracefully return or kill the program to test the sighandler");
	getchar();

	return 0;
}

