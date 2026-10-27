# Third-party notices

The MIT licence in `LICENSE` covers the original code in this repository. The
components below are third-party work and keep their own licences.

## Frotz — `ceapp/frotz/`

`ceapp/frotz/core/` is the portable Frotz Z-machine interpreter core (GPLv2),
unchanged except for two prototypes in `process.c` that were made `static` to
match their definitions. `ceapp/frotz/frotz_ce.c` and `ceapp/frotz/cfile.c` are
the Windows CE front-end and file layer written for this project (MIT, like the
rest of the tree), but they are built and distributed together with the core.

- Licence: GNU General Public License, version 2 — full text in
  `ceapp/frotz/COPYING`.
- Source: the Frotz interpreter by Stefan Jokisch and the Frotz maintainers,
  <https://gitlab.com/DavidGriffith/frotz>.

Because GPLv2 applies to that subdirectory, distributing a binary built from
`ceapp/frotz/` puts the usual GPLv2 obligations on that binary (source
availability, licence text). Nothing outside `ceapp/frotz/` is affected.

## DSi shell — `ceapp/dsshell/`

A port of ThroatyMumbo's WinCE64 shell (`scratch/wince64-ref/bsp/shell`),
touch-adapted for the DSi.

- Licence: MIT — full text in `ceapp/dsshell/LICENSE-ThroatyMumbo.txt`.
- Copyright (c) 2026 ThroatyMumbo.

## BlocksDS ARM7 makefile — `arm7/Makefile`

The ARM7 makefile is the BlocksDS target template.

- Licence: CC0-1.0, per the `SPDX-License-Identifier` header.
- SPDX-FileContributor: Antonio Niño Díaz, 2023.

## Lua — `ceapp/lua/`

`ceapp/lua/lua_ce.c` and `ceapp/lua/shim.c` are the Windows CE front-end and
shim written for this project; `ceapp/lua/inc/` holds small hand-written
header shims. The Lua 5.1.4 core itself is **not** vendored here — it is built
from a separate source drop. Lua is MIT licensed, Copyright (c) 1994-2012 Lua.org,
PUC-Rio.

## Microsoft Windows CE material — not distributed here

This repository does not contain Microsoft code. The CE 5.0 kernel working copy
(`nk/`), the CE 6.0 explorer port (`shell6/`), the Microsoft-derived kernel
assembly (`asm/armtrap.S`, `asm/ExVector.S`, `asm/intrlock.S`, `asm/ksarm.inc`,
`asm/kxarm.inc`), the corelibc/lmem ports (`coredll/crt/`, `coredll/lmem/`), the
CRT `_FILEX` struct body (`compat/crt/filex.h`) and the CE 6 SDK header overlays
(`compat/ce6sdk/`) are covered by the Microsoft Shared Source Licence for Windows
CE and are excluded by `.gitignore`. See `LOCAL-SOURCES.md` for how to restore
them from your own copy.

`coredll/thunks_w32.c`, `coredll/thunks_fs.c` and `dsi/fs20_stubs.h` contain
generated symbol/slot tables and prototypes derived from the CE API surface.
They are regenerated from your own licensed copies by `tools/gen_thunks.py`.
