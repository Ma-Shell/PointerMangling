BINDIR = ../bin
CURRENT_DIR = $(shell pwd)
PASS_LIB = $(BINDIR)/mangle/libLLVMMangle.so
PASS_OPTS = -Xclang -load -Xclang $(PASS_LIB)
LIBDIR = $(shell readlink -f ../lib)
LIB_STA = $(LIBDIR)/mangle.a
LIB_DYN = $(LIBDIR)/mangle.so

all: $(BINDIR) $(PASS_LIB) $(LIB) $(LIB_DYN) test

.PHONY: test
test: test_n.out test_n.ll test_ms.out test_md.out test_m.ll test_ms.ll.out test_md.ll.out stress_test_n.ll stress_test_n.out stress_test_ms.out stress_test_md.out stress_test_m.ll

$(LIBDIR)/%.a: $(LIBDIR)/%.c
	$(MAKE) -C $(LIBDIR) $@

$(LIBDIR)/%.so: $(LIBDIR)/%.c
	$(MAKE) -C $(LIBDIR) $@

.PHONY: libs
libs: $(LIB_STA) $(LIB_DYN)

$(BINDIR):
	@echo "Creating bin-dir"
	mkdir $(BINDIR) && cd $(BINDIR) && cmake -g "Unix Makefile" $(CURRENT_DIR)/../pass
	@echo "======================"
	
$(PASS_LIB): ../pass/mangle/Mangle.cpp $(BINDIR)
	@echo "Making Library"
	$(MAKE) -C $(BINDIR)
	@echo "======================"

.PHONY: passlib
passlib: $(PASS_LIB)

%_ms.out: %.c $(BINDIR) $(PASS_LIB) $(LIB_STA)
	../compile_sta.sh $(CFLAGS) $< -o $@
	@echo "======================"

%_md.out: %.c $(BINDIR) $(PASS_LIB) $(LIB_DYN)
	../compile_dyn.sh $(CFLAGS) $< -o $@
	@echo "======================"

%_m.ll: %.c $(BINDIR) $(PASS_LIB)
	../compile_llvm.sh $(CFLAGS) $< -o $@
	@echo "======================"

%_ms.ll.out: %_m.ll $(LIB_STA)
	llc $< -o $<.o -filetype=obj -relocation-model=pic
	clang $(CFLAGS) -fpic $<.o -o $@ $(LIB_STA) -ldl
	@echo "======================"

%_md.ll.out: %_m.ll $(LIB_DYN)
	llc $< -o $<.o -filetype=obj -relocation-model=pic
	clang $(CFLAGS) -fpic $<.o -o $@ $(LIB_DYN) -ldl
	@echo "======================"

%_n.ll: %.c
	clang $(CFLAGS) $< -S -emit-llvm -o $@

%_n.out: %.c
	clang $(CFLAGS) $< -o $@

.PHONY: clean_libs
clean_libs:
	-$(MAKE) -C $(LIBDIR) clean

.PHONY: clean_bindir
clean_bindir:
	-rm -rf $(BINDIR)

.PHONY: clean_test
clean_test:
	-rm *.out *.ll *.s *.o

clean: clean_libs clean_bindir clean_test

