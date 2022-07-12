/*	iLink.c
 *	Purpose:	Contains the functions that are used for controlling the i.Link hardware and writing/reading values to/from the PHY.
 *
 *	Last Updated:	2012/09/21
 *	Programmer:	SP193
 */

#include <thbase.h>
#include <stdio.h>

#include "main.h"
#include "sysinfo.h"
#include "iLink.h"

#define DISABLE_ILINK_DUMPING	//For testing within PCSX2

static struct ILINKMemMap *ILINKRegisterBase = (struct ILINKMemMap *)ILINK_REGISTER_BASE;

static inline void iLinkDisableIntr(void)
{
	ILINKRegisterBase->intr0Mask = 0;
	ILINKRegisterBase->intr1Mask = 0;
	ILINKRegisterBase->intr2Mask = 0;

	/* Set all interrupt bits, to acknowledge any pending interrupts that there may be. */
	ILINKRegisterBase->intr0 = 0xFFFFFFFF;
	ILINKRegisterBase->intr1 = 0xFFFFFFFF;
	ILINKRegisterBase->intr2 = 0xFFFFFFFF;
}

static inline int iLinkResetHW(void)
{
	/* Turn off, and then turn back on the LINK and PHY. If this is not done, the iLink hardware might not function correctly on some consoles. :( */
	ILINKRegisterBase->UnknownRegister7C = 0x40; /* Shut down. */
	DelayThread(400000);
	ILINKRegisterBase->UnknownRegister7C = 0;

	/* If the LINK is off, switch it on. */
	if (!(ILINKRegisterBase->ctrl2 & iLink_CTRL2_LPSEn))
		ILINKRegisterBase->ctrl2 = iLink_CTRL2_LPSEn;

	/* Wait for the clock to stabilize. */
	while (!(ILINKRegisterBase->ctrl2 & iLink_CTRL2_SOK))
		DelayThread(50);

	return 0;
}

int iLinkInitialize(void)
{
	DEBUG_PRINTF("SYSMAN: Initializing i.Link support.\n");

#ifndef DISABLE_ILINK_DUMPING
	iLinkDisableIntr();
	iLinkResetHW();
#endif

	return 0;
}

#define RdPhy 0x80000000
#define WrPhy 0x40000000

static void iLinkPhySync(void)
{
	while (ILINKRegisterBase->PHYAccess & (RdPhy | WrPhy))
	{
	};
}

unsigned char iLinkReadPhy(unsigned char address)
{
	ILINKRegisterBase->PHYAccess = (((unsigned int)address) << 24) | RdPhy;
	iLinkPhySync();

	while (!(ILINKRegisterBase->intr0 & iLink_INTR0_PhyRRx))
	{
	};
	ILINKRegisterBase->intr0 = iLink_INTR0_PhyRRx;

	return (ILINKRegisterBase->PHYAccess & 0xFF);
}

void iLinkWritePhy(unsigned char address, unsigned char data)
{
	ILINKRegisterBase->PHYAccess = (((unsigned int)address) << 24) | (((unsigned int)data) << 16) | WrPhy;
	iLinkPhySync();
}

int iLinkPHYGetNumPorts(void)
{
	return (iLinkReadPhy(2) & REG02_TOT_PRTS_MSK);
}

int iLinkPHYGetMaxSpeed(void)
{
	return (iLinkReadPhy(3) >> REG03_MAX_SPD_BSFT & REG03_MAX_SPD_MSK);
}

void iLinkPHYSetPage(unsigned char page)
{
	iLinkWritePhy(7, (page & REG07_PAGE_SEL_MSK) << REG07_PAGE_SEL_BSFT);
}

int iLinkGetHardwareInfo(t_PS2DBiLinkHardwareInfo *devinfo)
{
#ifndef DISABLE_ILINK_DUMPING
	unsigned int value;

	DEBUG_PRINTF("SYSMAN: Probing i.Link.\n");

	devinfo->NumPorts = iLinkPHYGetNumPorts();
	devinfo->MaxSpeed = iLinkPHYGetMaxSpeed();
	iLinkPHYSetPage(1);

	devinfo->ComplianceLevel = iLinkReadPhy(0x8);

	value = (unsigned int)iLinkReadPhy(0xA) << 16;
	value |= (unsigned int)iLinkReadPhy(0xB) << 8;
	value |= iLinkReadPhy(0xC);
	devinfo->VendorID = value;

	value = (unsigned int)iLinkReadPhy(0xD) << 16;
	value |= (unsigned int)iLinkReadPhy(0xE) << 8;
	value |= iLinkReadPhy(0xF);
	devinfo->ProductID = value;
#endif

	return 0;
}
