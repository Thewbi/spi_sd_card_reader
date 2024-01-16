#pragma once

#pragma pack(push)
#pragma pack(1)
typedef struct BootEntry {
	unsigned char	BS_jmpBoot[3];		/* Assembly instruction to jump to boot code */
	unsigned char	BS_OEMName[8]; 		/* OEM Name in ASCII */
	unsigned short	BPB_BytesPerSec; 	/* Bytes per sector. Allowed values include 512,1024, 2048, and 4096 */
	unsigned char	BPB_SecPerClus; 	/* Sectors per cluster (data unit). Allowed values are powers of 2, but the cluster size must be 32KB  or smaller */
	unsigned short	BPB_RsvdSecCnt;		/* Size in sectors of the reserved area */
	unsigned char	BPB_NumFATs; 		/* Number of FATs */
	unsigned short	BPB_RootEntryCnt;	/* Maximum number of files in the root directory for  FAT12 and FAT16. This is 0 for FAT32 */
	unsigned short	BPB_TotSec16; 		/* 16-bit value of number of sectors in file system */
	unsigned char	BPB_Media; 			/* Media type */
	unsigned short	BPB_FATSz16;		/* 16-bit size in sectors of each FAT for FAT12 and FAT16. For FAT32, this field is 0 */
	unsigned short	BPB_SecPerTrk; 		/* Sectors per track of storage device */
	unsigned short	BPB_NumHeads;		/* Number of heads in storage device */
	ULONG			BPB_HiddSec;		/* Number of sectors before the start of partition */
	ULONG			BPB_TotSec32;		/* 32-bit value of number of sectors in file system. Either this value or the 16-bit value above must be  0 */
	
	ULONG			BPB_FATSz32;		/* 32-bit size in sectors of one FAT */
	unsigned short	BPB_ExtFlags;		/* A flag for FAT */
	unsigned short	BPB_FSVer;			/* The major and minor version number */
	ULONG			BPB_RootClus;		/* Cluster where the root directory can be found */
	unsigned short	BPB_FSInfo;			/* Sector where FSINFO structure can be found */
	unsigned short	BPB_BkBootSec;		/* Sector where backup copy of boot sector is located */
	unsigned char	BPB_Reserved[12];	/* Reserved */
	unsigned char	BS_DrvNum;			/* BIOS INT13h drive number */
	unsigned char	BS_Reserved1;		/* Not used */
	unsigned char	BS_BootSig;			/* Extended boot signature to identify if the next three values are valid */
	ULONG			BS_VolID;			/* Volume serial number */
	unsigned char	BS_VolLab[11];		/* Volume label in ASCII. User defines when creating the file system */
	unsigned char	BS_FilSysType[8];	/* File system type label in ASCII */
}BootEntry;
#pragma pack(pop)

#pragma pack(push)
#pragma pack(1)
//FAT32DirectoryBlock;
//struct DirectoryEntry 
typedef struct
{
	uint8_t Name[11];       /* byes 0-10;  short name */
	uint8_t Attr;           /* byte 11;  Set to one of the File Attributes defined in FileSystem.h file */
	uint8_t NTRes;          /* byte 12; Set value to 0 when a file is created and never modify or look at it after that. */
	uint8_t CrtTimeTenth;   /* byte 13; 0 - 199, timestamp at file creating time; count of tenths of a sec. */
	uint16_t CrtTime;       /* byte 14-15;  Time file was created */
	uint16_t CrtDate;       /* byte 16-17; Date file was created */
	uint16_t LstAccDate;    /* byte 18-19; Last Access Date. Date of last read or write. */
	uint16_t FstClusHI;     /* byte 20-21; High word of */
	uint16_t WrtTime;       /* byte 22-23; Time of last write. File creation is considered a write */
	uint16_t WrtDate;       /* byte 24-25; Date of last write. Creation is considered a write.  */
	uint16_t FstClusLO;     /* byte 26-27;  Low word of this entry's first cluster number.  */
	uint32_t FileSize;      /* byte 28 - 31; 32bit DWORD holding this file's size in bytes */
}//__attribute((packed)) 
DirectoryEntry;
#pragma pack(pop)