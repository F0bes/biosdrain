#define DEBUG 1
#ifdef DEBUG
#define DEBUG_PRINTF(args...) printf(args)
#else
#define DEBUG_PRINTF(args...)
#endif

int SysmanCalcROMRegionSize(const void *ROMStart);
int SysmanCalcROMChipSize(unsigned int RegionSize);
int SysmanGetMACAddress(unsigned char *MAC_address);
