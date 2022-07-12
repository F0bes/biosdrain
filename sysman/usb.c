#include <stdio.h>

#include "main.h"
#include "sysinfo.h"
#include "usb.h"

int usbInitialize(void)
{
	DEBUG_PRINTF("SYSMAN: Initializing USB support.\n");
	return 0;
}

int usbGetHardwareInfo(t_PS2DBUSBHardwareInfo *devinfo)
{
	DEBUG_PRINTF("SYSMAN: Probing USB.\n");

	devinfo->HcRevision = *(volatile unsigned int *)0xBF801600;
	return 0;
}
