EE_BIN = biosdrain.elf
EE_OBJS = biosdrain.o OSDInit.o sysman_rpc.o ui/menu.o dump.o modelname.o
EE_OBJS += ui/fontqueue.o ui/tex/font/font_tex.o ui/tex/font/font_pallete_tex.o ui/fontengine.o
EE_OBJS += ui/graphic.o ui/graphic_vu.o ui/tex/bongo_tex_1.o ui/tex/bongo_tex_2.o ui/tex/biosdrain_tex.o
IRX_OBJS = irx/usbmass_bd_irx.o irx/usbd_irx.o irx/bdm_irx.o irx/bdmfs_vfat_irx.o irx/sysman_irx.o
# Bin2c objects that will be linked in
EE_OBJS += $(IRX_OBJS)
EE_LIBS = -lkernel -lpatches -ldebug -lgraph -ldma -ldraw

EE_DVP = dvp-as

# Git version
GIT_VERSION := "$(shell git describe --abbrev=4 --always --tags)"

EE_CFLAGS = -I$(shell pwd) -Werror -DGIT_VERSION="\"$(GIT_VERSION)\""

IRX_C_FILES = usbmass_bd_irx.c bdm_irx.c bdmfs_vfat_irx.c usbd_irx.c sysman_irx.c

all: sysman_irx $(EE_BIN)

# IRX files to be built and or bin2c'd
sysman_irx:
	$(MAKE) -C sysman

irx/sysman_irx.c: sysman/sysman.irx
	bin2c $< $@ sysman_irx

irx/usbd_irx.c: $(PS2SDK)/iop/irx/usbd.irx
	bin2c $< $@ usbd_irx

irx/usbmass_bd_irx.c: $(PS2SDK)/iop/irx/usbmass_bd.irx
	bin2c $< $@ usbmass_bd_irx

irx/bdm_irx.c: $(PS2SDK)/iop/irx/bdm.irx
	bin2c $< $@ bdm_irx

irx/bdmfs_vfat_irx.c: $(PS2SDK)/iop/irx/bdmfs_vfat.irx
	bin2c $< $@ bdmfs_vfat_irx

ui/tex/biosdrain_tex.c: ui/tex/biosdrain_tex.raw
	bin2c $< $@ biosdrain_tex

ui/tex/bongo_tex_1.c: ui/tex/bongo_tex_1.raw
	bin2c $< $@ bongo_tex_1

ui/tex/bongo_tex_2.c: ui/tex/bongo_tex_2.raw
	bin2c $< $@ bongo_tex_2

ui/tex/font/font_tex.c: ui/tex/font/font_tex.raw
	bin2c $< $@ font_tex

ui/tex/font/font_pallete_tex.c: ui/tex/font/font_pallete_tex.raw
	bin2c $< $@ font_pallete_tex

%.o: %.vsm
	$(EE_DVP) $< -o $@

clean:
	$(MAKE) -C sysman clean
	rm -f $(EE_OBJS) $(IRX_C_FILES)

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
