#define SYSMAN_RPC_NUM 0x00003913

#define MEM_IO_BLOCK_SIZE 0x20000

struct MemoryAccessParameters
{
	void *StartAddress;
	void *buffer;
	unsigned int NumBytes;
};

/* RPC function numbers */
enum SYSMAN_RPC_Functions
{
	SYSMAN_ReadMemory = 0x01,
	SYSMAN_WriteMemory,
	SYSMAN_CalcROMRegionSize,
	SYSMAN_CalcROMChipSize,
	SYSMAN_GetHardwareInfo,
	SYSMAN_GetMACAddress
};
