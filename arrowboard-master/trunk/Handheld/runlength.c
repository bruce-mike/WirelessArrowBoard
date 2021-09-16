//========================================================================
//       run length encoding utilities
//========================================================================
#include "LPC23xx.h"			/* LPC23XX/24xx Peripheral Registers */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "shareddefs.h"
#include "timer.h"
#include "queue.h"
#include "runlength.h"
#include "commboard.h"
#include "ssp.h"
#include "spiflashui.h"
#include "spiFlash.h"
#include "spiflashui.h"
#include "UI.h"

static WORD *colorMap; 

//static WORD imageSize;
static WORD imageWidth;
static WORD imageHeight;

//static int i;

static UINT nRepeats;
static BYTE nCurrentRepeatedData;
static UINT nCurrentIndex;
static int nRemainingStoredBytes;
static int nRemaingStoredBytesOffset;

int encodeLen(BYTE* pBuffer, int nLength)
{
   int nBytes = 0;
   while(0 != nLength)
   {
      if(nLength < 128)
      {
         *pBuffer = nLength;
      }
      else
      {
         *pBuffer = 0x80;
         *pBuffer |= (nLength&0x7f);
      }
      pBuffer++;
      nLength >>= 7;
      nBytes++;
   }
   return nBytes;
}

int decodeLen(BYTE* pBuffer, int* pnBytes)
{
   int nLength = 0;
   *pnBytes = 0;
   while(0 != (*pBuffer&0x80))
   {
      nLength |= ((*pBuffer&0x7f)<<(7*(*pnBytes)));
      pBuffer++;
      *pnBytes += 1;
   }
   nLength |= ((*pBuffer&0x7f)<<(7*(*pnBytes)));
   *pnBytes += 1;
   return nLength;
}

/////
// read the next block of graphics data
/////
void readNextBlock()
{
	int nLength = nRemainingStoredBytes;
	if(0 < nLength)
	{
		if(nLength > READ_DATA_1_LENGTH)
		{
			/////
			// still more than 16K left
			// so adjust remaining length and offset
			/////
			nRemainingStoredBytes -= READ_DATA_1_LENGTH;
			nRemaingStoredBytesOffset += READ_DATA_1_LENGTH;
			nLength = READ_DATA_1_LENGTH;
		}
		spiFlashReadData(READ_DATA_1, nRemaingStoredBytesOffset, nLength, SPI_FLASH_READ_DATA_NORMAL, ePLATFORM_TYPE_HANDHELD);
	}	
}

WORD nReadNextByte(void)
{
   BYTE nPixel = 0;
	 BYTE nCurrentData;
   int nBytes;
	
   if(0 < nRepeats)
   {
      /////
      // in the middle of repeating data
      /////
      nPixel = nCurrentRepeatedData;
		 
      nRepeats--;
   }
   else
   {
		  nCurrentData = READ_DATA_1[nCurrentIndex++];
			if( nCurrentIndex >= READ_DATA_1_LENGTH )
			{
				readNextBlock();
				nCurrentIndex = 0;
			}				
      if(255 == nCurrentData)
      {
        /////
        // beginning of repeated data
				//
				// calculate number of repeats
				// same as in decodeLen above,
				// except we look for nCurrentIndex going out of bounds
				// and reading a new block of data
        /////
				nRepeats = 0;
				nBytes = 0;
				while(0 != (READ_DATA_1[nCurrentIndex]&0x80))
				{
					nRepeats |= (READ_DATA_1[nCurrentIndex]&0x7f)<<(7*(nBytes));
					nCurrentIndex++;
					if( nCurrentIndex >= READ_DATA_1_LENGTH )
					{
						readNextBlock();
						nCurrentIndex = 0;
					}	
					nBytes += 1;
				}
				nRepeats |= (READ_DATA_1[nCurrentIndex]&0x7f)<<(7*(nBytes));

				nCurrentIndex++;
				if( nCurrentIndex >= READ_DATA_1_LENGTH )
				{
					readNextBlock();
					nCurrentIndex = 0;
				}	
				nRepeats--;
				nCurrentRepeatedData = READ_DATA_1[nCurrentIndex++];	
				if( nCurrentIndex >= READ_DATA_1_LENGTH )
				{
					readNextBlock();
					nCurrentIndex = 0;
				}				
        nPixel = nCurrentRepeatedData;
      }
      else
      {
         nPixel = nCurrentData;
			}
   }
   return (colorMap[nPixel]);
}

UINT nReadInit(ID graphicID, WORD* width, WORD* height)
{
  UINT nSize = 0;
	/////
	// locate the specified ID
	/////
	STORED_IDMAP* pStoredIDMap = locateUIData(graphicID);
	if(NULL != pStoredIDMap)
	{
		/////
		// read up to the 1st 16K into RAM
		/////
		int nLength = pStoredIDMap->nLength;
		int nOffset = pStoredIDMap->nOffset;
		STORED_GRAPHICS* pStoredGraphics = (STORED_GRAPHICS*)READ_DATA_1;
		nRemainingStoredBytes = 0;
		nRemaingStoredBytesOffset = nOffset;
		if(nLength > READ_DATA_1_LENGTH)
		{
			/////
			// more than 16K to read
			// so remember how much is left
			// and where it starts
			/////
			nRemainingStoredBytes = nLength-READ_DATA_1_LENGTH;
			nRemaingStoredBytesOffset += READ_DATA_1_LENGTH;
			nLength = READ_DATA_1_LENGTH;
		}
		spiFlashReadData(READ_DATA_1, nOffset, nLength, SPI_FLASH_READ_DATA_NORMAL, ePLATFORM_TYPE_HANDHELD);	 

	 imageWidth = pStoredGraphics->nWidth;
	 imageHeight = pStoredGraphics->nHeight;
	 
	 colorMap = getColorMap();
	 	 
	 nSize = imageWidth*imageHeight; 
	 *width = imageWidth;
	 *height = imageHeight;
   //nCurrentIndex = 0;
	 /////
	 // set index to start of stored graphics
	 /////
   nCurrentIndex = (int)pStoredGraphics->compressedData-(int)pStoredGraphics;	 
   nRepeats = 0;
	 
   nCurrentRepeatedData = '\0';	 
 }
 return nSize;
}
