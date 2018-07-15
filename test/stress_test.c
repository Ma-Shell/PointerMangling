#include <stdio.h>
#include <stdlib.h>

typedef void (*test_fun)();
struct test_struct {
	test_fun fun1;
	test_fun fun2;
};

void __attribute__ ((noinline)) test1() {
	puts("TEST1");
}

void __attribute__ ((noinline)) test2() {
	puts("TEST2");
}

void test() {
	struct test_struct s;
	s.fun1 = &test1;
	s.fun2 = test2;
	s.fun1();
	s.fun2();
	long long x = (long long)test1;
	long long y = (long long)s.fun1;
	s.fun2 = (test_fun)y;
	((test_fun)x)();
	((test_fun)y)();
	s.fun2();
}

int main(int argc, char** argv) {
	unsigned long long num = 100000000;
	if(argc > 1)
		num = strtoull(argv[1], NULL, 10);
	
	for(int i = 0; i < num; i++)
		test();

	return 0;
}