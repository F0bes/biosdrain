#pragma once

// Over engineered? Probably.

#include "config.h"
#include "sysman/sysinfo.h"
#include "sysman_rpc.h"

#include <tamtypes.h>

typedef struct
{
	const char *dump_name;
	const char *dump_fext;
	// non-zero if the dump has failed
	u32 (*dump_func)();
	u32 dump_data;
	u32 dump_size;
	u8 enabled;
} t_dump;

void dump_init(u32 use_usb);
void dump_exec();
void dump_cleanup();
