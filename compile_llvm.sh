#!/bin/sh

# If the script was executed using "source", CC is set to this script,
# so that "make" uses the mangling plugin
export CC=$(readlink -f $0)
echo "%============================%"
echo exported \$CC=$CC
echo "%============================%"
echo ""

export mangle_dir=$(dirname $(readlink -f $0))
clang -Xclang -load -Xclang $mangle_dir/bin/mangle/libLLVMMangle.so $@ -S -emit-llvm -Wno-unused-command-line-argument
unset mangle_dir
