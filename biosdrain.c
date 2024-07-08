#include "config.h"
#include "ui/menu.h"
#include "ui/fontqueue.h"
#include "ui/fontengine.h"
#include "dump.h"

#include <kernel.h>
#include <stdio.h>
#include <sio.h>
#include <stdlib.h>
#include <dirent.h> // mkdir()
#include <unistd.h> // rmdir()
#include <graph.h> // graph_wait_vsync()
#include <sbv_patches.h>
#include <loadfile.h>
#include <sifrpc.h>
#include <iopcontrol.h>

#include "sysman/sysinfo.h"
#include "sysman_rpc.h"

#include "OSDInit.h"

extern unsigned int size_bdm_irx;
extern unsigned char bdm_irx[];

extern unsigned int size_bdmfs_fatfs_irx;
extern unsigned char bdmfs_fatfs_irx[];

extern unsigned int size_usbmass_bd_irx;
extern unsigned char usbmass_bd_irx[];

extern unsigned int size_usbd_irx;
extern unsigned char usbd_irx[];

extern unsigned int size_sysman_irx;
extern unsigned char sysman_irx[];

void sysman_prerequesites()
{
	SifLoadModule("rom0:ADDDRV", 0, NULL);
	SifLoadModule("rom0:ADDROM2", 0, NULL);
}

t_SysmanHardwareInfo g_hardwareInfo;
void LoadSystemInformation()
{
	SysmanGetHardwareInfo(&g_hardwareInfo);
	menu_status("- ROM0 exists? %s\n", g_hardwareInfo.ROMs[0].IsExists ? "Yes" : "No");
	if (g_hardwareInfo.ROMs[0].IsExists)
	{
		menu_status("- ROM0 ADDR and SIZE: %08X %08X\n", g_hardwareInfo.ROMs[0].StartAddress, g_hardwareInfo.ROMs[0].size);
	}

	menu_status("- DVD exists? %s\n", g_hardwareInfo.DVD_ROM.IsExists ? "Yes" : "No");
	menu_status("- ROM1 exists? %s\n", g_hardwareInfo.ROMs[1].IsExists ? "Yes" : "No");
	menu_status("- ROM2 exists? %s\n", g_hardwareInfo.ROMs[2].IsExists ? "Yes" : "No");

	if (g_hardwareInfo.DVD_ROM.IsExists)
	{
		menu_status("- DVD ADDR and SIZE: %08X %08X\n", g_hardwareInfo.DVD_ROM.StartAddress, g_hardwareInfo.DVD_ROM.size);
	}
	if (g_hardwareInfo.ROMs[1].IsExists)
	{
		menu_status(" - ROM1 ADDR and SIZE: %08X %08X\n", g_hardwareInfo.ROMs[1].StartAddress, g_hardwareInfo.ROMs[1].size);
	}
	if (g_hardwareInfo.ROMs[2].IsExists)
	{
		menu_status(" - ROM2 ADDR and SIZE: %08X %08X\n", g_hardwareInfo.ROMs[2].StartAddress, g_hardwareInfo.ROMs[2].size);
	}
}

void load_irx_usb()
{
	int bdm_irx_id = SifExecModuleBuffer(&bdm_irx, size_bdm_irx, 0, NULL, NULL);
	printf("BDM ID: %d\n", bdm_irx_id);

	int bdmfs_fatfs_irx_id = SifExecModuleBuffer(&bdmfs_fatfs_irx, size_bdmfs_fatfs_irx, 0, NULL, NULL);
	printf("BDMFS FATFS ID: %d\n", bdmfs_fatfs_irx_id);

	int usbd_irx_id = SifExecModuleBuffer(&usbd_irx, size_usbd_irx, 0, NULL, NULL);
	printf("USBD ID is %d\n", usbd_irx_id);

	int usbmass_irx_id = SifExecModuleBuffer(&usbmass_bd_irx, size_usbmass_bd_irx, 0, NULL, NULL);
	printf("USB Mass ID is %d\n", usbmass_irx_id);
};

int wait_usb_ready()
{
	menu_status("Waiting for USB to be ready...\n");
	//struct stat buffer;
	int ret = -1;
	int retries = 600;

	while (ret != 0 && retries > 0) {
		//ret = stat("mass:/", &buffer);
		if (mkdir("mass:/tmp", 0777))
		{
			rmdir("mass:/tmp");
			ret = 0;
			break;
		}
		WaitSema(graphic_vsync_sema);
		retries--;
	}

	menu_status("USB ready after %d attempts.\n", 601 - retries);
	if(ret != 0)
	{
		menu_status("USB not ready after 10 seconds :(.\n Try a smaller FAT32 partition?.\n");
		return -1;
	}

	return 0;
}

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

u32 use_usb_dir = 0;
int determine_device()
{
#ifndef FORCE_USB
	// Check host first, can't open just host: on pcsx2, it's broken.
	if (!mkdir("host:tmp", 0777))
	{
		rmdir("host:tmp");
		use_usb_dir = 0;
	}
	else
#else
	menu_status("Built to force USB and skip HOST.\n");
#endif
	{
		printf("Resetting IOP, bye bye!\n");
		reset_iop();
		load_irx_usb();

		use_usb_dir = !wait_usb_ready();

		if(!use_usb_dir)
		{
			menu_status("USB not found, and HOST is not available, not continuing.\n");
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

int main(void)
{
	sio_puts("main()\n");

	menu_init();
	graphic_init();
	fontengine_init();
	graphic_biosdrain_fadein();

	menu_status("biosdrain - revision %s\n", GIT_VERSION);
	if (determine_device())
		goto exit_main;

	load_irx_sysman();

	menu_status("Dumping to %s\n", use_usb_dir ? "USB" : "HOST");

	LoadSystemInformation();

	dump_init(use_usb_dir);

	dump_exec();
	dump_cleanup();
exit_main:
	menu_status("Finished everything. You're free to turn off the system.\n");
	menu_status("\n");
	menu_status("Interested in PS2 development?\n");
	menu_status("Check out fobes.dev!\n");
	menu_status("Support me on ko-fi.com/f0bes!\n");
	SleepThread();
}
