# Local sources (not distributed)

This repository is the original part of the port. The Microsoft-derived files
that the build links against are covered by the Microsoft Shared Source Licence
for Windows CE and are not redistributed here; `.gitignore` keeps them out. A
fresh clone will not build until you copy them in from your own licensed CE 5.0 /
CE 6.0 source. This file lists what is missing and where each piece came from.

| Missing path | Upstream (in your own copy) | State |
|---|---|---|
| `nk/` | `WINCE500/PRIVATE/WINCEOS/COREOS/NK` plus `COREOS/INC`, `WINCEOS/INC`, `PRIVATE/WINCEOS/INC` | working copy of the CE 5.0 kernel, patched |
| `shell6/` | `WINCE600/PUBLIC/SHELL/OAK/HPC/EXPLORER` | working copy of the CE 6.0 explorer, patched |
| `asm/armtrap.S` | `WINCE500/PRIVATE/WINCEOS/COREOS/NK/KERNEL/ARM/armtrap.s` | ported (`armtrap.S.orig` is the pre-edit snapshot) |
| `asm/ExVector.S` | `.../NK/KERNEL/ARM/exvector.s` | ported |
| `asm/intrlock.S` | `.../NK/KERNEL/ARM/intrlock.s` | ported |
| `asm/ksarm.inc`, `asm/kxarm.inc` | `.../NK/KERNEL/ARM/` | copied |
| `coredll/crt/` | `WINCE500/PRIVATE/WINCEOS/COREOS/CORE/CORELIBC/CRTW32/{CONVERT,STRING,STDIO,FLOAT}/...` | ported (see below) |
| `coredll/lmem/` | `WINCE500/PRIVATE/WINCEOS/COREOS/CORE/LMEM/{heap.c,heap.h}` | ported (see below) |
| `compat/crt/filex.h` | `WINCE500/PRIVATE/WINCEOS/COREOS/CORE/CORELIBC/CRTW32/H/internal.h`, the `_FILEX` struct (the body after `#if !defined(_FILEX_DEFINED)`) | copied struct text; `compat/crt/filex_pre.h` (authored) stays tracked |
| `compat/ce6sdk/` | CE 6.0 SDK `COMMON/SDK/INC/{wtypes.h,intsafe.h}` | overlays: `_VARIANT_BOOL` paste fix, `ui64`/`i64` suffixes to `ULL`/`LL` |
| `compat/ci/` | generated | run `python3 tools_ci.py compat/ci <CE header dirs>` |
| `compat/winpath/` | generated | two symlinks whose names are the literal `..\..\` Windows include paths the shell sources use |
| `../ce5-ref/`, `../ce6-ref/` | your CE 5.0 / CE 6.0 media | reference trees; also hold the WM6 ROM modules |
| `../tools/` | local | `wfenter.sh`, `dsirun`, BIOS, test NAND and SD image |

## Rebuilding the ported files

The ports are close copies of the upstream files with mechanical edits. Copy the
upstream file into place, build, and re-apply the errors:

- MSVC-isms: `try` / `except` / `finally` / `__leave` have no GCC equivalent —
  `compat/ms_compat.h` turns them into plain C (`if (1)`, `else if (0)`), so the
  handler bodies become dead code.
- Pointer-subtraction and `LPCVOID`/`const` mismatches that MSVC accepts and GCC
  rejects.
- `#include "..\inc\x.h"` paths become `"../INC/x.h"`, and file names are fixed
  to the case on disk (`MainWnd.h` → `mainwnd.h`).

For `shell6/` the port also names the underscore-vs-capitalised call spellings
(5,126 sites) and rewrites the SEH blocks; `chk6.sh` reports what is still
missing.

## What is in the repository

Everything that is not in the table above: the DSi OAL and boot shim, the flat
PE loader, the FAT/SD service, the filesystem API set, the mini GWES and coredll
shims, the `compat/` GCC shims, the CE apps under `ceapp/`, the host tools under
`tools/`, and the build scripts.
