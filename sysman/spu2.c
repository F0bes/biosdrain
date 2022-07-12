#include <stdio.h>
#include <ssbusc.h>

#include "main.h"
#include "sysinfo.h"
#include "spu2.h"

int spu2Initialize(void)
{
	DEBUG_PRINTF("SYSMAN: Initializing SPU2 support.\n");

	SetBaseAddress(SSBUSC_DEV_SPU, 0xBF900000);	 /* 0xBF801404=0xBF900000 */
	SetBaseAddress(SSBUSC_DEV_SPU2, 0xBF900800); /* 0xBF80140C=0xBF900800 */
	SetDelay(SSBUSC_DEV_SPU, 0x200B31E1);		 /* 0xBF801014=0x200B31E1 */
	SetDelay(SSBUSC_DEV_SPU2, 0x200B31E1);		 /* 0xBF801414=0x200B31E1 */

	return 0;
}

int spu2GetHardwareInfo(t_PS2DBSPU2HardwareInfo *devinfo)
{
	DEBUG_PRINTF("SYSMAN: Probing SPU2.\n");

	devinfo->revision = *(volatile unsigned short int *)0xbf9007c4;
	return 0;
}
