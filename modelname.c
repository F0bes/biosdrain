/*
 Modelname Reader, modified by Fobes.
 Originally licensed as AFL3 under the PS2IDENT project
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <kernel.h>
#include <sifcmd.h>
#include <libcdvd.h>
#include <unistd.h>
#include <fcntl.h>

#include "OSDInit.h"

#include "modelname.h"

extern char ConsoleROMVER[];

static int sceCdAltRM(char *ModelName, int *stat)
{
	unsigned char rdata[9];
	unsigned char sdata;
	int result1, result2;

	sdata = 0;
	result1 = sceCdApplySCmd(0x17, &sdata, 1, rdata, 9);

	*stat = rdata[0];
	memcpy(ModelName, &rdata[1], 8);

	sdata = 8;
	result2 = sceCdApplySCmd(0x17, &sdata, 1, rdata, 9);

	*stat |= rdata[0];
	memcpy(&ModelName[8], &rdata[1], 8);

	return ((result1 != 0 && result2 != 0) ? 1 : 0);
}

int modelname_read(char *name)
{
	OSDInitROMVER();

	int stat, result, fd;

	/*	This function is a hybrid between the late ROM browser program and the HDD Browser.
		In v2.20, there was only a simple null-terminate before calling sceCdRM(), as below.
		However, this does not support the early PlayStation 2 models.
		In the HDD Browser, the model name buffer was a global instead of an argument.	*/
	memset(name, 0, MODEL_NAME_MAX_LEN);

	/*		Invoking the "Read model" command on the first SCPH-10000 seems to return an error code,
			but it seems to function on my SCPH-15000...although no valid name is returned!
			In the HDD OSD, "SCPH-10000" is returned when the ROM v1.00 is detected (It checks rom0:ROMVER).
			It attempts to read the model name from the ROM OSDSYS program, when ROM v1.01 (late SCPH-10000 and all SCPH-15000 models) is detected.

			The model name was originally hardcoded into the OSDSYS. The MECHACON EEPROM did not have the model name recorded in it.

			Oddly, the console models that come with v1.01, like the SCPH-15000 and DTL-H10000,
			will always be identified as "SCPH-10000" by their ROM OSDSYS programs.
	*/
	if (ConsoleROMVER[0] == '0' && ConsoleROMVER[1] == '1' && ConsoleROMVER[2] == '0')
	{
		if (ConsoleROMVER[3] == '0') // For ROM v1.00 (Early SCPH-10000 units).
			strcpy(name, "SCPH-10000");
		else
		{ // For ROM v1.01 (Late SCPH-10000, and all SCPH-15000 units).
			if ((fd = open("rom0:OSDSYS", O_RDONLY)) >= 0)
			{ // The model name is located at this address.
				lseek(fd, 0x8C808, SEEK_SET);
				read(fd, name, MODEL_NAME_MAX_LEN);
				close(fd);
			}
			else
				strcpy(name, "Unknown");
		}

		return 0; // Original returned -1
	}
	else
	{
		if ((result = sceCdAltRM(name, &stat)) == 1)
		{
			if (stat & 0x80)
				return -2;
			if ((stat & 0x40) || name[0] == '\0')
				strcpy(name, "Unknown");

			return 0; // Original returned -1
		}
		else
			return -2;
	}
}
