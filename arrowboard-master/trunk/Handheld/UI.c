/*****************************************************************************
 *   UI.c:  User Interface C file
 *
 *   History
 *   2014.07.24  ver 1.00    Prelimnary version
 *
*****************************************************************************/
#include "LPC23xx.h"			/* LPC23XX/24xx Peripheral Registers */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "shareddefs.h"
#include "sharedinterface.h"
#include "timer.h"
#include "queue.h"
#include "gen_UI.h"
#include "commboard.h"
#include "spiflashui.h"
#include "spiFlash.h"
#include "ssp.h"
#include "runlength.h"
#include "UI.h"

#define numberIdMapElements 3

static union
{
	BYTE bFlashOffset[4]; //BYTE
	DWORD dFlashOffset;		//DWORD
}FLASH_OFFSET;

static WORD nIDs;
static BYTE *readData1;
static BYTE *readData2;

static STORED_IDMAP ids[2000];
static STORED_COLORMAP colors;
static int i;

void initUI(void)
{
	 BYTE status;
	 WORD combined;

	 readData1 = (BYTE *)READ_DATA_1; // 16KB of Ethernet Ram
	 readData2 = (BYTE *)READ_DATA_2; // 8KB of USB ram
	
   //Write Command: Write Enable the SPI Flash
	 spiFlashWriteCommand(0x06, TRUE, ePLATFORM_TYPE_HANDHELD);
	
	 //Write Command: Set to SPI Flash Status Register
	 spiFlashWriteCommand(0x05, FALSE, ePLATFORM_TYPE_HANDHELD);
	
	 //Read SPI Flash Status Register
	 spiFlashReadStatus(&status, TRUE, ePLATFORM_TYPE_HANDHELD);	
	
	 if(status == 0x02)
	 {	
		 spiFlashReadData(readData1, ID_LOOKUP_TABLE, sizeof(WORD), SPI_FLASH_READ_DATA_NORMAL, ePLATFORM_TYPE_HANDHELD);	 
	 }
	 
	 combined  = (readData1[0] & 0xff); 
	 combined |= (readData1[1] << 8 );

	 nIDs = combined;
	 
	 spiFlashReadData(readData1, ID_LOOKUP_TABLE, (nIDs*8)+2, SPI_FLASH_READ_DATA_NORMAL, ePLATFORM_TYPE_HANDHELD);	 
	 
	 fillIdMap();
}

void fillIdMap(void)
{
	int j = 2;	
	WORD combined;
	//printf("nIDS[%d]\n", nIDs);
	for(i=0; i<nIDs; i++)
	{
		// Get the IDs
		combined = (readData1[j++] & 0xff); 
		combined |= (readData1[j++] << 8 );
		ids[i].ID = combined;
		
		// Get the Length
		combined = (readData1[j++] & 0xff); 
		combined |= (readData1[j++] << 8 );
		ids[i].nLength  = combined;
		
		// Get the Offsets
		FLASH_OFFSET.bFlashOffset[0] = readData1[j++];
		FLASH_OFFSET.bFlashOffset[1] = readData1[j++];
		FLASH_OFFSET.bFlashOffset[2] = readData1[j++];
		FLASH_OFFSET.bFlashOffset[3] = readData1[j++];
		ids[i].nOffset = FLASH_OFFSET.dFlashOffset;
	}

#if 0	
    for(i=0; i<nIDs; i++)
    {
		printf("i[%d] ID[%04X] Length[%04X] Offset[%08X]\n",i, ids[i].ID,ids[i].nLength,ids[i].nOffset);
    }
#endif

}

void fillColorMap(WORD ID)
{
	int j = 0;
	WORD length;
	WORD combined;
	
	for(i=0; i<nIDs; i++)
	{
		if(ids[i].ID == ID)
		{
			length = (ids[i].nLength/2);
			spiFlashReadData(readData1, ids[i].nOffset, ids[i].nLength, SPI_FLASH_READ_DATA_NORMAL, ePLATFORM_TYPE_HANDHELD);	 
			break;
		}			
	}
	
	for(i=0; i<length; i++)
	{
		combined  = (readData1[j++] & 0xff); 
		combined |= (readData1[j++] << 8 );
		colors.mapData[i] = combined;
	}	
}

WORD* getColorMap(void)
{
	return ((WORD *)colors.mapData);	
}

void* getStoredUI(ID uID)
{	
	for(i=0; i<nIDs; i++)
	{
		if(ids[i].ID == uID)
		{
			spiFlashReadData(readData1, ids[i].nOffset, ids[i].nLength, SPI_FLASH_READ_DATA_NORMAL, ePLATFORM_TYPE_HANDHELD);	
			break;
		}			
	}
	
	return readData1;
}

STORED_IDMAP* locateUIData(ID uID)
{
	STORED_IDMAP* pRetVal = NULL;
	for(i=0; i<nIDs; i++)
	{
		if(ids[i].ID == uID)
		{
			pRetVal = (STORED_IDMAP*)&ids[i];
			break;
		}
	}
	return pRetVal;
}
void* getUIData(ID uID)
{
	void* pRetVal = NULL;
	for(i=0; i<nIDs; i++)
	{
		if(ids[i].ID == uID)
		{
			int nLength1 = ids[i].nLength;
			int nLength2 = 0;
			if(READ_DATA_1_LENGTH < nLength1)
			{
				nLength1 = READ_DATA_1_LENGTH;
				nLength2 = ids[i].nLength-READ_DATA_1_LENGTH;
			}
			spiFlashReadData(readData1, ids[i].nOffset, nLength1, SPI_FLASH_READ_DATA_NORMAL, ePLATFORM_TYPE_HANDHELD);	 
			if(0 != nLength2)
			{
					spiFlashReadData(readData2, ids[i].nOffset+READ_DATA_1_LENGTH, nLength2, SPI_FLASH_READ_DATA_NORMAL, ePLATFORM_TYPE_HANDHELD);	 
			}
			pRetVal = readData1;
			break;
		}			
	}
	return pRetVal;
}

BOOL doesUIIDExist(ID nID)
{
	BOOL bRetVal = FALSE;
		for(i=0; i<nIDs; i++)
	{
		if(ids[i].ID == nID)
		{
			bRetVal = TRUE;
			break;
		}			
	}
	return bRetVal;
}

