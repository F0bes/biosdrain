#include <stdio.h>
#include <dirent.h>
#include <kernel.h>
#include <sbv_patches.h>
#include <loadfile.h>
#include <sifrpc.h>
#include <libpad.h>
#include <debug.h>
#include <graph.h>
#include <gs_psm.h>
#include <gs_gp.h>
#include <draw.h>
#include <dma.h>
#include <packet.h>
#include <iopheap.h>
#include <iopcontrol.h>
#include <unistd.h>
#include "sysman/sysinfo.h"
#include "sysman_rpc.h"

#include "OSDInit.h"

void draw_logo_fast();

#define double_printf(fmt, ...)     \
	scr_printf(fmt, ##__VA_ARGS__); \
	printf(fmt, ##__VA_ARGS__);     \
	draw_logo_fast();

extern unsigned int size_biosdrain_tex;
extern unsigned char biosdrain_tex[];

extern unsigned int size_bdm_irx;
extern unsigned char bdm_irx[];

extern unsigned int size_bdmfs_vfat_irx;
extern unsigned char bdmfs_vfat_irx[];

extern unsigned int size_usbmass_bd_irx;
extern unsigned char usbmass_bd_irx[];

extern unsigned int size_usbd_irx;
extern unsigned char usbd_irx[];

extern unsigned int size_sysman_irx;
extern unsigned char sysman_irx[];

enum FILE_PATH
{
	FILE_PATH_ROM0,
	FILE_PATH_ROM1,
	FILE_PATH_ROM2,
	FILE_PATH_EROM,
	FILE_PATH_MEC,
	FILE_PATH_NVM,
};

const char *file_paths_usb[6] =
	{
		"mass:BIOS.ROM0",
		"mass:BIOS.ROM1",
		"mass:BIOS.ROM2",
		"mass:BIOS.EROM",
		"mass:BIOS.MEC",
		"mass:BIOS.NVM",
};

const char *file_paths_host[6] =
	{
		"host:BIOS.ROM0",
		"host:BIOS.ROM1",
		"host:BIOS.ROM2",
		"host:BIOS.EROM",
		"host:BIOS.MEC",
		"host:BIOS.NVM",
};

u32 use_usb_dir = 0;

// hacked together for now
const char *get_file_path(int index)
{
	if (use_usb_dir)
		return file_paths_usb[index];
	else
		return file_paths_host[index];
}

void sysman_prerequesites()
{
	SifLoadModule("rom0:ADDDRV", 0, NULL);
	SifLoadModule("rom0:ADDROM2", 0, NULL);

	// Replace ? with a null terminator, required on some regions
	char eromdrv[] = "rom1:EROMDRV?";
	if (OSDGetDVDPlayerRegion(&eromdrv[12]) == 0 || eromdrv[12] != '\0')
	{
		eromdrv[12] = '\0'; // Replace '?' with a NULL.
	}

	// Finally, load the encrypted module
	// Note, this doesn't work on pcsx2 and we will assume that there is no
	// erom
	SifLoadModuleEncrypted(eromdrv, 0, NULL);
}

t_SysmanHardwareInfo g_hardwareInfo;
void LoadSystemInformation()
{
	SysmanGetHardwareInfo(&g_hardwareInfo);

	double_printf("[EE] BOOT ROM Info:\n");
	double_printf("[EE] - ROM0 exists? %s\n", g_hardwareInfo.ROMs[0].IsExists ? "Yes" : "No");
	if (g_hardwareInfo.ROMs[0].IsExists)
	{
		double_printf("[EE] - ROM0 ADDR and SIZE: %08X %08X \n", g_hardwareInfo.ROMs[0].StartAddress, g_hardwareInfo.ROMs[0].size);
	}

	double_printf("[EE] DVD ROM Info:\n");
	double_printf("[EE] - DVD exists? %s\n", g_hardwareInfo.DVD_ROM.IsExists ? "Yes" : "No");
	double_printf("[EE] - ROM1 exists? %s\n", g_hardwareInfo.ROMs[1].IsExists ? "Yes" : "No");
	double_printf("[EE] - EROM exists? %s\n", g_hardwareInfo.erom.IsExists ? "Yes" : "No");
	double_printf("[EE] - ROM2 exists? %s\n", g_hardwareInfo.ROMs[2].IsExists ? "Yes" : "No");

	if (g_hardwareInfo.DVD_ROM.IsExists)
	{
		double_printf("[EE] - DVD ADDR and SIZE: %08X %08X \n", g_hardwareInfo.DVD_ROM.StartAddress, g_hardwareInfo.DVD_ROM.size);
	}
	if (g_hardwareInfo.ROMs[1].IsExists)
	{
		double_printf("[EE]  - ROM1 ADDR and SIZE: %08X %08X\n", g_hardwareInfo.ROMs[1].StartAddress, g_hardwareInfo.ROMs[1].size);
	}
	if (g_hardwareInfo.erom.IsExists)
	{
		// Uses GetLoadcoreInternalData() to get the address of the encrypted module
		// Turn 0xBE040000 -> 0x1E040000
		g_hardwareInfo.erom.StartAddress &= ~(0xA << 0x1C);
		double_printf("[EE]  - EROM ADDR and SIZE: %08X %08X\n", g_hardwareInfo.erom.StartAddress, g_hardwareInfo.erom.size);
	}
	if (g_hardwareInfo.ROMs[2].IsExists)
	{
		double_printf("[EE]  - ROM2 ADDR and SIZE: %08X %08X\n", g_hardwareInfo.ROMs[2].StartAddress, g_hardwareInfo.ROMs[2].size);
	}
}

void load_irx_usb()
{
	int usbd_irx_id = SifExecModuleBuffer(&usbd_irx, size_usbd_irx, 0, NULL, NULL);
	printf("USBD ID is %d\n", usbd_irx_id);

	int bdm_irx_id = SifExecModuleBuffer(&bdm_irx, size_bdm_irx, 0, NULL, NULL);
	printf("BDM ID: %d\n", bdm_irx_id);

	int bdmfs_vfat_irx_id = SifExecModuleBuffer(&bdmfs_vfat_irx, size_bdmfs_vfat_irx, 0, NULL, NULL);
	printf("BDMFS VFAT ID: %d\n", bdmfs_vfat_irx_id);

	int usbmass_irx_id = SifExecModuleBuffer(&usbmass_bd_irx, size_usbmass_bd_irx, 0, NULL, NULL);
	printf("USB Mass ID is %d\n", usbmass_irx_id);

	printf("Waiting 5 seconds for the USB driver to detect the USB device\n");
	// We are too fast for the USB driver sometimes, so wait ~ 5 seconds
	u32 v_cnt = 0;
	while (v_cnt < 300)
	{
		graph_wait_vsync();
		v_cnt++;
	}
};

int determine_device()
{
	// Check host first, can't open just host: on pcsx2, it's broken.
	if (!mkdir("host:tmp", 0777))
	{
		rmdir("host:tmp");

		double_printf("[EE] HOST found, will skip IOP reset\n");

		use_usb_dir = 0;

		sbv_patch_enable_lmb();
		return 0;
	}
	else
	{
		SifInitRpc(0);

		// Reset IOP
		while (!SifIopReset("", 0x80000000))
			;
		while (!SifIopSync())
			;

		SifInitRpc(0);

		sbv_patch_enable_lmb();
		load_irx_usb();
	}

	// To end up here we must've loaded the required USB modules.
	// Now check for a USB device
	if (!mkdir("mass:tmp", 0777))
	{
		rmdir("mass:tmp");

		double_printf("[EE] USB device found.\n");
		use_usb_dir = 1;
	}
	else
	{
		double_printf("[EE] USB not found, and host: is not available, not continuing.\n");
		return 1;
	}

	return 0;
}

void load_irx_sysman()
{
	sysman_prerequesites();
	int sysman_irx_id = SifExecModuleBuffer(&sysman_irx, size_sysman_irx, 0, NULL, NULL);
	printf("SYSMAN ID is %d\n", sysman_irx_id);

	SysmanInit();
}

static u8 buffer[0x400000] __attribute__((aligned(64)));

void dump_area(u32 start, u32 size)
{
	u32 area_ptr = start;
	u32 buf_ptr = (u32)buffer;

	for (int block = 0; block < (size / MEM_IO_BLOCK_SIZE); block++)
	{
		SysmanSync(0);
		while (SysmanReadMemory((void *)(area_ptr), (void *)(buf_ptr), MEM_IO_BLOCK_SIZE, 1) != 0)
			nopdelay();
		area_ptr += MEM_IO_BLOCK_SIZE;
		buf_ptr += MEM_IO_BLOCK_SIZE;
	}
	if ((size % MEM_IO_BLOCK_SIZE) != 0)
	{
		SysmanSync(0);
		while (SysmanReadMemory((void *)(area_ptr), (void *)(buf_ptr), (size % MEM_IO_BLOCK_SIZE), 1) != 0)
			nopdelay();
	}
}

void dump_rom0()
{
	dump_area(g_hardwareInfo.ROMs[0].StartAddress, 0x400000);

	double_printf("[EE] Finished dumping ROM0, writing to file\n");

	FlushCache(0);
	FILE *file = fopen(get_file_path(FILE_PATH_ROM0), "wb+");

	fwrite(buffer, 1, 0x400000, file);

	FlushCache(0);

	fclose(file);
	double_printf("[EE] Finished writing\n");
	return;
}

void dump_rom1()
{
	dump_area(g_hardwareInfo.ROMs[1].StartAddress, g_hardwareInfo.ROMs[1].size);

	double_printf("[EE] Finished dumping ROM1, writing to file\n");

	FlushCache(0);
	FILE *file = fopen(get_file_path(FILE_PATH_ROM1), "wb+");

	fwrite(buffer, 1, g_hardwareInfo.ROMs[1].size, file);

	FlushCache(0);

	fclose(file);
	double_printf("[EE] Finished writing\n");
	return;
}

void dump_rom2()
{
	dump_area(g_hardwareInfo.ROMs[2].StartAddress, g_hardwareInfo.ROMs[2].size);

	double_printf("[EE] Finished dumping ROM2, writing to file\n");

	FlushCache(0);
	FILE *file = fopen(get_file_path(FILE_PATH_ROM2), "wb+");

	fwrite(buffer, 1, g_hardwareInfo.ROMs[2].size, file);

	FlushCache(0);

	fclose(file);
	double_printf("[EE] Finished writing\n");
	return;
}

void dump_erom()
{
	dump_area(g_hardwareInfo.erom.StartAddress, g_hardwareInfo.erom.size);

	double_printf("[EE] Finished dumping EROM, writing to file\n");

	FlushCache(0);
	FILE *file = fopen(get_file_path(FILE_PATH_EROM), "wb+");

	fwrite(buffer, 1, g_hardwareInfo.erom.size, file);

	FlushCache(0);

	fclose(file);
	double_printf("[EE] Finished writing\n");
	return;
}

packet_t *p;
u32 logo_qword;
void biosdrain_logo()
{
	dma_channel_initialize(DMA_CHANNEL_GIF, NULL, 0);
	dma_channel_fast_waits(DMA_CHANNEL_GIF);
	dma_wait_fast();
	// We rely on debug for drawing text currently, and our framebuffer starts at 0
	// This should be good enough for now
	const u32 fixed_texture_address = 0x96000;
	p = packet_init(270, PACKET_NORMAL);
	qword_t *q = p->data;

	q = draw_texture_transfer(q, &biosdrain_tex[0], 256, 256, GS_PSM_32, fixed_texture_address, 256);
	q = draw_texture_flush(q);
	q = draw_finish(q);
	dma_channel_send_chain(DMA_CHANNEL_GIF, p->data, q - p->data, 0, 0);
	dma_wait_fast();
	// draw_wait_finish();

	packet_free(p);
	p = packet_init(15, PACKET_NORMAL);

	// Could be optimized ALOT more, but this is fine for now
	for (int alpha = 0; alpha < 0xFF; alpha++)
	{
		q = p->data;
		{
			PACK_GIFTAG(q, GIF_SET_TAG(1, 1, GIF_PRE_ENABLE, GS_SET_PRIM(GS_PRIM_SPRITE, 0, 1, 0, 1, 0, 1, 0, 0), GIF_FLG_PACKED, 9),
						GIF_REG_AD | (GIF_REG_AD << 4) | (GIF_REG_AD << 8) | (GS_REG_TEX0 << 12) | (GIF_REG_RGBAQ << 16) |
							(GIF_REG_UV << 20) | (GIF_REG_XYZ2 << 24) | (GIF_REG_UV << 28) | ((u64)GIF_REG_XYZ2 << 32));
			q++;
			// ALPHA
			q->dw[0] = GS_SET_ALPHA(0, 2, 0, 2, 0);
			q->dw[1] = GS_REG_ALPHA;
			q++;
			// XYOFFSET
			q->dw[0] = GS_SET_XYOFFSET(0, 0);
			q->dw[1] = GS_REG_XYOFFSET;
			q++;
			// TEST
			q->dw[0] = GS_SET_TEST(0, 0, 0, 0, 0, 0, 1, 1);
			q->dw[1] = GS_REG_TEST;
			q++;
			// TEX0
			q->dw[0] = GS_SET_TEX0(fixed_texture_address / 64, 4, GS_PSM_32, 8, 8, 0, 1, 0, 0, 0, 0, 0);
			q->dw[1] = GS_REG_TEX0;
			q++;
			// RGBAQ
			q->dw[0] = (u64)((0xFF) | ((u64)0xFF << 32));
			q->dw[1] = (u64)((0xFF) | ((u64)alpha << 32));
			q++;
			// UV
			q->dw[0] = GS_SET_ST(0, 0);
			q->dw[1] = (u64)(0);
			q++;
			// XYZ2
			q->dw[0] = (u64)((((300 << 4)) | (((u64)(100 << 4)) << 32)));
			q->dw[1] = (u64)(0);
			q++;
			// UV
			q->dw[0] = GS_SET_ST(256 << 4, 80 << 4);
			q->dw[1] = (u64)(0);
			q++;
			// XYZ2
			q->dw[0] = (u64)((((556 << 4)) | (((u64)(180 << 4)) << 32)));
			q->dw[1] = (u64)(0);
			q++;
			q = draw_finish(q);

			logo_qword = q - p->data;

			dma_channel_send_normal(DMA_CHANNEL_GIF, p->data, q - p->data, 0, 0);
			draw_wait_finish();
		}
		graph_wait_vsync();
	}
	packet_free(p);
}

// Hack to work around the fact that when scr_print blits to the screen
// it clears that entire row of the frame
void draw_logo_fast()
{
	dma_channel_send_normal(DMA_CHANNEL_GIF, p->data, logo_qword, 0, 0);
	draw_wait_finish();
}

int main(void)
{
	graph_wait_vsync();
	init_scr();
	biosdrain_logo();
	printf("[EE] BiosDrain Starting\n");
	scr_setXY(0, 1);
	double_printf("[EE] Fobes BiosDrain Starting (beta, might not work right)\n");
	double_printf("[EE] Loading bundled IRX\n");

	if (determine_device())
		goto exit_main;

	load_irx_sysman();

	double_printf("[EE] Dumping ROM\n");

	LoadSystemInformation();

	dump_rom0();
	if (g_hardwareInfo.ROMs[1].IsExists)
	{
		dump_rom1();
	}
	if (g_hardwareInfo.ROMs[2].IsExists)
	{
		dump_rom2();
	}
	if (g_hardwareInfo.erom.IsExists)
	{
		dump_erom();
	}

exit_main:
	double_printf("[EE] Finished everything.\n");
	SleepThread();
}
