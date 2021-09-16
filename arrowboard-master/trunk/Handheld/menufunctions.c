//=================================================================================
// these functions are defined in the UI builder
// and implemented here.
// on entry and on exit functions take an argument of a pointer to a page structure
// the others take an argument of a pointer to an element structure
//
// periodic functions return the number of milliseconds
// to wait until the function is called again
/////

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "shareddefs.h"
#include "sharedinterface.h"
#include "timer.h"
#include "queue.h"

#include "gen_UI.h"
#include "commboard.h"
#include "gen_UI.h"
#include "menufunctions.h"
#include "commands.h"
#include "spiflashui.h"
#include "uidriver.h"
#include "status.h"
#include "hdlc.h"
#include "lcd.h"
#include "storedconfig.h"
#include "wireless.h"

//#define DEBUG_MENU_CMDS

static BOOL bStatusGetConfig = TRUE;
static eCOMMANDS eStatusCommand = eCOMMAND_STATUS_ALARMS;
static BOOL flashHigh = TRUE;
static BOOL bNightMode = FALSE;
static eBRIGHTNESS_CONTROL eBrightnessControl = eBRIGHTNESS_CONTROL_NONE;
static unsigned short nDisallowedPatternMask = 0;
static eACTUATOR_TYPES eActuatorType = eACTUATOR_TYPE_UKNOWN;
static eACTUATOR_BUTTON_MODE eActuatorButtonMode     = eACTUATOR_BUTTON_MANUAL_MODE;
static eACTUATOR_CALIBRATION_HINT eActuatorCalibrationHint = eACTUATOR_CALIBRATION_HINT_0;
static eACTUATOR_COMMANDS eLastActuatorCommand = eACTUATOR_COMMAND_NOOP;
static int nActuatorImageCurrentIndex = 0;
static BOOL bCalibrateTiltUpInProgress = FALSE;
static BOOL bCalibrateTiltDownInProgress = FALSE;

static BOOL bActuatorHomeInProgress = FALSE;

static BOOL bCloneGettingData = FALSE;
static BOOL bCloneGotBrightnessControl = FALSE;
static BOOL bCloneGotDisallowedPatterns = FALSE;
static BOOL bCloneGotActuatorType = FALSE;
static BOOL bCloneGotAuxBatteryType = FALSE;
static BOOL bCloneGotActuatorButtonMode = FALSE;
static BOOL bCloneGotActuatorCalibrationHint = FALSE;

static int nAlarmUpDownPageOffset = ALARM_PAGE_UP_OFFSET;

static TIMERCTL actuatorManualCommandTimer;
static TIMERCTL actuatorAutoTouchInProgressTimer;
static TIMERCTL actuatorLimitTimer;
/////
// Assume that the actuator will reach a limit
// switch (Up or Down) within 45 seconds.
// There will be some slop in communication
// so it will probably be somewhat more than 45 seconds
/////
#define ACTUATOR_LIMIT_TIMER_MS 45000
#define ACTUATOR_HOME_FROM_RIGHT_TIMER_MS 3200
#define ACTUATOR_HOME_FROM_LEFT_TIMER_MS 5200

/////
// uncomment this to allow actuator auto mode
//
#define ALLOW_ACTUATOR_AUTO_MODE 1 //v90110
/////


static BOOL bMaintModeAllLightsOn = FALSE;
static BOOL bMaintModeAllLightsOff = FALSE;
static BOOL bMaintModeErroredLightsOff = FALSE;

static int sendCommand(eCOMMANDS eCommand, unsigned short nData)
{
	eINTERFACE eInterface = whichInterfaceIsActive();
	
	#ifdef PACKET_PRINT
		printf("\nsendCommand "); outputCommand( eCommand );
		printf(" to "); printfInterface( eInterface, FALSE ); 
		printf(" data:0x%X\n", nData);
	#endif
	
	commandSendCommand(eInterface, eCommand, nData);
	return 0;
}


static BOOL isPatternAllowed(eDISPLAY_TYPES eDisplayType)
{
	BOOL bAllowed = TRUE;
	if(nDisallowedPatternMask & (1<<(int)eDisplayType))
	{
		bAllowed = FALSE;
	}
	return bAllowed;
}
static void disallowPattern(eDISPLAY_TYPES eDisplayType)
{
	nDisallowedPatternMask |= (1<<(int)eDisplayType);
	sendCommand(eCOMMAND_SET_DISALLOWED_PATTERNS,nDisallowedPatternMask);
}
static void allowPattern(eDISPLAY_TYPES eDisplayType)
{
		nDisallowedPatternMask &= ~(1<<(int)eDisplayType);
		sendCommand(eCOMMAND_SET_DISALLOWED_PATTERNS,nDisallowedPatternMask);
}
//=================================================
// on page entry/exit
//=================================================
int OnEntryTiltStatus(struct page* pPage)
{
	sendCommand(eCOMMAND_ACTUATOR_SWITCH,eACTUATOR_COMMAND_STOP);
	//sendCommand(eCOMMAND_STATUS_ACTUATOR_LIMIT, 0);
	eLastActuatorCommand = eACTUATOR_COMMAND_STOP;
	initTimer(&actuatorManualCommandTimer);
	initTimer(&actuatorAutoTouchInProgressTimer);
	return 0;
}

int OnExitTiltStop(struct page* pPage)
{
	sendCommand(eCOMMAND_ACTUATOR_SWITCH,eACTUATOR_COMMAND_STOP);
	return 0;
}
int OnExitCalibrateTiltCancel(struct page* thePage)
{
	sendCommand(eCOMMAND_DO_ACTUATOR_CALIBRATION,eACTUATOR_LIMIT_CALIBRATE_CANCEL);
	bCalibrateTiltUpInProgress = FALSE;
	bCalibrateTiltDownInProgress = FALSE;
	return 0;
}
int OnExitMaintMode(struct page* thePage)
{
	sendCommand(eCOMMAND_DISPLAY_CHANGE,eDISPLAY_TYPE_BLANK);
	bMaintModeAllLightsOn = FALSE;
	bMaintModeAllLightsOff = FALSE;
	bMaintModeErroredLightsOff = FALSE;
	return 0;
}
int OnExitStatusAlarm(struct page* pPage)
{
	/////
	// next time we get to this page
	// this is the page offset to show
	/////
	nAlarmUpDownPageOffset = ALARM_PAGE_UP_OFFSET;
	return 0;
}
//=================================================
// external interface
// updates from arrow board
//=================================================
//=================================================
// arrow board brightness controls
//=================================================
void UpdateArrowBoardBrightnessControl(eBRIGHTNESS_CONTROL eControl)
{
	eBrightnessControl = eControl;
	if(bCloneGettingData)
	{
		storedConfigSetArrowBoardBrightnessControl(eBrightnessControl);
		bCloneGotBrightnessControl = TRUE;
	}
}

//=================================================
// disallowed patterns
//=================================================
void UpdateDisallowedPatterns(unsigned short nPatterns)
{
	nDisallowedPatternMask = nPatterns;
	if(bCloneGettingData)
	{
		storedConfigSetDisallowedPatterns(nDisallowedPatternMask);
		bCloneGotDisallowedPatterns = TRUE;
	}
}

//=================================================
// actuator types
//=================================================
void UpdateActuatorType(eACTUATOR_TYPES eType)
{
	eActuatorType = eType;
	if(bCloneGettingData)
	{
		storedConfigSetActuatorType(eActuatorType);
		bCloneGotActuatorType = TRUE;
	}
}

//=================================================
// default UI
//=================================================
void UpdateDefaultUI(int nUI)
{
	int nDefaultUI = storedConfigGetDefaultUI();
	if(0 >= nDefaultUI)
	{
		nDefaultUI = STANDARD_UI;
	}
	if(0 >= nUI)
	{
		nUI = STANDARD_UI;
	}
	if(nDefaultUI != nUI)
	{
		eINTERFACE eInterface = whichInterfaceIsActive();
		storedConfigSetDefaultUI(nUI);
		uiLoadUI(nUI);
		uiDriverSendGetConfig(eInterface);
	}
}

//=================================================
void UpdateScreenMode(int nMode)
//=================================================
{  
   if(nMode < SCREEN_MODE_STANDARD ||
        nMode > SCREEN_MODE_SINGLE)
   {
        nMode = SCREEN_MODE_STANDARD;
   }
   
printf("UpdateScreenMode[%d]\r\n", nMode);   
   
   if(storedConfigGetScreenMode() != nMode)
   {
printf("storedConfigSetScreenMode[%d]\r\n", nMode);
       storedConfigSetScreenMode(nMode);
   }
}
       
//=================================================
// actutor mode
//=================================================
void UpdateActuatorButtonMode(eACTUATOR_BUTTON_MODE eMode)
{
#ifndef ALLOW_ACTUATOR_AUTO_MODE
	eActuatorButtonMode = eACTUATOR_BUTTON_MANUAL_MODE;
#else
	eActuatorButtonMode = eMode;
	if(bCloneGettingData)
	{
		storedConfigSetActuatorButtonMode(eActuatorButtonMode);
		bCloneGotActuatorButtonMode = TRUE;
	}
#endif
}

//=================================================
// actutor calibration hint
//=================================================
void UpdateActuatorCalibrationHint(eACTUATOR_CALIBRATION_HINT eHInt)
{
	eActuatorCalibrationHint = eHInt;
	if(bCloneGettingData)
	{
		storedConfigSetActuatorCalibrationHint(eActuatorCalibrationHint);
		bCloneGotActuatorCalibrationHint = TRUE;
	}
}
//=================================================
// periodic command, check status, update status button image
//=================================================
void SetStatusToGetConfig()
{
	eStatusCommand = eCOMMAND_GET_MODEL_TYPE;
	bStatusGetConfig = TRUE;
}
void SetStatusToGetStatus()
{
	eStatusCommand = eCOMMAND_STATUS_ALARMS;
	bStatusGetConfig = FALSE;
}
/*
 * This is called from a periodic CheckStatus, 0x02.  The present
 * eStatusCommand is sent out.  After the command is sent, the next
 * command, eStatusCommand, is set up for the next time this function 
 * is called.  i.e. The next eStatusCommand is a function of the present
 * command
 */
int CheckStatus(struct element* pElement)
{
	WORD alarmLevel;
	#ifdef PACKET_PRINT
	printf("CheckStatus "); outputCommand( eStatusCommand ); printf("\n");
	#endif   	
	sendCommand(eStatusCommand, 0);
	if(bStatusGetConfig)
	{
		switch(eStatusCommand)
		{
			case eCOMMAND_GET_MODEL_TYPE:
				eStatusCommand = eCOMMAND_STATUS_SIGN_DISPLAY;
				break;
			case eCOMMAND_STATUS_SIGN_DISPLAY:
				eStatusCommand = eCOMMAND_STATUS_DRIVER_BOARD_SOFTWARE_VERSION;
				break;	
			case eCOMMAND_STATUS_DRIVER_BOARD_SOFTWARE_VERSION:
				eStatusCommand = eCOMMAND_GET_ACTUATOR_TYPE;
				break;					
			case eCOMMAND_GET_ACTUATOR_TYPE:
				eStatusCommand = eCOMMAND_GET_DISALLOWED_PATTERNS;
				break;
			case eCOMMAND_GET_DISALLOWED_PATTERNS:
				eStatusCommand = eCOMMAND_GET_BRIGHTNESS_CONTROL;
				break;
			case eCOMMAND_GET_BRIGHTNESS_CONTROL:
				eStatusCommand = eCOMMAND_GET_ACTUATOR_BUTTON_MODE;
				break;
			case eCOMMAND_GET_ACTUATOR_BUTTON_MODE:
            eStatusCommand = eCOMMAND_STATUS_LINE_CURRENT; //jgr added
				break;
         case eCOMMAND_GET_RSSI_VALUE:
            eStatusCommand = eCOMMAND_STATUS_LINE_CURRENT;
				break;
         case eCOMMAND_STATUS_LINE_CURRENT:  // jgr added
            eStatusCommand = eCOMMAND_GET_ACTUATOR_CALIBRATION;
				break;
         case eCOMMAND_GET_ACTUATOR_CALIBRATION:
         default:
				eStatusCommand = eCOMMAND_STATUS_ALARMS;
				SetStatusToGetStatus(); // set bStatusGetConfig = FALSE
				break;
		}

	}
	else
	{
		switch(eStatusCommand)
		{
			default:
			case eCOMMAND_STATUS_ALARMS:
				eStatusCommand = eCOMMAND_STATUS_BATTERY_VOLTAGE;
				break;
			case eCOMMAND_STATUS_BATTERY_VOLTAGE:
				eStatusCommand = eCOMMAND_STATUS_SIGN_DISPLAY;
				break;
			case eCOMMAND_STATUS_SIGN_DISPLAY:
				eStatusCommand = eCOMMAND_STATUS_ACTUATOR_LIMIT;
				break;
			case eCOMMAND_STATUS_ACTUATOR_LIMIT:
				eStatusCommand = eCOMMAND_STATUS_LINE_CURRENT;
				break;
         case eCOMMAND_STATUS_LINE_CURRENT:   // jgr
            eStatusCommand = eCOMMAND_STATUS_TEMPERATURE;
				break;
			case eCOMMAND_STATUS_TEMPERATURE:
				eStatusCommand = eCOMMAND_STATUS_ALARMS;
				break;
		}

		alarmLevel = getAlarms();
	
		if(alarmLevel == ALARM_LEVEL_HIGH)
		{
			if(flashHigh)
			{
				uiDisplayElement(pElement, ALARM_LEVEL_HIGH);
				flashHigh = FALSE;
			}
			else
			{
				uiDisplayElement(pElement, ALARM_LEVEL_NONE);
				flashHigh = TRUE;
			}	
		}
		else if(alarmLevel == ALARM_LEVEL_MEDIUM)
		{
			if(flashHigh)
			{
				uiDisplayElement(pElement, ALARM_LEVEL_MEDIUM);
				flashHigh = FALSE;
			}
			else
			{
				uiDisplayElement(pElement, ALARM_LEVEL_NONE);
				flashHigh = TRUE;
			}	
		}
		else
		{
			uiDisplayElement(pElement, alarmLevel);		
		}
	}
	if(bStatusGetConfig)
	{
		return 500;
	}
	else
	{
		return 1000;
	}
}

int CheckStatusVoltageScreen(struct element* pElement)
{
    static BYTE statusCmd = (BYTE)eCOMMAND_STATUS_BATTERY_VOLTAGE; 
    int index;

	
    sendCommand((eCOMMANDS)statusCmd, 0);
	
    if(statusCmd == eCOMMAND_STATUS_BATTERY_VOLTAGE)
    {
        statusCmd = eCOMMAND_STATUS_ALARMS;
    }
    else
    {
        statusCmd = eCOMMAND_STATUS_BATTERY_VOLTAGE;
    }
            
    index = WhichImageAlarmState(pElement);
    uiDisplayElement(pElement, index);
        
    return 1000;
}

int CheckStatusConfigScreen(struct element* pElement)
{	
	static BYTE statusCmd = (BYTE)eCOMMAND_STATUS_DRIVER_BOARD_SOFTWARE_VERSION;
	int index;
    

	sendCommand((eCOMMANDS)statusCmd, 0);
    
	if(statusCmd == eCOMMAND_STATUS_DRIVER_BOARD_SOFTWARE_VERSION)
  {
    statusCmd = eCOMMAND_GET_MODEL_TYPE;
  }
  else if(statusCmd == eCOMMAND_GET_MODEL_TYPE)
  {
    statusCmd = eCOMMAND_GET_ACTUATOR_TYPE;
  }
  else if(statusCmd == eCOMMAND_GET_ACTUATOR_TYPE)
  {
    statusCmd = eCOMMAND_STATUS_BATTERY_VOLTAGE;
  }
  else if(statusCmd == eCOMMAND_STATUS_BATTERY_VOLTAGE)
  {
    statusCmd = eCOMMAND_STATUS_ALARMS;
  }
  else
  {
    statusCmd = eCOMMAND_STATUS_DRIVER_BOARD_SOFTWARE_VERSION;
  }
	
  index = WhichImageAlarmState(pElement);
  uiDisplayElement(pElement, index);
        
  return 1000;
}


//=================================================
// periodic command, display current pattern
//=================================================
static eDISPLAY_TYPES eCurrentDisplayType = eDISPLAY_TYPE_BLANK;
BOOL bResetDisplayType = TRUE;

void resetPatternImages()
{
		bResetDisplayType = TRUE;
}
void setCurrentDisplayType(eDISPLAY_TYPES eType)
{
	//printf("1-setCurrentDisplayType eType[%d] eCurrentDisplayType[%d]\n", eType, eCurrentDisplayType);
	if(eCurrentDisplayType != eType)
	{
			bResetDisplayType = TRUE;
	}
	eCurrentDisplayType = eType;
}

eDISPLAY_TYPES getCurrentDisplayType()
{
	return eCurrentDisplayType;
}
int ExitPageAfterDelay(struct element* pElement)
{
	//printf("ExitPageAfterDelay\n");

	/////
	// don't want to get back here
	// in case we don't pop the page
	// (as in simple UI)
	//
	pElement->pPeriodicFunction = NULL;
	
	/////
	// pop the current page
	// load the previous page
	/////

	uiDriverPopPage(TRUE);
	return 10000;
}

int DisplayCurrentPattern(struct element* pElement)
{
	eMODEL eModel = uiDriverGetModel();

	#ifdef DISPLAY_PRINT
	printf("DisplayCurrentPattern 0x%X\n",eModel);
	#endif  
 	
	if(eMODEL_NONE != eModel &&
				eMODEL_TRAILER_UPDATING != eModel)
	{
		eDISPLAY_TYPES eLocalDisplayType = eCurrentDisplayType;
		switch(eModel)
		{
			case eMODEL_VEHICLE_25_LIGHT_SEQUENTIAL:
			case eMODEL_TRAILER_25_LIGHT_SEQUENTIAL:
			case eMODEL_VEHICLE_15_LIGHT_SEQUENTIAL:
			case eMODEL_TRAILER_15_LIGHT_SEQUENTIAL:
			case eMODEL_VEHICLE_15_LIGHT_FLASHING:
			case eMODEL_TRAILER_15_LIGHT_FLASHING:
			default:
					break;
			case eMODEL_TRAILER_15_LIGHT_WIG_WAG:
			case eMODEL_VEHICLE_15_LIGHT_WIG_WAG:
				if(eDISPLAY_TYPE_FOUR_CORNER == eLocalDisplayType)
				{
					eLocalDisplayType = eDISPLAY_TYPE_WIG_WAG;
				}
				break;
		}
		//printf("Periodically: DisplayCurrentPattern\n");
		if(bResetDisplayType)
		{
			/////
			// display type has changed out from under us
			// get the correct count and list of image IDS
			/////
			pElement->nCurrentImageIndex = 0;
			pElement->nGraphics = uiDriverGetDisplayPatternIDS(eLocalDisplayType, pElement->graphicIDS);
			bResetDisplayType = FALSE;
		}
		/////
		// show the graphic
		/////	
		uiDisplayElement(pElement, pElement->nCurrentImageIndex++);
		if(pElement->nCurrentImageIndex >= pElement->nGraphics)
		{
			pElement->nCurrentImageIndex = 0;
		}
	}
	/////
	// this is the preferred time delay
	// for the next timeout
	/////
	return DISPLAY_TIME_MS/pElement->nGraphics;
}

//=================================================
// image generation functions
// return value is Image ID or 0 for image was generated
//=================================================
int ShowVersionText(struct element* pElement)
{
	//printf("ShowVersionText\n");
	printf("eELEMENT_TYPE[%d]\n",pElement->eType);
	printf("nLeft[%d]\n",pElement->nLeft);
	printf("nTop[%d]\n",pElement->nTop);
	printf("nWidth[%d]\n",pElement->nWidth);
	printf("nHeight[%d]\n",pElement->nHeight);
	LCD_Draw_Rectangle(pElement->nLeft,pElement->nTop,pElement->nWidth,pElement->nHeight,color_black);
	LCD_Text_Foreground_Color1(color_red);
	LCD_Write_String("ShowVersionText",pElement->nLeft+10,pElement->nTop+10, LAYER_ONE);
	
	return 0;
}
//=================================================
// periodic command, check to see if we are connected
//=================================================

#ifdef ORIG
int CheckConnected(struct element* pElement)
{
	static BOOL bIsDisplayNotConnected = FALSE;
	int nGraphicIndex = BUTTON_DISABLED_INDEX;
	int alarmMask;

	#ifdef DISPLAY_PRINT
	printf("CheckConnected\n");
	#endif
	
	if( devicesAreConnected( ) )
	{
		nGraphicIndex = BUTTON_ENABLED_INDEX;
		/////
		// DEH 121216-1
		// Low Voltage Test
		///// MJB

		alarmMask = getAlarmBitmap();

		if(alarmMask & ALARM_BITMAP_LAMPS_DISABLED)
		{
			  // "LOW VOLTAGE\rLAMPS DISABLED"
			 	nGraphicIndex = 4;
		}
    else if((alarmMask & ALARM_BITMAP_LOW_LINE_VOLTAGE) && 
								(alarmMask & ALARM_BITMAP_LOW_LINE_VOLTAGE))
		{
			  // "LOW VOLTAGE"
						nGraphicIndex = 3;
		}
  }
	else  // not connected
	{
		if(TRUE == bIsDisplayNotConnected)
		{
			// "CHECK\rARROW BOARD\rPOWER"
			nGraphicIndex = 5;
			bIsDisplayNotConnected = FALSE;
		}
		else
		{
			nGraphicIndex = BUTTON_DISABLED_INDEX;
			bIsDisplayNotConnected = TRUE;
		}
	}
	
	uiDisplayElement(pElement, nGraphicIndex);
	return 2000;
}

#endif

int CheckConnected(struct element* pElement)
{
#define PATTERNS_ENABLED_INDEX  0
#define INDEX_CONNECTING        1
#define INDEX_LOW_VOLTAGE       3
#define INDEX_LAMPS_DISABLED    4
#define INDEX_CHECK_POWER       5
#define INDEX_NOT_CONNECTED     6
    
#define PERIODIC_CHECK_MSEC     500
#define INITIAL_CONNECT_TICKS   16 // 16 x 500mSec = 8 seconds
#define TOGGLE_ERROR_MSG_TICKS  4  // switch between error messages every 2 seconds
      
    static BOOL bHasInitialConnection = FALSE;
    static int  nGraphicIndex = INDEX_CONNECTING;
    static BYTE stateCounter = 0;
    WORD alarmMask;

    if(bHasInitialConnection == FALSE)
    {
        if(devicesAreConnected( ) == TRUE)
        {
            bHasInitialConnection = TRUE;
        }
        else
        {
            if(stateCounter < INITIAL_CONNECT_TICKS)
            {
                stateCounter++;
            }
            else
            {
                // failed to connect on power-up, exit intial connection 
                // state and show error messages
                bHasInitialConnection = TRUE;
            }
        }
    }

    if(bHasInitialConnection == TRUE)
    {
        if(devicesAreConnected( ))
        {
            nGraphicIndex = PATTERNS_ENABLED_INDEX;
        
            alarmMask = getAlarmBitmap();
            if(alarmMask & ALARM_BITMAP_LAMPS_DISABLED)
            {
                nGraphicIndex = INDEX_LAMPS_DISABLED;
            }
            else if(alarmMask & ALARM_BITMAP_LOW_BATTERY)
            {
                nGraphicIndex = INDEX_LOW_VOLTAGE;
            }
        }    
        else
        {
            if(++stateCounter > 4)
            {
                stateCounter = 0;
                nGraphicIndex = (nGraphicIndex == INDEX_NOT_CONNECTED) ? INDEX_CHECK_POWER : INDEX_NOT_CONNECTED;
            }
        }
    }        
        
    uiDisplayElement(pElement, nGraphicIndex);
    return PERIODIC_CHECK_MSEC;
}


//=================================================
// periodic command, check to see if we are connected
// and if the tilt option is installed
//=================================================
int CheckConnectedAndTilt(struct element* pElement)
{
	int nGraphicIndex = BUTTON_DISABLED_INDEX;

	#ifdef DEBUG_MENU_CMDS
	printf("CheckConnectedAndTilt\n");
	#endif   
	
	if( devicesAreConnected() )
	{
		if(eACTUATOR_TYPE_NONE != eActuatorType &&
									eACTUATOR_TYPE_UKNOWN != eActuatorType)
		{
			nGraphicIndex = BUTTON_ENABLED_INDEX;
			if(NULL != pElement->pWhichImageFunction)
			{
				nGraphicIndex = pElement->pWhichImageFunction(pElement);
			}	
		}
	}

	uiDisplayElement(pElement, nGraphicIndex);
	return 2000;
}
//=================================================
// periodic command, check to see if we are paired
//=================================================
int CheckPaired(struct element* theElement)
{
	#ifdef DEBUG_MENU_CMDS
	printf("CheckPaired\n");
	#endif
	
	if(IsEnabledWiredConnection(theElement))
	{
		printf("1-CheckPaired[%d]\n", devicesAreConnected()); 
		if(devicesAreConnected())
		{
			//printf("2-CheckPaired[%d]\n", devicesAreConnected());	
			uiDisplayElement(theElement, BUTTON_SELECTED_INDEX);
		}
		else
		{
			//printf("3-CheckPaired[%d]\n", devicesAreConnected());	
			uiDisplayElement(theElement, BUTTON_ENABLED_INDEX);
		}
		/////
		// send a command
		// so the paired bit has a chance to be processed
		/////
		sendCommand(eCOMMAND_GET_MODEL_TYPE, 0);
	}
	return 1000;
}
//=======================================================
// periodic command to check Arrow Board Model Type
//=======================================================
int CheckStatusArrowBoardType(struct element* pElement)
{
	int nImageIndex;
	#ifdef PACKET_PRINT
	printf("CheckStatusArrowBoardType 0x%X\n",uiDriverGetModel());
	#endif
	
	switch(uiDriverGetModel())
	{
		case eMODEL_VEHICLE_25_LIGHT_SEQUENTIAL:
		case eMODEL_TRAILER_25_LIGHT_SEQUENTIAL:
			nImageIndex = ARROW_BOARD_TYPE_25_LIGHT_INDEX;
			break;
	
	//////////////////////////////////////////
	//// Solar Trailer
	//////////////////////////////////////////
		case eMODEL_VEHICLE_15_LIGHT_SEQUENTIAL:
		case eMODEL_VEHICLE_15_LIGHT_FLASHING:
		case eMODEL_VEHICLE_15_LIGHT_WIG_WAG:
		case eMODEL_TRAILER_15_LIGHT_SEQUENTIAL:
		case eMODEL_TRAILER_15_LIGHT_FLASHING:
		case eMODEL_TRAILER_15_LIGHT_WIG_WAG:
			nImageIndex = ARROW_BOARD_TYPE_15_LIGHT_INDEX;
			break;
		
		case eMODEL_TRAILER_UPDATING:
		case eMODEL_NONE:
		default:
			nImageIndex = ARROW_BOARD_STYLE_UPDATING_INDEX;
			break;
	}
	
	uiDisplayElement(pElement, nImageIndex);
	return 1000;
	
}
//=======================================================
// periodic command to check Arrow Board Model Type
//=======================================================
int CheckStatusStyleType(struct element* pElement)
{
	int nImageIndex;

	#ifdef DEBUG_MENU_CMDS
	printf("CheckStatusStyleType 0x%X\n",uiDriverGetModel());
	#endif	
	
	switch(uiDriverGetModel())
	{
		case eMODEL_VEHICLE_25_LIGHT_SEQUENTIAL:
		case eMODEL_VEHICLE_15_LIGHT_SEQUENTIAL:
		case eMODEL_VEHICLE_15_LIGHT_FLASHING:
		case eMODEL_VEHICLE_15_LIGHT_WIG_WAG:
			nImageIndex = ARROW_BOARD_STYLE_VEHICLE_INDEX;
			break;
	
	//////////////////////////////////////////
	//// Solar Trailer
	//////////////////////////////////////////
		case eMODEL_TRAILER_25_LIGHT_SEQUENTIAL:
		case eMODEL_TRAILER_15_LIGHT_SEQUENTIAL:
		case eMODEL_TRAILER_15_LIGHT_FLASHING:
		case eMODEL_TRAILER_15_LIGHT_WIG_WAG:
			nImageIndex = ARROW_BOARD_STYLE_TRAILER_INDEX;
			break;
		
		case eMODEL_NONE:
		case eMODEL_TRAILER_UPDATING:
		default:
			nImageIndex = ARROW_BOARD_STYLE_UPDATING_INDEX;
			break;
	}
	uiDisplayElement(pElement, nImageIndex);
	return 1000;
}
//=======================================================
// periodic command to check electric actuator Type
//============================================================
int CheckStatusElectricActuatorType(struct element* pElement)
{
  int nImageIndex = WhichImageStatusElectricActuatorButton(pElement);
	
	#ifdef DEBUG_MENU_CMDS
	printf("CheckStatusElectricActuatorType\n");
	#endif
	
    uiDisplayElement(pElement, nImageIndex);    
    sendCommand(eCOMMAND_GET_ACTUATOR_BUTTON_MODE, 0);
    
	return 1000;
}

//=================================================
// periodic function in single page main screen
//==================================================
int CheckSelectedPatternButton(struct element* theElement)
{
	int nIndex =  IsSelectedPatternButton(theElement);
        
    uiDisplayElement(theElement, nIndex);
    
    return 1000;
}
//=================================================
// periodic function for radio signal strength
//==================================================
int checkRssiDigit1( struct element* theElement )
{
  int nIndex = WhichImageRssiDigit1(theElement);

	uiDisplayElement(theElement, nIndex);

 	return 1000;   
}   
int checkRssiDigit2( struct element* theElement )
{
  int nIndex = WhichImageRssiDigit2(theElement);
  
  uiDisplayElement(theElement, nIndex);
  
 	return 1000;
}   
//=================================================
// periodic command, check battery voltage, update image
//=================================================
int CheckBatteryVoltage(struct element* theElement)
{
	int nIndex = WhichImageBatteryVoltage(theElement);
	#ifdef PACKET_PRINT
	printf("CheckBatteryVoltage\n");
	#endif
	uiDisplayElement(theElement, nIndex);
	return 500;
}

//=================================================
// periodic command, check line current, update image
//=================================================
int CheckLineCurrentTensDigit(struct element* theElement)
{
	int nIndex = WhichImageLineCurrentTensDigit(theElement);
	uiDisplayElement(theElement, nIndex);
	return 500;
}

int CheckLineCurrentOnesDigit(struct element* theElement)
{
	int nIndex = WhichImageLineCurrentOnesDigit(theElement);
	uiDisplayElement(theElement, nIndex);
	return 500;
}
int CheckLineCurrentTenthsDigit(struct element* theElement)
{
	int nIndex = WhichImageLineCurrentTenthsDigit(theElement);
	uiDisplayElement(theElement, nIndex);
	return 500;
}

//====================================================
// periodic functions to update driver board
// software version
//====================================================

int checkImageDriverVersion1(struct element* theElement)
{
	int nIndex = WhichImageDriverVersionDigit1(theElement);
	uiDisplayElement(theElement, nIndex);
	return 500;
}

int checkImageDriverVersion2(struct element* theElement)
{
	int nIndex = WhichImageDriverVersionDigit2(theElement);
	uiDisplayElement(theElement, nIndex);
	return 500;
}

int checkImageDriverVersion3(struct element* theElement)
{
	int nIndex = WhichImageDriverVersionDigit3(theElement);
	uiDisplayElement(theElement, nIndex);
	return 500;
}

int checkImageDriverVersion4(struct element* theElement)
{
	int nIndex = WhichImageDriverVersionDigit4(theElement);
	uiDisplayElement(theElement, nIndex);
	return 500;
}

int checkImageDriverVersion5(struct element* theElement)
{
	int nIndex = WhichImageDriverVersionDigit5(theElement);
	uiDisplayElement(theElement, nIndex);
	return 500;
}


//=================================================
// periodic command, check system current, update image
//=================================================
int CheckSystemCurrentTensDigit(struct element* theElement)
{
	int nIndex = WhichImageSystemCurrentTensDigit(theElement);
	uiDisplayElement(theElement, nIndex);
	return 500;
}
int CheckSystemCurrentOnesDigit(struct element* theElement)
{
	int nIndex = WhichImageSystemCurrentOnesDigit(theElement);
	uiDisplayElement(theElement, nIndex);
	return 500;
}
int CheckSystemCurrentTenthsDigit(struct element* theElement)
{
	int nIndex = WhichImageSystemCurrentTenthsDigit(theElement);
	uiDisplayElement(theElement, nIndex);
	return 500;
} 

//=================================================
// periodic command, check system voltage, update image
//=================================================
int CheckSystemVoltageTensDigit(struct element* theElement)
{
	int nIndex = WhichSystemVoltageTensDigit(theElement);
	uiDisplayElement(theElement, nIndex);
	return 500;   
}

int CheckSystemVoltageOnesDigit(struct element* theElement)
{
   	int nIndex = WhichSystemVoltageOnesDigit(theElement);
	uiDisplayElement(theElement, nIndex);
	return 500;    
}

int CheckSystemVoltageTenthsDigit(struct element* theElement)
{
   	int nIndex = WhichSystemVoltageTenthsDigit(theElement);
	uiDisplayElement(theElement, nIndex);
	return 500;     
}

int CheckImageAlarmState(struct element* theElement)
{
   	int nIndex = WhichImageAlarmState(theElement);
	uiDisplayElement(theElement, nIndex);
	return 500;     
}    

int CheckImageVoltageState(struct element* theElement)
{
   	int nIndex = WhichImageVoltageState(theElement);
	uiDisplayElement(theElement, nIndex);
	return 500;     
}

int CheckImageSystemState(struct element* theElement)
{
	int index = WhichImageSystemState(theElement);

	uiDisplayElement(theElement, index);
	return 500;     
}  

int CheckImageAlarmLevelConnLowBatt(struct element* theElement)
{
	int index = WhichImageAlarmLevelConnLowBatt(theElement);   

	uiDisplayElement(theElement, index);
	return 500;     
} 

int CheckImageAlarmLevelLowVoltage(struct element* theElement)
{
	int index = WhichImageAlarmLevelLVDLowVolt(theElement);

	uiDisplayElement(theElement, index);
	return 500;     
}

int CheckImageAlarmLevelLampsDisabled(struct element* theElement)
{
	int index = WhichImageAlarmLevelBattRevDispError(theElement);

	uiDisplayElement(theElement, index);
	return 500;     
}

int CheckImageAlarmLevelTempHigh(struct element* theElement)
{
	int index = WhichImageAlarmLevelLineRevIndError(theElement);

	uiDisplayElement(theElement, index);
	return 500;     
}


//=================================================
// periodic command 90 degree actuator, move actuator up
//=================================================
int MoveActuatorUp(struct element* pElement)
{

	/////
	// return value is milliseconds
	// until we are called again
	/////
	int nRetVal = 500;
	int nImageIndex = BUTTON_ENABLED_INDEX;
	
	switch(eActuatorButtonMode)
	{
		case eACTUATOR_BUTTON_AUTO_MODE:
		{		
			nImageIndex = BUTTON_ENABLED_INDEX+ACTUATOR_AUTO_INDEX_OFFSET;
			if(eACTUATOR_COMMAND_MOVE_UP == eLastActuatorCommand)
			{
				unsigned short nActuatorStatus = getActuatorStatus();
		
				switch(getActuatorStatus())
				{
					case eACTUATOR_STATE_IDLE:
					case eACTUATOR_STATE_MOVING_UP:
					case eACTUATOR_STATE_MOVING_DOWN:
					case eACTUATOR_STATE_STALLED_MOVING_DOWN:
						/////
						// actuator doesn't say it is stalled
						// so send another command to make it move up
						/////
						if(eACTUATOR_LIMIT_TOP != getActuatorLimit())
						{
							if(isTimerExpired(&actuatorLimitTimer))
							{
								/////
								// we should be at the limit by now
								/////
								sendCommand(eCOMMAND_ACTUATOR_SWITCH,eACTUATOR_COMMAND_STOP);
								eLastActuatorCommand = eACTUATOR_COMMAND_STOP;
							}
							else
							{
								/////
								// still moving, so send the next move command
								/////
								sendCommand(eCOMMAND_ACTUATOR_SWITCH,eACTUATOR_COMMAND_MOVE_UP);
								eLastActuatorCommand = eACTUATOR_COMMAND_MOVE_UP;
								nImageIndex = BUTTON_SELECTED_INDEX+ACTUATOR_AUTO_INDEX_OFFSET;
							}
						}
						else
						{
							nImageIndex = BUTTON_DISABLED_INDEX+ACTUATOR_AUTO_INDEX_OFFSET;
							eLastActuatorCommand = eACTUATOR_COMMAND_STOP;
						}
						break;
					case eACTUATOR_STATE_STALLED_MOVING_UP:
						/////
						// actuator is stalled
						// so stop it
						/////
						sendCommand(eCOMMAND_ACTUATOR_SWITCH,eACTUATOR_COMMAND_STOP);
						eLastActuatorCommand = eACTUATOR_COMMAND_STOP;
						switch(getActuatorLimit())
						{
							case eACTUATOR_LIMIT_BOTTOM:
								nImageIndex = BUTTON_ENABLED_INDEX+ACTUATOR_AUTO_INDEX_OFFSET;
								break;
							case eACTUATOR_LIMIT_TOP:
								nImageIndex = BUTTON_DISABLED_INDEX+ACTUATOR_AUTO_INDEX_OFFSET;
								break;
							case eACTUATOR_LIMIT_NONE:
							case eACTUATOR_LIMIT_ERROR:		
								default:
								nImageIndex = BUTTON_ENABLED_INDEX+ACTUATOR_AUTO_INDEX_OFFSET;
								break;
						}
						break;
				}
			}
			else if(eACTUATOR_COMMAND_MOVE_DOWN == eLastActuatorCommand)
			{
				/////
				// moving down
				// so leave this button disabled
				/////
                if(SCREEN_MODE_SINGLE == storedConfigGetScreenMode())
                {
                    nImageIndex =  ACTUATOR_AUTO_STOP_ACTIVE_INDEX;
                }
                else
                {
                    nImageIndex = BUTTON_DISABLED_INDEX+ACTUATOR_AUTO_INDEX_OFFSET;
                }
			}
			else
			{
				/////
				// must be stopped
				// so set the button status
				/////
				switch(getActuatorLimit())
				{
					case eACTUATOR_LIMIT_BOTTOM:
						nImageIndex = BUTTON_ENABLED_INDEX+ACTUATOR_AUTO_INDEX_OFFSET;
						break;
					case eACTUATOR_LIMIT_TOP:
						nImageIndex = BUTTON_DISABLED_INDEX+ACTUATOR_AUTO_INDEX_OFFSET;
						break;
					case eACTUATOR_LIMIT_NONE:
					case eACTUATOR_LIMIT_ERROR:		
					default:
						nImageIndex = BUTTON_ENABLED_INDEX+ACTUATOR_AUTO_INDEX_OFFSET;
						break;
				}
			}
			
			uiDisplayElement(pElement, nImageIndex);
		}
			break;
		case eACTUATOR_BUTTON_MANUAL_MODE:
			//sendCommand(eCOMMAND_ACTUATOR_SWITCH,eACTUATOR_COMMAND_MOVE_UP);
			//eLastActuatorCommand = eACTUATOR_COMMAND_MOVE_UP;
			//nImageIndex = BUTTON_SELECTED_INDEX;
			break;
	}
	return nRetVal;
}

//=================================================
// periodic command 90 degree actuator move actuator down
//=================================================
int MoveActuatorDown(struct element* pElement)
{
	/////
	// return value is milliseconds
	// until we are called again
	/////
	int nRetVal = 500;
	int nImageIndex = BUTTON_ENABLED_INDEX;

	switch(eActuatorButtonMode)
	{
		case eACTUATOR_BUTTON_AUTO_MODE:
		{	
			nImageIndex = BUTTON_ENABLED_INDEX+ACTUATOR_AUTO_INDEX_OFFSET;
			if(eACTUATOR_COMMAND_MOVE_DOWN == eLastActuatorCommand)
			{
				unsigned short nActuatorStatus = getActuatorStatus();
				/////
				// ask for the actuator status
				// ask for the limit switch status
				// this data should be available soon
				/////
				switch(getActuatorStatus())
				{
					case eACTUATOR_STATE_IDLE:
					case eACTUATOR_STATE_MOVING_UP:
					case eACTUATOR_STATE_MOVING_DOWN:
					case eACTUATOR_STATE_STALLED_MOVING_UP:
						/////
						// actuator doesn't say it is stalled
						// so send another command to make it move down
						/////
						if(eACTUATOR_LIMIT_BOTTOM != getActuatorLimit())
						{
							if(isTimerExpired(&actuatorLimitTimer))
							{
								/////
								// we should be at the limit by now
								/////
								sendCommand(eCOMMAND_ACTUATOR_SWITCH,eACTUATOR_COMMAND_STOP);
								eLastActuatorCommand = eACTUATOR_COMMAND_STOP;
							}
							else
							{
								/////
								// still moving, so send the next move command
								/////
								sendCommand(eCOMMAND_ACTUATOR_SWITCH,eACTUATOR_COMMAND_MOVE_DOWN);
								eLastActuatorCommand = eACTUATOR_COMMAND_MOVE_DOWN;
								nImageIndex = BUTTON_SELECTED_INDEX+ACTUATOR_AUTO_INDEX_OFFSET;
							}
						}
						else
						{
							nImageIndex = BUTTON_DISABLED_INDEX+ACTUATOR_AUTO_INDEX_OFFSET;
							eLastActuatorCommand = eACTUATOR_COMMAND_STOP;
						}
						break;
					case eACTUATOR_STATE_STALLED_MOVING_DOWN:
						/////
						// actuator is stalled
						// so stop it
						/////
						sendCommand(eCOMMAND_ACTUATOR_SWITCH,eACTUATOR_COMMAND_STOP);
						eLastActuatorCommand = eACTUATOR_COMMAND_STOP;
						switch(getActuatorLimit())
						{
							case eACTUATOR_LIMIT_BOTTOM:
								nImageIndex = BUTTON_ENABLED_INDEX+ACTUATOR_AUTO_INDEX_OFFSET;
								break;
							case eACTUATOR_LIMIT_TOP:
								nImageIndex = BUTTON_DISABLED_INDEX+ACTUATOR_AUTO_INDEX_OFFSET;
								break;
							case eACTUATOR_LIMIT_NONE:
							case eACTUATOR_LIMIT_ERROR:		
								default:
								nImageIndex = BUTTON_ENABLED_INDEX+ACTUATOR_AUTO_INDEX_OFFSET;
								break;
						}
						break;
				}
			}
			else if(eACTUATOR_COMMAND_MOVE_UP == eLastActuatorCommand)
			{
				/////
				// moving up
				// so leave this button disabled
				/////
                if(SCREEN_MODE_SINGLE == storedConfigGetScreenMode())
                {
                    nImageIndex =  ACTUATOR_AUTO_STOP_ACTIVE_INDEX;
                }
                else
                {
                    nImageIndex = BUTTON_DISABLED_INDEX+ACTUATOR_AUTO_INDEX_OFFSET;
                }
			}
			else
			{
				/////
				// must be stopped
				// so set the button status
				/////
				switch(getActuatorLimit())
				{
					case eACTUATOR_LIMIT_BOTTOM:
						nImageIndex = BUTTON_DISABLED_INDEX+ACTUATOR_AUTO_INDEX_OFFSET;
						break;
					case eACTUATOR_LIMIT_TOP:
						nImageIndex = BUTTON_ENABLED_INDEX+ACTUATOR_AUTO_INDEX_OFFSET;
						break;
					case eACTUATOR_LIMIT_NONE:
					case eACTUATOR_LIMIT_ERROR:		
					default:
						nImageIndex = BUTTON_ENABLED_INDEX+ACTUATOR_AUTO_INDEX_OFFSET;
						break;
				}
			}
			
			uiDisplayElement(pElement, nImageIndex);
		}
		break;
		case eACTUATOR_BUTTON_MANUAL_MODE:
			//sendCommand(eCOMMAND_ACTUATOR_SWITCH,eACTUATOR_COMMAND_MOVE_DOWN);
			//eLastActuatorCommand = eACTUATOR_COMMAND_MOVE_DOWN;
			//nImageIndex = BUTTON_SELECTED_INDEX;
			break;
	}
	return nRetVal;
}
//=================================================
// periodic command 180 degree actuator, move actuator left
//=================================================
int MoveActuatorRight(struct element* pElement)
{
	/////
	// return value is milliseconds
	// until we are called again
	/////
	int nRetVal = 500;
	int nImageIndex = BUTTON_ENABLED_INDEX;
	switch(eActuatorButtonMode)
	{
		case eACTUATOR_BUTTON_AUTO_MODE:	
		{	
			nImageIndex = BUTTON_ENABLED_INDEX+ACTUATOR_AUTO_INDEX_OFFSET;
			if((eACTUATOR_COMMAND_MOVE_RIGHT == eLastActuatorCommand) && !bActuatorHomeInProgress)
			{
				unsigned short nActuatorStatus = getActuatorStatus();
				switch(getActuatorStatus())
				{
					case eACTUATOR_STATE_IDLE:
					case eACTUATOR_STATE_MOVING_RIGHT:
					case eACTUATOR_STATE_MOVING_LEFT:
					case eACTUATOR_STATE_STALLED_MOVING_LEFT:
						/////
						// actuator doesn't say it is stalled
						// so send another command to make it move right
						/////
						if(eACTUATOR_LIMIT_RIGHT != getActuatorLimit())
						{
							sendCommand(eCOMMAND_ACTUATOR_SWITCH,eACTUATOR_COMMAND_MOVE_RIGHT);
							eLastActuatorCommand = eACTUATOR_COMMAND_MOVE_RIGHT;
							nImageIndex = BUTTON_SELECTED_INDEX+ACTUATOR_AUTO_INDEX_OFFSET;
						}
						else
						{
							nImageIndex = BUTTON_DISABLED_INDEX+ACTUATOR_AUTO_INDEX_OFFSET;
							eLastActuatorCommand = eACTUATOR_COMMAND_STOP;
						}
						break;
					case eACTUATOR_STATE_STALLED_MOVING_RIGHT:
						/////
						// actuator is stalled
						// so stop it
						/////
						sendCommand(eCOMMAND_ACTUATOR_SWITCH,eACTUATOR_COMMAND_STOP);
						eLastActuatorCommand = eACTUATOR_COMMAND_STOP;
						switch(getActuatorLimit())
						{
							case eACTUATOR_LIMIT_LEFT:
								nImageIndex = BUTTON_ENABLED_INDEX+ACTUATOR_AUTO_INDEX_OFFSET;
								break;
							case eACTUATOR_LIMIT_RIGHT:
								nImageIndex = BUTTON_DISABLED_INDEX+ACTUATOR_AUTO_INDEX_OFFSET;
								break;
							case eACTUATOR_LIMIT_NONE:
							case eACTUATOR_LIMIT_ERROR:		
								default:
								nImageIndex = BUTTON_ENABLED_INDEX+ACTUATOR_AUTO_INDEX_OFFSET;
								break;
						}
						break;
				}
			}
			else if(eACTUATOR_COMMAND_MOVE_LEFT == eLastActuatorCommand)
			{
				/////
				// moving left
				// so leave this button disabled
				/////
				nImageIndex = BUTTON_DISABLED_INDEX+ACTUATOR_AUTO_INDEX_OFFSET;
			}
			else
			{
				switch(getActuatorLimit())
				{
					case eACTUATOR_LIMIT_LEFT:
						nImageIndex = BUTTON_ENABLED_INDEX+ACTUATOR_AUTO_INDEX_OFFSET;
						break;
					case eACTUATOR_LIMIT_RIGHT:
						nImageIndex = BUTTON_DISABLED_INDEX+ACTUATOR_AUTO_INDEX_OFFSET;
						break;
					case eACTUATOR_LIMIT_NONE:
					case eACTUATOR_LIMIT_ERROR:		
					default:
						nImageIndex = BUTTON_ENABLED_INDEX+ACTUATOR_AUTO_INDEX_OFFSET;
						break;
				}
			}
			
			uiDisplayElement(pElement, nImageIndex);
		}
			break;
		case eACTUATOR_BUTTON_MANUAL_MODE:
			//sendCommand(eCOMMAND_ACTUATOR_SWITCH,eACTUATOR_COMMAND_MOVE_RIGHT);
			//eLastActuatorCommand = eACTUATOR_COMMAND_MOVE_RIGHT;
			//nImageIndex = BUTTON_SELECTED_INDEX;
			break;
	}
	return nRetVal;
}

//=================================================
// periodic command 180 degree actuator move actuator left
//=================================================
int MoveActuatorLeft(struct element* pElement)
{
	/////
	// return value is milliseconds
	// until we are called again
	/////
	int nRetVal = 500;
	int nImageIndex = BUTTON_ENABLED_INDEX;

	switch(eActuatorButtonMode)
	{
		case eACTUATOR_BUTTON_AUTO_MODE:		
		{	
			nImageIndex = BUTTON_ENABLED_INDEX+ACTUATOR_AUTO_INDEX_OFFSET;
			if((eACTUATOR_COMMAND_MOVE_LEFT == eLastActuatorCommand) && !bActuatorHomeInProgress)
			{
				unsigned short nActuatorStatus = getActuatorStatus();
		
				switch(getActuatorStatus())
				{
					case eACTUATOR_STATE_IDLE:
					case eACTUATOR_STATE_MOVING_RIGHT:
					case eACTUATOR_STATE_MOVING_LEFT:
					case eACTUATOR_STATE_STALLED_MOVING_RIGHT:
						/////
						// actuator doesn't say it is stalled
						// so send another command to make it move left
						/////
						if(eACTUATOR_LIMIT_LEFT != getActuatorLimit())
						{
							sendCommand(eCOMMAND_ACTUATOR_SWITCH,eACTUATOR_COMMAND_MOVE_LEFT);
							eLastActuatorCommand = eACTUATOR_COMMAND_MOVE_LEFT;
							nImageIndex = BUTTON_SELECTED_INDEX+ACTUATOR_AUTO_INDEX_OFFSET;
						}
						else
						{
							nImageIndex = BUTTON_DISABLED_INDEX+ACTUATOR_AUTO_INDEX_OFFSET;
							eLastActuatorCommand = eACTUATOR_COMMAND_STOP;
						}
						break;
					case eACTUATOR_STATE_STALLED_MOVING_LEFT:
						/////
						// actuator is stalled
						// so stop it
						/////
						sendCommand(eCOMMAND_ACTUATOR_SWITCH,eACTUATOR_COMMAND_STOP);
						eLastActuatorCommand = eACTUATOR_COMMAND_STOP;
						switch(getActuatorLimit())
						{
							case eACTUATOR_LIMIT_LEFT:
								nImageIndex = BUTTON_ENABLED_INDEX+ACTUATOR_AUTO_INDEX_OFFSET;
								break;
							case eACTUATOR_LIMIT_RIGHT:
								nImageIndex = BUTTON_DISABLED_INDEX+ACTUATOR_AUTO_INDEX_OFFSET;
								break;
							case eACTUATOR_LIMIT_NONE:
							case eACTUATOR_LIMIT_ERROR:		
								default:
								nImageIndex = BUTTON_ENABLED_INDEX+ACTUATOR_AUTO_INDEX_OFFSET;
								break;
						}
						break;
				}
			}
			else if(eACTUATOR_COMMAND_MOVE_RIGHT == eLastActuatorCommand)
			{
				/////
				// moving right
				// so leave this button disabled
				/////
				nImageIndex = BUTTON_DISABLED_INDEX+ACTUATOR_AUTO_INDEX_OFFSET;
			}
			else
			{
				switch(getActuatorLimit())
				{
					case eACTUATOR_LIMIT_LEFT:
						nImageIndex = BUTTON_DISABLED_INDEX+ACTUATOR_AUTO_INDEX_OFFSET;
						break;
					case eACTUATOR_LIMIT_RIGHT:
						nImageIndex = BUTTON_ENABLED_INDEX+ACTUATOR_AUTO_INDEX_OFFSET;
						break;
					case eACTUATOR_LIMIT_NONE:
					case eACTUATOR_LIMIT_ERROR:		
					default:
						nImageIndex = BUTTON_ENABLED_INDEX+ACTUATOR_AUTO_INDEX_OFFSET;
						break;
				}
			}
			
			uiDisplayElement(pElement, nImageIndex);
		}	
		break;
	case eACTUATOR_BUTTON_MANUAL_MODE:	
			//sendCommand(eCOMMAND_ACTUATOR_SWITCH,eACTUATOR_COMMAND_MOVE_LEFT);
			//eLastActuatorCommand = eACTUATOR_COMMAND_MOVE_LEFT;
			//nImageIndex = BUTTON_SELECTED_INDEX;
		break;
	}
	return nRetVal;
}

//=================================================
// periodic command 180 degree actuator move actuator to the home position
//=================================================
int MoveActuatorNeutral(struct element* pElement)
{
	/////
	// return value is milliseconds
	// until we are called again
	/////
	int nRetVal = 500;
	int nImageIndex = BUTTON_ENABLED_INDEX+ACTUATOR_AUTO_INDEX_OFFSET;
	if((eACTUATOR_COMMAND_STOP != eLastActuatorCommand) && bActuatorHomeInProgress)
	{
		if(isTimerExpired(&actuatorLimitTimer))
		{
			/////
			// timer expired
			// if we started from a limit
			// then we should be at the home position now
			//
			// if we didn't start at the limit, 
			// then we should have reached the limit before this
			// so we don't know where we are.
			//
			// either way, stop the actuator
			/////
			sendCommand(eCOMMAND_ACTUATOR_SWITCH,eACTUATOR_COMMAND_STOP);
			eLastActuatorCommand = eACTUATOR_COMMAND_STOP;
			nImageIndex = BUTTON_ENABLED_INDEX+ACTUATOR_AUTO_INDEX_OFFSET;
			bActuatorHomeInProgress = FALSE;
		}
		else
		{
			if(eActuatorButtonMode == eACTUATOR_BUTTON_AUTO_MODE)
			{	
					//printf("0A--[ %d]\n", eLastActuatorCommand);
					if(eACTUATOR_COMMAND_STOP != eLastActuatorCommand)
					{
						unsigned short nActuatorStatus = getActuatorStatus();
						//printf("0B--[ %d]\n", nActuatorStatus);		
						switch(nActuatorStatus)
						{
							case eACTUATOR_STATE_IDLE:
								//printf("0c--[ %d][%d][%d]\n", eLastActuatorCommand,eACTUATOR_COMMAND_MOVE_RIGHT, eACTUATOR_COMMAND_MOVE_LEFT);
								switch(eLastActuatorCommand)
								{
									case eACTUATOR_COMMAND_MOVE_RIGHT:
										if(eACTUATOR_LIMIT_RIGHT != getActuatorLimit())
										{
											sendCommand(eCOMMAND_ACTUATOR_SWITCH,eACTUATOR_COMMAND_MOVE_RIGHT);
											eLastActuatorCommand = eACTUATOR_COMMAND_MOVE_RIGHT;
											nImageIndex = BUTTON_SELECTED_INDEX+ACTUATOR_AUTO_INDEX_OFFSET;
										}
										else
										{
											/////
											// reached the right limit
											// so we must not have been at a limit
											// when we started, so now move left
											// to the home position
											/////
											startTimer(&actuatorLimitTimer, ACTUATOR_HOME_FROM_RIGHT_TIMER_MS); 
											sendCommand(eCOMMAND_ACTUATOR_SWITCH,eACTUATOR_COMMAND_MOVE_LEFT);
											eLastActuatorCommand = eACTUATOR_COMMAND_MOVE_LEFT;
											nImageIndex = BUTTON_SELECTED_INDEX+ACTUATOR_AUTO_INDEX_OFFSET;
										}
										break;
									case eACTUATOR_COMMAND_MOVE_LEFT:
										if(eACTUATOR_LIMIT_LEFT != getActuatorLimit())
										{
											sendCommand(eCOMMAND_ACTUATOR_SWITCH,eACTUATOR_COMMAND_MOVE_LEFT);
											eLastActuatorCommand = eACTUATOR_COMMAND_MOVE_LEFT;
											nImageIndex = BUTTON_SELECTED_INDEX+ACTUATOR_AUTO_INDEX_OFFSET;
										}
										else
										{
											printf("ZZ1\n");
											sendCommand(eCOMMAND_ACTUATOR_SWITCH,eACTUATOR_COMMAND_STOP);
											eLastActuatorCommand = eACTUATOR_COMMAND_STOP;
											bActuatorHomeInProgress = FALSE;
											nImageIndex = BUTTON_ENABLED_INDEX+ACTUATOR_AUTO_INDEX_OFFSET;
										}
										break;
									case eACTUATOR_COMMAND_STOP:
										printf("ZZ2\n");
										bActuatorHomeInProgress = FALSE;
										nImageIndex = BUTTON_ENABLED_INDEX+ACTUATOR_AUTO_INDEX_OFFSET;
									default:
										printf("XX-[%d]\n", eLastActuatorCommand);
										break;
								}
								break;
							case eACTUATOR_STATE_MOVING_RIGHT:
								/////
								// actuator doesn't say it is stalled
								// so send another command to make it move right
								/////
								if(eACTUATOR_LIMIT_RIGHT != getActuatorLimit())
								{
									sendCommand(eCOMMAND_ACTUATOR_SWITCH,eACTUATOR_COMMAND_MOVE_RIGHT);
									eLastActuatorCommand = eACTUATOR_COMMAND_MOVE_RIGHT;
									nImageIndex = BUTTON_SELECTED_INDEX+ACTUATOR_AUTO_INDEX_OFFSET;
								}
								else
								{
									/////
									// reached the right limit
									// so we must not have been at a limit
									// when we started, so now move left
									// to the home position
									/////
									startTimer(&actuatorLimitTimer, ACTUATOR_HOME_FROM_RIGHT_TIMER_MS); 
									sendCommand(eCOMMAND_ACTUATOR_SWITCH,eACTUATOR_COMMAND_MOVE_LEFT);
									eLastActuatorCommand = eACTUATOR_COMMAND_MOVE_LEFT;
									nImageIndex = BUTTON_SELECTED_INDEX+ACTUATOR_AUTO_INDEX_OFFSET;
								}
								break;
							case eACTUATOR_STATE_MOVING_LEFT:
								/////
								// actuator doesn't say it is stalled
								// so send another command to make it move left
								/////
								if(eACTUATOR_LIMIT_LEFT != getActuatorLimit())
								{
									sendCommand(eCOMMAND_ACTUATOR_SWITCH,eACTUATOR_COMMAND_MOVE_LEFT);
									eLastActuatorCommand = eACTUATOR_COMMAND_MOVE_LEFT;
									nImageIndex = BUTTON_SELECTED_INDEX+ACTUATOR_AUTO_INDEX_OFFSET;
								}
								else
								{
									printf("ZZ3\n");
									sendCommand(eCOMMAND_ACTUATOR_SWITCH,eACTUATOR_COMMAND_STOP);
									eLastActuatorCommand = eACTUATOR_COMMAND_STOP;
									bActuatorHomeInProgress = FALSE;
									nImageIndex = BUTTON_ENABLED_INDEX+ACTUATOR_AUTO_INDEX_OFFSET;
								}
								break;
							case eACTUATOR_STATE_STALLED_MOVING_LEFT:
								/////
								// actuator is stalled
								// so stop it
								/////
								printf("ZZ4\n");
								sendCommand(eCOMMAND_ACTUATOR_SWITCH,eACTUATOR_COMMAND_STOP);
								eLastActuatorCommand = eACTUATOR_COMMAND_STOP;
								bActuatorHomeInProgress = FALSE;
								nImageIndex = BUTTON_ENABLED_INDEX+ACTUATOR_AUTO_INDEX_OFFSET;
								break;
							case eACTUATOR_STATE_STALLED_MOVING_RIGHT:
								/////
								// reached the right limit
								// so we must not have been at a limit
								// when we started, so now move left
								// to the home position
								/////
								printf("ZZ5\n");
								if(eACTUATOR_LIMIT_RIGHT == getActuatorLimit())
								{
									startTimer(&actuatorLimitTimer, ACTUATOR_HOME_FROM_RIGHT_TIMER_MS); 
									sendCommand(eCOMMAND_ACTUATOR_SWITCH,eACTUATOR_COMMAND_MOVE_LEFT);
									eLastActuatorCommand = eACTUATOR_COMMAND_MOVE_LEFT;
									nImageIndex = BUTTON_SELECTED_INDEX+ACTUATOR_AUTO_INDEX_OFFSET;
								}
								else
								{
									printf("ZZ6\n");
									/////
									// actuator is stalled
									// so stop it
									/////
									sendCommand(eCOMMAND_ACTUATOR_SWITCH,eACTUATOR_COMMAND_STOP);
									eLastActuatorCommand = eACTUATOR_COMMAND_STOP;
									bActuatorHomeInProgress = FALSE;
									nImageIndex = BUTTON_ENABLED_INDEX+ACTUATOR_AUTO_INDEX_OFFSET;
								}
								break;
						}
					}
			}
		}	
		uiDisplayElement(pElement, nImageIndex);
	}
	return nRetVal;
}
//=================================================
// periodic command 180 degree actuator tilt animation
//=================================================
int Tilt180Animation(struct element* pElement)
{
	int nRetVal = 250;
	/////
	// return value is milliseconds
	// until we are called again
	if(eActuatorButtonMode == eACTUATOR_BUTTON_MANUAL_MODE)
	{
		if(uiDriverTouchInProgress())
		{
			switch(eLastActuatorCommand)
			{
				/////
				// move up is move right
				/////
				case eACTUATOR_COMMAND_MOVE_UP:
				{
					switch(getActuatorStatus())
						{
							case eACTUATOR_STATE_IDLE:
							case eACTUATOR_STATE_MOVING_RIGHT:
							case eACTUATOR_STATE_MOVING_LEFT:
								/////
								// actuator doesn't say it is stalled
								/////
								switch(nActuatorImageCurrentIndex)
								{
									case ACTUATOR_180_IMAGE_NULL_INDEX:
										nActuatorImageCurrentIndex = ACTUATOR_180_IMAGE_DOWN_INDEX;
									break;
									case ACTUATOR_180_IMAGE_DOWN_INDEX:
										nActuatorImageCurrentIndex = ACTUATOR_180_IMAGE_RIGHT45_INDEX;
										break;
		
									case ACTUATOR_180_IMAGE_RIGHT45_INDEX:
										nActuatorImageCurrentIndex = ACTUATOR_180_IMAGE_RIGHT90_INDEX;
										break;
									default:
									case ACTUATOR_180_IMAGE_RIGHT90_INDEX:
										nActuatorImageCurrentIndex = ACTUATOR_180_IMAGE_NULL_INDEX;
										break;
								}
								break;
							case eACTUATOR_STATE_STALLED_MOVING_RIGHT:
							case eACTUATOR_STATE_STALLED_MOVING_LEFT:
								/////
								// actuator is stalled
								// we should see that it is stopped in awhile
								/////
								break;
						}
						uiDisplayElement(pElement, nActuatorImageCurrentIndex);
					}
					break;
				/////
				// move down is move left
				/////
				case eACTUATOR_COMMAND_MOVE_LEFT:
					{
						switch(getActuatorStatus())
						{
							case eACTUATOR_STATE_IDLE:
							case eACTUATOR_STATE_MOVING_RIGHT:
							case eACTUATOR_STATE_MOVING_LEFT:
								/////
								// actuator doesn't say it is stalled
								/////
								switch(nActuatorImageCurrentIndex)
								{
									case ACTUATOR_180_IMAGE_NULL_INDEX:
										nActuatorImageCurrentIndex = ACTUATOR_180_IMAGE_DOWN_INDEX;
									break;
									case ACTUATOR_180_IMAGE_DOWN_INDEX:
										nActuatorImageCurrentIndex = ACTUATOR_180_IMAGE_LEFT45_INDEX;
										break;
		
									case ACTUATOR_180_IMAGE_LEFT45_INDEX:
										nActuatorImageCurrentIndex = ACTUATOR_180_IMAGE_LEFT90_INDEX;
										break;
									default:
									case ACTUATOR_180_IMAGE_LEFT90_INDEX:
										nActuatorImageCurrentIndex = ACTUATOR_180_IMAGE_NULL_INDEX;
										break;
								}
								break;
							case eACTUATOR_STATE_STALLED_MOVING_RIGHT:
							case eACTUATOR_STATE_STALLED_MOVING_LEFT:
								/////
								// actuator is stalled
								// we should see that it is stopped in awhile
								/////
								break;
						}
						uiDisplayElement(pElement, nActuatorImageCurrentIndex);
					}
					break;
				case eACTUATOR_COMMAND_STOP:
					{
							/////
							// actuator is stopped
							// so try to figure out where it is stalled
							/////
							switch(getActuatorLimit())
							{
								case eACTUATOR_LIMIT_NONE:
								case eACTUATOR_LIMIT_ERROR:		
									nActuatorImageCurrentIndex = ACTUATOR_90_IMAGE_NULL_INDEX;
									break;
								case eACTUATOR_LIMIT_BOTTOM:
									nActuatorImageCurrentIndex = ACTUATOR_90_IMAGE_DOWN_INDEX;
									break;
								case eACTUATOR_LIMIT_TOP:
									nActuatorImageCurrentIndex = ACTUATOR_90_IMAGE_UP_INDEX;
									break;
								default:
									nActuatorImageCurrentIndex = ACTUATOR_90_IMAGE_NULL_INDEX;
									break;
							}
							uiDisplayElement(pElement, nActuatorImageCurrentIndex);
					}
					break;
				default:
					break;
			}
		}
	}
	else if(eActuatorButtonMode == eACTUATOR_BUTTON_AUTO_MODE)
	{
		if(!uiDriverTouchInProgress())
		{
			switch(eLastActuatorCommand)
			{
				/////
				// move up is move right
				/////
				case eACTUATOR_COMMAND_MOVE_UP:
				{
					switch(getActuatorStatus())
						{
							case eACTUATOR_STATE_IDLE:
							case eACTUATOR_STATE_MOVING_RIGHT:
							case eACTUATOR_STATE_MOVING_LEFT:
								/////
								// actuator doesn't say it is stalled
								/////
								switch(nActuatorImageCurrentIndex)
								{
									case ACTUATOR_180_IMAGE_NULL_INDEX:
										nActuatorImageCurrentIndex = ACTUATOR_180_IMAGE_DOWN_INDEX;
									break;
									case ACTUATOR_180_IMAGE_DOWN_INDEX:
										nActuatorImageCurrentIndex = ACTUATOR_180_IMAGE_RIGHT45_INDEX;
										break;
		
									case ACTUATOR_180_IMAGE_RIGHT45_INDEX:
										nActuatorImageCurrentIndex = ACTUATOR_180_IMAGE_RIGHT90_INDEX;
										break;
									default:
									case ACTUATOR_180_IMAGE_RIGHT90_INDEX:
										nActuatorImageCurrentIndex = ACTUATOR_180_IMAGE_NULL_INDEX;
										break;
								}
								break;
							case eACTUATOR_STATE_STALLED_MOVING_RIGHT:
							case eACTUATOR_STATE_STALLED_MOVING_LEFT:
								/////
								// actuator is stalled
								// we should see that it is stopped in awhile
								/////
								break;
						}
						uiDisplayElement(pElement, nActuatorImageCurrentIndex);
					}
					break;
				/////
				// move down is move left
				/////
				case eACTUATOR_COMMAND_MOVE_LEFT:
					{
						switch(getActuatorStatus())
						{
							case eACTUATOR_STATE_IDLE:
							case eACTUATOR_STATE_MOVING_RIGHT:
							case eACTUATOR_STATE_MOVING_LEFT:
								/////
								// actuator doesn't say it is stalled
								/////
								switch(nActuatorImageCurrentIndex)
								{
									case ACTUATOR_180_IMAGE_NULL_INDEX:
										nActuatorImageCurrentIndex = ACTUATOR_180_IMAGE_DOWN_INDEX;
									break;
									case ACTUATOR_180_IMAGE_DOWN_INDEX:
										nActuatorImageCurrentIndex = ACTUATOR_180_IMAGE_LEFT45_INDEX;
										break;
		
									case ACTUATOR_180_IMAGE_LEFT45_INDEX:
										nActuatorImageCurrentIndex = ACTUATOR_180_IMAGE_LEFT90_INDEX;
										break;
									default:
									case ACTUATOR_180_IMAGE_LEFT90_INDEX:
										nActuatorImageCurrentIndex = ACTUATOR_180_IMAGE_NULL_INDEX;
										break;
								}
								break;
							case eACTUATOR_STATE_STALLED_MOVING_RIGHT:
							case eACTUATOR_STATE_STALLED_MOVING_LEFT:
								/////
								// actuator is stalled
								// we should see that it is stopped in awhile
								/////
								break;
						}
						uiDisplayElement(pElement, nActuatorImageCurrentIndex);
					}
					break;
				case eACTUATOR_COMMAND_STOP:
					{
							/////
							// actuator is stopped
							// so try to figure out where it is stalled
							/////
							switch(getActuatorLimit())
							{
								case eACTUATOR_LIMIT_NONE:
								case eACTUATOR_LIMIT_ERROR:		
									nActuatorImageCurrentIndex = ACTUATOR_90_IMAGE_NULL_INDEX;
									break;
								case eACTUATOR_LIMIT_BOTTOM:
									nActuatorImageCurrentIndex = ACTUATOR_90_IMAGE_DOWN_INDEX;
									break;
								case eACTUATOR_LIMIT_TOP:
									nActuatorImageCurrentIndex = ACTUATOR_90_IMAGE_UP_INDEX;
									break;
								default:
									nActuatorImageCurrentIndex = ACTUATOR_90_IMAGE_NULL_INDEX;
									break;
							}
							uiDisplayElement(pElement, nActuatorImageCurrentIndex);
					}
					break;
				default:
					break;
			}			
		}
	}	
	return nRetVal;
}
//=================================================
// periodic command 90 degree actuator tilt animation
//=================================================
int Tilt90Animation(struct element* pElement)
{
	int nRetVal = 250;
	/////
	// return value is milliseconds
	// until we are called again
	//printf("Tilt90Animation: eLastActuatorCommand[%d]\n",eLastActuatorCommand);
	
	if(eActuatorButtonMode == eACTUATOR_BUTTON_MANUAL_MODE)
	{
		if(uiDriverTouchInProgress())
		{
			//printf("Tilt90Animation: eLastActuatorCommand[%d]\n",eLastActuatorCommand);
			switch(eLastActuatorCommand)
			{
				case eACTUATOR_COMMAND_MOVE_UP:
				{
					switch(getActuatorStatus())
						{
							case eACTUATOR_STATE_IDLE:
							case eACTUATOR_STATE_MOVING_UP:
							case eACTUATOR_STATE_MOVING_DOWN:
								/////
								// actuator doesn't say it is stalled
								/////
								nActuatorImageCurrentIndex++;
								if(5 <= nActuatorImageCurrentIndex)
								{
									nActuatorImageCurrentIndex = 0;
								}
								break;
							case eACTUATOR_STATE_STALLED_MOVING_UP:
							case eACTUATOR_STATE_STALLED_MOVING_DOWN:
								/////
								// actuator is stalled
								// we should see that it is stopped in awhile
								/////
								break;
						}
						uiDisplayElement(pElement, nActuatorImageCurrentIndex);
					}
					break;
				case eACTUATOR_COMMAND_MOVE_DOWN:
					{
						switch(getActuatorStatus())
						{
							case eACTUATOR_STATE_IDLE:
							case eACTUATOR_STATE_MOVING_UP:
							case eACTUATOR_STATE_MOVING_DOWN:
								/////
								// actuator doesn't say it is stalled
								/////
								nActuatorImageCurrentIndex--;
								if(0 > nActuatorImageCurrentIndex)
								{
									nActuatorImageCurrentIndex = 4;
								}
								break;
							case eACTUATOR_STATE_STALLED_MOVING_UP:
							case eACTUATOR_STATE_STALLED_MOVING_DOWN:
								/////
								// actuator is stalled
								// we should see that it is stopped in awhile
								/////
								break;
						}
						uiDisplayElement(pElement, nActuatorImageCurrentIndex);
					}
					break;
				case eACTUATOR_COMMAND_STOP:
					{
							/////
							// actuator is stopped
							// so try to figure out where it is stalled
							/////
							switch(getActuatorLimit())
							{
								case eACTUATOR_LIMIT_NONE:
								case eACTUATOR_LIMIT_ERROR:		
									nActuatorImageCurrentIndex = ACTUATOR_90_IMAGE_NULL_INDEX;
									break;
								case eACTUATOR_LIMIT_BOTTOM:
									nActuatorImageCurrentIndex = ACTUATOR_90_IMAGE_DOWN_INDEX;
									break;
								case eACTUATOR_LIMIT_TOP:
									nActuatorImageCurrentIndex = ACTUATOR_90_IMAGE_UP_INDEX;
									break;
								default:
									nActuatorImageCurrentIndex = ACTUATOR_90_IMAGE_NULL_INDEX;
									break;
							}
							uiDisplayElement(pElement, nActuatorImageCurrentIndex);
					}
					break;
				default:
					break;
			}
		}
	}
	else if(eActuatorButtonMode == eACTUATOR_BUTTON_AUTO_MODE)
	{
		if(!uiDriverTouchInProgress())
		{
			switch(eLastActuatorCommand)
			{
				case eACTUATOR_COMMAND_MOVE_UP:
				{
					switch(getActuatorStatus())
						{
							case eACTUATOR_STATE_IDLE:
							case eACTUATOR_STATE_MOVING_UP:
							case eACTUATOR_STATE_MOVING_DOWN:
								/////
								// actuator doesn't say it is stalled
								/////
								nActuatorImageCurrentIndex++;
								if(5 <= nActuatorImageCurrentIndex)
								{
									nActuatorImageCurrentIndex = 0;
								}
								break;
							case eACTUATOR_STATE_STALLED_MOVING_UP:
							case eACTUATOR_STATE_STALLED_MOVING_DOWN:
								/////
								// actuator is stalled
								// we should see that it is stopped in awhile
								/////
								break;
						}
						uiDisplayElement(pElement, nActuatorImageCurrentIndex);
					}
					break;
				case eACTUATOR_COMMAND_MOVE_DOWN:
					{
						switch(getActuatorStatus())
						{
							case eACTUATOR_STATE_IDLE:
							case eACTUATOR_STATE_MOVING_UP:
							case eACTUATOR_STATE_MOVING_DOWN:
								/////
								// actuator doesn't say it is stalled
								/////
								nActuatorImageCurrentIndex--;
								if(0 > nActuatorImageCurrentIndex)
								{
									nActuatorImageCurrentIndex = 4;
								}
								break;
							case eACTUATOR_STATE_STALLED_MOVING_UP:
							case eACTUATOR_STATE_STALLED_MOVING_DOWN:
								/////
								// actuator is stalled
								// we should see that it is stopped in awhile
								/////
								break;
						}
						uiDisplayElement(pElement, nActuatorImageCurrentIndex);
					}
					break;
				case eACTUATOR_COMMAND_STOP:
					{
							/////
							// actuator is stopped
							// so try to figure out where it is stalled
							/////
							switch(getActuatorLimit())
							{
								case eACTUATOR_LIMIT_NONE:
								case eACTUATOR_LIMIT_ERROR:		
									nActuatorImageCurrentIndex = ACTUATOR_90_IMAGE_NULL_INDEX;
									break;
								case eACTUATOR_LIMIT_BOTTOM:
									nActuatorImageCurrentIndex = ACTUATOR_90_IMAGE_DOWN_INDEX;
									break;
								case eACTUATOR_LIMIT_TOP:
									nActuatorImageCurrentIndex = ACTUATOR_90_IMAGE_UP_INDEX;
									break;
								default:
									nActuatorImageCurrentIndex = ACTUATOR_90_IMAGE_NULL_INDEX;
									break;
							}
							uiDisplayElement(pElement, nActuatorImageCurrentIndex);
					}
					break;
				default:
					break;
			}			
		}
	}
	return nRetVal;
}
//=================================================
// periodic command clone get data
//=================================================
int CloneGettingData(struct element* pElement)
{
	if(bCloneGettingData)
	{
		if( bCloneGotBrightnessControl & 
				bCloneGotDisallowedPatterns & 
				bCloneGotActuatorType & 
				bCloneGotAuxBatteryType &
				bCloneGotActuatorButtonMode &
				bCloneGotActuatorCalibrationHint)
		{
			bCloneGettingData = FALSE;
			uiDisplayElement(pElement, BUTTON_ENABLED_INDEX);
		}
	}
	return 500;
}

//=================================================
// periodic command actuator up calibration
//=================================================
int CalibrateTiltUp(struct element* pElement)
{
	if(bCalibrateTiltUpInProgress)
	{
		sendCommand(eCOMMAND_DO_ACTUATOR_CALIBRATION, eACTUATOR_LIMIT_CALIBRATE_BEGIN_UP);
		uiDisplayElement(pElement, CALIBRATE_TILT_END_INDEX);
	}
	else
	{
		uiDisplayElement(pElement, CALIBRATE_TILT_ENABLED_INDEX);
	}
	return 500;
}
//=================================================
// periodic command actuator up calibration
//=================================================
int CalibrateTiltDown(struct element* pElement)
{
	if(bCalibrateTiltDownInProgress)
	{
		sendCommand(eCOMMAND_DO_ACTUATOR_CALIBRATION, eACTUATOR_LIMIT_CALIBRATE_BEGIN_DOWN);
		uiDisplayElement(pElement, CALIBRATE_TILT_END_INDEX);
	}
	else
	{
		uiDisplayElement(pElement, CALIBRATE_TILT_ENABLED_INDEX);
	}
	return 500;
}
//=================================================
// pushbutton command change arrow board display
//=================================================
int SendDisplayCommand(struct element* pElement, unsigned char bTouchInProgress, unsigned char bThisButtonIsSelected)
{
	int nImageIndex = BUTTON_ENABLED_INDEX;
	if(!bTouchInProgress)
	{	
		switch(pElement->eType)
		{
				case eELEMENT_TYPE_RADIO_BUTTON:
					{
						/////
						// radio buttons have this data
						// others do not
						/////
						RADIO_BUTTON_SELECTION_ELEMENT *pRadioButtonElement = (RADIO_BUTTON_SELECTION_ELEMENT*)pElement;
						if(isPatternAllowed((eDISPLAY_TYPES)pRadioButtonElement->nData))
						{
							eDISPLAY_TYPES eType = (eDISPLAY_TYPES)pRadioButtonElement->nData;
							/////
							// issue the command
							/////
							sendCommand(eCOMMAND_DISPLAY_CHANGE, eType);
							setCurrentDisplayType(eType);
						
							/////
							// make sure we show the newly displayed pattern
							/////
							bResetDisplayType = TRUE;		
							nImageIndex = BUTTON_SELECTED_INDEX;							
							/////
							// redisplay all of the buttons
							// so we show the correct one highlighted
							/////
							uiRefreshCurrentPage();
							
							/////
							// exit this screen
							// after a brief delay
							/////
							pRadioButtonElement->pPeriodicFunction = ExitPageAfterDelay;
							startTimer(&pRadioButtonElement->buttonTimer, PAGE_HOLD_TIME_MS);
						}
						
					}
					break;
				case eELEMENT_TYPE_NAVIGATION_BUTTON:
				case eELEMENT_TYPE_NAVIGATION_RETURN_BUTTON:
				case eELEMENT_TYPE_NAVIGATION_TO_MAIN_BUTTON:
				case eELEMENT_TYPE_DISPLAYONLY:
				case eELEMENT_TYPE_SLIDER:
					break;
		}
	}
	else
	{
		if(bThisButtonIsSelected)
		{
			nImageIndex = BUTTON_PRESSED_INDEX;
		}
		else
		{
			nImageIndex = BUTTON_ENABLED_INDEX;
		}
	}
	return nImageIndex;
}
//=================================================
// pushbutton command determine which page ID to navigate to
//=================================================
int WhichPatternSelectPage(struct element* pElement, unsigned char bTouchInProgress, unsigned char bThisButtonIsSelected)
{
	static int nPageIndex = 0;
	int nRetID;
	PUSHBUTTON_NAVIGATION_ELEMENT *pNavigationButton = (PUSHBUTTON_NAVIGATION_ELEMENT*)pElement;
	switch(uiDriverGetModel())
	{
		case eMODEL_VEHICLE_25_LIGHT_SEQUENTIAL:
		case eMODEL_TRAILER_25_LIGHT_SEQUENTIAL:
			break;
		case eMODEL_VEHICLE_15_LIGHT_SEQUENTIAL:
		case eMODEL_TRAILER_15_LIGHT_SEQUENTIAL:
		case eMODEL_TRAILER_15_LIGHT_WIG_WAG:
		case eMODEL_VEHICLE_15_LIGHT_WIG_WAG:
			nPageIndex = 1;
			break;
		case eMODEL_VEHICLE_15_LIGHT_FLASHING:
		case eMODEL_TRAILER_15_LIGHT_FLASHING:

			nPageIndex = 2;
			break;
		case eMODEL_NONE:
			nRetID = 0;
			return nRetID;
	}
	nRetID = pNavigationButton->nextPageIDS[nPageIndex];
	if(0 == nRetID)
	{
		nRetID = pNavigationButton->nextPageIDS[0];
	}
	//printf("WhichPatternSelectPage[%X]\n", nRetID);
	return nRetID;
}
//=================================================
// pushbutton command determine which page ID to navigate to
//=================================================
int WhichTiltPageSelect(struct element* pElement, unsigned char bTouchInProgress, unsigned char bThisButtonIsSelected)
{
	int nRetID = 0;
	PUSHBUTTON_NAVIGATION_ELEMENT *pNavigationButton = (PUSHBUTTON_NAVIGATION_ELEMENT*)pElement;
	/////
	// determine which page index
	// based upon tilt configuration
	/////
	switch(eActuatorType)
	{
		case eACTUATOR_TYPE_90_DEGREE_POWER_TILT:
			nRetID = pNavigationButton->nextPageIDS[0];
			break;
		case eACTUATOR_TYPE_180_DEGREE_POWER_TILT:
			nRetID = pNavigationButton->nextPageIDS[1];
			break;
		case eACTUATOR_TYPE_UKNOWN:
		case eACTUATOR_TYPE_NONE:
			/////
			// no page for this one, 
			// so return null page ID
			/////
			nRetID = 0;
		default:
				break;
	}
	//printf("WhichTiltPageSelect[%X]\n", nRetID);
	return nRetID;
}

//=================================================
// pushbutton command to actuator
//=================================================
int SendActuatorCommand(struct element* pElement, unsigned char bTouchInProgress, unsigned char bThisButtonIsSelected)
{
	int nImageIndex = BUTTON_ENABLED_INDEX;
	if(!bTouchInProgress)
	{
		RADIO_BUTTON_SELECTION_ELEMENT *pButton = (RADIO_BUTTON_SELECTION_ELEMENT*)pElement;
		sendCommand(eCOMMAND_ACTUATOR_SWITCH, pButton->nData);
		//sendCommand(eCOMMAND_STATUS_ACTUATOR_LIMIT, 0);
	}
	else
	{
		if(bThisButtonIsSelected)
		{
			nImageIndex = BUTTON_PRESSED_INDEX;
		}
		else
		{
			nImageIndex = BUTTON_ENABLED_INDEX;
		}
	}
	return nImageIndex;
}

#if 0
static void actuatorButtonStateMachine(int *imageIndex, BYTE bTouchInProgress, eACTUATOR_COMMANDS eActuatorCommand)
{
	switch(eABNextState)
	{
		case eAB_IDLE:
			
			if(bTouchInProgress)
			{
				if(eActuatorButtonMode == eACTUATOR_BUTTON_AUTO_MODE)
				{					
					eLastActuatorButtonMode	= eACTUATOR_BUTTON_AUTO_MODE;
					
					eABNextState = eAB_PRESSED_START_AUTO_MODE_TIMER;					
				}
				else if(eActuatorButtonMode == eACTUATOR_BUTTON_MANUAL_MODE)
				{					
					eLastActuatorButtonMode	= eACTUATOR_BUTTON_MANUAL_MODE;
					
				  eABNextState = eAB_PRESSED_MOVE_ACTUATOR_IN_MANUAL_MODE;					
				}
			}
			else
			{					
				eABNextState = eAB_IDLE;
			}
			break;
		
		case eAB_PRESSED_START_AUTO_MODE_TIMER:
			
			if(bTouchInProgress)
			{			
				if(isTimerExpired(&actuatorAutoTouchInProgressTimer))
				{
					startTimer(&actuatorAutoTouchInProgressTimer,ACTUATOR_BUTTON_MAX_HOLD_TIME);
					
					eABNextState = eAB_PRESSED_WAIT_FOR_AUTO_MODE_TIMER_TO_EXPIRE;
				}
			}
			else
			{
				eLastActuatorCommand = eActuatorCommand;
			
				sendCommand(eCOMMAND_ACTUATOR_SWITCH, eLastActuatorCommand);
				//sendCommand(eCOMMAND_STATUS_ACTUATOR_LIMIT, 0);
			
				
				eABNextState = eAB_IDLE;
			}
			break;
		
		case eAB_PRESSED_WAIT_FOR_AUTO_MODE_TIMER_TO_EXPIRE:
			
			if(bTouchInProgress)
			{								
				if(isTimerExpired(&actuatorAutoTouchInProgressTimer))
				{				
					eABNextState = eAB_PRESSED_MOVE_ACTUATOR_IN_MANUAL_MODE;
					
					eActuatorButtonMode = eACTUATOR_BUTTON_MANUAL_MODE;
					
					eLastActuatorButtonMode = eACTUATOR_BUTTON_AUTO_MODE;
				}
				else
				{
					eABNextState = eAB_PRESSED_WAIT_FOR_AUTO_MODE_TIMER_TO_EXPIRE;					
				}
			}
			else
			{
				eLastActuatorCommand = eActuatorCommand;
			
				sendCommand(eCOMMAND_ACTUATOR_SWITCH, eLastActuatorCommand);
				//sendCommand(eCOMMAND_STATUS_ACTUATOR_LIMIT, 0);
			
				
				eABNextState = eAB_IDLE;
			}
			break;
			
		case eAB_PRESSED_MOVE_ACTUATOR_IN_MANUAL_MODE:
			
			if(bTouchInProgress)
			{
				if(isTimerExpired(&actuatorManualCommandTimer))
				{
					eLastActuatorCommand = eActuatorCommand;
					
					sendCommand(eCOMMAND_ACTUATOR_SWITCH, eLastActuatorCommand);
					//sendCommand(eCOMMAND_STATUS_ACTUATOR_LIMIT, 0);				
					
					startTimer(&actuatorManualCommandTimer, MANUAL_ACTUATOR_COMMAND_TIMER);
					
					*imageIndex = BUTTON_SELECTED_INDEX;
				}
				
				eABNextState = eAB_PRESSED_MOVE_ACTUATOR_IN_MANUAL_MODE;	
			}
			else
			{
				//if(eLastActuatorButtonMode == eACTUATOR_BUTTON_AUTO_MODE)
				//{
				//	eActuatorButtonMode = eACTUATOR_BUTTON_AUTO_MODE;
				//	
				//	//printf("Switching from Manual Back to Auto Mode\n");
				//}
				//else
				//{
				//	printf("We Are Still In Manual Mode\n");
				//}
				
				eLastActuatorCommand = eACTUATOR_COMMAND_STOP;
									
				sendCommand(eCOMMAND_ACTUATOR_SWITCH, eACTUATOR_COMMAND_STOP);
				//sendCommand(eCOMMAND_STATUS_ACTUATOR_LIMIT, 0);
				
				*imageIndex = BUTTON_ENABLED_INDEX;
				
				eABNextState = eAB_IDLE;	
			}
			
			break;
	}	
}
#endif

//=================================================
// pushbutton command 180 degree actuator left
//=================================================
int Actuator180LeftPress(struct element* theElement, unsigned char bTouchInProgress, unsigned char bThisButtonIsSelected)
{
	/////
	// return the image index
	/////
	int nImageIndex = theElement->nCurrentImageIndex;
	
	/////
	// blank the sign
	// so it won't interfere with reading the actuator current
	/////
	if(eDISPLAY_TYPE_BLANK != getCurrentDisplayType())
	{
		sendCommand(eCOMMAND_DISPLAY_CHANGE, eDISPLAY_TYPE_BLANK);
		setCurrentDisplayType(eDISPLAY_TYPE_BLANK);
	}
	switch(eActuatorButtonMode)
	{
		case eACTUATOR_BUTTON_AUTO_MODE:
			if(bThisButtonIsSelected)
			{
				if(!bTouchInProgress)
				{
					/////
					// button released
					// so start the movement
					/////
					sendCommand(eCOMMAND_ACTUATOR_SWITCH, eACTUATOR_COMMAND_MOVE_LEFT);
					eLastActuatorCommand = eACTUATOR_COMMAND_MOVE_LEFT;
					nImageIndex = BUTTON_SELECTED_INDEX+ACTUATOR_AUTO_INDEX_OFFSET;
				}
			}
			break;
		case eACTUATOR_BUTTON_MANUAL_MODE:
			if(bThisButtonIsSelected)
			{
				if(bTouchInProgress)
				{
					if(isTimerExpired(&actuatorManualCommandTimer))
					{
						sendCommand(eCOMMAND_ACTUATOR_SWITCH, eACTUATOR_COMMAND_MOVE_LEFT);
						eLastActuatorCommand = eACTUATOR_COMMAND_MOVE_LEFT;
						nImageIndex = BUTTON_SELECTED_INDEX;
						startTimer(&actuatorManualCommandTimer, MANUAL_ACTUATOR_COMMAND_TIMER);
					}
				}
				else
				{
						sendCommand(eCOMMAND_ACTUATOR_SWITCH, eACTUATOR_COMMAND_STOP);
						eLastActuatorCommand = eACTUATOR_COMMAND_STOP;
						nImageIndex = BUTTON_ENABLED_INDEX;
				}
			}
			break;
	}
	bActuatorHomeInProgress = FALSE;
	return (nImageIndex);
}
//=================================================
// pushbutton command 180 degree actuator right
//=================================================
int Actuator180RightPress(struct element* theElement, unsigned char bTouchInProgress, unsigned char bThisButtonIsSelected)
{
	/////
	// return the image index
	/////
	int nImageIndex = theElement->nCurrentImageIndex;
	/////
	// blank the sign
	// so it won't interfere with reading the actuator current
	/////
	if(eDISPLAY_TYPE_BLANK != getCurrentDisplayType())
	{
		sendCommand(eCOMMAND_DISPLAY_CHANGE, eDISPLAY_TYPE_BLANK);
		setCurrentDisplayType(eDISPLAY_TYPE_BLANK);
	}
	
	switch(eActuatorButtonMode)
	{
		case eACTUATOR_BUTTON_AUTO_MODE:
			if(bThisButtonIsSelected)
			{
				if(!bTouchInProgress)
				{
					/////
					// button released
					// so start the movement
					/////
					sendCommand(eCOMMAND_ACTUATOR_SWITCH, eACTUATOR_COMMAND_MOVE_RIGHT);
					eLastActuatorCommand = eACTUATOR_COMMAND_MOVE_RIGHT;
					nImageIndex = BUTTON_SELECTED_INDEX+ACTUATOR_AUTO_INDEX_OFFSET;
				}
			}
			break;
		case eACTUATOR_BUTTON_MANUAL_MODE:
			if(bThisButtonIsSelected)
			{
				if(bTouchInProgress)
				{
					if(isTimerExpired(&actuatorManualCommandTimer))
					{
						sendCommand(eCOMMAND_ACTUATOR_SWITCH, eACTUATOR_COMMAND_MOVE_RIGHT);
						eLastActuatorCommand = eACTUATOR_COMMAND_MOVE_RIGHT;
						nImageIndex = BUTTON_SELECTED_INDEX;
						startTimer(&actuatorManualCommandTimer, MANUAL_ACTUATOR_COMMAND_TIMER);
					}
				}
				else
				{
						sendCommand(eCOMMAND_ACTUATOR_SWITCH, eACTUATOR_COMMAND_STOP);
						eLastActuatorCommand = eACTUATOR_COMMAND_STOP;
						nImageIndex = BUTTON_ENABLED_INDEX;
				}
			}
			break;
	}
	bActuatorHomeInProgress = FALSE;
	return (nImageIndex);
}
//=================================================
// pushbutton command 180 degree actuator to home position
//=================================================
int Actuator180NeutralPress(struct element* theElement, unsigned char bTouchInProgress, unsigned char bThisButtonIsSelected)
{
	/////
	// return the image index
	/////
	int nImageIndex = theElement->nCurrentImageIndex;
	/////
	// blank the sign
	// so it won't interfere with reading the actuator current
	/////
	if(eDISPLAY_TYPE_BLANK != getCurrentDisplayType())
	{
		sendCommand(eCOMMAND_DISPLAY_CHANGE, eDISPLAY_TYPE_BLANK);
		setCurrentDisplayType(eDISPLAY_TYPE_BLANK);
	}
	switch(eActuatorButtonMode)
	{
		case eACTUATOR_BUTTON_AUTO_MODE:
			if(bThisButtonIsSelected)
			{
				/////
				// figure out where we are now
				// and then move to the home position
			/////
			switch(getActuatorLimit())
			{
					default:
					case eACTUATOR_LIMIT_NONE:
					case eACTUATOR_LIMIT_ERROR:
						/////
						// don't know where we are
						// so pick a direction
						// go until we see the limit switch
						// and then back the other way
						/////
						startTimer(&actuatorLimitTimer, ACTUATOR_LIMIT_TIMER_MS);
						sendCommand(eCOMMAND_ACTUATOR_SWITCH,eACTUATOR_COMMAND_MOVE_RIGHT);
						eLastActuatorCommand = eACTUATOR_COMMAND_MOVE_RIGHT;
						nImageIndex = BUTTON_SELECTED_INDEX+ACTUATOR_AUTO_INDEX_OFFSET;
						bActuatorHomeInProgress = TRUE;
						break;
					case eACTUATOR_LIMIT_LEFT:
						/////
						// start the home timer
						// If we are at a limit
						// then we should travel only this long 
						// to get to the home position
						/////
						startTimer(&actuatorLimitTimer, ACTUATOR_HOME_FROM_LEFT_TIMER_MS); 			
						sendCommand(eCOMMAND_ACTUATOR_SWITCH,eACTUATOR_COMMAND_MOVE_RIGHT);
						eLastActuatorCommand = eACTUATOR_COMMAND_MOVE_RIGHT;
						nImageIndex = BUTTON_SELECTED_INDEX+ACTUATOR_AUTO_INDEX_OFFSET;
						bActuatorHomeInProgress = TRUE;
						break;
					case eACTUATOR_LIMIT_RIGHT:
						/////
						// start the home timer
						// If we are at a limit
						// then we should travel only this long 
						// to get to the home position
						/////
						startTimer(&actuatorLimitTimer, ACTUATOR_HOME_FROM_RIGHT_TIMER_MS); 
						sendCommand(eCOMMAND_ACTUATOR_SWITCH,eACTUATOR_COMMAND_MOVE_LEFT);
						eLastActuatorCommand = eACTUATOR_COMMAND_MOVE_LEFT;
						nImageIndex = BUTTON_SELECTED_INDEX+ACTUATOR_AUTO_INDEX_OFFSET;
						bActuatorHomeInProgress = TRUE;
						break;
				}
			}
			break;
		case eACTUATOR_BUTTON_MANUAL_MODE:
			break;
	}
	return (nImageIndex);
}
//=================================================
// pushbutton command 180 degree actuator stop
//=================================================
int Actuator180StopPress(struct element* theElement, unsigned char bTouchInProgress, unsigned char bThisButtonIsSelected)
{
	/////
	// return the image index
	/////
	int nImageIndex = theElement->nCurrentImageIndex;
	if(eActuatorButtonMode == eACTUATOR_BUTTON_AUTO_MODE)
	{
		if(!bTouchInProgress)
		{
			eLastActuatorCommand = eACTUATOR_COMMAND_STOP;
			//printf("Actuator180StopPress\n");
			sendCommand(eCOMMAND_ACTUATOR_SWITCH, eLastActuatorCommand);
			//sendCommand(eCOMMAND_STATUS_ACTUATOR_LIMIT, 0);
		}
	}
	bActuatorHomeInProgress = FALSE;
	return  (nImageIndex);
}

int Actuator90UpPress(struct element* theElement, unsigned char bTouchInProgress, unsigned char bThisButtonIsSelected)
{
	/////
	// return the image index
	/////
	int nImageIndex = theElement->nCurrentImageIndex;	
	
	/////
	// blank the sign
	// so it won't interfere with reading the actuator current
	/////
	if(eDISPLAY_TYPE_BLANK != getCurrentDisplayType())
	{
		sendCommand(eCOMMAND_DISPLAY_CHANGE, eDISPLAY_TYPE_BLANK);
		setCurrentDisplayType(eDISPLAY_TYPE_BLANK);
	}
	
	switch(eActuatorButtonMode)
	{
		case eACTUATOR_BUTTON_AUTO_MODE:
			if(bThisButtonIsSelected)
			{
				if(!bTouchInProgress)
				{                   
                    if(eLastActuatorCommand == eACTUATOR_COMMAND_MOVE_DOWN)
                    {
                        sendCommand(eCOMMAND_ACTUATOR_SWITCH, eACTUATOR_COMMAND_STOP);
                        eLastActuatorCommand = eACTUATOR_COMMAND_STOP;
                        nImageIndex = ACTUATOR_AUTO_STOP_SELECTED_INDEX;
                    }
                    else
                    {
                        /////
                        // button released
                        // so start the movement
                        /////
                        sendCommand(eCOMMAND_ACTUATOR_SWITCH, eACTUATOR_COMMAND_MOVE_UP);
                        eLastActuatorCommand = eACTUATOR_COMMAND_MOVE_UP;
                        nImageIndex = BUTTON_SELECTED_INDEX+ACTUATOR_AUTO_INDEX_OFFSET;
                        /////
                        // start the limit timer
                        // in case there is no upper limit switch
                        /////
                        startTimer(&actuatorLimitTimer, ACTUATOR_LIMIT_TIMER_MS);
                    }
				}
			}
			break;
		case eACTUATOR_BUTTON_MANUAL_MODE:
			if(bThisButtonIsSelected)
			{
				if(bTouchInProgress)
				{
					if(isTimerExpired(&actuatorManualCommandTimer))
					{
						sendCommand(eCOMMAND_ACTUATOR_SWITCH, eACTUATOR_COMMAND_MOVE_UP);
						eLastActuatorCommand = eACTUATOR_COMMAND_MOVE_UP;
						nImageIndex = BUTTON_SELECTED_INDEX;
						startTimer(&actuatorManualCommandTimer, MANUAL_ACTUATOR_COMMAND_TIMER);
					}
				}
				else
				{
						sendCommand(eCOMMAND_ACTUATOR_SWITCH, eACTUATOR_COMMAND_STOP);
						eLastActuatorCommand = eACTUATOR_COMMAND_STOP;
						nImageIndex = BUTTON_ENABLED_INDEX;
				}
			}
			break;
	}
	return (nImageIndex);
}

//=================================================
// pushbutton command 90 degree actuator down
//=================================================
int Actuator90DownPress(struct element* theElement, unsigned char bTouchInProgress, unsigned char bThisButtonIsSelected)
{
	/////
	// return the image index
	// don't know which one now, so return 0
	/////
	int nImageIndex = theElement->nCurrentImageIndex;
	
	/////
	// blank the sign
	// so it won't interfere with reading the actuator current
	/////
	if(eDISPLAY_TYPE_BLANK != getCurrentDisplayType())
	{
		sendCommand(eCOMMAND_DISPLAY_CHANGE, eDISPLAY_TYPE_BLANK);
		
		setCurrentDisplayType(eDISPLAY_TYPE_BLANK);
	}

	switch(eActuatorButtonMode)
	{
		case eACTUATOR_BUTTON_AUTO_MODE:
			if(bThisButtonIsSelected)
			{
				if(!bTouchInProgress)
				{
                    if(eLastActuatorCommand == eACTUATOR_COMMAND_MOVE_UP)
                    {
                        sendCommand(eCOMMAND_ACTUATOR_SWITCH, eACTUATOR_COMMAND_STOP);
                        eLastActuatorCommand = eACTUATOR_COMMAND_STOP;
                        nImageIndex = ACTUATOR_AUTO_STOP_SELECTED_INDEX;
                    }
                    else
                    {
                        /////
                        // button released
                        // so start the movement
                        /////
                        sendCommand(eCOMMAND_ACTUATOR_SWITCH, eACTUATOR_COMMAND_MOVE_DOWN);
                        eLastActuatorCommand = eACTUATOR_COMMAND_MOVE_DOWN;
                        nImageIndex = BUTTON_SELECTED_INDEX+ACTUATOR_AUTO_INDEX_OFFSET;
                        /////
                        // start the limit timer
                        // in case there is no lower limit switch
                        /////
                        startTimer(&actuatorLimitTimer, ACTUATOR_LIMIT_TIMER_MS);
                    }
				}
			}
			break;
		case eACTUATOR_BUTTON_MANUAL_MODE:
			if(bThisButtonIsSelected)
			{
				if(bTouchInProgress)
				{
					if(isTimerExpired(&actuatorManualCommandTimer))
					{
						sendCommand(eCOMMAND_ACTUATOR_SWITCH, eACTUATOR_COMMAND_MOVE_DOWN);
						eLastActuatorCommand = eACTUATOR_COMMAND_MOVE_DOWN;
						nImageIndex = BUTTON_SELECTED_INDEX;
						startTimer(&actuatorManualCommandTimer, MANUAL_ACTUATOR_COMMAND_TIMER);
					}
				}
				else
				{
						sendCommand(eCOMMAND_ACTUATOR_SWITCH, eACTUATOR_COMMAND_STOP);
						eLastActuatorCommand = eACTUATOR_COMMAND_STOP;
						nImageIndex = BUTTON_ENABLED_INDEX;
				}
			}
			break;
	}
	
	return (nImageIndex);
}

//=================================================
// pushbutton command 90 degree actuator stop
//=================================================
int Actuator90StopPress(struct element* theElement, unsigned char bTouchInProgress, unsigned char bThisButtonIsSelected)
{
	/////
	// return the image index
	// don't know which one now, so return 0
	/////
	if(eActuatorButtonMode == eACTUATOR_BUTTON_AUTO_MODE)
	{
		if(!bTouchInProgress)
		{
			eLastActuatorCommand = eACTUATOR_COMMAND_STOP;
			//printf("Actuator90StopPress\n");
			sendCommand(eCOMMAND_ACTUATOR_SWITCH, eLastActuatorCommand);
		}
	}
	return  (theElement->nCurrentImageIndex);
}


//=======+++++++++++++++=============
//=================================================
// pushbutton command Cal Trans UI actuator UP
//=================================================
int CalTransActuatorUp(struct element* theElement, unsigned char bTouchInProgress, unsigned char bThisButtonIsSelected)
{
	/////
	// return the image index
	/////
	int nImageIndex = theElement->nCurrentImageIndex;
	if(bTouchInProgress && bThisButtonIsSelected)
	{
		/////
		// the button has been pressed for 100ms
		/////
				
		/////
		// blank the sign
		// so it won't interfere with reading the actuator current
		/////
		if(eDISPLAY_TYPE_BLANK != getCurrentDisplayType())
		{
			sendCommand(eCOMMAND_DISPLAY_CHANGE, eDISPLAY_TYPE_BLANK);
			setCurrentDisplayType(eDISPLAY_TYPE_BLANK);
		}

		if(isTimerExpired(&actuatorManualCommandTimer))
		{
			eLastActuatorCommand = eACTUATOR_COMMAND_MOVE_UP;
			sendCommand(eCOMMAND_ACTUATOR_SWITCH, eLastActuatorCommand);
			//sendCommand(eCOMMAND_STATUS_ACTUATOR_LIMIT, 0);
			startTimer(&actuatorManualCommandTimer, MANUAL_ACTUATOR_COMMAND_TIMER);
			nImageIndex = BUTTON_SELECTED_INDEX;
		}
	}
	else
	{
			sendCommand(eCOMMAND_ACTUATOR_SWITCH, eACTUATOR_COMMAND_STOP);
			//sendCommand(eCOMMAND_STATUS_ACTUATOR_LIMIT, 0);
			nImageIndex = BUTTON_ENABLED_INDEX;
		//	bDebounceInProgress = FALSE;
	}
	return (nImageIndex);
}
//=================================================
// pushbutton command Cal Trans UI actuator DOWN
//=================================================
int CalTransActuatorDown(struct element* theElement, unsigned char bTouchInProgress, unsigned char bThisButtonIsSelected)
{
	/////
	// return the image index
	/////
	int nImageIndex = theElement->nCurrentImageIndex;
	
	if(bTouchInProgress && bThisButtonIsSelected)
	{
		/////
		// the button has been pressed for 100ms
		/////
				
		/////
		// blank the sign
		// so it won't interfere with reading the actuator current
		/////
		if(eDISPLAY_TYPE_BLANK != getCurrentDisplayType())
		{
			sendCommand(eCOMMAND_DISPLAY_CHANGE, eDISPLAY_TYPE_BLANK);
			setCurrentDisplayType(eDISPLAY_TYPE_BLANK);
		}

		if(isTimerExpired(&actuatorManualCommandTimer))
		{
			eLastActuatorCommand = eACTUATOR_COMMAND_MOVE_DOWN;
			sendCommand(eCOMMAND_ACTUATOR_SWITCH, eLastActuatorCommand);
			//sendCommand(eCOMMAND_STATUS_ACTUATOR_LIMIT, 0);
			startTimer(&actuatorManualCommandTimer, MANUAL_ACTUATOR_COMMAND_TIMER);
			nImageIndex = BUTTON_SELECTED_INDEX;
		}
	}
	else
	{
			sendCommand(eCOMMAND_ACTUATOR_SWITCH, eACTUATOR_COMMAND_STOP);
			//sendCommand(eCOMMAND_STATUS_ACTUATOR_LIMIT, 0);
			nImageIndex = BUTTON_ENABLED_INDEX;
	}
	return (nImageIndex);
}
//===========+++++++++++++++=============
//=================================================
// pushbutton command wireless config
//=================================================
int SendWirelessConfig(struct element* pElement, unsigned char bTouchInProgress, unsigned char bThisButtonIsSelected)
{
	int nImageIndex = BUTTON_ENABLED_INDEX;
	
	#ifdef DEBUG_MENU_CMDS
	printf("SendWirelessConfig\n");
	#endif	
	if(!bTouchInProgress)
	{
		sendCommand(eCOMMAND_WIRELESS_CONFIG, (wirelessGetOurESN() >> 16) & 0x0000FFFFUL);
	}
	else
	{
		if(bThisButtonIsSelected)
		{
			nImageIndex = BUTTON_PRESSED_INDEX;
		}
		else
		{
			nImageIndex = BUTTON_ENABLED_INDEX;
		}
	}
	
	return nImageIndex;
}
//=================================================
// pushbutton command alarm screen up/Down button pressed.
//=================================================

int AlarmUpDownButtonPress(struct element* theElement, unsigned char bTouchInProgress, unsigned char bThisButtonIsSelected)
{
	int nImageIndex = ALARM_UPDOWN_DOWN_ENABLED_INDEX;
	if(!bTouchInProgress)
	{
		if(ALARM_PAGE_UP_OFFSET == nAlarmUpDownPageOffset)
		{
			nAlarmUpDownPageOffset = ALARM_PAGE_DOWN_OFFSET;
			nImageIndex = ALARM_UPDOWN_UP_ENABLED_INDEX;
		}
		else
		{
			nAlarmUpDownPageOffset = ALARM_PAGE_UP_OFFSET;
			nImageIndex = ALARM_UPDOWN_DOWN_ENABLED_INDEX;
		}
		uiDisplayCurrentPage();
	}
	else
	{
		if(ALARM_PAGE_UP_OFFSET == nAlarmUpDownPageOffset)
		{
			nImageIndex = ALARM_UPDOWN_DOWN_PRESSED_INDEX;	
		}
		else
		{
			nImageIndex = ALARM_UPDOWN_UP_PRESSED_INDEX;
		}

	}
	return nImageIndex;
}
//=================================================
// pushbutton command pop page before proceeding
//=================================================
int PopPageFirst(struct element* theElement, unsigned char bTouchInProgress, unsigned char bThisButtonIsSelected)
{
	int nImageIndex = BUTTON_ENABLED_INDEX;
	if(!bTouchInProgress)
	{
		/////
		// pop the page
		// don't load the previous page
		// don't know which page to go to next
		// so return 0
		/////
		uiDriverPopPage(FALSE);
	}
	else
	{
		if(bThisButtonIsSelected)
		{
			nImageIndex = BUTTON_PRESSED_INDEX;
		}
		else
		{
			nImageIndex = BUTTON_ENABLED_INDEX;
		}
	}
	return nImageIndex;
}

//=================================================
// pushbutton set arrow board auto brightness
//=================================================
int SetArrowBoardAutoBrightness(struct element* theElement, unsigned char bTouchInProgress, unsigned char bThisButtonIsSelected)
{
	int nImageIndex = BUTTON_ENABLED_INDEX;
	if(!bTouchInProgress)
	{	
		eBrightnessControl = eBRIGHTNESS_CONTROL_AUTO;
		sendCommand(eCOMMAND_SET_BRIGHTNESS_CONTROL,eBrightnessControl);
		nImageIndex = BUTTON_SELECTED_INDEX;
		uiRefreshCurrentPage();
	}
	else
	{
		if(bThisButtonIsSelected)
		{
			nImageIndex = BUTTON_PRESSED_INDEX;
		}
		else
		{
			nImageIndex = BUTTON_ENABLED_INDEX;
		}
	}
	
	return nImageIndex;
}
//=================================================
// pushbutton set arrow board brightness to bright
//=================================================
int SetArrowBoardBright(struct element* theElement, unsigned char bTouchInProgress, unsigned char bThisButtonIsSelected)
{
	int nImageIndex = BUTTON_ENABLED_INDEX;
	if(!bTouchInProgress)
	{
		eBrightnessControl = eBRIGHTNESS_CONTROL_MANUAL_BRIGHT;
		sendCommand(eCOMMAND_SET_BRIGHTNESS_CONTROL,eBrightnessControl);
		nImageIndex = BUTTON_SELECTED_INDEX;
		uiRefreshCurrentPage();
	}
	else
	{
		if(bThisButtonIsSelected)
		{
			nImageIndex = BUTTON_PRESSED_INDEX;
		}
		else
		{
			nImageIndex = BUTTON_ENABLED_INDEX;
		}	
	}
	
	return nImageIndex;
}
//=================================================
// pushbutton set arrow board brightness to medium
//=================================================
int SetArrowBoardMedium(struct element* theElement, unsigned char bTouchInProgress, unsigned char bThisButtonIsSelected)
{
	int nImageIndex = BUTTON_ENABLED_INDEX;
	if(!bTouchInProgress)
	{
		eBrightnessControl = eBRIGHTNESS_CONTROL_MANUAL_MEDIUM;
		sendCommand(eCOMMAND_SET_BRIGHTNESS_CONTROL,eBrightnessControl);
		nImageIndex = BUTTON_SELECTED_INDEX;
		uiRefreshCurrentPage();
	}	
	else
	{
		if(bThisButtonIsSelected)
		{
			nImageIndex = BUTTON_PRESSED_INDEX;
		}
		else
		{
			nImageIndex = BUTTON_ENABLED_INDEX;
		}
	}
	
	return nImageIndex;
}
//=================================================
// pushbutton set arrow board brightness to dim
//=================================================
int SetArrowBoardDim(struct element* theElement, unsigned char bTouchInProgress, unsigned char bThisButtonIsSelected)
{
	int nImageIndex = BUTTON_ENABLED_INDEX;
	if(!bTouchInProgress)
	{
		eBrightnessControl = eBRIGHTNESS_CONTROL_MANUAL_DIM;
		sendCommand(eCOMMAND_SET_BRIGHTNESS_CONTROL,eBrightnessControl);
		nImageIndex = BUTTON_SELECTED_INDEX;
		uiRefreshCurrentPage();
	}
	else
	{
		if(bThisButtonIsSelected)
		{
			nImageIndex = BUTTON_PRESSED_INDEX;
		}
		else
		{
			nImageIndex = BUTTON_ENABLED_INDEX;
		}
	}
	
	return nImageIndex;
}

//===================================================================
// pushbutton set handheld night/day (Toggle between Night/Day Mode)
//===================================================================
#define NIGHT_DAY_BUTTON_NIGHT_ENABLED 0
#define NIGHT_DAY_BUTTON_NIGHT_SELECTED 1
#define NIGHT_DAY_BUTTON_DAY_ENABLED 2
#define NIGHT_DAY_BUTTON_DAY_SELECTED 3
int SetNightDay(struct element* theElement, unsigned char bTouchInProgress, unsigned char bThisButtonIsSelected)
{
	int nImageIndex = NIGHT_DAY_BUTTON_NIGHT_ENABLED;
	if(!bTouchInProgress)
	{
		if(bNightMode)
		{
			/////
			// switch to day mode
			/////
			LCD_Set_Display_Brightness(BRIGHTNESS_DAY_MODE);
			bNightMode = FALSE;
			nImageIndex = NIGHT_DAY_BUTTON_NIGHT_ENABLED;
		}
		else
		{	
			/////
			// switch to night mode
			/////
			LCD_Set_Display_Brightness(BRIGHTNESS_NIGHT_MODE);
			bNightMode = TRUE;
			nImageIndex = NIGHT_DAY_BUTTON_DAY_ENABLED;
		}		
		
		uiDisplayCurrentPage();
	}
	else
	{
		if(bNightMode)
		{
			nImageIndex = NIGHT_DAY_BUTTON_DAY_SELECTED;	
		}
		else
		{
			nImageIndex = NIGHT_DAY_BUTTON_NIGHT_SELECTED;
		}
	}
	
	return nImageIndex;
}

//===================================================================
// touchscreen calibrate for testing
//===================================================================
int DoTouchScreenCalibrate(struct element* theElement, unsigned char bTouchInProgress, unsigned char bThisButtonIsSelected)
{
	int nImageIndex = BUTTON_ENABLED_INDEX;
	if(!bTouchInProgress)
	{
		//printf("DoTouchScreenCalibrate\n");
		LCD_TP_Calibrate(TRUE);
		
		//printf("TP Calibration Verification\n");
		LCD_TP_Calibrate(FALSE);
	}	
	else
	{
		if(bThisButtonIsSelected)
		{
			nImageIndex = BUTTON_PRESSED_INDEX;
		}
		else
		{
			nImageIndex = BUTTON_ENABLED_INDEX;
		}
	}	
	return nImageIndex;
}

//===================================================================
// perform software reset
//===================================================================
int DoReset(struct element* theElement, unsigned char bTouchInProgress, unsigned char bThisButtonIsSelected)
{
	int nImageIndex = BUTTON_ENABLED_INDEX;
	if(!bTouchInProgress)
	{
		sendCommand(eCOMMAND_DO_RESET, 0);
		uiDriverPopToMain();
	}
	else
	{
		if(bThisButtonIsSelected)
		{
			nImageIndex = BUTTON_PRESSED_INDEX;
		}
		else
		{
			nImageIndex = BUTTON_ENABLED_INDEX;
		}
	}
	return nImageIndex;
}

int DoCalibrateTiltUp(struct element* theElement, unsigned char bTouchInProgress, unsigned char bThisButtonIsSelected)
{
	int nImageIndex = CALIBRATE_TILT_ENABLED_INDEX;
	if(!bTouchInProgress)
	{
		if(bCalibrateTiltUpInProgress)
		{
			sendCommand(eCOMMAND_DO_ACTUATOR_CALIBRATION, eACTUATOR_LIMIT_CALIBRATE_GRAB_LIMIT);
			bCalibrateTiltUpInProgress = FALSE;
		}
		else
		{
			sendCommand(eCOMMAND_DO_ACTUATOR_CALIBRATION, eACTUATOR_LIMIT_CALIBRATE_BEGIN_UP);
			bCalibrateTiltUpInProgress = TRUE;
			nImageIndex = CALIBRATE_TILT_END_INDEX;
		}
		uiDisplayCurrentPage();
	}
	return nImageIndex;
}
int DoCalibrateTiltDown(struct element* theElement, unsigned char bTouchInProgress, unsigned char bThisButtonIsSelected)
{
	int nImageIndex = CALIBRATE_TILT_ENABLED_INDEX;
	if(!bTouchInProgress)
	{
		if(bCalibrateTiltDownInProgress)
		{
			sendCommand(eCOMMAND_DO_ACTUATOR_CALIBRATION, eACTUATOR_LIMIT_CALIBRATE_GRAB_LIMIT);
			bCalibrateTiltDownInProgress = FALSE;
		}
		else
		{
			sendCommand(eCOMMAND_DO_ACTUATOR_CALIBRATION, eACTUATOR_LIMIT_CALIBRATE_BEGIN_DOWN);
			bCalibrateTiltDownInProgress = TRUE;
			nImageIndex = CALIBRATE_TILT_END_INDEX;
		}
		uiDisplayCurrentPage();
	}
	return nImageIndex;
}
int DoCancelCalibrateTilt(struct element* theElement, unsigned char bTouchInProgress, unsigned char bThisButtonIsSelected)
{
	int nImageIndex = CALIBRATE_TILT_ENABLED_INDEX;
	if(!bTouchInProgress)
	{
			sendCommand(eCOMMAND_DO_ACTUATOR_CALIBRATION, eACTUATOR_LIMIT_CALIBRATE_CANCEL);
			bCalibrateTiltUpInProgress = FALSE;
			bCalibrateTiltDownInProgress = FALSE;
	}
	return nImageIndex;
}

int DoSelectEnglishUI(struct element* theElement, unsigned char bTouchInProgress, unsigned char bThisButtonIsSelected)
{
	int nImageIndex = BUTTON_ENABLED_INDEX;
	if(!bTouchInProgress)
	{
		UpdateDefaultUI(STANDARD_UI);
	}
	else
	{
		if(bThisButtonIsSelected)
		{
			nImageIndex = BUTTON_PRESSED_INDEX;
		}
		else
		{
			nImageIndex = BUTTON_ENABLED_INDEX;
		}
	}
	return nImageIndex;
}
int DoSelectSpanishUI(struct element* theElement, unsigned char bTouchInProgress, unsigned char bThisButtonIsSelected)
{
	int nImageIndex = BUTTON_ENABLED_INDEX;
	if(!bTouchInProgress)
	{
		UpdateDefaultUI(SPANISH_UI);
	}
	else
	{
		if(bThisButtonIsSelected)
		{
			nImageIndex = BUTTON_PRESSED_INDEX;
		}
		else
		{
			nImageIndex = BUTTON_ENABLED_INDEX;
		}
	}
	return nImageIndex;
}
int DoSelectCustomUI(struct element* theElement, unsigned char bTouchInProgress, unsigned char bThisButtonIsSelected)
{
	int nImageIndex = BUTTON_ENABLED_INDEX;
	if(!bTouchInProgress)
	{
		UpdateDefaultUI(CUSTOM_UI);
	}
	else
	{
		if(bThisButtonIsSelected)
		{
			nImageIndex = BUTTON_PRESSED_INDEX;
		}
		else
		{
			nImageIndex = BUTTON_ENABLED_INDEX;
		}
	}
	return nImageIndex;
}

int DoSelectStandardScreen(struct element* theElement, unsigned char bTouchInProgress, unsigned char bThisButtonIsSelected)
{
	int nImageIndex = BUTTON_ENABLED_INDEX;
      
	if(!bTouchInProgress)
	{
		UpdateScreenMode(SCREEN_MODE_STANDARD);
        uiSetFirstPage(PAGE_ID_Main);
   		nImageIndex = BUTTON_SELECTED_INDEX;
		uiRefreshCurrentPage();

	}
	else
	{
		if(bThisButtonIsSelected)
		{
			nImageIndex = BUTTON_PRESSED_INDEX;
		}
		else
		{
			nImageIndex = BUTTON_ENABLED_INDEX;
		}
	}
	return nImageIndex;
}    

int DoSelectSingleScreen(struct element* theElement, unsigned char bTouchInProgress, unsigned char bThisButtonIsSelected)
{
	int nImageIndex = BUTTON_ENABLED_INDEX;
       
	if(!bTouchInProgress)
	{
		UpdateScreenMode(SCREEN_MODE_SINGLE);
        uiSetFirstPage(PAGE_ID_SinglePageMain);
   		nImageIndex = BUTTON_SELECTED_INDEX;
		uiRefreshCurrentPage();
	}
	else
	{
		if(bThisButtonIsSelected)
		{
			nImageIndex = BUTTON_PRESSED_INDEX;
		}
		else
		{
			nImageIndex = BUTTON_ENABLED_INDEX;
		}
	}
	return nImageIndex;
}    


int DoAllLightsOn(struct element* theElement, unsigned char bTouchInProgress, unsigned char bThisButtonIsSelected)
{
	int nImageIndex = BUTTON_ENABLED_INDEX;
	if(!bTouchInProgress)
	{
		sendCommand(eCOMMAND_DISPLAY_CHANGE, eDISPLAY_TYPE_ALL_LIGHTS_ON);
		bMaintModeAllLightsOn = TRUE;
		bMaintModeAllLightsOff = FALSE;
		bMaintModeErroredLightsOff = FALSE;
		nImageIndex = BUTTON_SELECTED_INDEX;
		uiRefreshCurrentPage();
	}
	else
	{
		if(bThisButtonIsSelected)
		{
			nImageIndex = BUTTON_PRESSED_INDEX;
		}
		else
		{
			nImageIndex = BUTTON_ENABLED_INDEX;
		}	
	}
	return nImageIndex;
}
int DoAllLightsOff(struct element* theElement, unsigned char bTouchInProgress, unsigned char bThisButtonIsSelected)
{
	int nImageIndex = BUTTON_ENABLED_INDEX;
	if(!bTouchInProgress)
	{
		sendCommand(eCOMMAND_DISPLAY_CHANGE, eDISPLAY_TYPE_BLANK);
		bMaintModeAllLightsOn = FALSE;
		bMaintModeAllLightsOff = TRUE;
		bMaintModeErroredLightsOff = FALSE;
		nImageIndex = BUTTON_SELECTED_INDEX;
		uiRefreshCurrentPage();
	}
	else
	{
		if(bThisButtonIsSelected)
		{
			nImageIndex = BUTTON_PRESSED_INDEX;
		}
		else
		{
			nImageIndex = BUTTON_ENABLED_INDEX;
		}	
	}
	return nImageIndex;
}
int DoErroredLightsOff(struct element* theElement, unsigned char bTouchInProgress, unsigned char bThisButtonIsSelected)
{
	int nImageIndex = BUTTON_ENABLED_INDEX;
	if(!bTouchInProgress)
	{
		sendCommand(eCOMMAND_DISPLAY_CHANGE, eDISPLAY_TYPE_ERRORED_LIGHTS_OFF);
		bMaintModeAllLightsOn = FALSE;
		bMaintModeAllLightsOff = FALSE;
		bMaintModeErroredLightsOff = TRUE;
		nImageIndex = BUTTON_SELECTED_INDEX;
		uiRefreshCurrentPage();
	}
	else
	{
		if(bThisButtonIsSelected)
		{
			nImageIndex = BUTTON_PRESSED_INDEX;
		}
		else
		{
			nImageIndex = BUTTON_ENABLED_INDEX;
		}	
	}
	return nImageIndex;
}
int DoClonePush(struct element* theElement, unsigned char bTouchInProgress, unsigned char bThisButtonIsSelected)
{
	int nImageIndex = BUTTON_ENABLED_INDEX;
	if(!bTouchInProgress)
	{
		/////
		// send the stored configuration
		/////
		sendCommand(eCOMMAND_SET_ACTUATOR_TYPE, storedConfigGetActuatorType());
		sendCommand(eCOMMAND_SET_DISALLOWED_PATTERNS, storedConfigGetDisallowedPatterns());
		sendCommand(eCOMMAND_SET_BRIGHTNESS_CONTROL, storedConfigGetArrowBoardBrightnessControl());
		sendCommand(eCOMMAND_SET_ACTUATOR_BUTTON_MODE, storedConfigGetActuatorButtonMode());
		
		/////
		// and get the config back again
		// so this UI can use it
		/////
		sendCommand(eCOMMAND_GET_ACTUATOR_TYPE, 0);
		sendCommand(eCOMMAND_GET_DISALLOWED_PATTERNS, 0);
		sendCommand(eCOMMAND_GET_BRIGHTNESS_CONTROL, 0);
		sendCommand(eCOMMAND_GET_AUX_BATTERY_TYPE, 0);
		sendCommand(eCOMMAND_GET_ACTUATOR_BUTTON_MODE, 0);
		uiDisplayCurrentPage();
	}
	else
	{
		if(bThisButtonIsSelected)
		{
			nImageIndex = BUTTON_PRESSED_INDEX;
		}
		else
		{
			nImageIndex = BUTTON_ENABLED_INDEX;
		}
	}
	return nImageIndex;
}
int DoClonePull(struct element* theElement, unsigned char bTouchInProgress, unsigned char bThisButtonIsSelected)
{
	int nImageIndex = BUTTON_ENABLED_INDEX;
	if(!bTouchInProgress)
	{
		sendCommand(eCOMMAND_GET_ACTUATOR_TYPE, 0);
		sendCommand(eCOMMAND_GET_DISALLOWED_PATTERNS, 0);
		sendCommand(eCOMMAND_GET_BRIGHTNESS_CONTROL, 0);
		sendCommand(eCOMMAND_GET_AUX_BATTERY_TYPE, 0);
		sendCommand(eCOMMAND_GET_ACTUATOR_BUTTON_MODE, 0);
		bCloneGettingData = TRUE;
		bCloneGotBrightnessControl  = FALSE;
		bCloneGotDisallowedPatterns = FALSE;
		bCloneGotActuatorType = FALSE;
		bCloneGotAuxBatteryType = FALSE;
		bCloneGotActuatorButtonMode = FALSE;
		nImageIndex = BUTTON_SELECTED_INDEX;
		uiDisplayCurrentPage();
	}
	else
	{
		if(bThisButtonIsSelected)
		{
			nImageIndex = BUTTON_PRESSED_INDEX;
		}
		else
		{
			nImageIndex = BUTTON_ENABLED_INDEX;
		}	
	}
	return nImageIndex;
}
int DoSetActuatorButtonAutoMode(struct element* theElement, unsigned char bTouchInProgress, unsigned char bThisButtonIsSelected)
{
#ifndef ALLOW_ACTUATOR_AUTO_MODE
	int nImageIndex = BUTTON_DISABLED_INDEX;	
#else
	int nImageIndex = BUTTON_ENABLED_INDEX;
	if(!bTouchInProgress)
	{
		eActuatorButtonMode = eACTUATOR_BUTTON_AUTO_MODE;
		sendCommand(eCOMMAND_SET_ACTUATOR_BUTTON_MODE, eACTUATOR_BUTTON_AUTO_MODE);
		nImageIndex = BUTTON_SELECTED_INDEX;
		uiRefreshCurrentPage();
	}
	else
	{
		if(bThisButtonIsSelected)
		{
			nImageIndex = BUTTON_PRESSED_INDEX;
		}
		else
		{
			nImageIndex = BUTTON_ENABLED_INDEX;
		}
	}
#endif
	return nImageIndex;
}
int DoSetActuatorButtonManualMode(struct element* theElement, unsigned char bTouchInProgress, unsigned char bThisButtonIsSelected)
{
	int nImageIndex = BUTTON_ENABLED_INDEX;
	if(!bTouchInProgress)
	{
		eActuatorButtonMode = eACTUATOR_BUTTON_MANUAL_MODE;
		sendCommand(eCOMMAND_SET_ACTUATOR_BUTTON_MODE, eACTUATOR_BUTTON_MANUAL_MODE);
		nImageIndex = BUTTON_SELECTED_INDEX;
		uiRefreshCurrentPage();
	}
	else
	{
		if(bThisButtonIsSelected)
		{
			nImageIndex = BUTTON_PRESSED_INDEX;
		}
		else
		{
			nImageIndex = BUTTON_ENABLED_INDEX;
		}	
	}
	return nImageIndex;
}
int DoTogglePatternSelectAllow(struct element* theElement, unsigned char bTouchInProgress, unsigned char bThisButtonIsSelected)
{
	int nImageIndex = BUTTON_ENABLED_INDEX;
	if(!bTouchInProgress)
	{
		RADIO_BUTTON_SELECTION_ELEMENT *pRadioButtonElement = (RADIO_BUTTON_SELECTION_ELEMENT*)theElement;
		if(isPatternAllowed((eDISPLAY_TYPES)pRadioButtonElement->nData))
		{
			disallowPattern((eDISPLAY_TYPES)pRadioButtonElement->nData);
			nImageIndex = BUTTON_SELECTED_INDEX;
		}
		else
		{
			allowPattern((eDISPLAY_TYPES)pRadioButtonElement->nData);
		}
		uiRefreshCurrentPage();
	}
	else
	{
		if(bThisButtonIsSelected)
		{
			nImageIndex = BUTTON_PRESSED_INDEX;
		}
		else
		{
			nImageIndex = BUTTON_ENABLED_INDEX;
		}
	}
	return nImageIndex;
}
int DoSetControllerDefaults(struct element* theElement, unsigned char bTouchInProgress, unsigned char bThisButtonIsSelected)
{
	int nImageIndex = BUTTON_ENABLED_INDEX;
	if(!bTouchInProgress)
	{
		storedConfigSetDefaultUI(STANDARD_UI);
	}
	else
	{
		if(bThisButtonIsSelected)
		{
			nImageIndex = BUTTON_PRESSED_INDEX;
		}
		else
		{
			nImageIndex = BUTTON_ENABLED_INDEX;
		}
	}
	return nImageIndex;;
}
int DoSetTiltFrame180(struct element* theElement, unsigned char bTouchInProgress, unsigned char bThisButtonIsSelected)
{
	int nImageIndex = BUTTON_ENABLED_INDEX;
	if(!bTouchInProgress)
	{
		eActuatorType = eACTUATOR_TYPE_180_DEGREE_POWER_TILT;
		sendCommand(eCOMMAND_SET_ACTUATOR_TYPE, eActuatorType);
		nImageIndex = BUTTON_SELECTED_INDEX;
		uiRefreshCurrentPage();
	}
	else
	{
		if(bThisButtonIsSelected)
		{
			nImageIndex = BUTTON_PRESSED_INDEX;
		}
		else
		{
			nImageIndex = BUTTON_ENABLED_INDEX;
		}
	}
	return nImageIndex;
}
int DoSetTiltFrame90(struct element* theElement, unsigned char bTouchInProgress, unsigned char bThisButtonIsSelected)
{
	int nImageIndex = BUTTON_ENABLED_INDEX;
	if(!bTouchInProgress)
	{
		eActuatorType = eACTUATOR_TYPE_90_DEGREE_POWER_TILT;
		sendCommand(eCOMMAND_SET_ACTUATOR_TYPE, eActuatorType);
		nImageIndex = BUTTON_SELECTED_INDEX;
		uiRefreshCurrentPage();
	}
	else
	{
		if(bThisButtonIsSelected)
		{
			nImageIndex = BUTTON_PRESSED_INDEX;
		}
		else
		{
			nImageIndex = BUTTON_ENABLED_INDEX;
		}
	}
	return nImageIndex;
}
int DoSetTiltFrameNone(struct element* theElement, unsigned char bTouchInProgress, unsigned char bThisButtonIsSelected)
{
	int nImageIndex = BUTTON_ENABLED_INDEX;
	if(!bTouchInProgress)
	{
		eActuatorType = eACTUATOR_TYPE_NONE;
		sendCommand(eCOMMAND_SET_ACTUATOR_TYPE, eActuatorType);
		nImageIndex = BUTTON_SELECTED_INDEX;
		uiRefreshCurrentPage();
	}
	else
	{
		if(bThisButtonIsSelected)
		{
			nImageIndex = BUTTON_PRESSED_INDEX;
		}
		else
		{
			nImageIndex = BUTTON_ENABLED_INDEX;
		}
	}
	return nImageIndex;
}
//=================================================
// Which image to display functions
//=================================================
//=================================================
// Is Radio Button selected
//=================================================
int IsSelectedPatternButton(struct element* theElement)
{
	int nRetVal = BUTTON_DISABLED_INDEX;
	RADIO_BUTTON_SELECTION_ELEMENT *pRadioButtonElement = (RADIO_BUTTON_SELECTION_ELEMENT*)theElement;

	if(isPatternAllowed((eDISPLAY_TYPES)pRadioButtonElement->nData))	
	{
		nRetVal = BUTTON_ENABLED_INDEX;
		if(getCurrentDisplayType() == (eDISPLAY_TYPES)pRadioButtonElement->nData)
		{
			nRetVal = BUTTON_SELECTED_INDEX;
		}
	}
	return nRetVal;
}
int ButtonImageOffsetWigWag(struct element* theElement)
{
	int nOffset = 0;
	switch(uiDriverGetModel())
	{
		case eMODEL_VEHICLE_25_LIGHT_SEQUENTIAL:
		case eMODEL_TRAILER_25_LIGHT_SEQUENTIAL:
		case eMODEL_VEHICLE_15_LIGHT_SEQUENTIAL:
		case eMODEL_TRAILER_15_LIGHT_SEQUENTIAL:
		case eMODEL_VEHICLE_15_LIGHT_FLASHING:
		case eMODEL_TRAILER_15_LIGHT_FLASHING:
		case eMODEL_NONE:
		case eMODEL_TRAILER_UPDATING:
			break;

		case eMODEL_TRAILER_15_LIGHT_WIG_WAG:
		case eMODEL_VEHICLE_15_LIGHT_WIG_WAG:
			/////
			// wig wag buttons are 4 over from four corner buttons
			nOffset = 4;
			break;
	}
	return nOffset;
	
}

int IsSelectedEnglish(struct element* theElement)
{
	return (STANDARD_UI == storedConfigGetDefaultUI());
}
int IsSelectedSpanish(struct element* theElement)
{
	return (SPANISH_UI == storedConfigGetDefaultUI());
}
int IsSelectedCustom(struct element* theElement)
{
	return (CUSTOM_UI == storedConfigGetDefaultUI());
}


/////
// these will be used in the helper functions below
// that determine which status lines to show
// STATUS_ALARM_ HIGH is when Up/Down button is on Up (shows Down)
// STATUS_ALARM_ LOW is when Up/Down button is on Down (shows Up)
#define STATUS_ALARM_INDEX_HIGH_ON 0
#define STATUS_ALARM_INDEX_HIGH_OFF 1
#define STATUS_ALARM_INDEX_LOW_ON 2
#define STATUS_ALARM_INDEX_LOW_OFF 3

int WhichImageAlarmUpDownButton(struct element* theElement)
{
	int nImageIndex = ALARM_UPDOWN_DOWN_ENABLED_INDEX;
	if(ALARM_PAGE_UP_OFFSET == nAlarmUpDownPageOffset)
	{
		nImageIndex = ALARM_UPDOWN_UP_ENABLED_INDEX;
	}
	return nImageIndex;
}
int WhichImageAggregateAlarm(struct element* theElement)
{
	int nIndex = 2;
	if(devicesAreConnected())
	{
		switch(getAlarms())
		{
			case ALARM_LEVEL_NONE:
			default:
				nIndex = 0;
				break;
			case ALARM_LEVEL_MEDIUM:
				nIndex = 1;
				break;
			case ALARM_LEVEL_HIGH:
				nIndex = 2;
				break;
		}
	}
	return nIndex;
}

int WhichImageAlarmLevelBattRevDispError(struct element* theElement)
{
	int nIndex = STATUS_ALARM_INDEX_HIGH_ON;
	WORD alarmBitmap = getAlarmBitmap();
    
	if(0 == (alarmBitmap & ALARM_BITMAP_LAMPS_DISABLED))
	{
        nIndex = STATUS_ALARM_INDEX_HIGH_OFF;
    }

	return nIndex;
}

int WhichImageAlarmLevelConnLowBatt(struct element* theElement)
{
	int nIndex = STATUS_ALARM_INDEX_HIGH_ON;
    
    if(devicesAreConnected())
	{
        nIndex = STATUS_ALARM_INDEX_HIGH_OFF;
	}
	
	return nIndex;
}

int WhichImageAlarmLevelControlTempAuxError(struct element* theElement)
{
	int nIndex = STATUS_ALARM_INDEX_HIGH_ON;
	WORD alarmBitmap = getAlarmBitmap();
	if(ALARM_PAGE_UP_OFFSET == nAlarmUpDownPageOffset)
	{
		if(0 == (alarmBitmap & ALARM_BITMAP_OVER_TEMP))
		{
			nIndex = STATUS_ALARM_INDEX_HIGH_OFF;
		}
	}
	else
	{
		nIndex = STATUS_ALARM_INDEX_LOW_ON;
		if(0 == (alarmBitmap & ALARM_BITMAP_AUX_ERRORS))
		{
			nIndex = STATUS_ALARM_INDEX_LOW_OFF;
		}
	}
	return nIndex;
}
int WhichImageAlarmLevel(struct element* theElement)
{
	int nIndex = 0;
	return nIndex;
}

int WhichImageAlarmLevelLVDLowVolt(struct element* theElement)
{
	int nIndex = STATUS_ALARM_INDEX_HIGH_ON;
	WORD alarmBitmap = getAlarmBitmap();
    
    if(0 == (alarmBitmap & ALARM_BITMAP_LOW_BATTERY))
	{
        nIndex = STATUS_ALARM_INDEX_HIGH_OFF;
	}
    
	return nIndex;
}

int WhichImageAlarmLevelLineRevIndError(struct element* theElement)
{
	int nIndex = STATUS_ALARM_INDEX_HIGH_ON;
	WORD alarmBitmap = getAlarmBitmap();
    
	if(0 == (alarmBitmap & ALARM_BITMAP_OVER_TEMP))
	{
        nIndex = STATUS_ALARM_INDEX_HIGH_OFF;
	}

	return nIndex;
}

/////
// state image index are from 2-4
// (these are the colored blocks next to the button
/////
#define STATUS_ALARM_STATE_IMAGE_GOOD_INDEX 2
#define STATUS_ALARM_STATE_IMAGE_BAD_INDEX 3
#define STATUS_ALARM_STATE_IMAGE_VERY_BAD_INDEX 4
int WhichImageAlarmState(struct element* theElement)
{

	int nIndex = STATUS_ALARM_STATE_IMAGE_VERY_BAD_INDEX;
	if(devicesAreConnected())
	{
		switch(getAlarms())
		{
			case ALARM_LEVEL_NONE:
			default:
				nIndex = STATUS_ALARM_STATE_IMAGE_GOOD_INDEX;
				break;
			case ALARM_LEVEL_MEDIUM:
				nIndex = STATUS_ALARM_STATE_IMAGE_BAD_INDEX;
				break;
			case ALARM_LEVEL_HIGH:
				nIndex = STATUS_ALARM_STATE_IMAGE_VERY_BAD_INDEX;
				break;
		}
	}
	return nIndex;
}


int WhichImageBatteryVoltage(struct element* theElement)
{
    WORD nVolts = getBatteryVoltage();
	int nIndex = 4-((nVolts-TRAILER_MOUNT_MIN_LINE_VOLTS)/((TRAILER_MOUNT_MAX_LINE_VOLTS-TRAILER_MOUNT_MIN_LINE_VOLTS)/N_VOLT_DIVISIONS));
    
	if(4 < nIndex)
	{
        nIndex = 4;
    }
    if(0 > nIndex)
    {
        nIndex = 0;
    }
	
	//printf("nVolts[%d] nIndex[%d]\n", nVolts, nIndex);
	return nIndex;
}


int WhichImageLineCurrentTensDigit(struct element* theElement)
{
	int nIndex = (getLineCurrent()/1000)%10;
	return nIndex;
}
int WhichImageLineCurrentOnesDigit(struct element* theElement)
{
	int nIndex = (getLineCurrent()/100)%10;
	return nIndex;
}
int WhichImageLineCurrentTenthsDigit(struct element* theElement)
{
	int nIndex = (getLineCurrent()/10)%10;
	return nIndex;
}
int WhichImageSystemCurrentTensDigit(struct element* theElement)
{
	int nIndex = (getSystemCurrent()/1000)%10;
	return nIndex;
}
int WhichImageSystemCurrentOnesDigit(struct element* theElement)
{
	int nIndex = (getSystemCurrent()/100)%10;
	return nIndex;
}
int WhichImageSystemCurrentTenthsDigit(struct element* theElement)
{
	int nIndex = (getSystemCurrent()/10)%10;
	return nIndex;
}
int WhichImageActuatorDown(struct element* theElement)
{
	int nIndex = BUTTON_ENABLED_INDEX;
	if(eACTUATOR_BUTTON_AUTO_MODE == eActuatorButtonMode)
	{
		nIndex = BUTTON_ENABLED_INDEX+ACTUATOR_AUTO_INDEX_OFFSET;
	}
	return nIndex;
}
int WhichImageActuatorUp(struct element* theElement)
{
	int nIndex = BUTTON_ENABLED_INDEX;
	if(eACTUATOR_BUTTON_AUTO_MODE == eActuatorButtonMode)
	{
		nIndex = BUTTON_ENABLED_INDEX+ACTUATOR_AUTO_INDEX_OFFSET;
	}
	return nIndex;
}
int WhichImageActuatorLeft(struct element* theElement)
{
	int nIndex = BUTTON_ENABLED_INDEX;
	if(eACTUATOR_BUTTON_AUTO_MODE == eActuatorButtonMode)
	{
		nIndex = BUTTON_ENABLED_INDEX+ACTUATOR_AUTO_INDEX_OFFSET;
	}
	return nIndex;
}
int WhichImageActuatorRight(struct element* theElement)
{
	int nIndex = BUTTON_ENABLED_INDEX;
	if(eACTUATOR_BUTTON_AUTO_MODE == eActuatorButtonMode)
	{
		nIndex = BUTTON_ENABLED_INDEX+ACTUATOR_AUTO_INDEX_OFFSET;
	}
	return nIndex;
}
int WhichImageActuatorNeutral(struct element* theElement)
{
	int nIndex = BUTTON_ENABLED_INDEX;
	if(eACTUATOR_BUTTON_AUTO_MODE == eActuatorButtonMode)
	{
		nIndex = BUTTON_ENABLED_INDEX+ACTUATOR_AUTO_INDEX_OFFSET;
	}
	return nIndex;
}
int WhichImageSystemState(struct element* theElement)
{
	/////
	// currently only two system states
	// good and bad
	/////
	int nIndex = STATUS_ALARM_STATE_IMAGE_GOOD_INDEX;
	if(!devicesAreConnected())
	{
		nIndex = STATUS_ALARM_STATE_IMAGE_VERY_BAD_INDEX;
	}
	return nIndex;
}

int WhichImageVoltageState(struct element* theElement)
{
	int nIndex = STATUS_ALARM_STATE_IMAGE_GOOD_INDEX;
	switch(uiDriverGetModel())
	{
		default:
		case eMODEL_NONE:
		case eMODEL_TRAILER_UPDATING:
			break;
		case eMODEL_VEHICLE_25_LIGHT_SEQUENTIAL:
		case eMODEL_VEHICLE_15_LIGHT_SEQUENTIAL:
		case eMODEL_VEHICLE_15_LIGHT_FLASHING:
		case eMODEL_VEHICLE_15_LIGHT_WIG_WAG:
			if(VEHICLE_MOUNT_LINE_VOLTS_OK > getBatteryVoltage())
			{
				nIndex = STATUS_ALARM_STATE_IMAGE_BAD_INDEX;
				if(VEHICLE_MOUNT_LINE_VOLTS_LOW > getBatteryVoltage())
				{
					nIndex = STATUS_ALARM_STATE_IMAGE_VERY_BAD_INDEX;
				}
			}
			break;
	
        // Solar Trailer
		case eMODEL_TRAILER_25_LIGHT_SEQUENTIAL:
		case eMODEL_TRAILER_15_LIGHT_SEQUENTIAL:
		case eMODEL_TRAILER_15_LIGHT_FLASHING:
		case eMODEL_TRAILER_15_LIGHT_WIG_WAG:
            if(TRAILER_MOUNT_LINE_VOLTS_OK > getBatteryVoltage())
            {
                nIndex = STATUS_ALARM_STATE_IMAGE_BAD_INDEX;
                if(TRAILER_MOUNT_LINE_VOLTS_LOW > getBatteryVoltage())
                {
                    nIndex = STATUS_ALARM_STATE_IMAGE_VERY_BAD_INDEX;
				}
            }
			break;
	}

    // don't want to change state to less bad than it is
	if(STATUS_ALARM_STATE_IMAGE_VERY_BAD_INDEX != nIndex)
	{
        if(TRAILER_MOUNT_LINE_VOLTS_OK > getBatteryVoltage())
		{
            nIndex = STATUS_ALARM_STATE_IMAGE_BAD_INDEX;
            if(TRAILER_MOUNT_LINE_VOLTS_LOW > getBatteryVoltage())
            {
                nIndex = STATUS_ALARM_STATE_IMAGE_VERY_BAD_INDEX;
            }
        }	
    }

    return nIndex;
}
//============================================================
// arrow board brightness radio button set
// which image is selected
//============================================================
int WhichImageIsAutoBrightSelected(struct element* theElement)
{
	int nIndex = BUTTON_ENABLED_INDEX;
	
	if(eBRIGHTNESS_CONTROL_AUTO == eBrightnessControl)
	{
		nIndex = BUTTON_SELECTED_INDEX;
	}
	return nIndex;
}
int WhichImageIsBrightSelected(struct element* theElement)
{
	int nIndex = BUTTON_ENABLED_INDEX;
	if(eBRIGHTNESS_CONTROL_MANUAL_BRIGHT == eBrightnessControl)
	{
		nIndex = BUTTON_SELECTED_INDEX;
	}
	return nIndex;
}
int WhichImageIsDimSelected(struct element* theElement)
{
	int nIndex = BUTTON_ENABLED_INDEX;
	if(eBRIGHTNESS_CONTROL_MANUAL_DIM == eBrightnessControl)
	{
		nIndex = BUTTON_SELECTED_INDEX;
	}
	return nIndex;
}
//============================================================
// Handheld RSSI
//============================================================
int WhichImageRssiDigit1(struct element* theElement)
{
  int nIndex = ABS16( getRssi( ) ); // For handheld
  
  // Make sure we're communicating wirelessly and a have sane rssi
  if( (100 < nIndex) || (eINTERFACE_WIRELESS != whichInterfaceIsActive( )) )
  {
    nIndex = 99;
  }
  nIndex = (nIndex/10) % 10;
	return nIndex;
}
int WhichImageRssiDigit2(struct element* theElement)
{
  int nIndex = ABS16( getRssi( ) ); // For hand held
  
  // Make sure we're communicating wirelessly and a have sane rssi
  if( (100 < nIndex) || (eINTERFACE_WIRELESS != whichInterfaceIsActive( )) )
  {
    nIndex = 99;
  }
  nIndex = (nIndex/1) % 10;
	return nIndex;
}
//============================================================
// handheld night/day button
// which image to show
//============================================================

int WhichImageNightDay(struct element* theElement)
{
	int nIndex = NIGHT_DAY_BUTTON_NIGHT_ENABLED;
	/////
	// if night mode
	// then the button should say "DAY"
	/////
	if(bNightMode)
	{
		nIndex = NIGHT_DAY_BUTTON_DAY_ENABLED;
	}
	return nIndex;
}
int WhichImageIsCalibrateTiltUpInProgress(struct element* theElement)
{
	int nImageIndex = CALIBRATE_TILT_ENABLED_INDEX;
	if(bCalibrateTiltUpInProgress)
	{
		nImageIndex = CALIBRATE_TILT_END_INDEX;
	}
	if(bCalibrateTiltDownInProgress)
	{
		nImageIndex = CALIBRATE_TILT_DISABLED_INDEX;
	}
	return nImageIndex;
}
int WhichImageIsCalibrateTiltDownInProgress(struct element* theElement)
{
	int nImageIndex = CALIBRATE_TILT_ENABLED_INDEX;
	if(bCalibrateTiltDownInProgress)
	{
		nImageIndex = CALIBRATE_TILT_END_INDEX;
	}
	if(bCalibrateTiltUpInProgress)
	{
		nImageIndex = CALIBRATE_TILT_DISABLED_INDEX;
	}
	return nImageIndex;
}

int WhichImageSetActuatorButtonModeAuto(struct element* theElement)
{
	int nImageIndex = BUTTON_ENABLED_INDEX;
	if(eACTUATOR_BUTTON_AUTO_MODE == eActuatorButtonMode)
	{
		nImageIndex = BUTTON_SELECTED_INDEX;
	}
	return nImageIndex;
}
int WhichImageSetActuatorButtonModeManual(struct element* theElement)
{
	int nImageIndex = BUTTON_ENABLED_INDEX;
	if(eACTUATOR_BUTTON_MANUAL_MODE == eActuatorButtonMode)
	{
		nImageIndex = BUTTON_SELECTED_INDEX;
	}
	return nImageIndex;
}
int WhichImagePatternSelectAllow(struct element* theElement)
{
	int nImageIndex = BUTTON_SELECTED_INDEX;
	RADIO_BUTTON_SELECTION_ELEMENT *pRadioButtonElement = (RADIO_BUTTON_SELECTION_ELEMENT*)theElement;
	if(isPatternAllowed((eDISPLAY_TYPES)pRadioButtonElement->nData))
	{
		nImageIndex = BUTTON_ENABLED_INDEX;
	}
	return nImageIndex;
}
int WhichImageTiltFrame180(struct element* theElement)
{
	int nImageIndex = BUTTON_ENABLED_INDEX;
	if(eACTUATOR_TYPE_180_DEGREE_POWER_TILT == eActuatorType)
	{
		nImageIndex = BUTTON_SELECTED_INDEX;
	}
	return nImageIndex;
}
int WhichImageTiltFrame90(struct element* theElement)
{
	int nImageIndex = BUTTON_ENABLED_INDEX;
	if(eACTUATOR_TYPE_90_DEGREE_POWER_TILT == eActuatorType)
	{
		nImageIndex = BUTTON_SELECTED_INDEX;
	}
	return nImageIndex;
}
int WhichImageTiltFrameNone(struct element* theElement)
{
	int nImageIndex = BUTTON_ENABLED_INDEX;
	if(eACTUATOR_TYPE_NONE == eActuatorType ||
					eACTUATOR_TYPE_UKNOWN == eActuatorType)
	{
		nImageIndex = BUTTON_SELECTED_INDEX;
	}
	return nImageIndex;
}
int WhichImageCloneGettingData(struct element* theElement)
{
	int nImageIndex = BUTTON_SELECTED_INDEX;
	if(bCloneGettingData)
	{
		if(bCloneGotBrightnessControl & 
			 bCloneGotDisallowedPatterns & 
			 bCloneGotActuatorType & 
			 bCloneGotAuxBatteryType &
			 bCloneGotActuatorButtonMode)
		{
			nImageIndex = BUTTON_ENABLED_INDEX;
		}
	}
	else
	{
			nImageIndex = BUTTON_ENABLED_INDEX;
	}
		
	return nImageIndex;
}

int WhichImageMaintModeAllLightsOn(struct element* theElement)
{
	int nImageIndex = BUTTON_ENABLED_INDEX;
	if(bMaintModeAllLightsOn)
	{
		nImageIndex = BUTTON_SELECTED_INDEX;
	}
		
	return nImageIndex;
}

int WhichImageMaintModeAllLightsOff(struct element* theElement)
{
	int nImageIndex = BUTTON_ENABLED_INDEX;
	if(bMaintModeAllLightsOff)
	{
		nImageIndex = BUTTON_SELECTED_INDEX;
	}
		
	return nImageIndex;
}
int WhichImageMaintModeErroredLightsOff(struct element* theElement)
{
	int nImageIndex = BUTTON_ENABLED_INDEX;
	if(bMaintModeErroredLightsOff)
	{
		nImageIndex = BUTTON_SELECTED_INDEX;
	}
		
	return nImageIndex;
}
int WhichTiltButtonImage(struct element* theElement)
{
	int nImageIndex = BUTTON_DISABLED_INDEX;
	switch(eActuatorType)
	{
		case eACTUATOR_TYPE_UKNOWN:
		case eACTUATOR_TYPE_NONE:
			nImageIndex = BUTTON_DISABLED_INDEX;
			break;
		case eACTUATOR_TYPE_90_DEGREE_POWER_TILT:
		case eACTUATOR_TYPE_180_DEGREE_POWER_TILT:
			switch(getActuatorLimit())
			{
				default:
				case eACTUATOR_LIMIT_NONE:
				case eACTUATOR_LIMIT_ERROR:
					nImageIndex = ACTUATOR_POSITION_UNKNOWN_INDEX;
					break;
				case eACTUATOR_LIMIT_BOTTOM:
					nImageIndex = ACTUATOR_POSITION_DOWN_INDEX;
					break;
				case eACTUATOR_LIMIT_TOP:
					nImageIndex = ACTUATOR_POSITION_UP_INDEX;
					break;

			} 
	}
	return nImageIndex;
}
int WhichImageStatusStyleButton(struct element* theElement)
{
	int nImageIndex = ARROW_BOARD_STYLE_VEHICLE_INDEX;
	switch(uiDriverGetModel())
	{
		default:
		case eMODEL_NONE:
			// "STYLE\rUPDATING..."
			nImageIndex = 4;
			break;
		case eMODEL_VEHICLE_25_LIGHT_SEQUENTIAL:
		case eMODEL_VEHICLE_15_LIGHT_SEQUENTIAL:
		case eMODEL_VEHICLE_15_LIGHT_FLASHING:
		case eMODEL_VEHICLE_15_LIGHT_WIG_WAG:
			nImageIndex = ARROW_BOARD_STYLE_VEHICLE_INDEX;
			break;
	
	//////////////////////////////////////////
	//// Solar Trailer
	//////////////////////////////////////////
		case eMODEL_TRAILER_25_LIGHT_SEQUENTIAL:
		case eMODEL_TRAILER_15_LIGHT_SEQUENTIAL:
		case eMODEL_TRAILER_15_LIGHT_FLASHING:
		case eMODEL_TRAILER_15_LIGHT_WIG_WAG:
			nImageIndex = ARROW_BOARD_STYLE_TRAILER_INDEX;
			break;
	}
	return nImageIndex;
}
int WhichImageStatusArrowBoardButton(struct element* theElement)
{
	int nImageIndex = ARROW_BOARD_TYPE_25_LIGHT_INDEX;
	switch(uiDriverGetModel())
	{
		default:
		case eMODEL_NONE:
			// "ARROW BOARD\rUPDATING..."
			nImageIndex = 4;
			break;
		case eMODEL_VEHICLE_25_LIGHT_SEQUENTIAL:
		case eMODEL_TRAILER_25_LIGHT_SEQUENTIAL:

			nImageIndex = ARROW_BOARD_TYPE_25_LIGHT_INDEX;
			break;
	
	//////////////////////////////////////////
	//// Solar Trailer
	//////////////////////////////////////////
		case eMODEL_VEHICLE_15_LIGHT_SEQUENTIAL:
		case eMODEL_VEHICLE_15_LIGHT_FLASHING:
		case eMODEL_VEHICLE_15_LIGHT_WIG_WAG:
		case eMODEL_TRAILER_15_LIGHT_SEQUENTIAL:
		case eMODEL_TRAILER_15_LIGHT_FLASHING:
		case eMODEL_TRAILER_15_LIGHT_WIG_WAG:
			nImageIndex = ARROW_BOARD_TYPE_15_LIGHT_INDEX;
			break;
	}
	return nImageIndex;
}
int WhichImageStatusElectricActuatorButton(struct element* theElement)
{
	int nImageIndex;
	
	switch(eActuatorType)
	{	
        default:
		case eACTUATOR_TYPE_UKNOWN:
			// "ACTUATOR TYPE\rUPDATING..."
			nImageIndex = ACTUATOR_TYPE_UPDATING_INDEX;
		  break;
		case eACTUATOR_TYPE_NONE:
			nImageIndex = ACTUATOR_TYPE_NONE_BUTTON_INDEX;
			break;
		case eACTUATOR_TYPE_90_DEGREE_POWER_TILT:
			nImageIndex = ACTUATOR_TYPE_90_BUTTON_INDEX;
			break;
		case eACTUATOR_TYPE_180_DEGREE_POWER_TILT:
			nImageIndex = ACTUATOR_TYPE_180_BUTTON_INDEX;
			break;
	}
    
	return nImageIndex;
}
int WhichImageStatusControllerSoftwareButton(struct element* theElement)
{
	int nImageIndex = 2;
	return nImageIndex;
}
int WhichImageStatusDriverSoftwareButton(struct element* theElement)
{
	int nImageIndex = 2;
	return nImageIndex;
}
int WhichImagePaired(struct element* theElement)
{
	int nImageIndex = BUTTON_DISABLED_INDEX;
	if(IsEnabledWiredConnection(theElement))
	{
		//printf("1-WhichImagePaired[%d]\n", devicesAreConnected());
		if(devicesAreConnected())
		{
			//printf("2-WhichImagePaired[%d]\n", devicesAreConnected());
			nImageIndex = BUTTON_SELECTED_INDEX;
		}
		else
		{
			//printf("3-WhichImagePaired[%d]\n", devicesAreConnected());
			nImageIndex = BUTTON_ENABLED_INDEX;
		}
	}
	return nImageIndex;
}

//=======================================
// Hand Held (Controller) Version Number
//=======================================
int WhichImageControllerVersionDigit1(struct element* theElement)
{
	int nIndex = (getHandHeldSoftwareVersion()/10000)%10;
	//printf("Digit1: HandHeldSoftwareVersion[%d] index[%d]\n",getHandHeldSoftwareVersion(),nIndex);
	return nIndex;
}
int WhichImageControllerVersionDigit2(struct element* theElement)
{
	int nIndex = (getHandHeldSoftwareVersion()/1000)%10;	
	//printf("Digit2: HandHeldSoftwareVersion[%d] index[%d]\n",getHandHeldSoftwareVersion(),nIndex);
	return nIndex;
}
int WhichImageControllerVersionDigit3(struct element* theElement)
{
	int nIndex = (getHandHeldSoftwareVersion()/100)%10;	
	//printf("Digit3: HandHeldSoftwareVersion[%d] index[%d]\n",getHandHeldSoftwareVersion(),nIndex);
	return nIndex;
}
int WhichImageControllerVersionDigit4(struct element* theElement)
{
	int nIndex = (getHandHeldSoftwareVersion()/10)%10;	
	//printf("Digit4: HandHeldSoftwareVersion[%d] index[%d]\n",getHandHeldSoftwareVersion(),nIndex);
	return nIndex;
}
int WhichImageControllerVersionDigit5(struct element* theElement)
{
	int nIndex = (getHandHeldSoftwareVersion()%10);	
	//printf("Digit5: HandHeldSoftwareVersion[%d] index[%d]\n",getHandHeldSoftwareVersion(),nIndex);
	return nIndex;
}

//================================
// Driver Board Version Number
//================================
int WhichImageDriverVersionDigit1(struct element* theElement)
{
	int nIndex = (getDriverBoardSoftwareVersion()/10000)%10;
	//printf("Digit1: DriverBoardSoftwareVersion[%d] index[%d]\n",getDriverBoardSoftwareVersion(),nIndex);
	return nIndex;
}
int WhichImageDriverVersionDigit2(struct element* theElement)
{
	int nIndex = (getDriverBoardSoftwareVersion()/1000)%10;
	//printf("Digit2: DriverBoardSoftwareVersion[%d] index[%d]\n",getDriverBoardSoftwareVersion(),nIndex);
	return nIndex;
}
int WhichImageDriverVersionDigit3(struct element* theElement)
{
	int nIndex = (getDriverBoardSoftwareVersion()/100)%10;
	//printf("Digit3: DriverBoardSoftwareVersion[%d] index[%d]\n",getDriverBoardSoftwareVersion(),nIndex);
	return nIndex;
}
int WhichImageDriverVersionDigit4(struct element* theElement)
{
	int nIndex = (getDriverBoardSoftwareVersion()/10)%10;
	//printf("Digit4: DriverBoardSoftwareVersion[%d] index[%d]\n",getDriverBoardSoftwareVersion(),nIndex);
	return nIndex;
}
int WhichImageDriverVersionDigit5(struct element* theElement)
{
	int nIndex = (getDriverBoardSoftwareVersion()%10);
	//printf("Digit5: DriverBoardSoftwareVersion[%d] index[%d]\n",getDriverBoardSoftwareVersion(),nIndex);
	return nIndex;
}

int WhichImageSelectedStandardMain(struct element* theElement)
{
    int nImageIndex = BUTTON_ENABLED_INDEX;
    
    
    if(SCREEN_MODE_STANDARD == storedConfigGetScreenMode())
    {
        nImageIndex = BUTTON_SELECTED_INDEX;
    }
    
    return nImageIndex;
}

int WhichImageSelectedSingleMain(struct element* theElement)
{
    int nImageIndex = BUTTON_ENABLED_INDEX;
 
    
    if(SCREEN_MODE_SINGLE == storedConfigGetScreenMode())
    {
        nImageIndex = BUTTON_SELECTED_INDEX;
    }
    
    return nImageIndex;
}

int WhichSystemVoltageTensDigit(struct element* theElement)
{
	int nIndex = (getBatteryVoltage()/1000)%10;
	return nIndex;
}

int WhichSystemVoltageOnesDigit(struct element* theElement)
{
	int nIndex = (getBatteryVoltage()/100)%10;
	return nIndex;
}	

int WhichSystemVoltageTenthsDigit(struct element* theElement)
{
	int nIndex = (getBatteryVoltage()/10)%10;
	return nIndex;
}

//========================================================================
// is enabled functions
//=========================================================================
int IsEnabledConnect(struct element* theElement)
{
	int bRetVal = FALSE;
	if(devicesAreConnected())
	{
		bRetVal = TRUE;

	}
	return bRetVal;
}

int CheckEnabledConnect(struct element* theElement)
{
    int index = 1;
    
    if(devicesAreConnected())
    {
        index = 2;
    }
    
    uiDisplayElement(theElement, index);

    return 500;
}
       
int IsEnabledModel(struct element* theElement)
{
	int bRetVal = FALSE;
	if(devicesAreConnected())
	{
		if(eMODEL_NONE != uiDriverGetModel())
		{
			bRetVal = TRUE;
		}
	}
	return bRetVal;
}
int IsEnabledWiredConnection(struct element* theElement)
{
	return areWeConnectedTo(eINTERFACE_RS485);
}
int IsEnabledSpanishUI(struct element* theElement)
{
	return uiDoesIDExist(SPANISH_UI);
}
int IsEnabledCustomUI(struct element* theElement)
{
	return uiDoesIDExist(CUSTOM_UI);
}
int IsEnabledActuator(struct element* theElement)
{
	if(eACTUATOR_TYPE_UKNOWN == eActuatorType ||
		eACTUATOR_TYPE_NONE == eActuatorType)
	{
		return FALSE;
	}
	return TRUE;
}
int IsAutoActuatorEnabled(struct element* theElement)
{
#ifdef ALLOW_ACTUATOR_AUTO_MODE
    return TRUE;
#else
    return FALSE;
#endif
}
int IsEnabledPatternAllowed(struct element* theElement)
{
	BOOL bRetVal = FALSE;
	RADIO_BUTTON_SELECTION_ELEMENT *pRadioButtonElement = (RADIO_BUTTON_SELECTION_ELEMENT*)theElement;
	if(isPatternAllowed((eDISPLAY_TYPES)pRadioButtonElement->nData))
	{
		bRetVal = TRUE;
	}
	return bRetVal;
}
int IsEnabledActuatorAutoMode(struct element* theElement)
{
	BOOL bRetVal = FALSE;
	if(eACTUATOR_BUTTON_AUTO_MODE == eActuatorButtonMode)
	{
		bRetVal = TRUE;
	}
	return bRetVal;
}

