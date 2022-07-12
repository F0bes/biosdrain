#include <types.h>
#include <dev9.h>
#include <dev9regs.h>
#include <aifregs.h>
#include <loadcore.h>
#include <speedregs.h>
#include <smapregs.h>
#include <sysclib.h>
#include <stdio.h>
#include <thbase.h>

#include "main.h"
#include "sysinfo.h"
#include "dev9.h"

static int (*pdev9GetEEPROM)(u16 *buf) = NULL;

static int _smap_read_phy(unsigned int address)
{
	unsigned int i, PHYRegisterValue;
	int result;
	USE_SMAP_EMAC3_REGS;

	PHYRegisterValue = (address & SMAP_E3_PHY_REG_ADDR_MSK) | SMAP_E3_PHY_READ | ((SMAP_DsPHYTER_ADDRESS & SMAP_E3_PHY_ADDR_MSK) << SMAP_E3_PHY_ADDR_BITSFT);

	i = 0;
	result = 0;
	SMAP_EMAC3_SET(SMAP_R_EMAC3_STA_CTRL, PHYRegisterValue);

	do
	{
		if (SMAP_EMAC3_GET(SMAP_R_EMAC3_STA_CTRL) & SMAP_E3_PHY_OP_COMP)
		{
			if (SMAP_EMAC3_GET(SMAP_R_EMAC3_STA_CTRL) & SMAP_E3_PHY_OP_COMP)
			{
				if ((result = SMAP_EMAC3_GET(SMAP_R_EMAC3_STA_CTRL)) & SMAP_E3_PHY_OP_COMP)
				{
					result >>= SMAP_E3_PHY_DATA_BITSFT;
					break;
				}
			}
		}

		DelayThread(1000);
		i++;
	} while (i < 100);

	if (i >= 100)
		DEBUG_PRINTF("smap: %s: > %d ms\n", "_smap_read_phy", i);

	return result;
}

static inline int GetPHYData(t_PS2DBSSBUSHardwareInfo *devinfo)
{
	unsigned short int idr1, idr2;

	idr1 = _smap_read_phy(SMAP_DsPHYTER_PHYIDR1);
	idr2 = _smap_read_phy(SMAP_DsPHYTER_PHYIDR2);

	/*	IDR1's value is generated with OUI<<2>>8, so do the reverse.
		The bits are swapped around, and the most significant 2 bits are omitted.
	*/
	devinfo->SPEED.SMAP_PHY_OUI = idr1 << 6 | (idr2 >> 10);
	devinfo->SPEED.SMAP_PHY_VMDL = idr2 >> 4 & 0x3F;
	devinfo->SPEED.SMAP_PHY_REV = idr2 & SMAP_PHY_IDR2_REV_MSK;

	return 0;
}

/* Modified slib_get_lib_by_name() from slib. len is the length of the module's name (inclusive of the NULL terminator). */
static inline iop_library_t *FindLibrary(const char *name, int len)
{
	lc_internals_t *InternalData;
	iop_library_t *libptr;

	InternalData = GetLoadcoreInternalData();
	libptr = InternalData->let_next;
	while ((libptr != 0))
	{
		if (!memcmp(libptr->name, name, len))
			return libptr;
		libptr = libptr->prev;
	}

	return NULL;
}

int dev9Initialize(void)
{
	struct irx_export_table *dev9_export_table;

	DEBUG_PRINTF("SYSMAN: Initializing DEV9 support.\n");

	// Do not link to DEV9.IRX directly. If DEV9.IRX cannot be loaded because no SPEED device is connected, this module can't load either.
	if ((dev9_export_table = (struct irx_export_table *)FindLibrary("dev9", 4)) != NULL)
	{
		pdev9GetEEPROM = dev9_export_table->fptrs[9];
	}

	return 0;
}

int dev9GetHardwareInfo(t_PS2DBSSBUSHardwareInfo *devinfo)
{
	int *mode;
	USE_DEV9_REGS;
	USE_AIF_REGS;
	USE_SPD_REGS;

	DEBUG_PRINTF("SYSMAN: Probing DEV9.\n");

	devinfo->revision = DEV9_REG(DEV9_R_REV);

	if (pdev9GetEEPROM != NULL)
	{
		devinfo->status = PS2DB_SSBUS_HAS_SPEED;
		devinfo->SPEED.rev1 = SPD_REG16(SPD_R_REV_1);
		devinfo->SPEED.rev3 = SPD_REG16(SPD_R_REV_3);
		devinfo->SPEED.rev8 = SPD_REG16(SPD_R_REV_8);

		if (SPD_REG16(SPD_R_REV_3) & SPD_CAPS_SMAP)
			GetPHYData(devinfo);
	}
	else
	{
		devinfo->status = 0;
	}

	if ((mode = QueryBootMode(6)) != NULL)
	{
		if ((*(u16 *)mode & 0xfe) == 0x60)
		{
			printf("SYSMAN: T10K detected.\n");

			if (aif_regs[AIF_IDENT] == 0xa1)
			{
				printf("AIF controller revision: %d.\n", aif_regs[AIF_REVISION]);
				devinfo->status |= PS2DB_SSBUS_HAS_AIF;
				devinfo->AIFRevision = aif_regs[AIF_REVISION];
			}
		}
	}

	return 0;
}

int dev9GetSPEEDMACAddress(unsigned char *MACAddress)
{
	unsigned short int data[3];
	int result;

	if (pdev9GetEEPROM != NULL)
	{
		if ((result = pdev9GetEEPROM(data)) == 0)
		{
			memcpy(MACAddress, data, 6);
		}
	}
	else
		result = -1;

	return result;
}
