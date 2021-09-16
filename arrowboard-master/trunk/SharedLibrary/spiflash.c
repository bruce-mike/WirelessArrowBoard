#include <string.h>
#include <stdio.h>
#include "lpc23xx.h"
#include "timer.h"
#include "queue.h"
#include "spiflash.h"
#include "ssp.h"
#include "hdlc.h"

static BYTE SPIWRCommand[1];


void spiFlashErase(BYTE eraseType, DWORD eraseAddress, ePLATFORM_TYPE ePlatformType)
{
	BYTE status;
	
	union
	{
		BYTE bEraseAddress[4];
		DWORD dEraseAddress;
	}SPI_Erase_Address;
	
	SPI_Erase_Address.dEraseAddress = spiFlashSwapDword(eraseAddress);
	
	//Write Enable the SPI Flash
	spiFlashWriteCommand(SPI_FLASH_WRITE_ENABLE,TRUE,ePlatformType);	
	
	// Erase Type (Sector(4kb), Block(64kb), Chip(Entire Chip) )
	SPIWRCommand[0] = eraseType;	
	
	spiFlashWriteCommand(eraseType,FALSE,ePlatformType);
	
  // Write Address plus Data
	spiFlashWriteAddress(SPI_Erase_Address.bEraseAddress,ePlatformType);
	
	// Deassert CS	
	if(ePlatformType == ePLATFORM_TYPE_ARROW_BOARD)
	{
		FIO0SET |= (1 << uP_SPI1_xCS);
	}
	else
	{
		FIO0SET |= (1 << uP_SPI0_xCS);
	}
	
	//Check Status
	spiFlashWriteCommand(SPI_FLASH_READ_STATUS,FALSE,ePlatformType);	
	
	do
	{
		spiFlashReadStatus(&status,FALSE,ePlatformType);
		//printf("spiFlashErase: FlashStatus[%X]\n",status);
		
	}while((status & SPI_FLASH_WRITE_IN_PROGRESS) == SPI_FLASH_WRITE_IN_PROGRESS);
	
	// Deassert CS	
	if(ePlatformType == ePLATFORM_TYPE_ARROW_BOARD)
	{
		FIO0SET |= (1 << uP_SPI1_xCS);
	}
	else
	{
		FIO0SET |= (1 << uP_SPI0_xCS);
	}
		
	//printf("FlashStatus[%X]\n",status);
}

BOOL spiFlashVerifyData(BYTE *data, DWORD spiReadAddress, UINT imageSize, BYTE readType, ePLATFORM_TYPE ePlatformType)
{
		UINT i;
		BOOL flashVerified = TRUE;

	union
		{
			BYTE SPIRDAddress[3];
			DWORD SPIReadAddress;
		}	SPI_Read_Address;
		
		SPI_Read_Address.SPIReadAddress = spiReadAddress;
	
		//Read Type(Normal,Fast,Double)
		spiFlashWriteCommand(readType,FALSE,ePlatformType);
	
		// Write Address to read from
		spiFlashWriteAddress(SPI_Read_Address.SPIRDAddress,ePlatformType);
		
		for(i=0; i<imageSize; i++)
		{
			SSPReceive(data,1,ePlatformType);
			
			//if(data[0] != rightStemArrow1[i])
			//{
			//	flashVerified = FALSE;
			//	break;
			//}
		}
		
		if(ePlatformType == ePLATFORM_TYPE_ARROW_BOARD)
		{
			FIO0SET |= (1 << uP_SPI1_xCS);
		}
		else
		{
			FIO0SET |= (1 << uP_SPI0_xCS);
		}

		return flashVerified;
}

void spiFlashWriteCommand(BYTE command, BOOL resetCS, ePLATFORM_TYPE ePlatformType)
{
	//Write Enable the SPI Flash
	SPIWRCommand[0] = command;
	
	SSPSend(SPIWRCommand,1,ePlatformType); // One byte for command
	
	if(resetCS)
	{	
		// Deassert CS
		if(ePlatformType == ePLATFORM_TYPE_ARROW_BOARD)
		{
			FIO0SET |= (1 << uP_SPI1_xCS);
		}
		else
		{
			FIO0SET |= (1 << uP_SPI0_xCS);
		}
	}		
}

void spiFlashWriteAddress(BYTE *address, ePLATFORM_TYPE ePlatformType)
{
	SSPSend(address,3,ePlatformType); // SPI FLASH Address is 3 bytes
}

void spiFlashReadStatus(BYTE *status, BOOL resetCS, ePLATFORM_TYPE ePlatformType)
{
	SSPReceive(status,1,ePlatformType); // One byte
	
	if(resetCS)
	{	
		// Deassert CS
		if(ePlatformType == ePLATFORM_TYPE_ARROW_BOARD)
		{
			FIO0SET |= (1 << uP_SPI1_xCS);
		}
		else
		{
			FIO0SET |= (1 << uP_SPI0_xCS);
		}
	}		
}

void spiFlashReadData(BYTE *data, DWORD spiReadAddress, UINT imageSize, BYTE readType, ePLATFORM_TYPE ePlatformType)
{
		union
		{
			BYTE SPIRDAddress[4];
			DWORD SPIReadAddress;
		}	SPI_Read_Address;
		
		SPI_Read_Address.SPIReadAddress = spiFlashSwapDword(spiReadAddress);
		
		//Read Type(Normal,Fast,Double)
		spiFlashWriteCommand(readType,FALSE,ePlatformType);
		
		// Write Address to read from
		spiFlashWriteAddress(SPI_Read_Address.SPIRDAddress,ePlatformType);

		SSPReceive(data,imageSize,ePlatformType);
		
		// Deassert CS
		if(ePlatformType == ePLATFORM_TYPE_ARROW_BOARD)
		{
			FIO0SET |= (1 << uP_SPI1_xCS);
		}
		else
		{
			FIO0SET |= (1 << uP_SPI0_xCS);
		}
}

void spiFlashWriteData(BYTE *data, DWORD startAddress, UINT imageSize, ePLATFORM_TYPE ePlatformType)
{
	int i;
	BYTE status;
	
	union
	{
		BYTE bWriteAddress[4];
		DWORD dWriteAddress;
	}SPI_Write_Address;
	
	SPI_Write_Address.dWriteAddress = spiFlashSwapDword(startAddress);

	//Write Enable the SPI Flash
	spiFlashWriteCommand(SPI_FLASH_WRITE_ENABLE,TRUE,ePlatformType);
	
	spiFlashWriteCommand(SPI_FLASH_PAGE_PROGRAM,FALSE,ePlatformType);

	//Write Address plus Data
	spiFlashWriteAddress(SPI_Write_Address.bWriteAddress,ePlatformType);
	
	for(i=0; i<imageSize; i++)
	{
			SSPSend(&data[i],1,ePlatformType);
	}
	
	// Must Deassert CS immediately after every page(256 bytes) write
	if(ePlatformType == ePLATFORM_TYPE_ARROW_BOARD)
	{
		FIO0SET |= (1 << uP_SPI1_xCS);
	}
	else
	{
		FIO0SET |= (1 << uP_SPI0_xCS);
	}
	
	//Write Enable the SPI Flash
	spiFlashWriteCommand(SPI_FLASH_WRITE_ENABLE,TRUE,ePlatformType);
	
	//Read SPI Flash Status	
	spiFlashWriteCommand(SPI_FLASH_READ_STATUS,FALSE,ePlatformType);
	
	do
	{
		spiFlashReadStatus(&status,FALSE,ePlatformType);
		
	}while(status == (SPI_FLASH_WRITE_IN_PROGRESS | SPI_FLASH_WRITE_ENABLE_LATCH));
		
	//printf("FlashStatus[%X]\n",status);
	
	// Deassert CS
	if(ePlatformType == ePLATFORM_TYPE_ARROW_BOARD)
	{
		FIO0SET |= (1 << uP_SPI1_xCS);
	}
	else
	{
		FIO0SET |= (1 << uP_SPI0_xCS);
	}
}

DWORD spiFlashSwapDword( DWORD Data ) 
{
	DWORD swappedData;
	
  swappedData = ((( Data & 0xFF000000 ) >> 24 ) |
								 (( Data & 0x00FF0000 ) >>  8 ) |
								 (( Data & 0x0000FF00 ) <<  8 ) |
								 (( Data & 0x000000FF ) << 24 ));
	
	return (swappedData >> 8);
}

WORD spiFlashSwapWord(WORD Data)
{	
	return((Data>>8) | (Data<<8));
}

