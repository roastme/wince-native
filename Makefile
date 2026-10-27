# Native CE kernel for the DSi ARM9. Build inside the BlocksDS env:  ../tools/wfenter.sh make
# R is the workspace root that holds this repo plus ce5-ref/, ce6-ref/ and tools/.
# Override it if your layout differs:  make R=/path/to/workspace
R        ?= $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/..)
CC       := /opt/wonderful/toolchain/gcc-arm-none-eabi/bin/arm-none-eabi-gcc
H        := $(R)/ce5-ref/work/cd1x/OSFiles
NK       := nk
BUILD    := build

CFLAGS   := -std=gnu89 -fms-extensions -fshort-wchar -fno-strict-aliasing -fno-common -w -O1 -g -MMD -MP -ffunction-sections -fdata-sections \
            -mcpu=arm946e-s+nofp -marm -ffreestanding -fno-builtin \
            -include compat/ms_compat.h -include compat/kernel_pre.h \
            -DARM -D_ARM_ -D_M_ARM=4 -DARMV4I -D_WIN32_CE=0x500 -D_WIN32_WCE=500 -DUNDER_CE -DINTERNATIONAL \
            -DNOMM -DDSI_NATIVE -DIN_KERNEL -DWINCEMACRO -DDBGSUPPORT -DUNICODE -D_UNICODE \
            -Icompat/ci -I$(NK)/INC -I$(NK)/COREOS_INC -I$(NK)/WINCEOS_INC -I$(NK)/KERNEL -I$(NK)/KERNEL/ARM \
            -I$(H)/PUBLIC/COMMON/OAK/INC -I$(H)/PUBLIC/COMMON/SDK/INC -I$(H)/PUBLIC/COMMON/DDK/INC -I$(H)/SDK/INC

SRC_C    := $(filter-out $(NK)/KERNEL/cscode.c $(NK)/KERNEL/accchk.c, $(wildcard $(NK)/KERNEL/*.c)) \
            $(NK)/KERNEL/ARM/mdarm.c $(NK)/KERNEL/ARM/unwind.c $(NK)/KERNEL/ARM/strings.c $(NK)/KERNEL/ARM/thmunwnd.c
SRC_S    := asm/armtrap.S asm/ExVector.S asm/intrlock.S asm/dsitrap.S dsi/wm6blob.S
APP_C    := app/app.c app/ce_shim.c gwes/gdi.c gwes/wmgr.c gwes/wstr.c gwes/exports.c shell/wm_shell.c
DSI_C    := dsi/oal.c dsi/start.c dsi/romhdr.c dsi/stubs.c dsi/demo.c dsi/video.c dsi/flatld.c dsi/sd.c dsi/fat.c dsi/fs20.c
OBJ      := $(patsubst %.c,$(BUILD)/%.o,$(SRC_C) $(DSI_C) $(APP_C)) $(patsubst %.S,$(BUILD)/%.o,$(SRC_S))
ASFLAGS  := -mcpu=arm946e-s -mfpu=vfp -mfloat-abi=soft -Iasm -Ink/INC -DARMV4I -DWM6_COREDLL='"$(R)/ce5-ref/work/wm6rom/flat/coredll.dll"' -DWM6_FILESYS='"$(R)/ce5-ref/work/wm6rom/flat/filesys.exe"' $(ASEXTRA)

BLOCKSDS ?= /opt/blocksds/core
SPECS    := $(BLOCKSDS)/sys/crts/dsi_arm9.specs
ARM7ELF  := arm7/arm7.elf

all: $(BUILD)/native.elf

# Kernel + DSi shim linked with the BlocksDS ARM9 crt0/linker script (libnds is only there for crt0 and -lc).
$(BUILD)/native.elf: $(OBJ)
	@echo "  LD $@"
	@$(CC) -mcpu=arm946e-s+nofp -marm -specs=$(SPECS) -Wl,--gc-sections -Wl,--allow-multiple-definition -Wl,-Map,$(BUILD)/native.map -o $@ $(OBJ) \
		-L$(BLOCKSDS)/libs/libnds/lib -Wl,--start-group -lnds9 -lc -lgcc -Wl,--end-group

$(BUILD)/native.nds: $(BUILD)/native.elf $(ARM7ELF)
	@echo "  NDSTOOL $@"
	@$(BLOCKSDS)/tools/ndstool/ndstool -c $@ -7 $(ARM7ELF) -9 $< -b $(BLOCKSDS)/sys/icon.gif "native CE;Windows CE kernel;wince-native" -uc 3 -g WCN0 01 NATIVECE

rom: $(BUILD)/native.nds

-include $(OBJ:.o=.d)

$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	@echo "  CC $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/%.o: %.S
	@mkdir -p $(dir $@)
	@echo "  AS $<"
	@$(CC) $(ASFLAGS) -MMD -MP -x assembler-with-cpp -c $< -o $@

clean:
	rm -rf $(BUILD)
.PHONY: all clean rom

# Application code sees only the public SDK headers (Win32 calls become PSL traps), not the kernel's own headers.
APP_CFLAGS := $(filter-out -include% compat/% -DIN_KERNEL -DDSI_NATIVE,$(CFLAGS))
$(patsubst %.c,$(BUILD)/%.o,$(APP_C)): $(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	@echo "  CC(app) $<"
	@$(CC) $(APP_CFLAGS) -include compat/ms_compat.h -c $< -o $@

$(ARM7ELF): arm7/source/main.c
	@echo "  ARM7 $@"
	@$(MAKE) -C arm7 --no-print-directory

# CE 5 coredll (CORE/DLL) as its own object set. Not linked into native.elf yet: it is built to check that it compiles and to list its undefined symbols.
# WINCEMACRO makes IMPLICIT_CALL/IMPLICIT_DECL expand to kernel trap addresses; -fgnu89-inline makes the _inline helpers in kfuncs.h emit bodies.
CDLL     := $(R)/ce5-ref/src/WINCE500/PRIVATE/WINCEOS/COREOS/CORE/DLL
CDLL_C   := apis.c cnnclpth.c coresecchk.c cscode.c random.c strings.c strsafew.c time.c username.c
CDLL_CPP := ceptrcoredll.cpp coredll.cpp
CDLL_OBJ := $(addprefix $(BUILD)/coredll/,$(CDLL_C:.c=.o) $(CDLL_CPP:.cpp=.o) thunks.o thunks_w32.o thunks_fs.o stubs.o mem.o wcs.o nls.o)
LMEM     := $(R)/ce5-ref/src/WINCE500/PRIVATE/WINCEOS/COREOS/CORE/LMEM
LMEM_OBJ := $(addprefix $(BUILD)/coredll/lmem/,heap.o rmem.o)
CRT      := $(R)/ce5-ref/src/WINCE500/PRIVATE/WINCEOS/COREOS/CORE/CORELIBC/CRTW32
CRT_PORT := CONVERT/wcstol.c STRING/strlen.c STDIO/_filbuf.c STDIO/fgets.c STDIO/fputs.c STDIO/fseeki64.c STDIO/ftelli64.c STDIO/getwclk.c STDIO/putwclk.c STDIO/stioinit.c STDIO/ungetwclk.c FLOAT/fltcvt.c
CRT_SRC  := $(filter-out $(CRT_PORT:%=$(CRT)/%),$(wildcard $(CRT)/CONVERT/*.c $(CRT)/STRING/*.c $(CRT)/MISC/*.c $(CRT)/STRSAFE/*.c $(CRT)/LOWIO/*.c $(CRT)/HEAP/*.c $(CRT)/STDIO/*.c))
# C++ CRT sources the coredll build needs: _newmode (new_mode.cpp), _callnewh (handler.cpp)
CRT_CXX  := HEAP/new_mode.cpp HEAP/handler.cpp
CRT_OBJ  := $(patsubst $(CRT)/%.c,$(BUILD)/coredll/crt/%.o,$(CRT_SRC)) $(patsubst %.c,$(BUILD)/coredll/crt/%.o,$(CRT_PORT)) $(patsubst %.cpp,$(BUILD)/coredll/crt/%.o,$(CRT_CXX))
CXX      := $(patsubst %gcc,%g++,$(CC))
CDLL_FLAGS := -w -march=armv5te -fms-extensions -fshort-wchar -fpermissive -fno-exceptions -fno-rtti -fgnu89-inline \
            -include compat/ms_compat.h -include windows.h -D_WIN32 -D_M_ARM=4 -DEDB -DARM -D_ARM_ -DARMV4I \
            -D_WIN32_CE=0x500 -DUNDER_CE -DUNICODE -D_UNICODE -DWINCEOEM -DWINCEMACRO -D_FORCENAMELESSUNION \
            -Icompat/cshim -Icompat/ce6sdk -Icompat/winpath -Icompat/ci \
            -I$(H)/PUBLIC/COMMON/SDK/INC -I$(H)/PUBLIC/COMMON/OAK/INC \
            -I$(R)/ce5-ref/src/WINCE500/PRIVATE/WINCEOS/COREOS/CORE/INC \
            -I$(R)/ce5-ref/src/WINCE500/PRIVATE/WINCEOS/COREOS/NK/INC \
            -I$(R)/ce5-ref/src/WINCE500/PRIVATE/WINCEOS/COREOS/GWE/INC -I$(CDLL)

coredll-objs: $(CDLL_OBJ) $(LMEM_OBJ) $(CRT_OBJ)
$(BUILD)/coredll/%.o: $(CDLL)/%.c
	@mkdir -p $(dir $@)
	@echo "  CC $<"
	@$(CC) -std=gnu99 $(CDLL_FLAGS) -c $< -o $@
$(BUILD)/coredll/lmem/%.o: coredll/lmem/%.c
	@mkdir -p $(dir $@)
	@echo "  CC $<"
	@$(CC) -std=gnu99 $(CDLL_FLAGS) -I$(LMEM) -c $< -o $@

$(BUILD)/coredll/lmem/rmem.o: $(LMEM)/rmem.c
	@mkdir -p $(dir $@)
	@echo "  CC $<"
	@$(CC) -std=gnu99 $(CDLL_FLAGS) -I$(LMEM) -c $< -o $@

$(BUILD)/coredll/crt/%.o: $(CRT)/%.c
	@mkdir -p $(dir $@)
	@echo "  CC $<"
	@$(CC) -std=gnu99 -Icompat/crt -include compat/crt/filex_pre.h $(CDLL_FLAGS) -I$(CRT)/H -I$(dir $<) -include compat/crt/filex.h -D_CRTBLD -DWINCEINTERNAL -c $< -o $@

$(BUILD)/coredll/crt/%.o: $(CRT)/%.cpp
	@mkdir -p $(dir $@)
	@echo "  CXX $<"
	@$(CXX) -std=gnu++98 -Icompat/crt -include compat/crt/filex_pre.h $(CDLL_FLAGS) -I$(CRT)/H -I$(dir $<) -include compat/crt/filex.h -D_CRTBLD -DWINCEINTERNAL -c $< -o $@

$(patsubst %.c,$(BUILD)/coredll/crt/%.o,$(CRT_PORT)): $(BUILD)/coredll/crt/%.o: coredll/crt/%.c
	@mkdir -p $(dir $@)
	@echo "  CC $<"
	@$(CC) -std=gnu99 -Icompat/crt -include compat/crt/filex_pre.h $(CDLL_FLAGS) -I$(CRT)/H -I$(dir $<) -include compat/crt/filex.h -D_CRTBLD -DWINCEINTERNAL -c $< -o $@

$(BUILD)/coredll/nls.o: coredll/crt/nls.c
	@mkdir -p $(dir $@)
	@echo "  CC $<"
	@$(CC) -std=gnu99 $(CDLL_FLAGS) -c $< -o $@

$(BUILD)/coredll/wcs.o: coredll/crt/wcs.c
	@mkdir -p $(dir $@)
	@echo "  CC $<"
	@$(CC) -std=gnu99 $(CDLL_FLAGS) -c $< -o $@

$(BUILD)/coredll/mem.o: coredll/crt/mem.c
	@mkdir -p $(dir $@)
	@echo "  CC $<"
	@$(CC) -std=gnu99 $(CDLL_FLAGS) -fno-tree-loop-distribute-patterns -c $< -o $@

$(BUILD)/coredll/thunks_w32.o: coredll/thunks_w32.c
	@mkdir -p $(dir $@)
	@echo "  CC $<"
	@$(CC) -std=gnu99 $(CDLL_FLAGS) -c $< -o $@

$(BUILD)/coredll/thunks_fs.o: coredll/thunks_fs.c
	@mkdir -p $(dir $@)
	@echo "  CC $<"
	@$(CC) -std=gnu99 $(CDLL_FLAGS) -c $< -o $@

$(BUILD)/coredll/stubs.o: coredll/stubs.c
	@mkdir -p $(dir $@)
	@echo "  CC $<"
	@$(CC) -std=gnu99 $(CDLL_FLAGS) -c $< -o $@

$(BUILD)/coredll/thunks.o: coredll/thunks.c
	@mkdir -p $(dir $@)
	@echo "  CC $<"
	@$(CC) -std=gnu99 $(CDLL_FLAGS) -c $< -o $@
$(BUILD)/coredll/%.o: $(CDLL)/%.cpp
	@mkdir -p $(dir $@)
	@echo "  CXX $<"
	@$(CXX) -std=gnu++98 $(CDLL_FLAGS) -c $< -o $@
.PHONY: coredll-objs
