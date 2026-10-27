#!/bin/sh
# syntax-check CE kernel sources with GCC: chk.sh file.c
# R is the workspace root: the parent of this repo (holds ce5-ref/, ce6-ref/, tools/).
R="$(cd "$(dirname "$0")/.." && pwd)"
export PATH="${WONDERFUL_TOOLCHAIN:-/opt/wonderful}/toolchain/gcc-arm-none-eabi/bin:$PATH"
W=$R/ce5-ref/work/cd1x/OSFiles; NK=$R/wince-native/nk
exec arm-none-eabi-gcc -fsyntax-only -fshort-wchar -w -march=armv5te -std=gnu89 -fms-extensions -include $R/wince-native/compat/ms_compat.h -include $R/wince-native/compat/kernel_pre.h \
 -DARM -D_ARM_ -D_M_ARM=4 -DARMV4I -D_M_ARM_ -D_WIN32_CE=0x500 -DUNDER_CE -DINTERNATIONAL -D_WIN32_WCE=500 -DNOMM -DIN_KERNEL -DWINCEMACRO -DDBGSUPPORT -DUNICODE -D_UNICODE \
 -I$R/wince-native/compat/ci -I$NK/INC -I$NK/KERNEL -I$NK/KERNEL/ARM -I$W/PUBLIC/COMMON/OAK/INC -I$W/PUBLIC/COMMON/SDK/INC -I$W/PUBLIC/COMMON/DDK/INC -I$W/SDK/INC \
 -I$R/ce5-ref/src/WINCE500/PRIVATE/WINCEOS/COREOS/INC -I$R/ce5-ref/src/WINCE500/PRIVATE/WINCEOS/INC "$@"
