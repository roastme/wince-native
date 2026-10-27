#!/bin/sh
# syntax-check shell (explorer) C++ sources: chkxx.sh file.cpp [extra flags]
# R is the workspace root: the parent of this repo (holds ce5-ref/, ce6-ref/, tools/).
R="$(cd "$(dirname "$0")/.." && pwd)"
E=$R/ce6-ref/src/WinVol/WINCE600/PUBLIC/SHELL/OAK/HPC/EXPLORER; W=$R/ce5-ref/work/cd1x/OSFiles
exec $R/tools/wfenter.sh /opt/wonderful/toolchain/gcc-arm-none-eabi/bin/arm-none-eabi-g++ -fsyntax-only -fshort-wchar -fpermissive -w -march=armv5te -fms-extensions -fno-exceptions -fno-rtti \
 -DARM -D_ARM_ -DARMV4I -D_WIN32_CE=0x500 -DUNDER_CE -DUNICODE -D_UNICODE -include $R/wince-native/compat/ms_compat.h \
 -I$E/INC -I$E/MAIN -I$E/TASKBAR -I$W/PUBLIC/COMMON/OAK/INC -I$W/PUBLIC/COMMON/SDK/INC -I$W/SDK/INC "$@"
