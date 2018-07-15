all: mangle.a mangle.so

%.so: %.c
	clang -DSCHEME_$(SCHEME) $(CFLAGS) -shared $< -o $@ -Os

%.a: %.c
	clang -DSCHEME_$(SCHEME) $(CFLAGS) -c -fPIC $< -o $<.o -Os
	ar rcs $@ $<.o

clean:
	- rm *.o *.a *.so
