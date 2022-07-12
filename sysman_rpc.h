#pragma once
#include "sysman/dev9.h"
#include "sysman/iLink.h"
#include "sysman/usb.h"
#include "sysman/sysman_rpc.h"

void SysmanInit(void);
void SysmanDeinit(void);
int SysmanReadMemory(const void *MemoryStart, void *buffer, unsigned int NumBytes, int mode);
int SysmanWriteMemory(void *MemoryStart, const void *buffer, unsigned int NumBytes, int mode);
int SysmanSync(int mode);
int SysmanCalcROMRegionSize(const void *ROMStart);
int SysmanCalcROMChipSize(unsigned int RegionSize);
int SysmanGetMACAddress(unsigned char *MAC_address);

int SysmanGetHardwareInfo(t_SysmanHardwareInfo *hwinfo);
