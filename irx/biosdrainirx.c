#include "../common_rpc.h"
#include "irx_imports.h"
#include <irx.h>
#include <sifrpc.h>
#include <ssbusc.h>
#include <string.h>
#include <intrman.h>

// Smaller dump size might take longer,
// but it uses less ram, and I don't know how much we can play with here.
#define DUMP_BLOCK_SIZE 0x20000

#define MODULE_NAME "biosdrainirx"
IRX_ID(MODULE_NAME, 1, 1);

static u8 buff[DUMP_BLOCK_SIZE] __attribute__((aligned(64)));
void DumpROM(u32 ee_dest)
{
	printf("Dumping ROM into EE %08x!\n", ee_dest);
	u32 rom0_ptr = 0x1FC00000;
	u32 part = 0;
	int intr;
	SifDmaTransfer_t dmat;
	dmat.src = &rom0_ptr;
	dmat.size = DUMP_BLOCK_SIZE;
	dmat.attr = SIF_DMA_FROM_IOP;
	dmat.dest = (void *)ee_dest;

	CpuSuspendIntr(&intr);
	while (part != (0x400000 / DUMP_BLOCK_SIZE))
	{
		dmat.src = (void *)buff;
		dmat.dest = (void *)ee_dest;

		memcpy(buff, (void *)rom0_ptr, DUMP_BLOCK_SIZE);

		sceSifSetDma(&dmat, 1);

		part++;
		rom0_ptr += (DUMP_BLOCK_SIZE);
		ee_dest += (DUMP_BLOCK_SIZE);
	}
	CpuResumeIntr(intr);

	return;
}

static SifRpcDataQueue_t g_Rpc_Queue __attribute__((aligned(64)));
static SifRpcServerData_t g_Rpc_Server __attribute((aligned(64)));
static u32 g_Rpc_Buffer[1024] __attribute((aligned(64)));
void *rpcCommandHandler(int command, void *data, int size)
{
	printf("RPC command received: %d\n", command);
	if (command == RPC_CMD_DUMP)
	{
		DumpROM(*(u32 *)data);
	}
	printf("handler return\n");
	return data;
}

void rpcThreadFunc(void *unused)
{
	SifInitRpc(0);
	SifSetRpcQueue(&g_Rpc_Queue, GetThreadId());

	SifRegisterRpc(&g_Rpc_Server, RPC_ID, (void *)rpcCommandHandler, (u32 *)&g_Rpc_Buffer, 0, 0, &g_Rpc_Queue);

	SifRpcLoop(&g_Rpc_Queue);
	// Our thread should be yielding, waiting for an rpc command
}

int _start(int argc, char *argv[])
{
	printf("[IOP] BiosDrainIRX says hello!\n");
	iop_thread_t rpcThread;
	rpcThread.attr = TH_C;
	rpcThread.thread = &rpcThreadFunc;
	rpcThread.priority = 100;
	rpcThread.stacksize = 0x9000;

	u32 rpcThreadID = CreateThread(&rpcThread);
	if (rpcThreadID > 0)
	{
		StartThread(rpcThreadID, NULL);
	}
	else
	{
		printf("[iop] Failed to create thread\n");
	}

	return 0;
}
