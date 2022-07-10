EE_OBJS	= biosdrain.o biosdrainirx_irx.o usbmass_bd_irx.o usbd_irx.o bdm_irx.o bdmfs_vfat_irx.o
EE_BIN = biosdrain.elf
EE_LIBS = -lkernel -lpatches -ldebug -lgraph
EE_CFLAGS = -Werror
IRX_C_FILES = biosdrainirx.c usbmass_bd_irx.c bdm_irx.c bdmfs_vfat_irx.c usbd_irx.c

all: $(EE_BIN)

biosdrainirx_irx.c:
	$(MAKE) -C irx
	bin2c irx/biosdrainirx.irx biosdrainirx_irx.c biosdrainirx_irx

usbd_irx.c: $(PS2SDK)/iop/irx/usbd.irx
	bin2c $< usbd_irx.c usbd_irx

usbmass_bd_irx.c: $(PS2SDK)/iop/irx/usbmass_bd.irx
	bin2c $< usbmass_bd_irx.c usbmass_bd_irx

bdm_irx.c: $(PS2SDK)/iop/irx/bdm.irx
	bin2c $< bdm_irx.c bdm_irx

bdmfs_vfat_irx.c: $(PS2SDK)/iop/irx/bdmfs_vfat.irx
	bin2c $< bdmfs_vfat_irx.c bdmfs_vfat_irx

bdm:
	bin2c irx_deps/bdm.irx bdm_irx.c bdm
	bin2c irx_deps/bdmfs_vfat.irx bdmfs_vfat_irx.c bdmfs_vfat

clean:
	$(MAKE) -C irx clean
	rm -f $(EE_BIN) $(EE_OBJS) $(IRX_C_FILES)

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
