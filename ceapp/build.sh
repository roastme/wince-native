#!/bin/sh
# Build a Windows CE application (PE .exe importing coredll.dll) with GCC.  usage: build.sh out.exe src.c...
# Run inside tools/wfenter.sh. Every symbol the sources leave undefined (other than libgcc's) becomes a coredll import.
set -e
W="$(cd "$(dirname "$0")/.." && pwd)"
R="$(cd "$W/.." && pwd)"
H="$R/ce5-ref/work/cd1x/OSFiles"
CC=/opt/wonderful/toolchain/gcc-arm-none-eabi/bin/arm-none-eabi-gcc
NM=/opt/wonderful/toolchain/gcc-arm-none-eabi/bin/arm-none-eabi-nm
OUT="$1"; shift
O="$W/build/ceapp/$(basename "$OUT" .exe)"
mkdir -p "$O"
CFLAGS="-std=gnu99 -fms-extensions -fshort-wchar -fno-strict-aliasing -fno-common -w -Os -g -ffunction-sections -fdata-sections
  -mcpu=arm946e-s+nofp -marm -ffreestanding -fno-builtin -include $W/compat/ms_compat.h -include $W/ceapp/ceapp_pre.h ${EXTRA_INC:-}
  -DARM -D_ARM_ -D_M_ARM=4 -DARMV4I -D_WIN32_CE=0x500 -D_WIN32_WCE=500 -DUNDER_CE -DUNICODE -D_UNICODE
  -DWIN32_NO_OLE -I$W/ceapp -I$W/compat/ci -I$H/PUBLIC/COMMON/SDK/INC -I$H/SDK/INC -I$H/PUBLIC/COMMON/OAK/INC"
CFLAGS="$CFLAGS ${EXTRA_CFLAGS:-}"
OBJS=""
for s in "$W/ceapp/crt0.c" "$@"; do
  o="$O/$(basename "$s" .c).o"
  $CC $CFLAGS -c "$s" -o "$o"
  OBJS="$OBJS $o"
done
LIBGCC="$($CC -mcpu=arm946e-s+nofp -marm -print-libgcc-file-name)"
# Undefined names not provided by libgcc are coredll imports.
$NM -u $OBJS | awk 'NF==2{print $2} NF==1{print $1}' | grep -v ':$' | sort -u > "$O/undef.txt"
$NM -g --defined-only $OBJS | awk '{print $3}' | sort -u > "$O/def.txt"
$NM -g --defined-only "$LIBGCC" 2>/dev/null | awk '{print $3}' | sort -u > "$O/gcc.txt"
comm -23 "$O/undef.txt" "$O/def.txt" | comm -23 - "$O/gcc.txt" > "$O/imports.txt"
{
  echo '.section .iat,"aw"'; echo '.align 2'; echo '.global __iat_start'; echo '__iat_start:'
  while read -r n; do echo "__imp_$n: .word 0"; done < "$O/imports.txt"
  echo '.text'
  while read -r n; do printf '.global %s\n.type %s,%%function\n%s: ldr ip, 1f_%s\n ldr pc, [ip]\n1f_%s: .word __imp_%s\n' "$n" "$n" "$n" "$n" "$n" "$n"; done < "$O/imports.txt"
} | sed 's/1f_/.Lp_/g' > "$O/stubs.S"
$CC -mcpu=arm946e-s+nofp -marm -c "$O/stubs.S" -o "$O/stubs.o"
$CC -nostdlib -mcpu=arm946e-s+nofp -marm -T "$W/ceapp/ceapp.ld" -Wl,-q -Wl,--gc-sections -Wl,-Map,"$O/app.map" \
  -o "$O/app.elf" $OBJS "$O/stubs.o" "$LIBGCC"
python3 "$W/tools/mkexe.py" "$O/app.elf" "$O/imports.txt" "$OUT"
