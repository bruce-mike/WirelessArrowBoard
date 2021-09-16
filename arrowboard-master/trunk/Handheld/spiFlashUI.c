#include <stdio.h>
#include "lpc23xx.h"
#include "shareddefs.h"
#include "sharedinterface.h"
#include "timer.h"
#include "queue.h"
#include "gen_UI.h"
#include "LCD.h"
#include "UI.h"
#include "spiFlashUI.h"

void spiFlashInit()
{
}

void spiFlashDoWork()
{
}

STORED_UI* spiFlashOpenUI(ID uiID)
{
	return ((STORED_UI*)getStoredUI(uiID));
}

void spiFlashFillColorMap(ID mapID)
{
	fillColorMap(mapID);
}

void* spiFlashGetData(ID uID)
{
	return getUIData(uID);
}

BOOL spiFlashDoesIDExist(ID nID)
{
	return doesUIIDExist(nID);
}
void spiFlashDisplayGraphic(ID graphicID, int nLeft, int nTop)
{
	LCD_Display_Bitmap(graphicID,nLeft,nTop);
}
