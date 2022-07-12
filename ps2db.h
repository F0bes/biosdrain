#pragma once
#include <tamtypes.h>

typedef struct PS2DBROMInformation{
	u8 IsExists;
	u8 padding;
	u16 crc16;
	u32 StartAddress;
	u32 size;
} t_PS2DBROMHardwareInfo;

typedef struct PS2DBIOPInformation{
	u16 revision;
	u16 padding;
	u32 RAMSize;
} t_PS2DBIOPHardwareInfo;

typedef struct PS2DBiLinkInformation{
	u8 NumPorts;
	u8 MaxSpeed;
	u8 ComplianceLevel;
	u8 padding;
	u32 VendorID;
	u32 ProductID;
} t_PS2DBiLinkHardwareInfo;

typedef struct PS2DBSPEEDDevInformation{
	u16 rev1;	//revision
	u16 rev3;	//caps
	u16 rev8;
	u8 SMAP_PHY_VMDL;
	u8 SMAP_PHY_REV;
	u32 SMAP_PHY_OUI;
} t_PS2DBSPEEDDevHardwareInfo;

#define PS2DB_SSBUS_HAS_SPEED	0x01
#define PS2DB_SSBUS_HAS_AIF		0x02

typedef struct PS2DBSSBUSInformation{
	u8 revision;
	u8 status;
	u8 AIFRevision;
	u8 padding;
	t_PS2DBSPEEDDevHardwareInfo SPEED;
} t_PS2DBSSBUSHardwareInfo;

typedef struct PS2DBEEInformation{
	u8 implementation;
	u8 revision;
	u8 FPUImplementation;
	u8 FPURevision;
	u16 padding;
	u8 ICacheSize;
	u8 DCacheSize;
	u32 RAMSize;
} t_PS2DBEEHardwareInfo;

typedef struct PS2DBSPU2HardwareInfo{
	u16 revision;
	u16 padding;
} t_PS2DBSPU2HardwareInfo;

typedef struct PS2DBUSBHardwareInfo{
	u8 HcRevision;
	u8 padding[3];
} t_PS2DBUSBHardwareInfo;

typedef struct PS2DBGSHardwareInfo{
	u8 revision;
	u8 id;
	u16 padding;
} t_PS2DBGSHardwareInfo;

#define PS2IDB_NEWENT_FORMAT_VERSION	0x0103

struct PS2IDB_NewMainboardEntryHeader{
	char magic[2];	//"2N"
	u16 version;
};

#define PS2IDB_STAT_ERR_MVER			0x01
#define PS2IDB_STAT_ERR_MNAME			0x02
#define PS2IDB_STAT_ERR_MRENEWDATE		0x04
#define PS2IDB_STAT_ERR_ILINKID			0x08
#define PS2IDB_STAT_ERR_CONSOLEID		0x10
#define PS2IDB_STAT_ERR_ADD010			0x20
#define PS2IDB_STAT_ERR_MODDED			0x80

struct PS2IDBMainboardEntry{
	t_PS2DBROMHardwareInfo BOOT_ROM;
	t_PS2DBROMHardwareInfo DVD_ROM;
	t_PS2DBEEHardwareInfo ee;
	t_PS2DBIOPHardwareInfo iop;
	t_PS2DBGSHardwareInfo gs;
	t_PS2DBSSBUSHardwareInfo ssbus;
	t_PS2DBiLinkHardwareInfo iLink;
	t_PS2DBUSBHardwareInfo usb;
	t_PS2DBSPU2HardwareInfo spu2;

	u32 MachineType;		//The value returned through the MachineType() syscall.
	u16 ROMGEN_MonthDate;
	u16 ROMGEN_Year;
	u16 MPUBoardID;
	u16 BoardInf;
	u8 MECHACONVersion[4];	// RR MM DD TT, where RR = (7-bit) region, MM = major revision, mm = minor revision, TT = system type.
	s8 ModelName[16];
	s8 romver[16];
	s8 MainboardName[16];
	u8 ModelID[3];
	u8 EMCSID;
	u8 ConModelID[2];
	u8 MRenewalDate[5];
	u8 status;
	//Known as "ADD0x10" in the SONY service tools, it's word 0x10 of the EEPROM from older consoles and word 0x01 of Dragon units. It's used to identify important revisions.
	u16 ADD010;
	s8 ContributorName[16];
	u16 padding;
};

#define PS2IDB_FORMAT_VERSION	0x0113

struct PS2IDBHeader{
	char magic[4];	//"P2DB"
	u16 version;
	u16 components;
	//Followed by component number of 32-bit offsets, which point to the start of a struct PS2IDBComponentTable entry and its entries.
};

struct PS2IDBComponentEntry{
	u32 revision;
	char name[48];
};

enum PS2IDB_COMPONENT{
	PS2IDB_COMPONENT_MAINBOARD	= 0,
	PS2IDB_COMPONENT_EE,
	PS2IDB_COMPONENT_GS,
	PS2IDB_COMPONENT_IOP,
	PS2IDB_COMPONENT_SSBUSIF,
	PS2IDB_COMPONENT_SPU2,
	PS2IDB_COMPONENT_MECHACON,
	PS2IDB_COMPONENT_SPEED,
	PS2IDB_COMPONENT_ETH_PHY_VEND,
	PS2IDB_COMPONENT_SYSTEM_TYPE,
	PS2IDB_COMPONENT_ETH_PHY_MODEL,
	PS2IDB_COMPONENT_MG_REGION,
	PS2IDB_COMPONENT_MRP_BOARD,
	PS2IDB_COMPONENT_MODEL_ID,
	PS2IDB_COMPONENT_EMCS_ID,
	PS2IDB_COMPONENT_ADD010,

	PS2IDB_COMPONENT_COUNT
};

struct PS2IDBComponentTable{
	u16 id;
	u16 entries;
};
