#include <errno.h>
#include <irx.h>
#include <intrman.h>
#include <loadcore.h>
#include <sifcmd.h>
#include <sifman.h>
#include <stdio.h>
#include <sysclib.h>
#include <types.h>
#include <thbase.h>
#include <sysmem.h>

#include "main.h"
#include "sysinfo.h"
#include "romdrv/romdrv.h"
#include "rom.h"
#include "iLink.h"
#include "dev9.h"
#include "usb.h"
#include "spu2.h"

#include "sysman_rpc.h"

#define MODNAME "System_Manager"
IRX_ID(MODNAME, 1, 3);

/* Data used for registering the RPC servers */
static SifRpcServerData_t rpc_sdata;
static unsigned char rpc_buffer[128];
static SifRpcDataQueue_t rpc_qdata;

static unsigned char IOBuffer[2][MEM_IO_BLOCK_SIZE];

/* Function prototypes */
static void SYSMAN_RPC_srv(void *args);
static void *SYSMAN_rpc_handler(int fno, void *buffer, int size);

/* Common thread creation data */
static iop_thread_t thread_data = {
	TH_C,			 /* rpc_thp.attr */
	0,				 /* rpc_thp.option */
	&SYSMAN_RPC_srv, /* rpc_thp.thread */
	0x600,			 /* rpc_thp.stacksize */
	0x69,			 /* rpc_thp.piority */
};

/* Entry point */
int _start(int argc, char *argv[])
{
	int result;

	if (ROMInitialize() == 0 && iLinkInitialize() == 0 && dev9Initialize() == 0 && usbInitialize() == 0 && spu2Initialize() == 0)
	{
		DEBUG_PRINTF("SYSMAN: IOP peripherals initialized successfully.\n");

		StartThread(CreateThread(&thread_data), NULL);
		result = MODULE_RESIDENT_END;
	}
	else
		result = MODULE_NO_RESIDENT_END;

	return result;
}

static void SYSMAN_RPC_srv(void *args)
{
	sceSifSetRpcQueue(&rpc_qdata, GetThreadId());
	sceSifRegisterRpc(&rpc_sdata, SYSMAN_RPC_NUM, &SYSMAN_rpc_handler, rpc_buffer, NULL, NULL, &rpc_qdata);
	sceSifRpcLoop(&rpc_qdata);
}

int SysmanReadMemory(const void *MemoryStart, void *buffer, unsigned int NumBytes)
{
	memcpy(buffer, MemoryStart, NumBytes);
	return 0;
}

int SysmanWriteMemory(void *MemoryStart, const void *buffer, unsigned int NumBytes)
{
	memcpy(MemoryStart, buffer, NumBytes);
	return 0;
}

int SysmanCalcROMRegionSize(const void *ROMStart)
{

	unsigned int i;
	int result, TotalROMSize;
	const struct RomDirEntry *RomDirEnt;

	for (result = -ENOENT, i = 0; i < 0x40000; i++)
	{
		if (((const char *)ROMStart)[i] == 'R' &&
			((const char *)ROMStart)[i + 1] == 'E' &&
			((const char *)ROMStart)[i + 2] == 'S' &&
			((const char *)ROMStart)[i + 3] == 'E' &&
			((const char *)ROMStart)[i + 4] == 'T')
		{
			result = 0;
			break;
		}
	}

	if (result >= 0)
	{
		RomDirEnt = (const struct RomDirEntry *)&((const char *)ROMStart)[i];
		TotalROMSize = 0;
		while (RomDirEnt->name[0] != '\0')
		{
			TotalROMSize += RomDirEnt->size;
			RomDirEnt++;
		}

		result = TotalROMSize;
	}

	return result;
}

int SysmanCalcROMChipSize(unsigned int RegionSize)
{
	unsigned int i, ChipSize;

	/* Determine the ROM chip size. It won't ever exceed 2^30. Do not allow the size field to use the sign bit. */
	for (i = 0; i < 31; i++)
	{
		ChipSize = 1 << i;
		if (ChipSize >= RegionSize)
			break;
	}

	return ChipSize;
}

static int EE_memcpy_async(const void *src, void *dest, unsigned int NumBytes)
{
	SifDmaTransfer_t dmat;
	int dmat_ID, OldState;

	dmat.src = (void *)src;
	dmat.dest = dest;
	dmat.size = NumBytes;
	dmat.attr = SIF_DMA_FROM_IOP;

	do
	{
		CpuSuspendIntr(&OldState);
		dmat_ID = sceSifSetDma(&dmat, 1);
		CpuResumeIntr(OldState);
	} while (dmat_ID == 0);

	return dmat_ID;
}

int SysmanGetHardwareInfo(t_SysmanHardwareInfo *hwinfo)
{
	int result;
	unsigned short int *BootMode6;

	asm("mfc0 %0, $15\n"
		: "=r"(hwinfo->iop.revision)
		:);

	hwinfo->iop.RAMSize = QueryMemSize();
	hwinfo->BoardInf = ((BootMode6 = (unsigned short int *)QueryBootMode(6)) != NULL) ? *BootMode6 : 0;
	hwinfo->MPUBoardID = *(volatile unsigned short int *)0xBF803800;
	hwinfo->ROMGEN_MonthDate = *(volatile unsigned short int *)0xBFC00100;
	hwinfo->ROMGEN_Year = *(volatile unsigned short int *)0xBFC00102;

	if ((result = ROMGetHardwareInfo(hwinfo)) == 0)
	{
		if ((result = dev9GetHardwareInfo(&hwinfo->ssbus)) == 0)
		{
			// if((result=iLinkGetHardwareInfo(&hwinfo->iLink))==0){ // PCSX2 does not like
			if ((result = usbGetHardwareInfo(&hwinfo->usb)) == 0)
			{
				if ((result = spu2GetHardwareInfo(&hwinfo->spu2)) == 0)
				{
					DEBUG_PRINTF("SYSMAN: IOP peripheral information gathered successfully.\n");
				}
			}
			//}
		}
	}

	return result;
}

int SysmanGetMACAddress(unsigned char *MAC_address)
{
	return dev9GetSPEEDMACAddress(MAC_address);
}

static void *SYSMAN_rpc_handler(int fno, void *buffer, int size)
{
	static unsigned char ResultBuffer[256];
	SifRpcReceiveData_t rd;
	unsigned int BytesToRead, BytesRemaining;
	unsigned char *destination;
	int BufferID, dmat_id, old_dmat_id;
	const unsigned char *source;

	switch (fno)
	{
	case SYSMAN_ReadMemory:
		destination = ((struct MemoryAccessParameters *)buffer)->buffer;
		source = ((struct MemoryAccessParameters *)buffer)->StartAddress;
		BytesRemaining = ((struct MemoryAccessParameters *)buffer)->NumBytes;
		BufferID = 0;
		dmat_id = -1;
		while (BytesRemaining > 0)
		{
			BytesToRead = BytesRemaining > MEM_IO_BLOCK_SIZE ? MEM_IO_BLOCK_SIZE : BytesRemaining;
			if ((*(int *)ResultBuffer = SysmanReadMemory(source, IOBuffer[BufferID], BytesToRead)) == 0)
			{
				old_dmat_id = dmat_id;
				dmat_id = EE_memcpy_async(IOBuffer[BufferID], destination, BytesToRead);
				if (old_dmat_id >= 0)
					while (sceSifDmaStat(old_dmat_id) >= 0)
					{
					}; // Do not overwrite data that has not been transferred over to the EE.
			}
			else
				break;

			destination += BytesToRead;
			source += BytesToRead;
			BytesRemaining -= BytesToRead;
			BufferID ^= 1;
		}
		break;
	case SYSMAN_WriteMemory:
		destination = ((struct MemoryAccessParameters *)buffer)->StartAddress;
		source = ((struct MemoryAccessParameters *)buffer)->buffer;
		BytesRemaining = ((struct MemoryAccessParameters *)buffer)->NumBytes;
		while (BytesRemaining > 0)
		{
			BytesToRead = BytesRemaining > MEM_IO_BLOCK_SIZE ? MEM_IO_BLOCK_SIZE : BytesRemaining;
			sceSifGetOtherData(&rd, (void *)source, IOBuffer, BytesToRead, 0);
			if ((*(int *)ResultBuffer = SysmanWriteMemory(destination, IOBuffer, BytesToRead)) != 0)
				break;

			destination += BytesToRead;
			source += BytesToRead;
			BytesRemaining -= BytesToRead;
		}
		break;
	case SYSMAN_CalcROMRegionSize:
		*(int *)ResultBuffer = SysmanCalcROMRegionSize(*(void **)buffer);
		break;
	case SYSMAN_CalcROMChipSize:
		*(int *)ResultBuffer = SysmanCalcROMChipSize(*(unsigned int *)buffer);
		break;
	case SYSMAN_GetHardwareInfo:
		*(int *)ResultBuffer = SysmanGetHardwareInfo((t_SysmanHardwareInfo *)&ResultBuffer[4]);
		break;
	case SYSMAN_GetMACAddress:
		*(int *)ResultBuffer = SysmanGetMACAddress(&ResultBuffer[4]);
		break;
	default:
		*(int *)ResultBuffer = -1;
	}

	return ResultBuffer;
}
