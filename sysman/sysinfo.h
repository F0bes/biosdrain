#pragma once
#include "../ps2db.h"

// IOP-side stuff
typedef struct SysmanHardwareInfo
{
	t_PS2DBROMHardwareInfo ROMs[3];
	t_PS2DBROMHardwareInfo erom;
	t_PS2DBROMHardwareInfo BOOT_ROM;
	t_PS2DBROMHardwareInfo DVD_ROM;
	t_PS2DBIOPHardwareInfo iop;
	t_PS2DBSSBUSHardwareInfo ssbus;
	t_PS2DBiLinkHardwareInfo iLink;
	t_PS2DBUSBHardwareInfo usb;
	t_PS2DBSPU2HardwareInfo spu2;
	unsigned short int ROMGEN_MonthDate;
	unsigned short int ROMGEN_Year;
	unsigned short int BoardInf;
	unsigned short int MPUBoardID;
} t_SysmanHardwareInfo;
