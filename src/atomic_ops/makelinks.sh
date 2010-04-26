#!/bin/sh
if ! test -h ia32; then
   ln -s ./asm_linkage/usr/src/uts/intel/ia32
fi
if ! test -h intel; then
    ln -s ./asm_linkage/usr/src/uts/intel
fi
if ! test -h sparc; then
    ln -s ./asm_linkage/usr/src/uts/sparc
fi

