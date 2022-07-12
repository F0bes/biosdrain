IOP_BIN  = sysman.irx
IOP_OBJS = romdrv/romdrv.o main.o iLink.o dev9.o usb.o spu2.o rom.o imports.o

IOP_CFLAGS += -Werror -fno-builtin -G8192 -mgpopt
IOP_LDFLAGS += -s

all: $(IOP_BIN)

clean:
	rm -f $(IOP_BIN) $(IOP_OBJS)

include $(PS2SDK)/Defs.make
include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.iopglobal
