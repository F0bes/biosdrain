#include <errno.h>
#include <intrman.h>
#include <ioman.h>
#include <loadcore.h>
#include <sysclib.h>
#include <irx.h>

#include "romdrv.h"

// Unlike the version used in UDNL, this is only 12-bytes (no padding field).
struct RomdirFileStat
{
	const struct RomDirEntry *romdirent;
	const void *data;
	const void *extinfo;
};

struct RomFileSlot
{
	int slotNum;
	int offset;
};

static struct RomImg images[ROMDRV_MAX_IMAGES];
static struct RomdirFileStat fileStats[ROMDRV_MAX_FILES];
static struct RomFileSlot fileSlots[ROMDRV_MAX_FILES];

/* Function prototypes */
static int romUnsupported(void);
static int romInit(iop_device_t *device);
static int romOpen(iop_file_t *fd, const char *path, int mode);
static int romClose(iop_file_t *);
static int romRead(iop_file_t *fd, void *buffer, int size);
static int romWrite(iop_file_t *fd, void *buffer, int size);
static int romLseek(iop_file_t *fd, int offset, int whence);
static struct RomImg *romGetImageStat(const void *start, const void *end, struct RomImg *ImageStat);
static struct RomdirFileStat *GetFileStatFromImage(const struct RomImg *ImageStat, const char *filename, struct RomdirFileStat *stat);

static iop_device_ops_t ops = {
	&romInit,
	(void *)&romUnsupported,
	(void *)&romUnsupported,
	&romOpen,
	&romClose,
	&romRead,
	&romWrite,
	&romLseek,
	(void *)&romUnsupported,
	(void *)&romUnsupported,
	(void *)&romUnsupported,
	(void *)&romUnsupported,
	(void *)&romUnsupported,
	(void *)&romUnsupported,
	(void *)&romUnsupported,
	(void *)&romUnsupported,
	(void *)&romUnsupported};

static iop_device_t DeviceOps = {
	"rom",
	IOP_DT_FS,
	1,
	"ROM/Flash",
	&ops};

int romdrv_init(void)
{
	DelDrv("rom");

	AddDrv(&DeviceOps);

	memset(images, 0, sizeof(images));
	memset(fileStats, 0, sizeof(fileStats));
	memset(fileSlots, 0, sizeof(fileSlots));

	// Add DEV2 (Boot ROM) as rom0. Unlike ROMDRV v1.1, the code for DEV1 is in the ADDDRV module.
	romGetImageStat((const void *)0x1fc00000, (const void *)0x1fc40000, &images[0]);
	return 0;
}

static int romUnsupported(void)
{
	return 0;
}

static int romInit(iop_device_t *device)
{
	return 0;
}

int romAddDevice(int unit, const void *image)
{
	int result, OldState;
	struct RomImg *stat;

	if (unit < ROMDRV_MAX_IMAGES)
	{
		CpuSuspendIntr(&OldState);

		if (images[unit].ImageStart == NULL)
		{
			stat = romGetImageStat(image, (const void *)((const u8 *)image + 0x8000), &images[unit]);
			CpuResumeIntr(OldState);

			result = stat != NULL ? 0 : ROMDRV_ADD_BAD_IMAGE;
		}
		else
		{
			result = ROMDRV_ADD_FAILED;
			CpuResumeIntr(OldState);
		}
	}
	else
		result = ROMDRV_ADD_FAILED;

	return result;
}

int romDelDevice(int unit)
{
	int result, OldState;

	if (unit < ROMDRV_MAX_IMAGES)
	{
		CpuSuspendIntr(&OldState);

		if (images[unit].ImageStart != NULL)
		{
			images[unit].ImageStart = 0;
			CpuResumeIntr(OldState);
			result = 0;
		}
		else
		{
			CpuResumeIntr(OldState);
			result = ROMDRV_DEL_FAILED;
		}
	}
	else
		result = ROMDRV_DEL_FAILED;

	return result;
}

static int romOpen(iop_file_t *fd, const char *path, int mode)
{
	struct RomImg *image;
	int OldState, slotNum, result;
	struct RomdirFileStat *stat;

	if (fd->unit < ROMDRV_MAX_IMAGES)
	{
		image = &images[fd->unit];

		if (image->ImageStart != NULL)
		{
			if (mode != O_RDONLY)
				return -EACCES;

			CpuSuspendIntr(&OldState);

			// Locate a free file slot.
			for (slotNum = 0; slotNum < ROMDRV_MAX_FILES; slotNum++)
			{
				if (fileStats[slotNum].data == NULL)
					break;
			}
			if (slotNum == ROMDRV_MAX_FILES)
			{
				CpuResumeIntr(OldState);
				return -ENOMEM;
			}

			stat = GetFileStatFromImage(image, path, &fileStats[slotNum]);
			CpuResumeIntr(OldState);

			if (stat != NULL)
			{
				result = 0;
				fileSlots[slotNum].slotNum = slotNum;
				fileSlots[slotNum].offset = 0;
				fd->privdata = &fileSlots[slotNum];
			}
			else
				result = -ENOENT;

			return result;
		}
		else
			return -ENXIO;
	}
	else
		return -ENXIO;
}

static int romClose(iop_file_t *fd)
{
	struct RomFileSlot *slot = (struct RomFileSlot *)fd->privdata;
	struct RomdirFileStat *stat;
	int result;

	if (slot->slotNum < ROMDRV_MAX_FILES)
	{
		stat = &fileStats[slot->slotNum];

		if (stat->data != NULL)
		{
			stat->data = NULL;
			result = 0;
		}
		else
			result = -EBADF;
	}
	else
		result = -EBADF;

	return result;
}

static int romRead(iop_file_t *fd, void *buffer, int size)
{
	struct RomFileSlot *slot = (struct RomFileSlot *)fd->privdata;
	struct RomdirFileStat *stat;

	stat = &fileStats[slot->slotNum];

	// Bounds check.
	if (stat->romdirent->size < slot->offset + size)
		size = stat->romdirent->size - slot->offset;

	if (size <= 0) // Ignore 0-byte reads.
		return 0;

	memcpy(buffer, (const u8 *)stat->data + slot->offset, size);
	slot->offset += size;

	return size;
}

static int romWrite(iop_file_t *fd, void *buffer, int size)
{
	return -EIO;
}

static int romLseek(iop_file_t *fd, int offset, int whence)
{
	struct RomFileSlot *slot = (struct RomFileSlot *)fd->privdata;
	struct RomdirFileStat *stat;
	u32 size;
	int newOffset;

	stat = &fileStats[slot->slotNum];
	size = stat->romdirent->size;

	switch (whence)
	{
	case SEEK_SET:
		newOffset = offset;
		break;
	case SEEK_CUR:
		newOffset = slot->offset + offset;
		break;
	case SEEK_END:
		newOffset = size + offset;
		break;
	default:
		return -EINVAL;
	}

	// Update offset.
	slot->offset = (size < newOffset) ? size : newOffset;

	return slot->offset;
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

// Similar to the function from UDNL.
static struct RomdirFileStat *GetFileStatFromImage(const struct RomImg *ImageStat, const char *filename, struct RomdirFileStat *stat)
{
	unsigned int i, offset, ExtInfoOffset;
	u8 filename_temp[12];
	const struct RomDirEntry *RomdirEntry;
	struct RomdirFileStat *result;

	offset = 0;
	ExtInfoOffset = 0;
	((u32 *)filename_temp)[0] = 0;
	((u32 *)filename_temp)[1] = 0;
	((u32 *)filename_temp)[2] = 0;
	for (i = 0; *filename >= 0x21 && i < sizeof(filename_temp); i++)
	{
		filename_temp[i] = *filename;
		filename++;
	}

	if (ImageStat->RomdirStart != NULL)
	{
		RomdirEntry = ImageStat->RomdirStart;

		do
		{ // Fast comparison of filenames.
			if (((u32 *)filename_temp)[0] == ((u32 *)RomdirEntry->name)[0] && ((u32 *)filename_temp)[1] == ((u32 *)RomdirEntry->name)[1] && (*(u16 *)&((u32 *)filename_temp)[2] == *(u16 *)&((u32 *)RomdirEntry->name)[2]))
			{

				stat->romdirent = RomdirEntry;
				stat->data = ImageStat->ImageStart + offset;
				stat->extinfo = NULL;

				if (RomdirEntry->ExtInfoEntrySize != 0)
				{ // Unlike the version within UDNL, the existence of the extinfo entry is optional.
					stat->extinfo = (void *)((u8 *)ImageStat->RomdirEnd + ExtInfoOffset);
				}

				result = stat;
				goto end;
			}

			offset += (RomdirEntry->size + 15) & ~15;
			ExtInfoOffset += RomdirEntry->ExtInfoEntrySize;
			RomdirEntry++;
		} while (((u32 *)RomdirEntry->name)[0] != 0x00000000); // Until the terminator entry is reached.

		result = NULL;
	}
	else
		result = NULL;

end:
	return result;
}

const struct RomImg *romGetDevice(int unit)
{
	int OldState;
	const struct RomImg *result;

	if (unit < ROMDRV_MAX_IMAGES)
	{
		CpuSuspendIntr(&OldState);

		if (images[unit].ImageStart != NULL)
		{
			result = &images[unit];
			CpuResumeIntr(OldState);
		}
		else
		{
			CpuResumeIntr(OldState);
			result = NULL;
		}
	}
	else
		result = NULL;

	return result;
}
