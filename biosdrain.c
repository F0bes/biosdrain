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

#ifdef SUPPORT_SYSTEM_2x6
#include <iopcontrol_special.h>
#endif

#include "OSDInit.h"

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

extern unsigned int size_ioprp;
extern unsigned char ioprp[];

void sysman_prerequesites()
{
#ifdef SUPPORT_SYSTEM_2x6
	SifLoadModule("rom0:ACDEV", 0, NULL); // registers arcade board flash memory as `rom1:`
#else
	SifLoadModule("rom0:ADDDRV", 0, NULL); // registers dvdplayer rom as `rom1:`
	SifLoadModule("rom0:ADDROM2", 0, NULL);
#endif
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
		WaitSema(graphic_vsync_sema);
		v_cnt++;
	}
};

void reset_iop()
{
	SifInitRpc(0);
	// Reset IOP
#ifdef SUPPORT_SYSTEM_2x6
	while (!SifIopRebootBuffer(ioprp, size_ioprp))
		;
#else
	while (!SifIopReset("", 0x0))
		;
#endif

	while (!SifIopSync())
		;
	SifInitRpc(0);

	sbv_patch_enable_lmb();
#ifdef SUPPORT_SYSTEM_2x6
	SifLoadModule("rom0:CDVDFSV", 0, NULL); // do it ASAP

	//biosdrain does not use memory card at all, this makes our life easier for dealing with arcade syscon:
	SifLoadModule("rom0:SIO2MAN", 0, NULL); // dependency of DONGLEMAN
	SifLoadModule("rom0:MCMAN", 0, NULL); // DONGLEMAN
	SifLoadModule("rom0:LED", 0, NULL); // LED setter
	SifLoadModule("rom0:DAEMON", 0, NULL); // security dongle checker. to keep arcade syscon happy
	//no need for EE RPC or anything.
#endif
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
		use_usb_dir = 1;

#ifndef NO_RESET_IOP_WHEN_USB
		reset_iop();
#endif
		load_irx_usb();

		if (!mkdir("mass:tmp", 0777))
		{
			rmdir("mass:tmp");
			use_usb_dir = 1;
		}
		else
		{
			menu_status("USB not found, and HOST is not available, not continuing.\n");
#ifdef NO_RESET_IOP_WHEN_USB
			menu_status("This is a noreset build.\n"
						"This is usually necessary for uLaunchELF and USB users.\n"
						"Please try the 'regular' biosdrain.elf build.\n");
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

int main(void)
{
	sio_puts("main()\n");

	menu_init();
	graphic_init();
	fontengine_init();
	graphic_biosdrain_fadein();

	menu_status("biosdrain - revision %s\n", GIT_VERSION);
#ifdef SUPPORT_SYSTEM_2x6
	menu_status("version with namco system 246/256 (COH-H models) support\n");
#endif
	if (determine_device())
		goto exit_main;

	load_irx_sysman();

	menu_status("Dumping to %s\n", use_usb_dir ? "USB" : "HOST");

	LoadSystemInformation();

	dump_init(use_usb_dir);

	dump_exec();
	dump_cleanup();
exit_main:
	menu_status("Finished everything.\n");
	SleepThread();
}

#ifdef SUPPORT_SYSTEM_2x6
void _libcglue_rtc_update() {} // system 246/256 doesnt have CDVDFSV module loaded on boot
#endif
