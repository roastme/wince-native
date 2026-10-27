#!/bin/sh
# syntax-check CE 6 shell (explorer) C++ sources against the CE 6 SDK headers: chk6.sh file.cpp
# R is the workspace root: the parent of this repo (holds ce5-ref/, ce6-ref/, tools/).
R="$(cd "$(dirname "$0")/.." && pwd)"
E=$R/wince-native/shell6; S=$R/ce6-ref/src/WinVol/WINCE600/PUBLIC
exec $R/tools/wfenter.sh /opt/wonderful/toolchain/gcc-arm-none-eabi/bin/arm-none-eabi-g++ -std=gnu++98 -fsyntax-only -fshort-wchar -fpermissive -w -march=armv5te -fms-extensions -fno-exceptions -fno-rtti \
 -D_WIN32 -D_M_ARM=4 -include $R/wince-native/compat/ms_compat.h -include $R/wince-native/compat/ce6_cxx.h -DARM -D_ARM_ -DARMV4I -D_WIN32_CE=0x600 -DUNDER_CE -DUNICODE -D_UNICODE -DWINCEOEM -D_FORCENAMELESSUNION -DWINCE_EMULATOR=0 \
 -I$R/wince-native/compat/ce6sdk -I$E/INC -I$E/MAIN -I$E/TASKBAR -I$S/COMMON/OAK/INC -I$S/COMMON/SDK/INC -I$S/IE/SDK/INC -I$S/SHELLSDK/SDK/INC -I$S/SHELL/OAK/INC -I$S/SHELL/OAK/HPC/INC "$@"
