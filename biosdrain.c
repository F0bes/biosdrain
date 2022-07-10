#include "common_rpc.h"

#include <stdio.h>
#include <dirent.h>
#include <kernel.h>
#include <sbv_patches.h>
#include <loadfile.h>
#include <sifrpc.h>
#include <libpad.h>
#include <debug.h>
#include <graph.h>
#include <iopheap.h>
#include <iopcontrol.h>

extern unsigned int size_biosdrainirx_irx;
extern unsigned char biosdrainirx_irx[];

extern unsigned int size_bdm_irx;
extern unsigned char bdm_irx[];

extern unsigned int size_bdmfs_vfat_irx;
extern unsigned char bdmfs_vfat_irx[];

extern unsigned int size_usbmass_bd_irx;
extern unsigned char usbmass_bd_irx[];

extern unsigned int size_usbd_irx;
extern unsigned char usbd_irx[];

int biosdrainirx_id;

void load_irx()
{
	if (opendir("host:"))
	{
		scr_printf("Host dir found, skipping IOP reset\n");
	}
	else
	{
		while (!SifIopReset(NULL, 0))
		{
		};
		while (!SifIopSync())
		{
		};
	}

	SifInitRpc(0);

	sbv_patch_enable_lmb(); // Enable load module buffer

	SifLoadModule("rom0:SIO2MAN", 0, 0);

	int usbd_irx_id = SifExecModuleBuffer(&usbd_irx, size_usbd_irx, 0, NULL, NULL);
	printf("USBD ID is %d\n", usbd_irx_id);

	int bdm_irx_id = SifExecModuleBuffer(&bdm_irx, size_bdm_irx, 0, NULL, NULL);
	printf("BDM ID: %d\n", bdm_irx_id);

	int bdmfs_vfat_irx_id = SifExecModuleBuffer(&bdmfs_vfat_irx, size_bdmfs_vfat_irx, 0, NULL, NULL);
	printf("BDMFS VFAT ID: %d\n", bdmfs_vfat_irx_id);

	int usbmass_irx_id = SifExecModuleBuffer(&usbmass_bd_irx, size_usbmass_bd_irx, 0, NULL, NULL);
	printf("USB Mass ID is %d\n", usbmass_irx_id);

	biosdrainirx_id = SifExecModuleBuffer(&biosdrainirx_irx, size_biosdrainirx_irx, 0, NULL, NULL);
	printf("BiosDrainIRX ID is %d\n", biosdrainirx_id);
}

static SifRpcClientData_t g_Rpc_Client __attribute__((aligned(64)));
unsigned sbuff[64] __attribute__((aligned(64)));

void init_sifclient()
{
	SifInitRpc(0);
	SifBindRpc(&g_Rpc_Client, RPC_ID, 0);

	u32 count = 0;
	while (1)
	{
		if (g_Rpc_Client.server != NULL)
			break;
		if (count++ == 1000000)
		{
			printf("[EE] Waiting for RPC server\n");
			count = 0;
		}
	}
	printf("[EE] SIF RPC server bound\n");
}

// Proper semaphores crash
static s32 g_Dump_Sema = 0;

static void intrDumpFinished(void *end_param)
{
	g_Dump_Sema++;
}

SifRpcEndFunc_t g_Dump_EndFunc = intrDumpFinished;
u8 buffer[0x400000] __attribute__((aligned(64)));
void dump_rom()
{
	u32 buffer_ptr __attribute__((aligned(64)));
	buffer_ptr = (u32)buffer;
	while (SifCheckStatRpc(&g_Rpc_Client) != 0)
		;

	SifCallRpc(&g_Rpc_Client, RPC_CMD_DUMP, 0, &buffer_ptr, sizeof(u32), NULL, 0, g_Dump_EndFunc, NULL);

	while (!g_Dump_Sema)
		;
	FlushCache(0);
	printf("[EE] ROM dumped\n");

	// Unload our module, it's probably not needed, but it's nice to free up
	// some IOP memory
	SifUnloadModule(biosdrainirx_id);

	// We are too fast for the USB driver sometimes, so wait a bit
	u32 v_cnt = 0;
	while (v_cnt < 300)
	{
		graph_wait_vsync();
		v_cnt++;
	}

	FILE *file = fopen("host:ROM0.bin", "wb+");
	if (file > 0)
	{
		scr_printf("[EE] Using HOST directory because it's available.\n");
	}
	else if ((file = fopen("mass:ROM0.bin", "wb+")) > 0)
	{
		scr_printf("[EE] Using USB directory because it's available and HOST is not\n");
	}
	else
	{
		scr_printf("[EE] HOST and MASS directories not available. Is your USB plugged in?\n");
		return;
	}

	scr_printf("Writing to file, please be patient, I'll tell you when I'm done :^)\n");

	fwrite(buffer, 1, sizeof(buffer), file);

	v_cnt = 0;
	while (v_cnt < 50)
	{
		graph_wait_vsync();
		v_cnt++;
	}

	fclose(file);
	scr_printf("Finished\n");
	printf("Finished\n");
	return;
}

int main(void)
{
	graph_wait_vsync();
	init_scr();

	printf("[EE] BiosDrain Starting\n");
	scr_setXY(0, 1);
	scr_printf("[EE] Fobes BiosDrain Starting (beta, might not work right)\n");
	scr_printf("Loading bundled IRX\n");
	load_irx();
	scr_printf("Initialising SIF connection\n");
	init_sifclient();
	scr_printf("Dumping ROM\n");

	dump_rom();

	SleepThread();
}
