#include "dump.h"

#include "ui/menu.h"

#include <libcdvd.h>
#include <kernel.h>

#include "sysman/sysinfo.h" // t_SysmanHardwareInfo
#include "sysman_rpc.h"

static u32 dump_rom0_func();
static u32 dump_rom1_func();
static u32 dump_rom2_func();
static u32 dump_erom_func();
static u32 dump_nvm_func();
static u32 dump_mec_func();

static u8 dump_shared_buffer[0x400000];

extern t_SysmanHardwareInfo g_hardwareInfo;

static t_dump dump_jobs[6];

static u32 dump_file_usb = 0;
static u32 dump_file(t_dump job)
{
	char path[256];
	sprintf(path, "%s%s", dump_file_usb ? "mass:" : "host:", job.dump_fname);

	FILE *f = fopen(path, "wb+");
	if (!f)
		return 1;

	fwrite(dump_shared_buffer, 1, job.dump_size, f);

	FlushCache(0);

	fclose(f);

	return 0;
}

void dump_init(u32 use_usb)
{
	dump_file_usb = use_usb;

	// ROM0
	{
		dump_jobs[0].dump_name = "ROM0";
		dump_jobs[0].dump_fname = "BIOS.rom0";
		dump_jobs[0].dump_func = dump_rom0_func;
		dump_jobs[0].dump_size = 0x400000;
		dump_jobs[0].enabled = g_hardwareInfo.ROMs[0].IsExists;
	}
	// ROM1
	{
		dump_jobs[1].dump_name = "ROM1";
		dump_jobs[1].dump_fname = "BIOS.rom1";
		dump_jobs[1].dump_func = dump_rom1_func;
		dump_jobs[1].dump_size = g_hardwareInfo.ROMs[1].size;
		dump_jobs[1].enabled = g_hardwareInfo.ROMs[1].IsExists;
	}
	// ROM2
	{
		dump_jobs[2].dump_name = "ROM2";
		dump_jobs[2].dump_fname = "BIOS.rom2";
		dump_jobs[2].dump_func = dump_rom2_func;
		dump_jobs[2].dump_size = g_hardwareInfo.ROMs[2].size;
		dump_jobs[2].enabled = g_hardwareInfo.ROMs[2].IsExists;
	}
	// EROM
	{
		dump_jobs[3].dump_name = "EROM";
		dump_jobs[3].dump_fname = "BIOS.erom";
		dump_jobs[3].dump_func = dump_erom_func;
		dump_jobs[3].dump_size = g_hardwareInfo.erom.size;
		dump_jobs[3].enabled = g_hardwareInfo.erom.IsExists;
	}
	// NVM
	{
		dump_jobs[4].dump_name = "NVM";
		dump_jobs[4].dump_fname = "BIOS.nvm";
		dump_jobs[4].dump_func = dump_nvm_func;
		dump_jobs[4].dump_size = 1024;
		dump_jobs[4].enabled = g_hardwareInfo.DVD_ROM.IsExists;
	}
	// MEC
	{
		dump_jobs[5].dump_name = "MEC";
		dump_jobs[5].dump_fname = "BIOS.mec";
		dump_jobs[5].dump_func = dump_mec_func;
		dump_jobs[5].dump_size = 4;
		dump_jobs[5].enabled = g_hardwareInfo.DVD_ROM.IsExists;
	}
}

void dump_exec()
{
	for (u32 i = 0; i < 6; i++)
	{
		if (dump_jobs[i].enabled)
		{
			menu_status("Dumping %s...", dump_jobs[i].dump_name);
			u32 ret = dump_jobs[i].dump_func();
			if (!ret)
			{
				FlushCache(0);
				menu_status("Writing to file...");
				dump_file(dump_jobs[i]);
				menu_status("Finished\n");
			}
			else
			{
				menu_status("Dump failed, result %d\n", ret);
			}
		}
	}
}

// Used for ROMx and EROM dumps
// Pretty much just a SysmanReadMemory wrapper
static void common_dump_func(u32 start, u32 size)
{
	u32 area_ptr = start;
	u32 buf_ptr = (u32)dump_shared_buffer;

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
static u32 dump_rom0_func()
{
	common_dump_func(g_hardwareInfo.ROMs[0].StartAddress, 0x400000);
	return 0;
}
static u32 dump_rom1_func()
{
	common_dump_func(g_hardwareInfo.ROMs[1].StartAddress, g_hardwareInfo.ROMs[1].size);
	return 0;
}
static u32 dump_rom2_func()
{
	common_dump_func(g_hardwareInfo.ROMs[2].StartAddress, g_hardwareInfo.ROMs[2].size);
	return 0;
}
static u32 dump_erom_func()
{
	common_dump_func(g_hardwareInfo.erom.StartAddress, g_hardwareInfo.erom.size);
	return 0;
}
static u32 dump_nvm_func()
{
	// The sceCdReadNVM function retrieves data one u16 at a time.
	// There are 512 u16 blocks in the NVRAM.

	u8 result;
	for (u32 i = 0; i < 512; i++)
	{
		if (sceCdReadNVM(i, (u16 *)dump_shared_buffer + i, &result) != 1 || result != 0)
		{
			return result;
		}
		
	}
	return 0;
}
static u32 dump_mec_func()
{
	// cmdNum 0x03: Mechacon cmd
	// inBuff[0] 0x00: Read MEC version
	// Mechacon version is only 4 bytes long

	const u16 cmd = 0;
	return !sceCdApplySCmd(0x03, (void *)&cmd, sizeof(cmd), (void *)dump_shared_buffer, 4);
}
