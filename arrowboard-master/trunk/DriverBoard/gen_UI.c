//========================================================
// UI functions
//
// Auto generated by ArrowBoardUIBuilder
// Do not edit
//========================================================
#include <string.h>
#include "shareddefs.h"
#include "sharedinterface.h"
#include "queue.h"
#include "timer.h"
#include "gen_UI.h"
//===========================================
// function ID to function pointer mapping
//===========================================
funcOnEntryExit *onEntryExitIDToFunction(int nID)
{
	funcOnEntryExit *pRetVal = NULL;
	switch(nID)
	{
		case EXIT_ENTRY_FUNCTION_ID_OnEntryTiltStatus:
			pRetVal = (funcOnEntryExit*)OnEntryTiltStatus;
			break;
		case EXIT_ENTRY_FUNCTION_ID_OnExitTiltStop:
			pRetVal = (funcOnEntryExit*)OnExitTiltStop;
			break;
		case EXIT_ENTRY_FUNCTION_ID_OnExitCalibrateTiltCancel:
			pRetVal = (funcOnEntryExit*)OnExitCalibrateTiltCancel;
			break;
		case EXIT_ENTRY_FUNCTION_ID_OnExitMaintMode:
			pRetVal = (funcOnEntryExit*)OnExitMaintMode;
			break;
		case EXIT_ENTRY_FUNCTION_ID_OnExitStatusAlarm:
			pRetVal = (funcOnEntryExit*)OnExitStatusAlarm;
			break;
		default:
			break;
	}
	return pRetVal;
}

funcPeriodic *periodicIDToFunction(int nID)
{
	funcPeriodic *pRetVal = NULL;
	switch(nID)
	{
		case PERIODIC_FUNCTION_ID_DisplayCurrentPattern:
			pRetVal = (funcPeriodic*)DisplayCurrentPattern;
			break;
		case PERIODIC_FUNCTION_ID_CheckStatus:
			pRetVal = (funcPeriodic*)CheckStatus;
			break;
		case PERIODIC_FUNCTION_ID_CheckConnected:
			pRetVal = (funcPeriodic*)CheckConnected;
			break;
		case PERIODIC_FUNCTION_ID_MoveActuatorUp:
			pRetVal = (funcPeriodic*)MoveActuatorUp;
			break;
		case PERIODIC_FUNCTION_ID_MoveActuatorDown:
			pRetVal = (funcPeriodic*)MoveActuatorDown;
			break;
		case PERIODIC_FUNCTION_ID_Tilt90Animation:
			pRetVal = (funcPeriodic*)Tilt90Animation;
			break;
		case PERIODIC_FUNCTION_ID_Tilt180Animation:
			pRetVal = (funcPeriodic*)Tilt180Animation;
			break;
		case PERIODIC_FUNCTION_ID_MoveActuatorRight:
			pRetVal = (funcPeriodic*)MoveActuatorRight;
			break;
		case PERIODIC_FUNCTION_ID_MoveActuatorLeft:
			pRetVal = (funcPeriodic*)MoveActuatorLeft;
			break;
		case PERIODIC_FUNCTION_ID_CheckConnectedAndTilt:
			pRetVal = (funcPeriodic*)CheckConnectedAndTilt;
			break;
		case PERIODIC_FUNCTION_ID_CloneGettingData:
			pRetVal = (funcPeriodic*)CloneGettingData;
			break;
		case PERIODIC_FUNCTION_ID_CalibrateTiltUp:
			pRetVal = (funcPeriodic*)CalibrateTiltUp;
			break;
		case PERIODIC_FUNCTION_ID_CalibrateTiltDown:
			pRetVal = (funcPeriodic*)CalibrateTiltDown;
			break;
		case PERIODIC_FUNCTION_ID_CheckPaired:
			pRetVal = (funcPeriodic*)CheckPaired;
			break;
		case PERIODIC_FUNCTION_ID_CheckBatteryVoltage:
			pRetVal = (funcPeriodic*)CheckBatteryVoltage;
			break;
		case PERIODIC_FUNCTION_ID_CheckLineVoltage:
			pRetVal = (funcPeriodic*)CheckLineVoltage;
			break;
		case PERIODIC_FUNCTION_ID_MoveActuatorNeutral:
			pRetVal = (funcPeriodic*)MoveActuatorNeutral;
			break;
		case PERIODIC_FUNCTION_ID_CheckLineCurrentOnesDigit:
			pRetVal = (funcPeriodic*)CheckLineCurrentOnesDigit;
			break;
		case PERIODIC_FUNCTION_ID_CheckLineCurrentTenthsDigit:
			pRetVal = (funcPeriodic*)CheckLineCurrentTenthsDigit;
			break;
		case PERIODIC_FUNCTION_ID_CheckSystemCurrentOnesDigit:
			pRetVal = (funcPeriodic*)CheckSystemCurrentOnesDigit;
			break;
		case PERIODIC_FUNCTION_ID_CheckSystemCurrentTenthsDigit:
			pRetVal = (funcPeriodic*)CheckSystemCurrentTenthsDigit;
			break;
		case PERIODIC_FUNCTION_ID_CheckLineCurrentTensDigit:
			pRetVal = (funcPeriodic*)CheckLineCurrentTensDigit;
			break;
		case PERIODIC_FUNCTION_ID_CheckSystemCurrentTensDigit:
			pRetVal = (funcPeriodic*)CheckSystemCurrentTensDigit;
			break;
		default:
			break;
	}
	return pRetVal;
}

funcPushbutton *pushbuttonIDToFunction(int nID)
{
	funcPushbutton *pRetVal = NULL;
	switch(nID)
	{
		case PUSHBUTTON_FUNCTION_ID_SendDisplayCommand:
			pRetVal = (funcPushbutton*)SendDisplayCommand;
			break;
		case PUSHBUTTON_FUNCTION_ID_SendActuatorCommand:
			pRetVal = (funcPushbutton*)SendActuatorCommand;
			break;
		case PUSHBUTTON_FUNCTION_ID_SendWirelessConfig:
			pRetVal = (funcPushbutton*)SendWirelessConfig;
			break;
		case PUSHBUTTON_FUNCTION_ID_PopPageFirst:
			pRetVal = (funcPushbutton*)PopPageFirst;
			break;
		case PUSHBUTTON_FUNCTION_ID_AlarmUpDownButtonPress:
			pRetVal = (funcPushbutton*)AlarmUpDownButtonPress;
			break;
		case PUSHBUTTON_FUNCTION_ID_WhichPatternSelectPage:
			pRetVal = (funcPushbutton*)WhichPatternSelectPage;
			break;
		case PUSHBUTTON_FUNCTION_ID_WhichTiltPageSelect:
			pRetVal = (funcPushbutton*)WhichTiltPageSelect;
			break;
		case PUSHBUTTON_FUNCTION_ID_Actuator90UpPress:
			pRetVal = (funcPushbutton*)Actuator90UpPress;
			break;
		case PUSHBUTTON_FUNCTION_ID_Actuator90DownPress:
			pRetVal = (funcPushbutton*)Actuator90DownPress;
			break;
		case PUSHBUTTON_FUNCTION_ID_Actuator90StopPress:
			pRetVal = (funcPushbutton*)Actuator90StopPress;
			break;
		case PUSHBUTTON_FUNCTION_ID_Actuator180RightPress:
			pRetVal = (funcPushbutton*)Actuator180RightPress;
			break;
		case PUSHBUTTON_FUNCTION_ID_Actuator180LeftPress:
			pRetVal = (funcPushbutton*)Actuator180LeftPress;
			break;
		case PUSHBUTTON_FUNCTION_ID_Actuator180StopPress:
			pRetVal = (funcPushbutton*)Actuator180StopPress;
			break;
		case PUSHBUTTON_FUNCTION_ID_SetNightDay:
			pRetVal = (funcPushbutton*)SetNightDay;
			break;
		case PUSHBUTTON_FUNCTION_ID_SetArrowBoardBright:
			pRetVal = (funcPushbutton*)SetArrowBoardBright;
			break;
		case PUSHBUTTON_FUNCTION_ID_SetArrowBoardDim:
			pRetVal = (funcPushbutton*)SetArrowBoardDim;
			break;
		case PUSHBUTTON_FUNCTION_ID_SetArrowBoardAutoBrightness:
			pRetVal = (funcPushbutton*)SetArrowBoardAutoBrightness;
			break;
		case PUSHBUTTON_FUNCTION_ID_DoTouchScreenCalibrate:
			pRetVal = (funcPushbutton*)DoTouchScreenCalibrate;
			break;
		case PUSHBUTTON_FUNCTION_ID_DoReset:
			pRetVal = (funcPushbutton*)DoReset;
			break;
		case PUSHBUTTON_FUNCTION_ID_DoCalibrateTiltUp:
			pRetVal = (funcPushbutton*)DoCalibrateTiltUp;
			break;
		case PUSHBUTTON_FUNCTION_ID_DoCalibrateTiltDown:
			pRetVal = (funcPushbutton*)DoCalibrateTiltDown;
			break;
		case PUSHBUTTON_FUNCTION_ID_DoSelectEnglishUI:
			pRetVal = (funcPushbutton*)DoSelectEnglishUI;
			break;
		case PUSHBUTTON_FUNCTION_ID_DoSelectSpanishUI:
			pRetVal = (funcPushbutton*)DoSelectSpanishUI;
			break;
		case PUSHBUTTON_FUNCTION_ID_DoSelectAuxBatteryYes:
			pRetVal = (funcPushbutton*)DoSelectAuxBatteryYes;
			break;
		case PUSHBUTTON_FUNCTION_ID_DoSelectAuxBatteryNo:
			pRetVal = (funcPushbutton*)DoSelectAuxBatteryNo;
			break;
		case PUSHBUTTON_FUNCTION_ID_DoAllLightsOn:
			pRetVal = (funcPushbutton*)DoAllLightsOn;
			break;
		case PUSHBUTTON_FUNCTION_ID_DoAllLightsOff:
			pRetVal = (funcPushbutton*)DoAllLightsOff;
			break;
		case PUSHBUTTON_FUNCTION_ID_DoClonePush:
			pRetVal = (funcPushbutton*)DoClonePush;
			break;
		case PUSHBUTTON_FUNCTION_ID_DoClonePull:
			pRetVal = (funcPushbutton*)DoClonePull;
			break;
		case PUSHBUTTON_FUNCTION_ID_DoSetActuatorButtonAutoMode:
			pRetVal = (funcPushbutton*)DoSetActuatorButtonAutoMode;
			break;
		case PUSHBUTTON_FUNCTION_ID_DoSetActuatorButtonManualMode:
			pRetVal = (funcPushbutton*)DoSetActuatorButtonManualMode;
			break;
		case PUSHBUTTON_FUNCTION_ID_DoTogglePatternSelectAllow:
			pRetVal = (funcPushbutton*)DoTogglePatternSelectAllow;
			break;
		case PUSHBUTTON_FUNCTION_ID_CalTransActuatorUp:
			pRetVal = (funcPushbutton*)CalTransActuatorUp;
			break;
		case PUSHBUTTON_FUNCTION_ID_CalTransActuatorDown:
			pRetVal = (funcPushbutton*)CalTransActuatorDown;
			break;
		case PUSHBUTTON_FUNCTION_ID_DoSetControllerDefaults:
			pRetVal = (funcPushbutton*)DoSetControllerDefaults;
			break;
		case PUSHBUTTON_FUNCTION_ID_DoSetTiltFrame90:
			pRetVal = (funcPushbutton*)DoSetTiltFrame90;
			break;
		case PUSHBUTTON_FUNCTION_ID_DoSetTiltFrame180:
			pRetVal = (funcPushbutton*)DoSetTiltFrame180;
			break;
		case PUSHBUTTON_FUNCTION_ID_DoSetTiltFrameNone:
			pRetVal = (funcPushbutton*)DoSetTiltFrameNone;
			break;
		case PUSHBUTTON_FUNCTION_ID_DoCancelCalibrateTilt:
			pRetVal = (funcPushbutton*)DoCancelCalibrateTilt;
			break;
		case PUSHBUTTON_FUNCTION_ID_DoSelectCustomUI:
			pRetVal = (funcPushbutton*)DoSelectCustomUI;
			break;
		case PUSHBUTTON_FUNCTION_ID_DoErroredLightsOff:
			pRetVal = (funcPushbutton*)DoErroredLightsOff;
			break;
		case PUSHBUTTON_FUNCTION_ID_Actuator180NeutralPress:
			pRetVal = (funcPushbutton*)Actuator180NeutralPress;
			break;
		default:
			break;
	}
	return pRetVal;
}

funcIsEnabledbutton *isEnabledIDToFunction(int nID)
{
	funcIsEnabledbutton *pRetVal = NULL;
	switch(nID)
	{
		case ISENABLED_FUNCTION_ID_IsEnabledConnect:
			pRetVal = (funcIsEnabledbutton*)IsEnabledConnect;
			break;
		case ISENABLED_FUNCTION_ID_IsEnabledModel:
			pRetVal = (funcIsEnabledbutton*)IsEnabledModel;
			break;
		case ISENABLED_FUNCTION_ID_IsEnabledWiredConnection:
			pRetVal = (funcIsEnabledbutton*)IsEnabledWiredConnection;
			break;
		case ISENABLED_FUNCTION_ID_IsEnabledActuator:
			pRetVal = (funcIsEnabledbutton*)IsEnabledActuator;
			break;
		case ISENABLED_FUNCTION_ID_IsEnabledSpanishUI:
			pRetVal = (funcIsEnabledbutton*)IsEnabledSpanishUI;
			break;
		case ISENABLED_FUNCTION_ID_IsEnabledPatternAllowed:
			pRetVal = (funcIsEnabledbutton*)IsEnabledPatternAllowed;
			break;
		case ISENABLED_FUNCTION_ID_IsEnabledCustomUI:
			pRetVal = (funcIsEnabledbutton*)IsEnabledCustomUI;
			break;
		default:
			break;
	}
	return pRetVal;
}

funcButtonImageOffsetbutton *buttonImageOffsetIDToFunction(int nID)
{
	funcButtonImageOffsetbutton *pRetVal = NULL;
	switch(nID)
	{
		case BUTTONIMAGEOFFSET_FUNCTION_ID_ButtonImageOffsetWigWag:
			pRetVal = (funcButtonImageOffsetbutton*)ButtonImageOffsetWigWag;
			break;
		default:
			break;
	}
	return pRetVal;
}

funcImageGeneration *imageGenerationIDToFunction(int nID)
{
	funcImageGeneration *pRetVal = NULL;
	switch(nID)
	{
		case IMAGEGENERATION_FUNCTION_ID_ShowVersionText:
			pRetVal = (funcImageGeneration*)ShowVersionText;
			break;
		default:
			break;
	}
	return pRetVal;
}

funcWhichImagebutton *whichImageIDToFunction(int nID)
{
	funcWhichImagebutton *pRetVal = NULL;
	switch(nID)
	{
		case WHICHIMAGE_FUNCTION_ID_IsSelectedPatternButton:
			pRetVal = (funcWhichImagebutton*)IsSelectedPatternButton;
			break;
		case WHICHIMAGE_FUNCTION_ID_WhichImageAggregateAlarm:
			pRetVal = (funcWhichImagebutton*)WhichImageAggregateAlarm;
			break;
		case WHICHIMAGE_FUNCTION_ID_WhichImageAlarmLevelBattRevDispError:
			pRetVal = (funcWhichImagebutton*)WhichImageAlarmLevelBattRevDispError;
			break;
		case WHICHIMAGE_FUNCTION_ID_WhichImageAlarmLevelConnLowBatt:
			pRetVal = (funcWhichImagebutton*)WhichImageAlarmLevelConnLowBatt;
			break;
		case WHICHIMAGE_FUNCTION_ID_WhichImageAlarmLevelControlTempAuxError:
			pRetVal = (funcWhichImagebutton*)WhichImageAlarmLevelControlTempAuxError;
			break;
		case WHICHIMAGE_FUNCTION_ID_WhichImageAlarmLevel:
			pRetVal = (funcWhichImagebutton*)WhichImageAlarmLevel;
			break;
		case WHICHIMAGE_FUNCTION_ID_WhichImageAlarmLevelLVDLowVolt:
			pRetVal = (funcWhichImagebutton*)WhichImageAlarmLevelLVDLowVolt;
			break;
		case WHICHIMAGE_FUNCTION_ID_WhichImageAlarmLevelLineRevIndError:
			pRetVal = (funcWhichImagebutton*)WhichImageAlarmLevelLineRevIndError;
			break;
		case WHICHIMAGE_FUNCTION_ID_WhichImageAlarmState:
			pRetVal = (funcWhichImagebutton*)WhichImageAlarmState;
			break;
		case WHICHIMAGE_FUNCTION_ID_WhichImageAlarmUpDownButton:
			pRetVal = (funcWhichImagebutton*)WhichImageAlarmUpDownButton;
			break;
		case WHICHIMAGE_FUNCTION_ID_WhichImageBatteryVoltage:
			pRetVal = (funcWhichImagebutton*)WhichImageBatteryVoltage;
			break;
		case WHICHIMAGE_FUNCTION_ID_WhichImageLineVoltage:
			pRetVal = (funcWhichImagebutton*)WhichImageLineVoltage;
			break;
		case WHICHIMAGE_FUNCTION_ID_WhichImageSystemState:
			pRetVal = (funcWhichImagebutton*)WhichImageSystemState;
			break;
		case WHICHIMAGE_FUNCTION_ID_WhichImageVoltageState:
			pRetVal = (funcWhichImagebutton*)WhichImageVoltageState;
			break;
		case WHICHIMAGE_FUNCTION_ID_WhichImageNightDay:
			pRetVal = (funcWhichImagebutton*)WhichImageNightDay;
			break;
		case WHICHIMAGE_FUNCTION_ID_WhichImageIsBrightSelected:
			pRetVal = (funcWhichImagebutton*)WhichImageIsBrightSelected;
			break;
		case WHICHIMAGE_FUNCTION_ID_WhichImageIsDimSelected:
			pRetVal = (funcWhichImagebutton*)WhichImageIsDimSelected;
			break;
		case WHICHIMAGE_FUNCTION_ID_WhichImageIsAutoBrightSelected:
			pRetVal = (funcWhichImagebutton*)WhichImageIsAutoBrightSelected;
			break;
		case WHICHIMAGE_FUNCTION_ID_WhichImageIsCalibrateTiltUpInProgress:
			pRetVal = (funcWhichImagebutton*)WhichImageIsCalibrateTiltUpInProgress;
			break;
		case WHICHIMAGE_FUNCTION_ID_WhichImageIsCalibrateTiltDownInProgress:
			pRetVal = (funcWhichImagebutton*)WhichImageIsCalibrateTiltDownInProgress;
			break;
		case WHICHIMAGE_FUNCTION_ID_WhichImageAuxBatteryYes:
			pRetVal = (funcWhichImagebutton*)WhichImageAuxBatteryYes;
			break;
		case WHICHIMAGE_FUNCTION_ID_WhichImageAuxBatteryNo:
			pRetVal = (funcWhichImagebutton*)WhichImageAuxBatteryNo;
			break;
		case WHICHIMAGE_FUNCTION_ID_WhichImageSetActuatorButtonModeAuto:
			pRetVal = (funcWhichImagebutton*)WhichImageSetActuatorButtonModeAuto;
			break;
		case WHICHIMAGE_FUNCTION_ID_WhichImageSetActuatorButtonModeManual:
			pRetVal = (funcWhichImagebutton*)WhichImageSetActuatorButtonModeManual;
			break;
		case WHICHIMAGE_FUNCTION_ID_WhichImagePatternSelectAllow:
			pRetVal = (funcWhichImagebutton*)WhichImagePatternSelectAllow;
			break;
		case WHICHIMAGE_FUNCTION_ID_IsSelectedEnglish:
			pRetVal = (funcWhichImagebutton*)IsSelectedEnglish;
			break;
		case WHICHIMAGE_FUNCTION_ID_IsSelectedSpanish:
			pRetVal = (funcWhichImagebutton*)IsSelectedSpanish;
			break;
		case WHICHIMAGE_FUNCTION_ID_WhichImageTiltFrame90:
			pRetVal = (funcWhichImagebutton*)WhichImageTiltFrame90;
			break;
		case WHICHIMAGE_FUNCTION_ID_WhichImageTiltFrame180:
			pRetVal = (funcWhichImagebutton*)WhichImageTiltFrame180;
			break;
		case WHICHIMAGE_FUNCTION_ID_WhichImageTiltFrameNone:
			pRetVal = (funcWhichImagebutton*)WhichImageTiltFrameNone;
			break;
		case WHICHIMAGE_FUNCTION_ID_WhichImageCloneGettingData:
			pRetVal = (funcWhichImagebutton*)WhichImageCloneGettingData;
			break;
		case WHICHIMAGE_FUNCTION_ID_IsSelectedCustom:
			pRetVal = (funcWhichImagebutton*)IsSelectedCustom;
			break;
		case WHICHIMAGE_FUNCTION_ID_WhichImageMaintModeAllLightsOn:
			pRetVal = (funcWhichImagebutton*)WhichImageMaintModeAllLightsOn;
			break;
		case WHICHIMAGE_FUNCTION_ID_WhichImageMaintModeAllLightsOff:
			pRetVal = (funcWhichImagebutton*)WhichImageMaintModeAllLightsOff;
			break;
		case WHICHIMAGE_FUNCTION_ID_WhichTiltButtonImage:
			pRetVal = (funcWhichImagebutton*)WhichTiltButtonImage;
			break;
		case WHICHIMAGE_FUNCTION_ID_WhichImageStatusArrowBoardButton:
			pRetVal = (funcWhichImagebutton*)WhichImageStatusArrowBoardButton;
			break;
		case WHICHIMAGE_FUNCTION_ID_WhichImageStatusStyleButton:
			pRetVal = (funcWhichImagebutton*)WhichImageStatusStyleButton;
			break;
		case WHICHIMAGE_FUNCTION_ID_WhichImageStatusElectricActuatorButton:
			pRetVal = (funcWhichImagebutton*)WhichImageStatusElectricActuatorButton;
			break;
		case WHICHIMAGE_FUNCTION_ID_WhichImageStatusControllerSoftwareButton:
			pRetVal = (funcWhichImagebutton*)WhichImageStatusControllerSoftwareButton;
			break;
		case WHICHIMAGE_FUNCTION_ID_WhichImageStatusDriverSoftwareButton:
			pRetVal = (funcWhichImagebutton*)WhichImageStatusDriverSoftwareButton;
			break;
		case WHICHIMAGE_FUNCTION_ID_WhichImageMaintModeErroredLightsOff:
			pRetVal = (funcWhichImagebutton*)WhichImageMaintModeErroredLightsOff;
			break;
		case WHICHIMAGE_FUNCTION_ID_WhichImagePaired:
			pRetVal = (funcWhichImagebutton*)WhichImagePaired;
			break;
		case WHICHIMAGE_FUNCTION_ID_WhichImageLineCurrentOnesDigit:
			pRetVal = (funcWhichImagebutton*)WhichImageLineCurrentOnesDigit;
			break;
		case WHICHIMAGE_FUNCTION_ID_WhichImageLineCurrentTenthsDigit:
			pRetVal = (funcWhichImagebutton*)WhichImageLineCurrentTenthsDigit;
			break;
		case WHICHIMAGE_FUNCTION_ID_WhichImageSystemCurrentOnesDigit:
			pRetVal = (funcWhichImagebutton*)WhichImageSystemCurrentOnesDigit;
			break;
		case WHICHIMAGE_FUNCTION_ID_WhichImageSystemCurrentTenthsDigit:
			pRetVal = (funcWhichImagebutton*)WhichImageSystemCurrentTenthsDigit;
			break;
		case WHICHIMAGE_FUNCTION_ID_WhichImageActuatorUp:
			pRetVal = (funcWhichImagebutton*)WhichImageActuatorUp;
			break;
		case WHICHIMAGE_FUNCTION_ID_WhichImageActuatorDown:
			pRetVal = (funcWhichImagebutton*)WhichImageActuatorDown;
			break;
		case WHICHIMAGE_FUNCTION_ID_WhichImageActuatorLeft:
			pRetVal = (funcWhichImagebutton*)WhichImageActuatorLeft;
			break;
		case WHICHIMAGE_FUNCTION_ID_WhichImageActuatorRight:
			pRetVal = (funcWhichImagebutton*)WhichImageActuatorRight;
			break;
		case WHICHIMAGE_FUNCTION_ID_WhichImageActuatorNeutral:
			pRetVal = (funcWhichImagebutton*)WhichImageActuatorNeutral;
			break;
		case WHICHIMAGE_FUNCTION_ID_WhichImageLineCurrentTensDigit:
			pRetVal = (funcWhichImagebutton*)WhichImageLineCurrentTensDigit;
			break;
		case WHICHIMAGE_FUNCTION_ID_WhichImageSystemCurrentTensDigit:
			pRetVal = (funcWhichImagebutton*)WhichImageSystemCurrentTensDigit;
			break;
		case WHICHIMAGE_FUNCTION_ID_WhichImageControllerVersionDigit1:
			pRetVal = (funcWhichImagebutton*)WhichImageControllerVersionDigit1;
			break;
		case WHICHIMAGE_FUNCTION_ID_WhichImageControllerVersionDigit2:
			pRetVal = (funcWhichImagebutton*)WhichImageControllerVersionDigit2;
			break;
		case WHICHIMAGE_FUNCTION_ID_WhichImageControllerVersionDigit3:
			pRetVal = (funcWhichImagebutton*)WhichImageControllerVersionDigit3;
			break;
		case WHICHIMAGE_FUNCTION_ID_WhichImageControllerVersionDigit4:
			pRetVal = (funcWhichImagebutton*)WhichImageControllerVersionDigit4;
			break;
		case WHICHIMAGE_FUNCTION_ID_WhichImageControllerVersionDigit5:
			pRetVal = (funcWhichImagebutton*)WhichImageControllerVersionDigit5;
			break;
		case WHICHIMAGE_FUNCTION_ID_WhichImageDriverVersionDigit1:
			pRetVal = (funcWhichImagebutton*)WhichImageDriverVersionDigit1;
			break;
		case WHICHIMAGE_FUNCTION_ID_WhichImageDriverVersionDigit2:
			pRetVal = (funcWhichImagebutton*)WhichImageDriverVersionDigit2;
			break;
		case WHICHIMAGE_FUNCTION_ID_WhichImageDriverVersionDigit3:
			pRetVal = (funcWhichImagebutton*)WhichImageDriverVersionDigit3;
			break;
		case WHICHIMAGE_FUNCTION_ID_WhichImageDriverVersionDigit4:
			pRetVal = (funcWhichImagebutton*)WhichImageDriverVersionDigit4;
			break;
		case WHICHIMAGE_FUNCTION_ID_WhichImageDriverVersionDigit5:
			pRetVal = (funcWhichImagebutton*)WhichImageDriverVersionDigit5;
			break;
		default:
			break;
	}
	return pRetVal;
}

