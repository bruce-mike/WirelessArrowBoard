/*****************************************************************************
 *   UI.h:  Header file - Functions used for the User Interface
 *
 *   History
 *   2014.07.24  ver 1.00    Prelimnary version
 *
******************************************************************************/
#ifndef __UI_H__
#define __UI_H__

#include "sharedinterface.h"

#define ID_LOOKUP_TABLE 0x002000
#if 0
__packed struct idMap
{
	WORD ID;
	WORD nLength;
	DWORD nOffset;
};

__packed struct storedUI
{
	WORD colorMapID;
	WORD firstPageID;
};

__packed struct colorMap
{
	WORD colorMap[256];
};

__packed struct storedPage
{
	WORD onEntryID;
	WORD onExitID;
	WORD nElements;
	WORD elementIDList[];
};

__packed struct storedElement
{
	WORD elementType;
	WORD nLeft;
	WORD nTop;
	WORD nWidth;
	WORD nHeight;
	WORD periodicFunctionID;
	WORD pushButtonFunctionID;
	WORD nType;
	WORD nData;
	WORD nGraphics;
	WORD graphicIDList[];
};

__packed struct storedGraphics
{
	WORD nWidth;
	WORD nHeight;
	WORD nCompressedBytes;
	BYTE compressedData[];
};
#endif
void initUI(void);
void fillIdMap(void);
void fillColorMap(WORD ID);
void fillStoredPage(WORD ID);
void fillStoredElements(WORD ID);

WORD* loadColorMap(void);
WORD* loadStoredUI(void);
WORD* getColorMap(void);
void* getStoredUI(ID uID);
STORED_IDMAP* locateUIData(ID uID);
void* getUIData(ID uID);
BOOL doesUIIDExist(ID nID);

#endif //#ifndef __UI_H__

