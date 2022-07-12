#include <errno.h>
#include <iopheap.h>
#include <kernel.h>
#include <sifrpc.h>
#include <string.h>

#include "sysman/sysinfo.h"
#include "sysman_rpc.h"

static SifRpcClientData_t SYSMAN_rpc_cd;
static unsigned char ReceiveBuffer[256] ALIGNED(64);
static unsigned char TransmitBuffer[64] ALIGNED(64);

void SysmanInit(void)
{
	while (SifBindRpc(&SYSMAN_rpc_cd, SYSMAN_RPC_NUM, 0) < 0 || SYSMAN_rpc_cd.server == NULL)
		nopdelay();
}

void SysmanDeinit(void)
{
	memset(&SYSMAN_rpc_cd, 0, sizeof(SifRpcClientData_t));
}

/*	Description:	Reads data from the specified region of memory in IOP address space.
	Arguments:
		const void *MemoryStart	-> The address of the region of memory to read from (Must be aligned!).
		void *buffer		-> A pointer to the buffer for storing the data read.
		unsigned int NumBytes	-> The number of bytes of data to read.
		int mode		-> Whether to not block (1 = asynchronous operation).
	Returned values:
		<0	-> An error code, multiplied by -1.
		0	-> The operation completed successfully.
*/
int SysmanReadMemory(const void *MemoryStart, void *buffer, unsigned int NumBytes, int mode)
{
	int RPC_res = 0;

	((struct MemoryAccessParameters *)TransmitBuffer)->StartAddress = (void *)MemoryStart;
	((struct MemoryAccessParameters *)TransmitBuffer)->buffer = buffer;
	((struct MemoryAccessParameters *)TransmitBuffer)->NumBytes = NumBytes;

	if (mode)
	{
		if ((RPC_res = SifCallRpc(&SYSMAN_rpc_cd, SYSMAN_ReadMemory, SIF_RPC_M_NOWAIT, TransmitBuffer, sizeof(struct MemoryAccessParameters), ReceiveBuffer, sizeof(int), NULL, NULL)) >= 0)
		{
			RPC_res = 0;
		}
		else if ((RPC_res = SifCallRpc(&SYSMAN_rpc_cd, SYSMAN_ReadMemory, 0, TransmitBuffer, sizeof(struct MemoryAccessParameters), ReceiveBuffer, sizeof(int), NULL, NULL)) >= 0)
		{
			RPC_res = *(int *)ReceiveBuffer;
		}
	}
	return RPC_res;
}

/*	Description:	Writes data to the specified region of memory in IOP address space.
	Arguments:
		void *MemoryStart	-> The address of the region of memory write to.
		const void *buffer	-> A pointer to the buffer that contains the data to be written (Must be aligned and flushed!).
		unsigned int NumBytes	-> The number of bytes of data to write.
		int mode		-> Whether to not block (1 = asynchronous operation).
	Returned values:
		<0	-> An error code, multiplied by -1.
		0	-> The operation completed successfully.
*/
int SysmanWriteMemory(void *MemoryStart, const void *buffer, unsigned int NumBytes, int mode)
{
	int RPC_res = 0;

	((struct MemoryAccessParameters *)TransmitBuffer)->StartAddress = MemoryStart;
	((struct MemoryAccessParameters *)TransmitBuffer)->buffer = (void *)buffer;
	((struct MemoryAccessParameters *)TransmitBuffer)->NumBytes = NumBytes;

	if (mode)
	{
		if ((RPC_res = SifCallRpc(&SYSMAN_rpc_cd, SYSMAN_WriteMemory, SIF_RPC_M_NOWAIT, TransmitBuffer, sizeof(struct MemoryAccessParameters), ReceiveBuffer, sizeof(int), NULL, NULL)) >= 0)
		{
			RPC_res = 0;
		}
		else if ((RPC_res = SifCallRpc(&SYSMAN_rpc_cd, SYSMAN_WriteMemory, 0, TransmitBuffer, sizeof(struct MemoryAccessParameters), ReceiveBuffer, sizeof(int), NULL, NULL)) >= 0)
		{
			RPC_res = *(int *)ReceiveBuffer;
		}
	}

	return RPC_res;
}

int SysmanSync(int mode)
{
	if (mode)
		return SifCheckStatRpc(&SYSMAN_rpc_cd);
	else
		while (SifCheckStatRpc(&SYSMAN_rpc_cd) != 0)
		{
		};

	return 0;
}

/*	Description:	Calculates the size of the IOPRP image at the specified address in IOP address space.
	Arguments:
		const void *ROMStart	-> The address of the IOPRP image, in IOP address space.
	Returned values:
		<0	-> An error code, multiplied by -1.
		0	-> The operation completed successfully.
*/
int SysmanCalcROMRegionSize(const void *ROMStart)
{
	int RPC_res = 0;

	*(const void **)TransmitBuffer = ROMStart;

	if ((RPC_res = SifCallRpc(&SYSMAN_rpc_cd, SYSMAN_CalcROMRegionSize, 0, TransmitBuffer, sizeof(void *), ReceiveBuffer, sizeof(int), NULL, NULL)) >= 0)
		RPC_res = *(int *)ReceiveBuffer;

	return RPC_res;
}

/*	Description:	Calculates the size of the ROM chip containing the specified image.
	Arguments:
		unsigned int RegionSize	-> The size of the image in the ROM chip, to calculate its size of.
	Returned values:
		<0	-> An error code, multiplied by -1.
		0	-> The operation completed successfully.
*/
int SysmanCalcROMChipSize(unsigned int RegionSize)
{
	int RPC_res = 0;

	*(unsigned int *)TransmitBuffer = RegionSize;

	if ((RPC_res = SifCallRpc(&SYSMAN_rpc_cd, SYSMAN_CalcROMChipSize, 0, TransmitBuffer, sizeof(void *), ReceiveBuffer, sizeof(int), NULL, NULL)) >= 0)
		RPC_res = *(int *)ReceiveBuffer;

	return RPC_res;
}

int SysmanGetHardwareInfo(t_SysmanHardwareInfo *hwinfo)
{
	int RPC_res = 0;

	if ((RPC_res = SifCallRpc(&SYSMAN_rpc_cd, SYSMAN_GetHardwareInfo, 0, NULL, 0, ReceiveBuffer, sizeof(int) + sizeof(t_SysmanHardwareInfo), NULL, NULL)) >= 0)
	{
		RPC_res = *(int *)ReceiveBuffer;
		memcpy(hwinfo, &ReceiveBuffer[4], sizeof(t_SysmanHardwareInfo));
	}

	return RPC_res;
}

int SysmanGetMACAddress(unsigned char *MAC_address)
{
	int RPC_res = 0;

	if ((RPC_res = SifCallRpc(&SYSMAN_rpc_cd, SYSMAN_GetMACAddress, 0, NULL, 0, ReceiveBuffer, sizeof(int) + 6, NULL, NULL)) >= 0)
	{
		RPC_res = *(int *)ReceiveBuffer;
		memcpy(MAC_address, &ReceiveBuffer[4], 6);
	}

	return RPC_res;
}
