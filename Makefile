
EE_OBJS = biosdrain.o biosdrain_tex.o OSDInit.o sysman_rpc.o
IRX_OBJS = irx/usbmass_bd_irx.o irx/usbd_irx.o irx/bdm_irx.o irx/bdmfs_vfat_irx.o irx/sysman_irx.o
# Bin2c objects that will be linked in
EE_OBJS += $(IRX_OBJS)

EE_LIBS = -lkernel -lpatches -ldebug -lgraph -ldma -lpacket -ldraw

# Git version
GIT_VERSION := "$(shell git describe --abbrev=4 --dirty --always --tags)"

EE_CFLAGS = -I$(shell pwd) -Werror -DGIT_VERSION="\"$(GIT_VERSION)\""

IRX_C_FILES = usbmass_bd_irx.c bdm_irx.c bdmfs_vfat_irx.c usbd_irx.c sysman_irx.c

EE_BIN_RESET = biosdrain.elf
EE_BIN_NORESET = biosdrain-noreset.elf

ifdef NO_RESET_IOP_WHEN_USB
EE_BIN = $(EE_BIN_NORESET)
EE_CFLAGS += -DNO_RESET_IOP_WHEN_USB
else
EE_BIN = $(EE_BIN_RESET)
endif

all:
	$(MAKE) -C . NO_RESET_IOP_WHEN_USB=1 _all
	rm -f biosdrain.o
	$(MAKE) -C . _all

_all: sysman_irx $(EE_BIN)

# IRX files to be built and or bin2c'd
sysman_irx:
	$(MAKE) -C sysman

irx/sysman_irx.c: sysman/sysman.irx
	bin2c $< irx/sysman_irx.c sysman_irx

irx/usbd_irx.c: $(PS2SDK)/iop/irx/usbd.irx
	bin2c $< irx/usbd_irx.c usbd_irx

irx/usbmass_bd_irx.c: $(PS2SDK)/iop/irx/usbmass_bd.irx
	bin2c $< irx/usbmass_bd_irx.c usbmass_bd_irx

irx/bdm_irx.c: $(PS2SDK)/iop/irx/bdm.irx
	bin2c $< irx/bdm_irx.c bdm_irx

irx/bdmfs_vfat_irx.c: $(PS2SDK)/iop/irx/bdmfs_vfat.irx
	bin2c $< irx/bdmfs_vfat_irx.c bdmfs_vfat_irx

biosdrain_tex.c: biosdrain_tex.tex
	bin2c $< irx/biosdrain_tex.c biosdrain_tex

clean:
	$(MAKE) -C sysman clean
	rm -f $(EE_BIN_RESET) $(EE_BIN_NORESET) $(EE_OBJS) $(IRX_C_FILES)

run: $(EE_BIN)
	ps2client execee host:$(EE_BIN)

wsl: $(EE_BIN)
	$(PCSX2) --elf="$(shell wslpath -w $(shell pwd))/$(EE_BIN)"

emu: $(EE_BIN)
	$(PCSX2) --elf="$(shell pwd)/$(EE_BIN)"

reset:
	ps2client reset
	ps2client netdump

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal
