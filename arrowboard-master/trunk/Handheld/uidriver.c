#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "shareddefs.h"
#include "sharedinterface.h"
#include "timer.h"
#include "queue.h"
#include "gen_UI.h"
#include "commboard.h"
#include "spiflashui.h"
#include "uidriver.h"
#include "LCD.h"
#include "menufunctions.h"
#include "commands.h"
#include "storedconfig.h"

static ID firstPageID;
static ID patternImagesID;
static PAGE CurrentPage;
static unsigned char ElementList[MAX_ELEMENTS_PER_PAGE][sizeof(PUSHBUTTON_NAVIGATION_ELEMENT)];
static int nPagesOnStack;
static ID PageStack[MAX_PAGE_STACK_DEPTH];
static ID previousPageID;
static int nXTouch;
static int nYTouch;
static eMODEL eModel = eMODEL_TRAILER_UPDATING;
static eACTUATOR_TYPES eActuatorType = eACTUATOR_TYPE_UKNOWN;

static volatile unsigned long nPageLoads;
static BOOL bTouchInProgress = FALSE;

#define TOUCH_INACTIVITY_TIMEOUT_MS 600000 // 10 minutes
//#define TOUCH_INACTIVITY_TIMEOUT_MS 9000 // 9 seconds MJB


static TIMERCTL touchInactivityTimer;


void uiDriverSetModel(unsigned short nModel)
{
	eModel = (eMODEL)nModel;
}
eMODEL uiDriverGetModel(void)
{
	return eModel;
}

void uiDriverSetActuatorType(eACTUATOR_TYPES type)
{
    eActuatorType = type;
printf("uiDriverSetActuatorType[%d]\r\n", type);
}
    
eACTUATOR_TYPES uiDriverGetActuatorType(void)
{
    return eActuatorType;
}

void uiDriverSendGetConfig(eINTERFACE eInterface)
{
	SetStatusToGetConfig();
}
//===============================================
// display the specified element
//===============================================
void uiDisplayElement(ELEMENT* pElement, int nImageIndex)
{
	ID nID = 0;
	if(NULL != pElement->pButtonImageOffsetFunction)
	{
			nImageIndex += pElement->pButtonImageOffsetFunction(pElement);
	}

	nID = pElement->graphicIDS[nImageIndex];
	if(eELEMENT_TYPE_DISPLAYONLY == pElement->eType)
	{
		DISPLAY_ONLY_SELECTION_ELEMENT* pDisplayElement = (DISPLAY_ONLY_SELECTION_ELEMENT *)pElement;
		if(NULL != pDisplayElement->pImageGenerationFunction)
		{
			nID = pDisplayElement->pImageGenerationFunction(pElement);
		}
	}
	if(0 != nID)
	{
		spiFlashDisplayGraphic(nID, pElement->nLeft, pElement->nTop);
	}

}
//===============================================
// refresh all radio buttons on the current page
//===============================================
void uiRefreshCurrentPage(void)
{
	int i;

	for(i=0; i<CurrentPage.nElements; i++)
	{
		ELEMENT* pElement = (ELEMENT *)ElementList[i];
		if(NULL != pElement->pWhichImageFunction)
		{
			int nImageIndex = pElement->pWhichImageFunction(pElement);
			if(nImageIndex != pElement->nCurrentImageIndex)
			{
				uiDisplayElement(pElement, nImageIndex);
				pElement->nCurrentImageIndex = nImageIndex;
			}
		}	
	}
}
//===============================================
// reload the currently displayed page
//===============================================
void uiDisplayCurrentPage(void)
{
	int i;
	
	LCD_Clear_Display(FALSE,LAYER_ONE);
	
	for(i=0; i<CurrentPage.nElements; i++)
	{
		ELEMENT* pElement = (ELEMENT *)ElementList[i];
		int nImageIndex = BUTTON_ENABLED_INDEX;

		if(NULL != pElement->pIsEnabledFunction)
		{
			if(!pElement->pIsEnabledFunction(pElement))
			{
				nImageIndex = BUTTON_DISABLED_INDEX;
			}
		}
		if(NULL != pElement->pWhichImageFunction)
		{

			nImageIndex = pElement->pWhichImageFunction(pElement);
		}
		uiDisplayElement(pElement, nImageIndex);
	}
}
//===============================================
// load the specified element
//===============================================
void uiLoadElement(STORED_ELEMENT* pStoredElement, ELEMENT* pElement)
{
	int nImageIndex = BUTTON_ENABLED_INDEX;
	int nIndex;
	pElement->eType = (eELEMENT_TYPE)pStoredElement->elementType;
	pElement->nLeft = pStoredElement->nLeft;
	pElement->nTop = pStoredElement->nTop;
	pElement->nWidth = pStoredElement->nWidth;
	pElement->nHeight = pStoredElement->nHeight;
	pElement->pPeriodicFunction = periodicIDToFunction(pStoredElement->periodicFunctionID);
	pElement->pWhichImageFunction = whichImageIDToFunction(pStoredElement->whichImageFunctionID);
	pElement->pIsEnabledFunction = isEnabledIDToFunction(pStoredElement->isEnabledFunctionID);
	pElement->pButtonImageOffsetFunction = buttonImageOffsetIDToFunction(pStoredElement->buttonImageOffsetFunctionID);

  initTimer(&pElement->buttonTimer);
	if(NULL != pElement->pPeriodicFunction)
	{
		
			startTimer(&pElement->buttonTimer, DEFAULT_PERIODIC_TIME_MS);
	}
	pElement->nCurrentImageIndex = 0;
	pElement->nGraphics = pStoredElement->nGraphics;
	for(nIndex=0; nIndex<pElement->nGraphics; nIndex++)
	{
		pElement->graphicIDS[nIndex] = pStoredElement->graphicIDList[nIndex];

	}
	
	switch(pElement->eType)
	{
		case eELEMENT_TYPE_RADIO_BUTTON:
			{
				RADIO_BUTTON_SELECTION_ELEMENT *pButton = (RADIO_BUTTON_SELECTION_ELEMENT*)pElement;
				pButton->pPushbuttonFunction = pushbuttonIDToFunction(pStoredElement->pushButtonFunctionID);
				pButton->nType = pStoredElement->nType;
				pButton->nData = pStoredElement->nData;
				pButton->bSelected = FALSE;	

			}
			break;
		case eELEMENT_TYPE_NAVIGATION_BUTTON:
			{
				PUSHBUTTON_NAVIGATION_ELEMENT *pButton = (PUSHBUTTON_NAVIGATION_ELEMENT*)pElement;
				pButton->pPushbuttonFunction = pushbuttonIDToFunction(pStoredElement->pushButtonFunctionID);
				pButton->nNextPages = pStoredElement->nNextPages;
				for(nIndex=0;nIndex<MAX_NEXT_PAGES; nIndex++)
				{

					pButton->nextPageIDS[nIndex] = pStoredElement->nextPagesIDList[nIndex];
				}
			}
			break;
		case eELEMENT_TYPE_NAVIGATION_RETURN_BUTTON:
			break;
		case eELEMENT_TYPE_NAVIGATION_TO_MAIN_BUTTON:
			break;
		case eELEMENT_TYPE_DISPLAYONLY:
			{
				DISPLAY_ONLY_SELECTION_ELEMENT *pButton = (DISPLAY_ONLY_SELECTION_ELEMENT*)pElement;
				pButton->pImageGenerationFunction = imageGenerationIDToFunction(pStoredElement->imageGenerationFunctionID);
			}
			break;
		case eELEMENT_TYPE_SLIDER:
			break;
		default:
			printf("uiLoadElement: UNKNOWN pElement->eType[%d]\n",pElement->eType);
			break;
	}
	
	/////
	//deterimine if enabled or not
	/////
	if(NULL != pElement->pIsEnabledFunction)
	{
		if(!pElement->pIsEnabledFunction(pElement))
		{
			nImageIndex = BUTTON_DISABLED_INDEX;
		}
	}
	/////
	// determine which graphic to display
	/////
	if(NULL != pElement->pWhichImageFunction)
	{
		nImageIndex = pElement->pWhichImageFunction(pElement);
		pElement->nCurrentImageIndex = nImageIndex;
	}
	/////
	// display the first element
	// it may be redisplayed soon 
	// with a different graphic selection
	/////

	uiDisplayElement(pElement, nImageIndex);
}

//===============================================
// load the specified page
//===============================================
void uiLoadPage(ID pageID)
{
	int i;
	int ids[MAX_ELEMENTS_PER_PAGE];
	STORED_PAGE* pStoredPage = (STORED_PAGE*)spiFlashGetData(pageID);

//***********************************************************
    printf("1-uiLoadPage[%X]\n", pageID);  // DBGP
//***********************************************************
     
	if(NULL != pStoredPage)
	{
		/////
		// bump the counter
		// so other software can 
		// tell if the page has changed
		/////
		nPageLoads++;
		if(pageID != previousPageID)
		{
			LCD_Clear_Display(FALSE,LAYER_ONE);
			CurrentPage.ColorMapID = pStoredPage->colorMapID;

			if(NULL != CurrentPage.pOnExit)
			{
				CurrentPage.pOnExit(&CurrentPage); 
			}
			CurrentPage.pOnEntry = onEntryExitIDToFunction(pStoredPage->onEntryID);
			CurrentPage.pOnExit = onEntryExitIDToFunction(pStoredPage->onExitID);
			CurrentPage.nElements = pStoredPage->nElements;	
			for(i=0; i<CurrentPage.nElements; i++)
			{
				ids[i] = pStoredPage->elementIDList[i];
			}
			spiFlashFillColorMap(CurrentPage.ColorMapID);
			for(i=0; i<CurrentPage.nElements; i++)
			{
				STORED_ELEMENT* pStoredElement = (STORED_ELEMENT*)spiFlashGetData(ids[i]);
				if(NULL != pStoredElement)
				{
					uiLoadElement(pStoredElement, (ELEMENT *)ElementList[i]);
				}
			}
		
			if(NULL != CurrentPage.pOnEntry)
			{
				CurrentPage.pOnEntry(&CurrentPage);
			}
			previousPageID = pageID;
		}
		resetPatternImages();
	}
  else
  {
    printf("Page Load Failed\n");
  }
}

//===============================================
// push the specified page id onto the stack
// then load the page
//===============================================
void uiDriverPushNewPage(ID pageID)
{
	memset((unsigned char*)&CurrentPage, 0, sizeof(CurrentPage));
	memset((unsigned char*)&ElementList, 0, sizeof(ElementList));
	//CurrentPage.pElementsList = (ELEMENT*)ElementList;
	PageStack[nPagesOnStack++] = pageID;
//********************************************************************************
printf("uiDriverPushNewPage nPagesOnStack[%d][%X]\n", nPagesOnStack, pageID); // DBGP
//********************************************************************************
	uiLoadPage(pageID);
}
//===============================================
// pop a page off of the page stack
// (don't let it go too far)
// then load the page
//===============================================
void uiDriverPopPage(BOOL bLoadNewPage)
{

	if(1 < nPagesOnStack)
	{
		if(NULL != CurrentPage.pOnExit)
		{
			/////
			// tossing out the current page
			// so call the onExit handler
			// and reset the pointer so it won't be called again
			/////
			CurrentPage.pOnExit(&CurrentPage);
			CurrentPage.pOnExit = NULL;
		}
		nPagesOnStack--;
		//printf("0-uiDriverPopPage nPagesOnStack[%d]\n", nPagesOnStack);

		if(bLoadNewPage)
		{
			//printf("2-uiDriverPopPage[%d] [%X]\n", nPagesOnStack, PageStack[nPagesOnStack-1]);
			uiLoadPage(PageStack[nPagesOnStack-1]);
		}
	}
}
//===============================================
// pop to the main page
//===============================================
void uiDriverPopToMain()
{		
	if(NULL != CurrentPage.pOnExit)
	{
		CurrentPage.pOnExit(&CurrentPage); 
	}
	nPagesOnStack = 0;
	uiDriverPushNewPage(firstPageID);
}
//===============================================
// load the specified UI
//===============================================
void uiLoadUI(ID uiID)
{
	STORED_UI *pStoredUI;
    
//	firstPageID = pStoredUI->firstPageID;
//	patternImagesID = pStoredUI->patternImagesID;

    if(SCREEN_MODE_SINGLE == storedConfigGetScreenMode())
    {        
       firstPageID = PAGE_ID_SinglePageMain;
       pStoredUI = spiFlashOpenUI(CUSTOM_UI);
    } 
    else
    {
        firstPageID = PAGE_ID_Main;
        pStoredUI = spiFlashOpenUI(STANDARD_UI);
    }

    patternImagesID = pStoredUI->patternImagesID;    
    
	memset((unsigned char*)&PageStack, 0, sizeof(PageStack));
	nPagesOnStack = 0;
	uiDriverPushNewPage(firstPageID);	
}

//===============================================
void uiSetFirstPage(unsigned int page)
//===============================================
{
    firstPageID = page;
}

//===============================================
// does the specified element exist?
//===============================================
BOOL uiDoesIDExist(ID nID)
{
	return spiFlashDoesIDExist(nID);
}

//===============================================
// initialize the UI driver code
//===============================================
void uiDriverInit()
{
	int nDefaultUI = storedConfigGetDefaultUI();
	if(0 >= nDefaultUI)
	{
		nDefaultUI = STANDARD_UI;
	}
	
   
	initTimer(&touchInactivityTimer);
	startTimer(&touchInactivityTimer, TOUCH_INACTIVITY_TIMEOUT_MS);
	
	eModel = (eMODEL)-1;
	nPageLoads = 0;
	uiLoadUI(nDefaultUI);
}

//===============================================
// handle a touch screen press
// called by the touch screen software
//===============================================
void uiDriverHandleTouchScreenPress(int nX, int nY)
{	
	nXTouch = nX;
	nYTouch = nY;	
}
typedef enum eTouchPadStates
{
	eTP_IDLE,
	eTP_PRESSED,
	eTP_RELEASED,
}eTOUCHPADSTATES;

static BOOL uiDriverProcessTouchScreenEvents()
{
	static ELEMENT* pPreviousPushbuttonElement = NULL;
	static eTOUCHPADSTATES eNextTPState = eTP_IDLE;
	BOOL bRetVal = FALSE;
	int nIndex;
	static int nXOld = 0;
	static int nYOld = 0;
	
	if(LCD_Sleep_Mode_Enabled())
	{	
		eNextTPState = eTP_IDLE;
	}
	else 
	{
		bTouchInProgress = FALSE;	
		if(LCD_Touch_In_Progress())
		{		
			bTouchInProgress = TRUE;
		}
		
		LCD_Reset_TP_Event();
	}

	switch(eNextTPState)
	{
		case eTP_IDLE:
			
			if(bTouchInProgress)
			{
				eNextTPState = eTP_PRESSED;
				
				//printf("Idle State -> Next State: eTP_PRESSED\n");
			}
			
			break;
			
		case eTP_PRESSED:
			
			if(!bTouchInProgress)
			{
				eNextTPState = eTP_RELEASED;
				
				//printf("eTP_PRESSED State -> Next State: eTP_RELEASED\n");
				
				break;
			}
			else
			{
				//printf("eTP_PRESSED State -> Processing\n");
				
				/////
				// we got a touch on the touch screen
				// look at the elements in the current page
				// to see if the user touched one of our buttons
				// if so, then execute any defined pushbutton procedure for this element
				/////
				for(nIndex=0; nIndex<CurrentPage.nElements; nIndex++)
				{
					ELEMENT* pElement = (ELEMENT *)ElementList[nIndex];
					if((nXTouch >= pElement->nLeft) && (nXTouch <= (pElement->nLeft+pElement->nWidth)))
					{
						if((nYTouch >= pElement->nTop) && (nYTouch <= (pElement->nTop+pElement->nHeight)))
						{				
							if(NULL != pElement->pIsEnabledFunction)
							{
									if(!pElement->pIsEnabledFunction(pElement))
									{
											continue;
									}
							}
										
							/////
							// got a match
							// was there a previously selected element?
							// if so, give it a chance to clean up
							/////
							switch(pElement->eType)
							{
								case eELEMENT_TYPE_RADIO_BUTTON:
								{
									RADIO_BUTTON_SELECTION_ELEMENT *pRadioButtonElement;
									if(NULL != pPreviousPushbuttonElement)
									{
										if(pElement != pPreviousPushbuttonElement)
										{
											pRadioButtonElement = (RADIO_BUTTON_SELECTION_ELEMENT*)pPreviousPushbuttonElement;
											if(NULL != pRadioButtonElement->pPushbuttonFunction)
											{
												int nImageIndex = pRadioButtonElement->pPushbuttonFunction(pElement, TRUE, FALSE);
												if(nImageIndex != pPreviousPushbuttonElement->nCurrentImageIndex)
												{
													uiDisplayElement(pPreviousPushbuttonElement, nImageIndex);
													pElement->nCurrentImageIndex = nImageIndex;
												}
											}
											pPreviousPushbuttonElement = NULL;
										}
									}
									/////
									// got a match
									// do we have a function to call?
									/////
									pRadioButtonElement = (RADIO_BUTTON_SELECTION_ELEMENT*)pElement;
									if(NULL != pRadioButtonElement->pPushbuttonFunction)
									{
										int nImageIndex = pRadioButtonElement->pPushbuttonFunction(pElement, TRUE, TRUE);
										if(nImageIndex != pElement->nCurrentImageIndex)
										{
										uiDisplayElement(pElement, nImageIndex);
											pElement->nCurrentImageIndex = nImageIndex;
										}
										pPreviousPushbuttonElement = pElement;
									}
									bRetVal = TRUE;
									return bRetVal;
								}
						
								case eELEMENT_TYPE_NAVIGATION_BUTTON:
								case eELEMENT_TYPE_NAVIGATION_RETURN_BUTTON:
								case eELEMENT_TYPE_NAVIGATION_TO_MAIN_BUTTON:
								case eELEMENT_TYPE_DISPLAYONLY:
								case eELEMENT_TYPE_SLIDER:
									bRetVal = FALSE;
									break;
							}
						}
					}
				}
			}
			break;
			
		case eTP_RELEASED:
			
			if(bTouchInProgress)
			{
				/////
				// pressed again before debounce timer expired
				// so back to the pressed state
				/////
				eNextTPState = eTP_PRESSED;
				
				//printf("eTP_RELEASED State -> Next State: eTP_PRESSED\n");
				
				break;
			}
			else
			{
					eNextTPState = eTP_IDLE;
				
					//printf("eTP_RELEASED State -> Next State: eTP_IDLE Processing\n");
				
					pPreviousPushbuttonElement = NULL;
		
					//printf("1-TOUCH RELEASED\n");

					/////
					// Ignore Duplicate X and Y coordinates 
					/////
					if((nXOld == nXTouch) && (nYOld == nYTouch))
					{
						return bRetVal;
					}
					//printf("2-TOUCH RELEASED\n");						
					nXOld = nXTouch;
					nYOld = nYTouch;
		
					/////
					// we got a touch on the touch screen
					// look at the elements in the current page
					// to see if the user touched one of our buttons
					// if so, then execute any defined pushbutton procedure for this element
					/////
					for(nIndex=0; nIndex<CurrentPage.nElements; nIndex++)
					{
						ELEMENT* pElement = (ELEMENT *)ElementList[nIndex];
						if((nXTouch >= pElement->nLeft) && (nXTouch <= (pElement->nLeft+pElement->nWidth)))
						{
							if((nYTouch >= pElement->nTop) && (nYTouch <= (pElement->nTop+pElement->nHeight)))
							{				
								if(NULL != pElement->pIsEnabledFunction)
								{
										if(!pElement->pIsEnabledFunction(pElement))
										{
												continue;
										}
								}
								/////
								// Generate a pleasant tone when a button is pressed 
								/////
								LCD_TP_Feedback();
										
								/////
								// got a match
								// do we have a function to call?
								/////
								switch(pElement->eType)
								{
									case eELEMENT_TYPE_RADIO_BUTTON:
									{
										RADIO_BUTTON_SELECTION_ELEMENT *pRadioButtonElement = (RADIO_BUTTON_SELECTION_ELEMENT*)pElement;
										if(NULL != pRadioButtonElement->pPushbuttonFunction)
										{
											int nImageIndex = pRadioButtonElement->pPushbuttonFunction(pElement, FALSE, TRUE);
											//printf("1-\n");
											if(nImageIndex != pElement->nCurrentImageIndex)
											{
												uiDisplayElement(pElement, nImageIndex);
												pElement->nCurrentImageIndex = nImageIndex;
											}
										}
										bRetVal = TRUE;
										return bRetVal;
									}
						
									case eELEMENT_TYPE_NAVIGATION_BUTTON:
									{
										PUSHBUTTON_NAVIGATION_ELEMENT* pNavigationElement = (PUSHBUTTON_NAVIGATION_ELEMENT*)pElement;

										int nNextPageID = pNavigationElement->nextPageIDS[0];
										if(NULL != pNavigationElement->pPushbuttonFunction)
										{
											int nID = pNavigationElement->pPushbuttonFunction(pElement, FALSE, TRUE);
											//printf("1-uiDriverProcessTouchScreenEvents nID[%X]\n", nID);
											if(0 != nID)
											{
												nNextPageID = nID;
												//printf("2-uiDriverProcessTouchScreenEvents nID[%X]\n", nID);
											}
										}
										//printf("3-uiDriverProcessTouchScreenEvents nNextPageID[%X]\n", nNextPageID);
										uiDriverPushNewPage(nNextPageID);
										bRetVal = TRUE;
										return bRetVal;
									}
						
									case eELEMENT_TYPE_NAVIGATION_RETURN_BUTTON:
										/////
										// pop the page
										// load the previous page
										/////
										uiDriverPopPage(TRUE);
										bRetVal = TRUE;
										return bRetVal;
									
									case eELEMENT_TYPE_NAVIGATION_TO_MAIN_BUTTON:
										/////
										// pop back to main
										/////
										uiDriverPopToMain();
										bRetVal = TRUE;
										return bRetVal;									
						
									case eELEMENT_TYPE_DISPLAYONLY:
										bRetVal = TRUE;
										return bRetVal;
	
									case eELEMENT_TYPE_SLIDER:
										bRetVal = TRUE;
										return bRetVal;
								}
							}
						}
					}
			}
			break;
	}

	return bRetVal;
}

//===============================================
// perform periodic work
//===============================================
void uiDriverDoWork()
{
	int nIndex;
	int nNextTimeout;
		
	
	/////
	// handle any touch events
	/////

	if(!uiDriverProcessTouchScreenEvents())
	{
		unsigned long nPrevPageLoads = nPageLoads;

		/////
		// still on the same page
		//
		// loop through all elements in the current page
		// check all periodic timers and execute the functions if necessary
		/////
		for(nIndex=0; nIndex<CurrentPage.nElements; nIndex++)
		{
			ELEMENT* pElement = (ELEMENT *)ElementList[nIndex];
			if(NULL != pElement->pPeriodicFunction)
			{
				if(isTimerExpired(&pElement->buttonTimer))
				{
					/////
					// timer is expired
					// invoke the function via function pointer
					/////
					nNextTimeout = pElement->pPeriodicFunction(pElement);
					if(nPrevPageLoads != nPageLoads)
					{
						/////
						// the page changed out from under us
						// so skip out
						/////
						break;
					}
					/////
					// and restart the timer
					/////
					if(!nNextTimeout)
					{
						nNextTimeout = 10000;
					}
						startTimer(&pElement->buttonTimer, nNextTimeout);
				}
			}
		}
	        
		//===============================================================
		// Power down the Hand Held after 10 minutes of touch inactivity.
		//===============================================================		
		if(isTimerExpired(&touchInactivityTimer))
		{
			//jgr printf("10 Minute TOUCH INACTIVITY TIMEOUT! Powering Off Hand Held\n");
           
// MJB - do not power down hand held with 10 minute timeout			
//			powerDownHandHeld();
		}

	}
	else 
	{

		//================================================================
		// If a touch is detected, give us another 10 minutes before 
		// powering down the Hand Held.
		//================================================================
		initTimer(&touchInactivityTimer);
		
		startTimer(&touchInactivityTimer, TOUCH_INACTIVITY_TIMEOUT_MS); 
		
		printf("TOUCH ACTIVITY! Restarting 10 Minute touchInactivityTimer\n");
	}


}

//===============================================
// get the graphic data for the specified
// display pattern
//===============================================
int uiDriverGetDisplayPatternIDS(eDISPLAY_TYPES eCurrentDisplayType, unsigned short graphicsIDS[])
{
	int nImages = 0;
	PACKED unsigned short *pIDs = NULL;
    STORED_UI *pStoredUI;
    STORED_PATTERN_IMAGES *pStoredPatternImages;

/* MJB KLUGE
    if(SCREEN_MODE_SINGLE == storedConfigGetScreenMode())
    {        
       pStoredUI = spiFlashOpenUI(CUSTOM_UI);
    } 
    else
    {
        pStoredUI = spiFlashOpenUI(STANDARD_UI);
    }    
*/
    pStoredUI = spiFlashOpenUI(STANDARD_UI);
    patternImagesID = pStoredUI->patternImagesID;      
    
    
	pStoredPatternImages = (STORED_PATTERN_IMAGES*)spiFlashGetData(patternImagesID);
	if(NULL != pStoredPatternImages)
	{
		switch(eCurrentDisplayType)
		{
			case eDISPLAY_TYPE_BLANK:
				nImages = pStoredPatternImages->blankImagesCount;
				pIDs = &pStoredPatternImages->blankImagesIDs[0];
				break;
			case eDISPLAY_TYPE_FOUR_CORNER:
				nImages = pStoredPatternImages->fourCornerImagesCount;
				pIDs = &pStoredPatternImages->fourCornerImagesIDs[0];
				break;
			case eDISPLAY_TYPE_DOUBLE_ARROW:
				nImages = pStoredPatternImages->doubleArrowImagesCount;
				pIDs = &pStoredPatternImages->doubleArrowImagesIDs[0];
				break;
			case eDISPLAY_TYPE_BAR:
				nImages = pStoredPatternImages->barImagesCount;
				pIDs = &pStoredPatternImages->barImagesIDs[0];
				break;
			case eDISPLAY_TYPE_RIGHT_ARROW:
				nImages = pStoredPatternImages->rightArrowImagesCount;
				pIDs = &pStoredPatternImages->rightArrowImagesIDs[0];
				break;
			case eDISPLAY_TYPE_LEFT_ARROW:
				nImages = pStoredPatternImages->leftArrowImagesCount;
				pIDs = &pStoredPatternImages->leftArrowImagesIDs[0];
				break;
			case eDISPLAY_TYPE_RIGHT_STEM_ARROW:
				nImages = pStoredPatternImages->rightStemArrowImagesCount;
				pIDs = &pStoredPatternImages->rightStemArrowImagesIDs[0];
				break;
			case eDISPLAY_TYPE_LEFT_STEM_ARROW:
				nImages = pStoredPatternImages->leftStemArrowImagesCount;
				pIDs = &pStoredPatternImages->leftStemArrowImagesIDs[0];
				break;
			case eDISPLAY_TYPE_RIGHT_WALKING_ARROW:
				nImages = pStoredPatternImages->rightWalkingArrowImagesCount;
				pIDs = &pStoredPatternImages->rightWalkingArrowImagesIDs[0];
				break;
			case eDISPLAY_TYPE_LEFT_WALKING_ARROW:
				nImages = pStoredPatternImages->leftWalkingArrowImagesCount;
				pIDs = &pStoredPatternImages->leftWalkingArrowImagesIDs[0];
				break;
			case eDISPLAY_TYPE_RIGHT_CHEVRON:
				nImages = pStoredPatternImages->rightChevronImagesCount;
				pIDs = &pStoredPatternImages->rightChevronImagesIDs[0];
				break;
			case eDISPLAY_TYPE_LEFT_CHEVRON:
				nImages = pStoredPatternImages->leftChevronImagesCount;
				pIDs = &pStoredPatternImages->leftChevronImagesIDs[0];
				break;
			case eDISPLAY_TYPE_DOUBLE_DIAMOND:
				nImages = pStoredPatternImages->doubleDiamondImagesCount;
				pIDs = &pStoredPatternImages->doubleDiamondImagesIDs[0];
				break;
			case eDISPLAY_TYPE_WIG_WAG:
				nImages = pStoredPatternImages->wigWagImagesCount;
				pIDs = &pStoredPatternImages->wigWagImagesIDs[0];
				break;
			case eDISPLAY_TYPE_ALL_LIGHTS_ON:
				break;
			case eDISPLAY_TYPE_ERRORED_LIGHTS_OFF:
				break;
		}
        
/////////////////////////////////////////////////////////////////
// MJB
        nImages = nImages/2;
        if(SCREEN_MODE_SINGLE == storedConfigGetScreenMode())
        {
            pIDs = &pIDs[nImages];
        }
/////////////////////////////////////////////////////////////////        
		if(NULL != pIDs)
		{
			int i;
			for(i=0;i<nImages;i++)
			{
				graphicsIDS[i] = pIDs[i];
			}
		}
	}
	return nImages;
}

BOOL uiDriverTouchInProgress(void)
{
	return bTouchInProgress;
}

