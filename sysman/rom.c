#include <ioman.h>
#include <intrman.h>
#include <loadcore.h>
#include <sysclib.h>
#include <sysmem.h>
#include <stdio.h>
#include <ssbusc.h>
#include <defs.h>

#include "main.h"
#include "sysinfo.h"
#include "sysman_rpc.h"
#include "romdrv/romdrv.h"
#include "rom.h"

static int EROMInitialize(void);
static int EROMGetHardwareInfo(t_PS2DBROMHardwareInfo *devinfo);

int ROMInitialize(void)
{
	DEBUG_PRINTF("SYSMAN: Initializing ROM support.\n");
	romdrv_init();

	EROMInitialize();

	return 0;
}

static int GetSizeFromDelay(int device)
{
	int size = (GetDelay(device) >> 16) & 0x1F;

	return (1 << size);
}

static struct RomImg *romGetImageStat(const void *start, const void *end, struct RomImg *ImageStat)
{
	const u32 *ptr;
	unsigned int offset;
	const struct RomDirEntry *file;
	u32 size;

	offset = 0;
	file = (struct RomDirEntry *)start;
	for (; file < (const struct RomDirEntry *)end; file++, offset += sizeof(struct RomDirEntry))
	{
		/* Check for a valid ROM filesystem (Magic: "RESET\0\0\0\0\0"). Must have the right magic and bootstrap code size (size of RESET = bootstrap code size). */
		ptr = (u32 *)file->name;
		if (ptr[0] == 0x45534552 && ptr[1] == 0x54 && (*(u16 *)&ptr[2] == 0) && (((file->size + 15) & ~15) == offset))
		{
			ImageStat->ImageStart = start;
			ImageStat->RomdirStart = ptr;
			size = file[1].size; // Get size of image from ROMDIR (after RESET).
			ImageStat->RomdirEnd = (const void *)((const u8 *)ptr + size);
			return ImageStat;
		}
	}

	ImageStat->ImageStart = 0;
	return NULL;
}

int ROMGetHardwareInfo(t_SysmanHardwareInfo *hwinfo)
{
	int result;
	unsigned int i, size;
	struct RomImg ImgStat;
	const struct RomImg *pImgStat;

	// Determine the sizes of the boot ROM and DVD ROM chips.
	// DEV2, BOOT ROM
	hwinfo->BOOT_ROM.StartAddress = 0x1FC00000; // Hardwired
	hwinfo->BOOT_ROM.crc16 = 0;
	hwinfo->BOOT_ROM.size = GetSizeFromDelay(SSBUSC_DEV_BOOTROM);
	hwinfo->BOOT_ROM.IsExists = 1;

	if (hwinfo->BOOT_ROM.size > 0)
		printf("DEV2: 0x%lx-0x%lx\n", hwinfo->BOOT_ROM.StartAddress, hwinfo->BOOT_ROM.StartAddress + hwinfo->BOOT_ROM.size - 1);

	// DEV1, DVD ROM
	hwinfo->DVD_ROM.StartAddress = GetBaseAddress(SSBUSC_DEV_DVDROM);
	hwinfo->DVD_ROM.crc16 = 0;
	hwinfo->DVD_ROM.size = GetSizeFromDelay(SSBUSC_DEV_DVDROM);
	hwinfo->DVD_ROM.IsExists = romGetImageStat((const void *)hwinfo->DVD_ROM.StartAddress, (const void *)(hwinfo->DVD_ROM.StartAddress + 0x4000), &ImgStat) != NULL;

	if (hwinfo->DVD_ROM.size > 0)
		printf("DEV1: 0x%lx-0x%lx\n", hwinfo->DVD_ROM.StartAddress, hwinfo->DVD_ROM.StartAddress + hwinfo->DVD_ROM.size - 1);

	// Process virtual directories
	// DEV2, BOOT ROM
	// rom0

	pImgStat = romGetDevice(0);

	if (pImgStat != NULL)
	{
		hwinfo->ROMs[0].IsExists = 1;
		hwinfo->ROMs[0].StartAddress = (u32)pImgStat->ImageStart;
		hwinfo->ROMs[0].size = 0;
		hwinfo->ROMs[0].crc16 = 0;
	}
	else
	{
		hwinfo->ROMs[0].IsExists = 0;
		hwinfo->ROMs[0].StartAddress = 0;
		hwinfo->ROMs[0].size = 0;
		hwinfo->ROMs[0].crc16 = 0;
	}

	// DEV1, DVD ROM
	/*	The DVD ROM contains the rom1, rom2 and erom regions, and these regions exist in this order within the DVD ROM chip.
		The rom2 region only exists on Chinese consoles.
		TOOL consoles have DEV1 installed, but it contains no filesystem and contains hardware IDs instead.	*/
	// rom1 (part of DEV1)

	// fobes: this is a hack, but I have 0 clue how ps2ident
	// manages to load unit 1 in romdrv
	romAddDevice(1, (void *)hwinfo->DVD_ROM.StartAddress);
	pImgStat = romGetDevice(1);

	if (pImgStat != NULL)
	{
		hwinfo->ROMs[1].IsExists = 1;
		hwinfo->ROMs[1].StartAddress = (u32)pImgStat->ImageStart;
		hwinfo->ROMs[1].size = 0;
		hwinfo->ROMs[1].crc16 = 0;
	}
	else
	{
		hwinfo->ROMs[1].IsExists = 0;
		hwinfo->ROMs[1].StartAddress = 0;
		hwinfo->ROMs[1].size = 0;
		hwinfo->ROMs[1].crc16 = 0;
	}

	// rom2 (part of DEV1)
	// fobes: continuing this hack, but for ROM2
	romAddDevice(2, (void*)0x1E400000);
	pImgStat = romGetDevice(2);
	if (pImgStat != NULL)
	{
		hwinfo->ROMs[2].IsExists = 1;
		hwinfo->ROMs[2].StartAddress = (u32)pImgStat->ImageStart;
		hwinfo->ROMs[2].size = 0;
		hwinfo->ROMs[2].crc16 = 0;
	}
	else
	{
		hwinfo->ROMs[2].IsExists = 0;
		hwinfo->ROMs[2].StartAddress = 0;
		hwinfo->ROMs[2].size = 0;
		hwinfo->ROMs[2].crc16 = 0;
	}

	if (hwinfo->ROMs[1].IsExists)
	{ // If rom1 exists, erom may exist.
		EROMGetHardwareInfo(&hwinfo->erom);
	}
	else
	{ // erom cannot exist if rom1 (hence EROMDRV) doesn't exist.
		hwinfo->erom.IsExists = 0;
	}

	for (i = 0; i <= 2; i++)
	{
		if (hwinfo->ROMs[i].IsExists)
		{
			if ((result = SysmanCalcROMRegionSize((void *)hwinfo->ROMs[i].StartAddress)) > 0)
			{
				printf("rom%u:\t%u bytes\n", i, result);
				hwinfo->ROMs[i].size = result;
			}
		}
	}

	/* The set size of DEV1 may not be its real size.
	   Now that the sizes of the individual regions are known, check that the size of DEV1 is fitting.
	   The DVD ROM contains the rom1, rom2 and erom regions, and these regions exist in this order within the DVD ROM chip.
	   The rom2 region only exists on Chinese consoles. */
	size = SysmanCalcROMChipSize(hwinfo->erom.StartAddress - hwinfo->ROMs[1].StartAddress + hwinfo->erom.size);
	printf("DVD ROM real size: %u (DEV1: %lu)\n", size, hwinfo->DVD_ROM.size);

	if (size < hwinfo->DVD_ROM.size)
		hwinfo->DVD_ROM.size = size;

	return 0;
}

/*! \brief Get pointer to head of module list, or next module in list.
 *  \ingroup iopmgr
 *
 *  \param cur_mod Pointer to module structure, or 0 to return the head.
 *  \return Pointer to module structure.
 *
 * return values:
 *   0 if end of list.
 *   pointer to head of module list if cur_mod=0.
 *   pointer to next module, if cur_mod!=0.
 */
static ModuleInfo_t *smod_get_next_mod(ModuleInfo_t *cur_mod)
{
	/* If cur_mod is 0, return the head of the list.  */
	if (!cur_mod)
	{
		return GetLoadcoreInternalData()->image_info;
	}
	else
	{
		return cur_mod->next;
	}
	return 0;
}

/*! \brief Get pointer to module structure for named module.
 *  \ingroup iopmgr
 *
 *  \param name Stringname of module (eg "atad_driver").
 *  \return Pointer to module structure.
 *
 * return values:
 *   0 if not found.
 *   pointer to module structure for loaded module if found.
 */
static ModuleInfo_t *smod_get_mod_by_name(const char *name)
{
	ModuleInfo_t *modptr;
	int len = strlen(name) + 1;

	modptr = smod_get_next_mod(NULL);
	while (modptr != NULL)
	{
		if (!memcmp(modptr->name, name, len))
			return modptr;

		modptr = modptr->next;
	}

	return NULL;
}

static void *Get_EROM_RAM_Address(const void *EntryPoint)
{
	unsigned int i;
	void *result;
	const unsigned int *ptr;

	for (i = 0, ptr = EntryPoint, result = NULL; i < 0x20; i++, ptr++)
	{
		if (*ptr == 0x03e00008)
		{ // jr $ra
			// End of function reached - scan failed.
			break;
		}
		else if ((*ptr) >> 16 == 0x3c04)
		{ // lui $a0, XXXX
			result = (void *)((*ptr) << 16);
			break;
		}
	}

	return result;
}

typedef struct
{
	unsigned int filename_hash;
	unsigned int fileoffset_hash;	   /* from erom start, obfuscated 		*/
	unsigned int filesize_hash;		   /* obfuscated 				*/
	unsigned int next_fileoffset_hash; /* from erom start, obfuscated 		*/
	unsigned int xordata_size_hash;	   /* in unsigned int, obfuscated		*/
} erom_filedescriptor_t;

#define HASHKEY0 0x38e38e39
static unsigned int mult_hi(unsigned int x, unsigned int y)
{
	unsigned int result;

	asm("multu %1, %2\n"
		"mfhi %0"
		: "=r"(result)
		: "r"(x), "r"(y));

	return result;
}
/*
 * function to get uint32 value from hash type 0
 */
static unsigned int get_val_from_hash0(unsigned int hash)
{
	unsigned int v0, v1, ret = 0;
	int i;

	for (i = 0; i < 32; i += 4)
	{
		v0 = mult_hi(hash, HASHKEY0) >> 2;
		hash -= (((v0 << 3) + v0) << 1);
		v1 = hash - 1;
		if (v1 >= 0x0a)
			v1 = hash - 2;
		ret |= (v1 << i);
		hash = v0;
	}

	return ret;
}

/*
 * function to generate a uint32 hash value from a string (6 chars max)
 */
static unsigned int get_string_hash(char *p_str)
{
	char str[7];

	strncpy(str, p_str, 6); /* filenames must be max 6 characters in erom */
	str[6] = 0;

	unsigned char *p, *p2;
	register unsigned int hash = 0;

	for (p = (unsigned char *)&str[0], p2 = (unsigned char *)&str[6]; (unsigned int)p < (unsigned int)p2; p++)
	{
		register unsigned int val, byte;

		byte = *p;

		if ((unsigned int)(byte - 'A') < '\x0d')
			val = byte - '@';
		else if ((unsigned char)byte == '\0')
			val = '\x0e';
		else if ((unsigned int)((unsigned char)byte >= 'N'))
			val = byte - '?';
		else if ((unsigned char)byte == ' ')
			val = '\x1c';
		else
			val = byte - '\x13';

		hash = (((hash << 2) + hash) << 3) + val;
	}

	return hash;
}

/* This function attempts to locate the first file descriptor, which describes the entire EROM area. */
static void *InitEROM(const void *start, const void *end, const erom_filedescriptor_t **erom_start)
{
	int OldState;
	unsigned int size, hash, *ptr;
	unsigned char *buffer;
	static const char token[] = " @ A B";
	char letter, token_local[sizeof(token)];
	void *result;

	memcpy(token_local, token, sizeof(token_local));

	size = (unsigned int)end - (unsigned int)start;
	result = NULL;

	CpuSuspendIntr(&OldState);
	buffer = AllocSysMemory(ALLOC_FIRST, size, NULL);
	CpuResumeIntr(OldState);

	if (buffer != NULL)
	{
		memcpy(buffer, start, size);

		for (letter = 'C'; letter < '['; letter++)
		{
			token_local[1] = token_local[3];
			token_local[3] = token_local[5];
			token_local[5] = letter;

			hash = get_string_hash(token_local);
			for (ptr = (unsigned int *)buffer; (unsigned int)ptr < (unsigned int)&buffer[size]; ptr++)
			{
				if (*ptr == hash)
				{
					*erom_start = (const erom_filedescriptor_t *)((((unsigned int)ptr - (unsigned int)buffer) >> 2 << 2) + (unsigned int)start);
					result = erom_start;
					goto erom_search_end;
				}
			}
		}

	erom_search_end:
		CpuSuspendIntr(&OldState);
		FreeSysMemory(buffer);
		CpuResumeIntr(OldState);
	}

	return result;
}

static const void *EromArea;
static const erom_filedescriptor_t *EROMStart;

static int EROMInitialize(void)
{
	ModuleInfo_t *eromdrv_module;

	if ((eromdrv_module = smod_get_mod_by_name("erom_file_driver")) != NULL)
	{
		if ((EromArea = Get_EROM_RAM_Address((const void *)eromdrv_module->entry)) != NULL)
		{
			InitEROM(EromArea, &((const unsigned char *)EromArea)[0x400], &EROMStart);
		}
		else
		{
			printf("SYSMAN: Error - Unable to locate EROM address.\n");
			EROMStart = NULL;
		}
	}
	else
	{
		printf("SYSMAN: EROM filesystem driver not found.\n");
		EromArea = NULL;
		EROMStart = NULL;
	}

	return 0;
}

static int EROMGetHardwareInfo(t_PS2DBROMHardwareInfo *devinfo)
{
	DEBUG_PRINTF("SYSMAN: Probing EROM.\n");
	if ((devinfo->IsExists = (EromArea != NULL && EROMStart != NULL)))
	{
		devinfo->StartAddress = (u32)EromArea;
		devinfo->size = get_val_from_hash0(EROMStart->fileoffset_hash);

		printf("SYSMAN: EROM detected: %p, %p, %lu\n", EromArea, EROMStart, devinfo->size);
	}
	else
	{
		printf("SYSMAN: EROM not detected.\n");
	}

	return 0;
}
