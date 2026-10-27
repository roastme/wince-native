#!/bin/sh
# Build the native CE kernel .nds and run it in the headless melonDS DSi runner. usage: run.sh [frames] [make vars...]
R="$(cd "$(dirname "$0")/.." && pwd)"
F="${1:-300}"; shift
LOG="$R/wince-native/build/native-build.log"  # project drive: the root disk is nearly full
"$R/tools/wfenter.sh" sh -c "cd $R/wince-native && make -j8 rom $*" > "$LOG" 2>&1 || { grep -E "rror" "$LOG"; echo "BUILD FAILED"; exit 1; }
exec "$R/tools/dsirun/dsirun" $DSIARGS -d "$R/tools/bios" -n "$R/tools/run/nand.mmc" -F "$R/tools/run/sdroot" -D "$R/tools/run/sd.img" \
  -r "$R/wince-native/build/native.nds" -l 0x02FF8000 -f "$F" 2>&1 | grep -v "^\[melon\]"
