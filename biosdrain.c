#include "config.h"
#include "ui/menu.h"

#include <stdio.h>
#include <dirent.h>
#include <kernel.h>
#include <sbv_patches.h>
#include <loadfile.h>
#include <libcdvd.h>
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
		"mass:BIOS.rom0",
		"mass:BIOS.rom1",
		"mass:BIOS.rom2",
		"mass:BIOS.erom",
		"mass:BIOS.mec",
		"mass:BIOS.nvm",
};

const char *file_paths_host[6] =
	{
		"host:BIOS.rom0",
		"host:BIOS.rom1",
		"host:BIOS.rom2",
		"host:BIOS.erom",
		"host:BIOS.mec",
		"host:BIOS.nvm",
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

	menu_status("BOOT ROM Info:\n");
	menu_status("- ROM0 exists? %s\n", g_hardwareInfo.ROMs[0].IsExists ? "Yes" : "No");
	if (g_hardwareInfo.ROMs[0].IsExists)
	{
		menu_status("- ROM0 ADDR and SIZE: %08X %08X \n", g_hardwareInfo.ROMs[0].StartAddress, g_hardwareInfo.ROMs[0].size);
	}

	menu_status("DVD ROM Info:\n");
	menu_status("- DVD exists? %s\n", g_hardwareInfo.DVD_ROM.IsExists ? "Yes" : "No");
	menu_status("- ROM1 exists? %s\n", g_hardwareInfo.ROMs[1].IsExists ? "Yes" : "No");
	menu_status("- EROM exists? %s\n", g_hardwareInfo.erom.IsExists ? "Yes" : "No");
	menu_status("- ROM2 exists? %s\n", g_hardwareInfo.ROMs[2].IsExists ? "Yes" : "No");

	if (g_hardwareInfo.DVD_ROM.IsExists)
	{
		menu_status("- DVD ADDR and SIZE: %08X %08X \n", g_hardwareInfo.DVD_ROM.StartAddress, g_hardwareInfo.DVD_ROM.size);
	}
	if (g_hardwareInfo.ROMs[1].IsExists)
	{
		menu_status(" - ROM1 ADDR and SIZE: %08X %08X\n", g_hardwareInfo.ROMs[1].StartAddress, g_hardwareInfo.ROMs[1].size);
	}
	if (g_hardwareInfo.erom.IsExists)
	{
		// Uses GetLoadcoreInternalData() to get the address of the encrypted module
		// Turn 0xBE040000 -> 0x1E040000
		g_hardwareInfo.erom.StartAddress &= ~(0xA << 0x1C);
		menu_status(" - EROM ADDR and SIZE: %08X %08X\n", g_hardwareInfo.erom.StartAddress, g_hardwareInfo.erom.size);
	}
	if (g_hardwareInfo.ROMs[2].IsExists)
	{
		menu_status(" - ROM2 ADDR and SIZE: %08X %08X\n", g_hardwareInfo.ROMs[2].StartAddress, g_hardwareInfo.ROMs[2].size);
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

void reset_iop()
{
	SifInitRpc(0);
	// Reset IOP
	while (!SifIopReset("", 0x0))
		;
	while (!SifIopSync())
		;
	SifInitRpc(0);

	sbv_patch_enable_lmb();
}

int determine_device()
{
#ifndef FORCE_USB
	// Check host first, can't open just host: on pcsx2, it's broken.
	if (!mkdir("host:tmp", 0777))
	{
		rmdir("host:tmp");

		menu_status("HOST found.\n");

		use_usb_dir = 0;
	}
	else
#else
	menu_status("Built to force USB and skip HOST.\n");
#endif
	{
		use_usb_dir = 1;

#ifndef NO_RESET_IOP_WHEN_USB
		reset_iop();
#endif

		load_irx_usb();

		if (!mkdir("mass:tmp", 0777))
		{
			rmdir("mass:tmp");

			menu_status("USB device found.\n");
			use_usb_dir = 1;
		}
		else
		{
			menu_status("USB not found, and HOST is not available, not continuing.\n");
			
#ifdef NO_RESET_IOP_WHEN_USB
			menu_status("This is a noreset build.\n"
					  " This is usually necessary for uLaunchELF and USB users.\n"
					  " Please try the 'regular' biosdrain.elf build.\n");
#endif

			return 1;
		}
	}

	return 0;
}

void load_irx_sysman()
{	
	sbv_patch_enable_lmb();
	sysman_prerequesites();
	int mod_res;
	int sysman_irx_id = SifExecModuleBuffer(&sysman_irx, size_sysman_irx, 0, NULL, &mod_res);
	printf("SYSMAN ID is %d (res %d)\n ", sysman_irx_id, mod_res);

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

	menu_status("Finished dumping ROM0, writing to file\n");

	FlushCache(0);
	FILE *file = fopen(get_file_path(FILE_PATH_ROM0), "wb+");

	fwrite(buffer, 1, 0x400000, file);

	FlushCache(0);

	fclose(file);
	menu_status("Finished writing\n");
	return;
}

void dump_rom1()
{
	dump_area(g_hardwareInfo.ROMs[1].StartAddress, g_hardwareInfo.ROMs[1].size);

	menu_status("Finished dumping ROM1, writing to file\n");

	FlushCache(0);
	FILE *file = fopen(get_file_path(FILE_PATH_ROM1), "wb+");

	fwrite(buffer, 1, g_hardwareInfo.ROMs[1].size, file);

	FlushCache(0);

	fclose(file);
	menu_status("Finished writing\n");
	return;
}

void dump_rom2()
{
	dump_area(g_hardwareInfo.ROMs[2].StartAddress, g_hardwareInfo.ROMs[2].size);

	menu_status("Finished dumping ROM2, writing to file\n");

	FlushCache(0);
	FILE *file = fopen(get_file_path(FILE_PATH_ROM2), "wb+");

	fwrite(buffer, 1, g_hardwareInfo.ROMs[2].size, file);

	FlushCache(0);

	fclose(file);
	menu_status("Finished writing\n");
	return;
}

void dump_erom()
{
	dump_area(g_hardwareInfo.erom.StartAddress, g_hardwareInfo.erom.size);

	menu_status("Finished dumping EROM, writing to file\n");

	FlushCache(0);
	FILE *file = fopen(get_file_path(FILE_PATH_EROM), "wb+");

	fwrite(buffer, 1, g_hardwareInfo.erom.size, file);

	FlushCache(0);

	fclose(file);
	menu_status("Finished writing\n");
	return;
}

void dump_nvm()
{
	u16 nvm_buffer[512];

	u8 result;
	for (u32 i = 0; i < 512; i++)
	{
		if (sceCdReadNVM(i, &nvm_buffer[i], &result) != 1 || result != 0)
		{
			menu_status("Failed to read NVM block %d\n", i);
			return;
		}
	}

	menu_status("Finished dumping NVM, writing to file\n");

	FlushCache(0);
	FILE *file = fopen(get_file_path(FILE_PATH_NVM), "wb+");

	fwrite(nvm_buffer, 1, sizeof(nvm_buffer), file);

	FlushCache(0);

	fclose(file);

	menu_status("Finished writing\n");
	return;
}

void dump_mec()
{

	u8 mec_version[4];
	const u32 cmd = 0;

	if (sceCdApplySCmd(0x03, (void *)&cmd, sizeof(cmd), &mec_version[0], sizeof(mec_version)))
	{
		menu_status("Finished dumping MEC, writing to file\n");
	}
	else
	{
		menu_status("Failed to read MEC\n");
		return;
	}

	FlushCache(0);
	FILE *file = fopen(get_file_path(FILE_PATH_MEC), "wb+");

	fwrite(mec_version, 1, sizeof(mec_version), file);

	FlushCache(0);

	fclose(file);
}

int main(void)
{
	printf("main()\n");
	menu_init();
	
	menu_logo_fadein();

	menu_status("Determining target device.\n");
	if (determine_device())
		goto exit_main;

	load_irx_sysman();

	menu_status("Dumping ROM\n");

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
	// Haven't looked into where the NVM / MEC is stored yet, I'll assume it's
	// with the DVD stuff as you need to use CDVD commands to read it.
	if (g_hardwareInfo.DVD_ROM.IsExists)
	{
		dump_nvm();
		dump_mec();
	}


exit_main:
	menu_status("Finished everything.\n");
	SleepThread();
}
