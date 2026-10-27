# wince-native

Windows CE 5.0 running natively on the DSi's ARM9 — no emulation layer, no MMU,
flat address space.

The CE 5.0 kernel from the shared-source tree is built for ARM946E-S and booted
from a BlocksDS `.nds`. A minimal OAL, a flat PE loader, a corelibc/GWES subset
and a shell bring it up far enough to run real Windows CE / Windows Mobile
binaries off the SD card: the WM6 `coredll.dll` and `filesys.exe`, plus CE apps
built against the public SDK headers (`ceapp/`).

## Disclaimer

This project was created almost entirely by AI and was not tested extensively. Backup NAND before using.

## Layout

| Path | What it is |
|---|---|
| `dsi/` | DSi OAL, boot shim, flat PE loader, FAT/SD service, filesystem API set 20, video |
| `asm/` | Kernel trap/exception entry, interrupt locking, `dsitrap.S` |
| `coredll/` | coredll shims: PSL thunks, stubs, corelibc ports |
| `gwes/` | Mini GWES: window manager, GDI subset, string helpers, exports |
| `shell/` | Built-in shell fallback |
| `app/` | Small CE app linked into the kernel image (PSL trap demo) |
| `ceapp/` | Apps built as separate CE `.exe`s: `dsshell`, `frotz`, `lua`, `hello` |
| `arm7/` | ARM7 side: key/touch sampling and the SD card driver |
| `compat/` | GCC shims for the Microsoft headers and CRT |
| `tools/` | Helpers: thunk generation, PE/ROM manipulation, ARM asm translation |

## What is not in this repository

Everything Microsoft-derived is excluded (see `.gitignore` and
`LOCAL-SOURCES.md`): the CE 5.0 kernel working copy (`nk/`), the CE 6.0 explorer
port (`shell6/`), the Microsoft kernel assembly, the corelibc/lmem ports, the CRT
`FILE` struct body (`compat/crt/filex.h`) and the CE 6 SDK header overlays. Those files are covered by the Microsoft Shared Source
Licence for Windows CE and are not redistributable here. A fresh clone therefore
**does not build** until you supply them from your own licensed copy.

Two more things live outside the repo by design:

- `../ce5-ref/`, `../ce6-ref/` — CE 5.0 / CE 6.0 CD images, extracted
  `OSFiles`, and the Windows Mobile 6 ROM modules (`coredll.dll`,
  `filesys.exe`) that `dsi/wm6blob.S` embeds at build time.
- `../tools/` — the local toolchain runner (`wfenter.sh`), `dsirun`, BIOS and
  the SD-card image used for testing.

`Makefile` and the `chk*.sh` scripts assume that layout: they take `R` to be the
parent directory of this repo. Override it with `make R=/path/to/workspace` if
yours differs.

## Building

Inside a BlocksDS/Wonderful toolchain environment:

```sh
../tools/wfenter.sh make            # kernel ELF
../tools/wfenter.sh make rom        # native.nds (kernel + ARM7)
```

Output lands in `build/` (`native.elf`, `native.nds`, `native.map`).

Syntax-check a single file against the CE headers:

```sh
./chk.sh nk/KERNEL/kwin32.c         # kernel C
./chkxx.sh shell6/MAIN/explorer.cpp # C++ shell sources
```

Build a CE app as a separate `.exe`:

```sh
../tools/wfenter.sh ceapp/build.sh build/ceapp/hello.exe ceapp/hello/hello.c
```

## Running

```sh
./run.sh 300                        # headless melonDS DSi, 300 frames
```

`run.sh` rebuilds `native.nds`, boots it in `tools/dsirun` with the test NAND and
SD image, and writes the build log to `build/native-build.log`.

## Licence

MIT — see `LICENSE`. Third-party components keep their own licences:
`ceapp/frotz/` is GPLv2 (`ceapp/frotz/COPYING`), `ceapp/dsshell/` is MIT
(ThroatyMumbo), `arm7/Makefile` is CC0 (BlocksDS). Details in
`THIRD-PARTY-NOTICES.md`.
