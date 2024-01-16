// ch341_spi_shiftreg.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "stdint.h" 
#include "inttypes.h"

#include <iostream>
#include <cstdint>
#include <vector>
#include <array>
#include <fstream>

#include "FAT32.h"

static void DumpByteArray(BYTE *b, int n){
	int i = 0;
	for (int i = 0; i < n; i++)
	{
		printf("Index=%d, Value=0x%x\r\n", i, (unsigned)b[i]);
	}
}

ULONG iDevIndex = 0; // first device
uint8_t chip_select = 0x80;
HANDLE h341 = NULL;

#define CMD0 0
#define CMD0_ARG 0x00000000
#define CMD0_CRC 0x94

#define CMD8 8
#define CMD8_ARG 0x000001AA
#define CMD8_CRC 0x87

#define CMD58 58
#define CMD58_ARG 0x00000000
#define CMD58_CRC 0x00

#define CMD55 55
#define CMD55_ARG 0x00000000
#define CMD55_CRC 0x00

#define ACMD41 41
#define ACMD41_ARG 0x40000000
#define ACMD41_CRC 0x00

#define CMD16 16
#define CMD16_ARG 0x00000200
#define CMD16_CRC 0x15

uint8_t SPI_transfer(uint8_t data) 
{
	//printf(">> %x\r\n", data);
	uint8_t ioBuf[1];
	ioBuf[0] = data;

	if (!CH341StreamSPI4(iDevIndex, chip_select, 1, ioBuf)) {
		printf("CH341StreamSPI4 failed\r\n");
		return 0x00;
	}

	//printf("<< %x\r\n", ioBuf[0]);
	return ioBuf[0];
}

uint8_t SPI_transfer_512(uint8_t *data, uint32_t len)
{
	//printf(">> %x\r\n", data);
	//uint8_t ioBuf[512] { 0xFF };
	//ioBuf[0] = data;

	if (!CH341StreamSPI4(iDevIndex, chip_select, len, data)) {
		printf("CH341StreamSPI4 failed\r\n");
		return 0x00;
	}

	//printf("<< %x\r\n", ioBuf[0]);
	//return ioBuf[0];
	return 0x01;
}

void SD_command(uint8_t cmd, uint32_t arg, uint8_t crc)
{
	// combine the transfer bits (0100 0000) with the 6 bit command identifier
	SPI_transfer(cmd | 0x40);

	// transmit 4 byte argument
	SPI_transfer((uint8_t)(arg >> 24));
	SPI_transfer((uint8_t)(arg >> 16));
	SPI_transfer((uint8_t)(arg >> 8));
	SPI_transfer((uint8_t)(arg >> 0));

	// transmit crc - combine with 01 transfer bits
	SPI_transfer(crc | 0x01);
}

uint8_t SD_readRes1()
{
	uint8_t i = 0;
	uint8_t res1 = 0;

	while ( (res1 = SPI_transfer(0xFF)) == 0xFF ) 
	{
		i++;

		// if no data is received after 8 bytes, this is defined to be an error
		// the device has to answer within 8 bytes
		if (i > 8)
		{
			printf("No response Res1 received after 8 bytes!\r\n");
			break;
		}
	}

	return res1;
}

#define PARAM_ERROR(X)      X & 0b01000000
#define ADDR_ERROR(X)       X & 0b00100000
#define ERASE_SEQ_ERROR(X)  X & 0b00010000
#define CRC_ERROR(X)        X & 0b00001000
#define ILLEGAL_CMD(X)      X & 0b00000100
#define ERASE_RESET(X)      X & 0b00000010
#define IN_IDLE(X)          X & 0b00000001

void SD_printR1(uint8_t res)
{
	if (res & 0b10000000)
	{
		printf("\tError: MSB = 1\r\n"); return;
	}
	if (res == 0)
	{
		printf("\tCard Ready\r\n"); return;
	}
	if (PARAM_ERROR(res))
		printf("\tParameter Error\r\n");
	if (ADDR_ERROR(res))
		printf("\tAddress Error\r\n");
	if (ERASE_SEQ_ERROR(res))
		printf("\tErase Sequence Error\r\n");
	if (CRC_ERROR(res))
		printf("\tCRC Error\r\n");
	if (ILLEGAL_CMD(res))
		printf("\tIllegal Command\r\n");
	if (ERASE_RESET(res))
		printf("\tErase Reset Error\r\n");
	if (IN_IDLE(res))
		printf("\tIn Idle State (0x00000001)\r\n");
}

void SD_readRes7(uint8_t* res)
{
	// has to read a 0x01
	res[0] = SD_readRes1();
	if (res[0] > 1)
	{
		printf("SD_readRes7 reading first byte failed!\r\n");
		return;
	}

	// read the rest four byte
	res[1] = SPI_transfer(0xFF);
	res[2] = SPI_transfer(0xFF);
	res[3] = SPI_transfer(0xFF);
	res[4] = SPI_transfer(0xFF);
}

void SD_readRes3(uint8_t* res)
{
	// has to read a 0x01
	res[0] = SD_readRes1();
	if (res[0] > 1)
	{
		printf("SD_readRes3 reading first byte failed!\r\n");
		return;
	}

	// read the rest four byte
	res[1] = SPI_transfer(0xFF);
	res[2] = SPI_transfer(0xFF);
	res[3] = SPI_transfer(0xFF);
	res[4] = SPI_transfer(0xFF);
}

#define CMD_VER(X)          ((X >> 4) & 0xF0) 
#define VOL_ACC(X)          (X & 0x1F) 
#define VOLTAGE_ACC_27_33   0b00000001 
#define VOLTAGE_ACC_LOW     0b00000010 
#define VOLTAGE_ACC_RES1    0b00000100 
#define VOLTAGE_ACC_RES2    0b00001000 

void SD_printR7(uint8_t *res) {

	//DumpByteArray(res, 5);

	SD_printR1(res[0]);    
	if (res[0] > 1) 
		return;    
	
	printf("\tCommand Version: ");
	printf("%x", CMD_VER(res[1] & 0xff));
	printf("\r\n");
	printf("\tVoltage Accepted: ");

	if(VOL_ACC(res[3]) == VOLTAGE_ACC_27_33)        
		printf("2.7-3.6V\r\n");
	else if(VOL_ACC(res[3]) == VOLTAGE_ACC_LOW)        
		printf("LOW VOLTAGE\r\n");
	else if(VOL_ACC(res[3]) == VOLTAGE_ACC_RES1)        
		printf("RESERVED\r\n");
	else if(VOL_ACC(res[3]) == VOLTAGE_ACC_RES2)        
		printf("RESERVED\r\n");
	else        
		printf("NOT DEFINED\r\n");

	printf("\tEcho: ");
	//DumpByteArray((BYTE), 1);
	printf("%x", res[4] & 0xff);
	printf("\r\n");
}

#define POWER_UP_STATUS(X)  X & 0x40 
#define CCS_VAL(X)          X & 0x40 
#define VDD_2728(X)         X & 0b10000000 
#define VDD_2829(X)         X & 0b00000001 
#define VDD_2930(X)         X & 0b00000010 
#define VDD_3031(X)         X & 0b00000100 
#define VDD_3132(X)         X & 0b00001000 
#define VDD_3233(X)         X & 0b00010000 
#define VDD_3334(X)         X & 0b00100000 
#define VDD_3435(X)         X & 0b01000000 
#define VDD_3536(X)         X & 0b10000000 

void SD_printR3(uint8_t *res)
{    
	SD_printR1(res[0]);

	if(res[0] > 1) 
		return;    

	printf("\tCard Power Up Status: ");
	if (POWER_UP_STATUS(res[1]))    
	{        
		printf("READY\r\n");
		printf("\tCCS Status: ");
		if(CCS_VAL(res[1]))
		{ 
			printf("1\r\n");
		}        
		else printf("0\r\n");
	}    
	else    
	{        
		printf("BUSY\r\n");
	}

	printf("\tVDD Window: ");
	if(VDD_2728(res[3])) 
		printf("2.7-2.8, ");
	if(VDD_2829(res[2])) 
		printf("2.8-2.9, ");
	if(VDD_2930(res[2])) 
		printf("2.9-3.0, ");
	if(VDD_3031(res[2])) 
		printf("3.0-3.1, ");
	if(VDD_3132(res[2])) 
		printf("3.1-3.2, ");
	if(VDD_3233(res[2])) 
		printf("3.2-3.3, ");
	if(VDD_3334(res[2])) 
		printf("3.3-3.4, ");
	if(VDD_3435(res[2])) 
		printf("3.4-3.5, ");
	if(VDD_3536(res[2])) 
		printf("3.5-3.6");
	printf("\r\n");
}

#define CMD17                   17 
#define CMD17_CRC               0x00 
#define SD_MAX_READ_ATTEMPTS    1563 

/******************************************************************************* 
Read single 512 byte block 
token = 0xFE - Successful read 
token = 0x0X - Data error 
token = 0xFF - Timeout 
*******************************************************************************/ 
uint8_t SD_readSingleBlock(uint32_t addr, uint8_t *buf, uint8_t *token) 
{    
	uint8_t res1;
	uint8_t read;    
	uint16_t readAttempts = 0;    
	
	// set token to none    
	*token = 0xFF;    
	
	//// assert chip select    
	//SPI_transfer(0xFF);    
	////CS_ENABLE();
	//CH341SetStream(iDevIndex, 0x81);
	//SPI_transfer(0xFF); 
	
	//Sleep(100);
	
	// send CMD17    
	SD_command(CMD17, addr, CMD17_CRC);    
	
	// read R1    
	res1 = SD_readRes1();    
	
	// if response received from card    
	if (res1 != 0xFF)    
	{
		// wait for a response token 0xFE because the SD Card takes some time to produce data (timeout = 100ms)        
		readAttempts = 0;        
		while(++readAttempts != SD_MAX_READ_ATTEMPTS)
		{
			//Sleep(1);
			if ((read = SPI_transfer(0xFF)) != 0xFF)
			{
				break;
			}
		}
		
		// if response token is 0xFE        
		if (read == 0xFE)        
		{            
			// read 512 byte block            
			for (uint16_t i = 0; i < 512; i++)
			{
				*buf++ = SPI_transfer(0xFF);
			}

			// read 512 byte block
			//SPI_transfer_512(buf, 512);
			
			// read 16-bit CRC            
			SPI_transfer(0xFF);            
			SPI_transfer(0xFF);
		}
		else
		{
			printf("Could not read from card!\r\n");
		}
		
		// set token to card response        
		*token = read;    
	}    
	
	//// deassert chip select    
	//SPI_transfer(0xFF);    
	////CS_DISABLE(); 
	//CH341SetStream(iDevIndex, 0x81);
	//SPI_transfer(0xFF);    
	
	return res1;
}

// https://github.com/hazelnusse/crc7/blob/master/crc7.cc
uint8_t CRCTable[256];

void GenerateCRCTable()
{
	int i, j;
	uint8_t CRCPoly = 0x89;  // the value of our CRC-7 polynomial

	// generate a table value for all 256 possible byte values
	for (i = 0; i < 256; ++i) {
		CRCTable[i] = (i & 0x80) ? i ^ CRCPoly : i;
		for (j = 1; j < 8; ++j) {
			CRCTable[i] <<= 1;
			if (CRCTable[i] & 0x80)
				CRCTable[i] ^= CRCPoly;
		}
	}
}

// adds a message byte to the current CRC-7 to get a the new CRC-7
uint8_t CRCAdd(uint8_t CRC, uint8_t message_byte)
{
	return CRCTable[(CRC << 1) ^ message_byte];
}

// returns the CRC-7 for a message of "length" bytes
uint8_t getCRC(uint8_t message[], int length)
{
	int i;
	uint8_t CRC = 0;

	for (i = 0; i < length; ++i)
		CRC = CRCAdd(CRC, message[i]);

	return CRC;
}

void PrintFrame(std::array<uint8_t, 6>& f)
{
	for (auto e : f)
		std::cout << std::hex << (int)e << " ";
	std::cout << std::endl;
}

uint32_t root_dir_sector_count(BootEntry* bpb)
{
	return ((bpb->BPB_RootEntryCnt * 32) + (bpb->BPB_BytesPerSec - 1)) / bpb->BPB_BytesPerSec;

}

/*
	Finds first data sector for given cluster number
 */
uint32_t first_sector_of_cluster(BootEntry* bpb, uint32_t cluster_num)
{
	uint32_t first_data_sector = bpb->BPB_RsvdSecCnt + (bpb->BPB_NumFATs * bpb->BPB_FATSz32) + root_dir_sector_count(bpb);

	return (((cluster_num - 2) * (bpb->BPB_SecPerClus)) + first_data_sector) * 512;
}

int _tmain(int argc, _TCHAR* argv[])
{
	GenerateCRCTable();

	std::vector<std::array<uint8_t, 6>> CommandFrames;
	CommandFrames.push_back({ {0x40, 0, 0, 0, 0, 0} });
	CommandFrames.push_back({ {0x48, 0, 0, 1, 0xAA, 0} });
	CommandFrames.push_back({ {0x69, 0x40, 0, 0, 0, 0} });
	CommandFrames.push_back({ {0x77, 0x00, 0, 0, 0, 0} });
	CommandFrames.push_back({ {0x7A, 0x00, 0, 0, 0, 0} });
	CommandFrames.push_back({ {0x50, 0x00, 0, 0x02, 0, 0} });

	std::cout << "Command, Argument, CRC7" << std::endl;
	for (auto& Frame : CommandFrames) {
		Frame[5] = (getCRC(Frame.data(), 5) << 1) | 1;
		PrintFrame(Frame);
	}

	printf("CH341 version: %lu\r\n", CH341GetVersion());
	printf("Device %lu name is '%s'\r\n", iDevIndex, CH341GetDeviceName(iDevIndex));

	printf("Opening device# %lu\r\n", iDevIndex);
	h341 = CH341OpenDevice(iDevIndex);
	if (h341 == NULL) {
		printf("OpenDevice(iDevIndex=%lu) failed\r\n", iDevIndex);
		return 0;
	}
	else {
		printf("OpenDevice(iDevIndex=%lu) succeeded!\r\n", iDevIndex);
	}

	// 0x80 send MSB bit first on SPI
	// 0x01 I don't know, but it is in example
	if (!CH341SetStream(iDevIndex, 0x81)) 
	{
		printf("CH341SetStream failed\r\n");
		
		printf("Closeing device# %lu\r\n", iDevIndex);
		CH341CloseDevice(iDevIndex);
		iDevIndex = 0UL;
		h341 = NULL;

		return 0;
	}

	// send 80 pulses to active SD card
	for (uint8_t i = 0; i < 10; i++)
	{
		SPI_transfer(0xFF);
	}

	//
	// CMD 0 - reset SD memory card - go to idle state
	//

	printf("CMD 0 - reset SD memory card - go to idle state\r\n");

	// send CMD0 - 40 00 00 00 00 95
	SD_command(CMD0, CMD0_ARG, CMD0_CRC);

	// read response of type R1
	uint8_t res1 = SD_readRes1();
	SD_printR1(res1);

	SPI_transfer(0xFF);

	//
	// CMD 8 - SENDS_IF_COND - Sends SD Memory card interface condition, which
	// includes host supply voltage information and asks the card whether card
	// supports voltage. 
	// Reserved bits have to be set to 0.
	//

	printf("CMD 8 - Send card interface condition.\r\n");

	// send CMD8 - 48 00 00 01 AA 87
	SD_command(CMD8, CMD8_ARG, CMD8_CRC);

	// read response of type R7
	uint8_t res7_buffer[5];
	SD_readRes7(res7_buffer);
	SD_printR7(res7_buffer);

	//
	// CMD 58 - read OCR (operation conditions register)
	//

	printf("CMD 58 - read OCR (operation conditions register)\r\n");

	// send CMD58 - 7A 00 00 00 00 fd
	SD_command(CMD58, CMD58_ARG, CMD58_CRC);

	// read response of type R3
	uint8_t res3_buffer[5];
	SD_readRes3(res3_buffer);
	SD_printR3(res3_buffer);

	res1 = 0x01;

	while (res1 != 0x00) 
	{
		//
		// CMD 55 - APP_CMD (put before each application specific command)
		//

		printf("CMD 55 - APP_CMD (put before each application specific command)\r\n");

		// send CMD55
		SD_command(CMD55, CMD55_ARG, CMD55_CRC);

		// read response of type R1
		res1 = SD_readRes1();
		SD_printR1(res1);
	
		//
		// ACMD 41 - SD_SEND_OP_COND
		//

		printf("ACMD 41 - SD_SEND_OP_COND\r\n");

		// send CMD41
		SD_command(ACMD41, ACMD41_ARG, ACMD41_CRC);

		// read response of type R1
		res1 = SD_readRes1();
		SD_printR1(res1);
	}

	/**/
	//
	// SECOND TIME
	// 
	// CMD 58 - read OCR (operation conditions register)
	//

	printf("CMD 58 - read OCR (operation conditions register)\r\n");

	// send CMD58 - 7A 00 00 00 00 fd
	SD_command(CMD58, CMD58_ARG, CMD58_CRC);

	// read response of type R3
	//uint8_t res3_buffer[5];
	SD_readRes3(res3_buffer);
	SD_printR3(res3_buffer);
	

	/*
	//
	// CMD 16 - set block length
	//

	printf("CMD 16 - set block length\r\n");

	// CMD16 - set block length ( not executed because on SDHC cards, the block length is 512 always )
	SD_command(CMD16, CMD16_ARG, CMD16_CRC);

	// read response of type R1
	res1 = SD_readRes1();
	SD_printR1(res1);
	*/

	//
	// CMD 17 - read single block
	//

	std::ofstream file("block_512.bin", std::ios::binary);

	uint8_t sdBuf[512];

	//for (uint32_t sector = 0x00000800; sector < 100000; sector++)
	//for (uint32_t sector = 0; sector < 1; sector++)
	//{
		//printf("CMD 17 - read single block. block: %02x\r\n", sector);
		//printf("CMD 17 - read single block. block: %d\r\n", sector);

		/*if (sector % 10000 == 0)
		{
			printf("CMD 17 - read single block. block: %d\r\n", sector);
		}*/

		uint8_t res[5];
		
		for (uint16_t i = 0; i < 512; i++)
		{
			sdBuf[i] = 0xFF;
		}
		uint8_t token;

		//SD_readSingleBlock(sector, sdBuf, &token);

		//SD_readSingleBlock(0x00000000, sdBuf, &token);
		
		//SD_readSingleBlock(0x0000064F, sdBuf, &token);
		SD_readSingleBlock(0x00000800, sdBuf, &token);
		//SD_readSingleBlock(0x00001000, sdBuf, &token);
		//SD_readSingleBlock(0x00200000, sdBuf, &token);

		//SD_readSingleBlock(0x00002000, sdBuf, &token);
		//SD_readSingleBlock(0x00002000, sdBuf, &token);

		
			
		//file.write((char *)sdBuf, 512);
			
		

		// output blocks that contain data other than 0x00
		//if (sdBuf[0] != 0x00)
		//{
		//	printf("CMD 17 - read single block. block: %02x\r\n", sector);

			/**/
			if (token == 0xFE)
			{
				for (uint16_t i = 0; i < 512; i++)
				{
					printf("%02x ", sdBuf[i]);
				}
				printf("\r\n");
			}
			
		//}

		//Sleep(1000);

	//}

	//		file.flush();
	//		file.close();
/**/
			//BootEntry* boot_entry = (BootEntry*)sdBuf;
			BootEntry boot_entry;
			memcpy(&boot_entry, sdBuf, sizeof(BootEntry));
			printf("BPB_BytesPerSec: %d\r\n", boot_entry.BPB_BytesPerSec);
			printf("BPB_SecPerClus: %d\r\n", boot_entry.BPB_SecPerClus);
			printf("BPB_RsvdSecCnt: %d\r\n", boot_entry.BPB_RsvdSecCnt);
			printf("BPB_NumFATs: %d\r\n", boot_entry.BPB_NumFATs);
			printf("BPB_RootClus: %d\r\n", boot_entry.BPB_RootClus);
			printf("BPB_FATSz32: %d\r\n", boot_entry.BPB_FATSz32);


			uint32_t first_sector_of_cluster_result = first_sector_of_cluster(&boot_entry, boot_entry.BPB_RootClus);
			//uint32_t first_sector_of_cluster_result = first_sector_of_cluster(&boot_entry, 0x800 + boot_entry.BPB_RootClus);
			printf("first_sector_of_cluster_result: %d\r\n", first_sector_of_cluster_result);
			

			//// https://www.cs.fsu.edu/~cop4610t/lectures/project3/Week11/Slides_week11.pdf
			//// https://www.youtube.com/watch?v=oIgvRUFojdA

			//// Figure out where the Data Region starts in the disk
			//uint32_t RootDirSectors = ((boot_entry.BPB_RootEntryCnt * 32) + (boot_entry.BPB_BytesPerSec - 1)) / boot_entry.BPB_BytesPerSec;
			//printf("RootDirSectors: %d\r\n", RootDirSectors);
			//
			//uint32_t FirstDataSector = boot_entry.BPB_RsvdSecCnt + (boot_entry.BPB_NumFATs * boot_entry.BPB_FATSz32) + RootDirSectors;
			//printf("FirstDataSector: %d\r\n", FirstDataSector);

			//ULONG test = boot_entry.BPB_RootClus - 2;

			//uint32_t FirstSectorOfCluster = ( (test) * boot_entry.BPB_SecPerClus ) + FirstDataSector;

			//printf("FirstSectorOfCluster: %d\r\n", FirstSectorOfCluster);
			


			
			
			for (uint16_t i = 0; i < 512; i++)
			{
				sdBuf[i] = 0x00;
			}

			//SD_readSingleBlock((first_sector_of_cluster_result / 512), sdBuf, &token);
			SD_readSingleBlock((0x0800 + (first_sector_of_cluster_result / 512) + 0), sdBuf, &token);
			//SD_readSingleBlock(FirstSectorOfCluster, sdBuf, &token);
			//SD_readSingleBlock(FirstDataSector / 512, sdBuf, &token);
			//SD_readSingleBlock( ( ((FirstDataSector * boot_entry.BPB_BytesPerSec) / 512) ), sdBuf, &token);
			//SD_readSingleBlock(0x00002000 + 0x20, sdBuf, &token);
			if (token == 0xFE)
			{
				for (uint16_t i = 0; i < 512; i++)
				{
					printf("%02x ", sdBuf[i]);
				}
				printf("\r\n");
			}

			for (uint32_t counter = 0; counter * sizeof(DirectoryEntry) < boot_entry.BPB_BytesPerSec; counter++)
			{
				DirectoryEntry directory_entry;
				memcpy(&directory_entry, sdBuf + (counter * sizeof(DirectoryEntry)), sizeof(DirectoryEntry));

				for (int jj = 0; jj < 11; jj++)
				{
					printf("%c", directory_entry.Name[jj]);
				}
				printf("\n");
			}

			//directory_entry.Attr

			

	printf("Closeing device# %lu\r\n", iDevIndex);
	CH341CloseDevice(iDevIndex);
	iDevIndex = 0UL;
	h341 = NULL;


	//int  N = 2;
	//BYTE ioBuf[2] = { 0xaa, 0x55 }; // 1010 1010
	//int ret = EXIT_SUCCESS;
	//HANDLE h341 = NULL;
	

	/*printf("CH341 SPI shift register example\r\n");

	printf("CH341 version: %lu\r\n", CH341GetVersion());

	printf("Device %lu name is '%s'\r\n", iDevIndex, CH341GetDeviceName(iDevIndex));

	printf("Opening device# %lu\r\n", iDevIndex);
	h341 = CH341OpenDevice(iDevIndex);
	if (h341 == NULL) {
		printf("OpenDevice(iDevIndex=%lu) failed\r\n", iDevIndex);
		ret = EXIT_FAILURE;
		goto exit0;
	}
	else {
		printf("OpenDevice(iDevIndex=%lu) succeeded!\r\n", iDevIndex);
	}*/

	//CH341ResetDevice(iDevIndex);
	//Sleep(500);

	//// 0x80 send MSB bit first on SPI
	//// 0x01 I don't know, but it is in example
	//if (!CH341SetStream(iDevIndex, 0x81)){
	//		printf("CH341SetStream failed\r\n");
	//		ret = EXIT_FAILURE;
	//		goto exit1;
	//}

	//while(1){
	//	
	// 
	//	printf("Sending to SPI:\r\n");
	//  ioBuf[0]=0xaa;
	//  ioBuf[1]=0x55;
	//	DumpByteArray(ioBuf, 2);
	// 
	//	if (!CH341StreamSPI4(iDevIndex, 0x80, 2, ioBuf)) {
	//		printf("CH341StreamSPI4 failed\r\n");
	//		ret = EXIT_FAILURE;
	//		goto exit1;
	//	}
	// 
	//	printf("SPI returned back data:\r\n");
	//	DumpByteArray(ioBuf, 2);
	// 
	//	Sleep(10);
	//	break; // comment it out to have stream of data - for Logic Analyzer
	//}


	// To ensure the proper operation of the SD card, the SD CLK signal should have a frequency in the range of
	//	100 to 400 kHz.

	// https://electronics.stackexchange.com/questions/602105/how-can-i-initialize-use-sd-cards-with-spi

	// ******************************************* 
	// Serial data transfer (I2C, RS232, SPI) 
	// ******************************************* 
	// CH341SetStream() konfiguriert I2C und SPI 
	// 
	// Bit 1-0: I2C speed 
	// 00 = low speed / 20KHz 
	// 01 = standard / 100KHz 
	// 10 = fast / 400KHz 
	// 11 = high speed / 750KHz 
	// 
	// Bit 2: SPI - Modus 
	// 0 = Standard SPI (D3=clock out, D5=serial out, D7 serial in 
	// 1 = SPI with two data lines (D3=clock out, D5,D4=serialout, D7,D6 serial in)
	// 
	// Bit 7: SPI 
	// 0 = LSB first 
	// 1 = MSB first 
	// other bits 0 (reserved) 
	// Function CH341SetStream(iIndex:cardinal; iMode:cardinal) :boolean;Stdcall; external
	
	//CH341SetStream(iDevIndex, 0x80);
	//CH341SetStream(iDevIndex, 0x81);
	//CH341SetStream(iDevIndex, 0x84);
	//CH341SetStream(iDevIndex, 0x85);
	//CH341SetStream(iDevIndex, 0x01);
	//Sleep(10);

	/*

	for (int i = 0; i < 5; i++)
	{
		
		ULONG write_len_ptr = 8;
		// https://electronics.stackexchange.com/questions/602105/how-can-i-initialize-use-sd-cards-with-spi
		
		const int chipSelect = 0x80; // CS control using CS0
		//const int chipSelect = 0x81;
		//const int chipSelect = 0x82;
		//const int chipSelect = 0x00;

		constexpr ULONG cmd_len = 8;

		CMD 0 - 40 00 00 00 00 95 ff ff - (response: 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0x01)
		CMD 0 - BYTE write_buffer[8] = { 0x40, 0x00, 0x00, 0x00, 0x00, 0x95, 0xff, 0xff }; // lec12_sd_card.pdf, This is the reset command, which puts the SD card into the SPI mode if executed when the CS line is low. 
		
		send 0xFF until you receive 0x01
		send 0xFF until you receive 0xFF
		

		// CMD 8 - check SD version
		// 4.3.13 Send Interface Condition Command (CMD8)
		// R7 response
		CMD 8 - 48 00 00 01 AA 87 ff ff - Response format F7 (5 bytes) (response: 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0x01)

			send 0xFF until you receive 0x01 which means the command has been processed

			// https://luckyresistor.me/cat-protector/software/sdcard-2/
			Then read 4 byte response (send 5 times 0xff to receive 4 byte) - 0x00 0x00 0x01 0xAA 
			The card will mirror the requested operation condition, if it accepts the operation condition and has activated it
			This is similar to a connection negotiation where partners negotiate the conditions of operation (See telnet).

			send 0xFF until you receive 0xFF

		// https://stackoverflow.com/questions/8080718/sdhc-microsd-card-and-spi-initialization
		do not send this: CMD 1 - 41 40 00 00 00 ff ff ff

		// CMD 58 - read OCR (operations conditions register)
		CMD 58 - 7A 00 00 00 00 fd ff ff
		Response R3 - 48 bits (6 byte) = 6 byte (0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0x01)
		Response: 0xFF 0x01 0x00 0xFF 0x80 0x00

		// ACMD41 to initialize the card and set the HCS flag (HCS == enables fast transfer rates)
		CMD41 0x40000000, crc = 0x77 - 69 40 00 00 00 77

		CMD9 49 00 00 00 00 AF
		


		// CMD55 arg: 0x0, CRC: any (CMD55 being the prefix to every ACMD)
		CMD 55 - 77 00 00 00 00 65 ff ff

		ACMD41 - 69 40 00 00 00 11 ff ff - (if response: 0x0, you're OK; if it's 0x1, goto 3.)

		// https://electronics.stackexchange.com/questions/77417/what-is-the-correct-command-sequence-for-microsd-card-initialization-in-spi
		Power On..
		Clock card at least 74 (I use 80) cycles with cs high
		CMD0 0, crc = 0x95
		CMD8 0x01aa, crc = 0x87
		CMD58 0, crc = 0xfd
		CMD55 0, crc = 0x65
		CMD41 0x40000000, crc = 0x77
		CMD9 0, crc = 0xaf
		CMD16, 512, crc = 0x81 (If you want block length of 512)

		Some random other commands..
		CMD17 0, crc = 0x3b (Read one block)
		CMD18 0, crc = 0x57 (Read multiple blocks)
		CMD24 0, crc = 0x6f (set write address for single block)
		CMD25 0, crc = 0x03 (set write address for first block)
		

		//BOOL WINAPI CH341StreamSPI4(  // Processing the SPI data stream, 4-wire interface, the clock line for the DCK / D3 pin, the output data line DOUT / D5 pin, the input data line for the DIN / D7 pin, chip line for the D0 / D1 / D2, the speed of about 68K bytes
		//	 SPI Timing: The DCK / D3 pin is clocked and defaults to the low level. The DOUT / D5 pin is output during the low period before the rising edge of the clock. The DIN / D7 pin is at a high level before the falling edge of the clock enter
		//	ULONG   iIndex,  // Specify the CH341 device serial number
		//	ULONG   iChipSelect,  // Chip select control, bit 7 is 0 is ignored chip select control, bit 7 is 1 parameter is valid: bit 1 bit 0 is 00/01/10 select D0 / D1 / D2 pin as low active chip select
		//	ULONG   iLength,  // The number of bytes of data to be transferred
		//	PVOID   ioBuffer);  // Point to a buffer, place the data to be written from DOUT, and return the data read from DIN
		
		bool read_result = CH341StreamSPI4(iDevIndex, chipSelect, cmd_len, write_buffer);
		if (read_result) {
			printf("read true\r\n");

			DumpByteArray(write_buffer, 8);
		}
		else {
			printf("read false:\r\n");
		}
		

		//Sleep(500);
		
	}
	*/
	
/*
	for (int i = 0; i < 10; i++)
	{
		constexpr ULONG cmd_len = 6;
		ULONG write_len_ptr = 6;
		BYTE write_buffer[cmd_len] = { 0x40, 0x00, 0x00, 0x00, 0x00, 0x95 }; // lec12_sd_card.pdf, This is the reset command, which puts the SD card into the SPI mode if executed when the CS line is low. 
		// https://electronics.stackexchange.com/questions/602105/how-can-i-initialize-use-sd-cards-with-spi

		bool write_result = CH341WriteData(iDevIndex, write_buffer, &write_len_ptr);
		if (write_result) {
			printf("write true\r\n");
		}
		else {
			printf("write false\r\n");
		}

		Sleep(10);

		constexpr ULONG read_buffer_len = 8;
		BYTE read_buffer[read_buffer_len]{ 0 };
		const int chipSelect = 0x80; // CS control using CS0
		bool read_result = CH341StreamSPI4(iDevIndex, chipSelect, read_buffer_len, read_buffer);
		if (read_result) {
			printf("read true\r\n");

			DumpByteArray(read_buffer, read_buffer_len);
		}
		else {
			printf("read false:\r\n");
		}

		Sleep(500);
	}
	*/

//exit1:
//	printf("Closeing device# %lu\r\n", iDevIndex);
//	CH341CloseDevice(iDevIndex);
//	iDevIndex = 0UL;
//
//exit0:
//	return ret;

	return 0;
}

