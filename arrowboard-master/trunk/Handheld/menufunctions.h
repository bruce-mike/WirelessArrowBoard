#ifndef MENU_FUNCTIONS_H
#define MENU_FUNCTIONS_H

#include "shareddefs.h"
#include "sharedinterface.h"
#include "queue.h"
#include "timer.h"
#include "gen_UI.h"

#define N_VOLT_DIVISIONS 5

//=======================================
// MJB - duplicate thresholds
//=======================================

#ifdef ORIG_TRUCK_MOUNT_VOLTAGES	// MJB
#define VEHICLE_MOUNT_MAX_LINE_VOLTS 1500
#define VEHICLE_MOUNT_MIN_LINE_VOLTS 1200
#define VEHICLE_MOUNT_LINE_VOLTS_OK 1400
#define VEHICLE_MOUNT_LINE_VOLTS_LOW 1300
#else
#define VEHICLE_MOUNT_MAX_LINE_VOLTS 1300
#define VEHICLE_MOUNT_MIN_LINE_VOLTS 1100
#define VEHICLE_MOUNT_LINE_VOLTS_OK 1200
#define VEHICLE_MOUNT_LINE_VOLTS_LOW 1150
#endif

#define TRAILER_MOUNT_MAX_LINE_VOLTS 1300
#define TRAILER_MOUNT_MIN_LINE_VOLTS 1100
#define TRAILER_MOUNT_LINE_VOLTS_OK 1200
#define TRAILER_MOUNT_LINE_VOLTS_LOW 1150

#define STANDARD_UI			UI1_ID
#define SPANISH_UI			UI3_ID
#define CUSTOM_UI				UI2_ID

#define CHECK_STATUS_TIMER_MS							500
#define MANUAL_ACTUATOR_COMMAND_TIMER     1000
#define PAGE_HOLD_TIME_MS 1000
#define DISPLAY_TIME_MS 1870
#define DAY_MODE_BRIGHTNESS LEVEL 0xFF
#define NIGHT_MODE_BRIGHTNESS_LEVEL 0x0F

#define ACTUATOR_90_IMAGE_NULL_INDEX 	0
#define ACTUATOR_90_IMAGE_DOWN_INDEX 	1
#define ACTUATOR_90_IMAGE_UP30_INDEX 	2
#define ACTUATOR_90_IMAGE_UP60_INDEX 	3
#define ACTUATOR_90_IMAGE_UP_INDEX 		4
#define ACTUATOR_90_IMAGE_BLANK_INDEX 5

#define ACTUATOR_180_IMAGE_NULL_INDEX			0
#define ACTUATOR_180_IMAGE_DOWN_INDEX			1
#define ACTUATOR_180_IMAGE_LEFT45_INDEX 	2
#define ACTUATOR_180_IMAGE_LEFT90_INDEX 	3
#define ACTUATOR_180_IMAGE_RIGHT45_INDEX	4
#define ACTUATOR_180_IMAGE_RIGHT90_INDEX	5

#define ACTUATOR_POSITION_DOWN_INDEX 		2
#define ACTUATOR_POSITION_UP_INDEX			3
#define ACTUATOR_POSITION_UNKNOWN_INDEX		4

#define ARROW_BOARD_STYLE_VEHICLE_INDEX		2
#define ARROW_BOARD_STYLE_TRAILER_INDEX		3
#define ARROW_BOARD_STYLE_UPDATING_INDEX    4

#define ARROW_BOARD_TYPE_25_LIGHT_INDEX		2
#define ARROW_BOARD_TYPE_15_LIGHT_INDEX		3

#define ACTUATOR_TYPE_NONE_BUTTON_INDEX		2
#define ACTUATOR_TYPE_90_BUTTON_INDEX		3
#define ACTUATOR_TYPE_180_BUTTON_INDEX		4
#define ACTUATOR_TYPE_UPDATING_INDEX        5

#define CALIBRATE_TILT_DISABLED_INDEX		1
#define CALIBRATE_TILT_ENABLED_INDEX		2
#define CALIBRATE_TILT_END_INDEX			3

#define ALARM_UPDOWN_DOWN_ENABLED_INDEX 0
#define ALARM_UPDOWN_DOWN_PRESSED_INDEX 1
#define ALARM_UPDOWN_UP_ENABLED_INDEX 2
#define ALARM_UPDOWN_UP_PRESSED_INDEX 3

#define ALARM_PAGE_UP_OFFSET	    0
#define ALARM_PAGE_DOWN_OFFSET	    2

enum {    
    SCREEN_MODE_STANDARD = 0,
    SCREEN_MODE_SINGLE   = 1,
    NUM_SCREEN_MODES     = 2
};

void setCurrentDisplayType(eDISPLAY_TYPES eType);
eDISPLAY_TYPES getCurrentDisplayType(void);
void resetPatternImages(void);
/////
// on entry/exit functions
/////
int OnEntryTiltStatus(struct page* pPage);
int OnExitTiltStop(struct page* pPage);
int OnExitCalibrateTiltCancel(struct page* thePage);
int OnExitMaintMode(struct page* thePage);
int OnExitStatusAlarm(struct page* pPage);

/////
// image generation functions
/////
int ShowVersionText(struct element* pElement);

/////
// periodic functions
/////
void SetStatusToGetConfig(void);
void SetStatusToGetStatus(void);
int CheckStatus(struct element* pElement);
int CheckStatusVoltageScreen(struct element* pElement);
int CheckStatusConfigScreen(struct element* pElement);
int CheckConnected(struct element* pElement);
int CheckConnectedAndTilt(struct element* pElement);
int DisplayCurrentPattern(struct element* pElement);
int CheckBatteryVoltage(struct element* theElement);
int CheckLineVoltage(struct element* theElement);
int CheckLineCurrentTensDigit(struct element* theElement);
int CheckLineCurrentOnesDigit(struct element* theElement);
int CheckLineCurrentTenthsDigit(struct element* theElement);
int CheckSystemCurrentTensDigit(struct element* theElement);
int CheckSystemCurrentOnesDigit(struct element* theElement);
int CheckSystemCurrentTenthsDigit(struct element* theElement);
int MoveActuatorUp(struct element* pElement);
int MoveActuatorDown(struct element* pElement);
int MoveActuatorRight(struct element* pElement);
int MoveActuatorNeutral(struct element* pElement);
int MoveActuatorLeft(struct element* pElement);
int Tilt180Animation(struct element* pElement);
int Tilt90Animation(struct element* pElement);
int CloneGettingData(struct element* pElement);
int CalibrateTiltUp(struct element* pElement);
int CalibrateTiltDown(struct element* pElement);
int CheckPaired(struct element* theElement);
int CheckSelectedPatternButton(struct element* theElement);
int CheckSystemVoltageTensDigit(struct element* theElement);
int CheckSystemVoltageOnesDigit(struct element* theElement);
int CheckSystemVoltageTenthsDigit(struct element* theElement);
int CheckEnabledConnect(struct element* theElement);
int checkImageDriverVersion1(struct element* theElement);
int checkImageDriverVersion2(struct element* theElement);
int checkImageDriverVersion3(struct element* theElement);
int checkImageDriverVersion4(struct element* theElement);
int checkImageDriverVersion5(struct element* theElement);
int checkRssiDigit1( struct element* theElement );
int checkRssiDigit2( struct element* theElement );

/////
// external interface
// receive updates from arrow board
/////
void UpdateArrowBoardBrightnessControl(eBRIGHTNESS_CONTROL eControl);
void UpdateDisallowedPatterns(unsigned short nPatterns);
void UpdateActuatorType(eACTUATOR_TYPES eType);
void UpdateDefaultUI(int nUI);
void UpdateScreenMode(int nMode);
void UpdateActuatorButtonMode(eACTUATOR_BUTTON_MODE eMode);
void UpdateActuatorCalibrationHint(eACTUATOR_CALIBRATION_HINT eHInt);

/////
// pushbutton functions
/////
int SendActuatorCommand(struct element* pElement, unsigned char bTouchInProgress, unsigned char bThisButtonIsSelected);
int Actuator180LeftPress(struct element* theElement, unsigned char bTouchInProgress, unsigned char bThisButtonIsSelected);
int Actuator180RightPress(struct element* theElement, unsigned char bTouchInProgress, unsigned char bThisButtonIsSelected);
int Actuator180NeutralPress(struct element* theElement, unsigned char bTouchInProgress, unsigned char bThisButtonIsSelected);
int Actuator180StopPress(struct element* theElement, unsigned char bTouchInProgress, unsigned char bThisButtonIsSelected);
int Actuator90UpPress(struct element* theElement, unsigned char bTouchInProgress, unsigned char bThisButtonIsSelected);
int Actuator90DownPress(struct element* theElement, unsigned char bTouchInProgress, unsigned char bThisButtonIsSelected);
int Actuator90StopPress(struct element* theElement, unsigned char bTouchInProgress, unsigned char bThisButtonIsSelected);
int CalTransActuatorUp(struct element* theElement, unsigned char bTouchInProgress, unsigned char bThisButtonIsSelected);
int CalTransActuatorDown(struct element* theElement, unsigned char bTouchInProgress, unsigned char bThisButtonIsSelected);
int SendDisplayCommand(struct element* pElement, unsigned char bTouchInProgress, unsigned char bThisButtonIsSelected);
int WhichPatternSelectPage(struct element* pElement, unsigned char bTouchInProgress, unsigned char bThisButtonIsSelected);
int WhichTiltPageSelect(struct element* pElement, unsigned char bTouchInProgress, unsigned char bThisButtonIsSelected);
int SendWirelessConfig(struct element* pElement, unsigned char bTouchInProgress, unsigned char bThisButtonIsSelected);
int SetArrowBoardAutoBrightness(struct element* theElement, unsigned char bTouchInProgress, unsigned char bThisButtonIsSelected);
int SetArrowBoardBright(struct element* theElement, unsigned char bTouchInProgress, unsigned char bThisButtonIsSelected);
int SetArrowBoardDim(struct element* theElement, unsigned char bTouchInProgress, unsigned char bThisButtonIsSelected);
int SetNightDay(struct element* theElement, unsigned char bTouchInProgress, unsigned char bThisButtonIsSelected);
int DoTouchScreenCalibrate(struct element* theElement, unsigned char bTouchInProgress, unsigned char bThisButtonIsSelected);
int DoReset(struct element* theElement, unsigned char bTouchInProgress, unsigned char bThisButtonIsSelected);
int DoCalibrateTiltUp(struct element* theElement, unsigned char bTouchInProgress, unsigned char bThisButtonIsSelected);
int DoCalibrateTiltDown(struct element* theElement, unsigned char bTouchInProgress, unsigned char bThisButtonIsSelected);
int DoCancelCalibrateTilt(struct element* theElement, unsigned char bTouchInProgress, unsigned char bThisButtonIsSelected);
int DoSelectEnglishUI(struct element* theElement, unsigned char bTouchInProgress, unsigned char bThisButtonIsSelected);
int DoSelectSpanishUI(struct element* theElement, unsigned char bTouchInProgress, unsigned char bThisButtonIsSelected);
int DoSelectCustomUI(struct element* theElement, unsigned char bTouchInProgress, unsigned char bThisButtonIsSelected);
int DoAllLightsOn(struct element* theElement, unsigned char bTouchInProgress, unsigned char bThisButtonIsSelected);
int DoAllLightsOff(struct element* theElement, unsigned char bTouchInProgress, unsigned char bThisButtonIsSelected);
int DoErroredLightsOff(struct element* theElement, unsigned char bTouchInProgress, unsigned char bThisButtonIsSelected);
int DoClonePush(struct element* theElement, unsigned char bTouchInProgress, unsigned char bThisButtonIsSelected);
int DoClonePull(struct element* theElement, unsigned char bTouchInProgress, unsigned char bThisButtonIsSelected);
int DoSetAcutatorButtonAutoMode(struct element* theElement, unsigned char bTouchInProgress, unsigned char bThisButtonIsSelected);
int DoSetAcutatorButtonManualMode(struct element* theElement, unsigned char bTouchInProgress, unsigned char bThisButtonIsSelected);
int DoTogglePatternSelectAllow(struct element* theElement, unsigned char bTouchInProgress, unsigned char bThisButtonIsSelected);
int DoSetControllerDefaults(struct element* theElement, unsigned char bTouchInProgress, unsigned char bThisButtonIsSelected);
int DoSetTiltFrame180(struct element* theElement, unsigned char bTouchInProgress, unsigned char bThisButtonIsSelected);
int DoSetTiltFrame90(struct element* theElement, unsigned char bTouchInProgress, unsigned char bThisButtonIsSelected);
int DoSetTiltFrameNone(struct element* theElement, unsigned char bTouchInProgress, unsigned char bThisButtonIsSelected);
int AlarmUpDownButtonPress(struct element* theElement, unsigned char bTouchInProgress, unsigned char bThisButtonIsSelected);
int DoSelectStandardScreen(struct element* theElement, unsigned char bTouchInProgress, unsigned char bThisButtonIsSelected);
int DoSelectSingleScreen(struct element* theElement, unsigned char bTouchInProgress, unsigned char bThisButtonIsSelected);


/////
// which image functions
/////
int IsSelectedPatternButton(struct element* theElement);
int IsSelectedEnglish(struct element* theElement);
int IsSelectedSpanish(struct element* theElement);
int IsSelectedCustom(struct element* theElement);
int IsSelectedStandardMain(struct element* theElement);
int IsSelectedSingleMain(struct element* theElement);
int WhichImageAggregateAlarm(struct element* theElement);
int WhichImageAlarmLevelBattRevDispError(struct element* theElement);
int WhichImageAlarmLevelConnLowBatt(struct element* theElement);
int WhichImageAlarmLevelControlTempAuxError(struct element* theElement);
int WhichImageAlarmLevel(struct element* theElement);
int WhichImageAlarmLevelLVDLowVolt(struct element* theElement);
int WhichImageAlarmLevelLineRevIndError(struct element* theElement);
int WhichImageAlarmStateImage(struct element* theElement);
int WhichImageAlarmUpDownButton(struct element* theElement);
int WhichImageBatteryVoltage(struct element* theElement);
int WhichImageLineCurrentOnesDigit(struct element* theElement);
int WhichImageLineCurrentTenthsDigit(struct element* theElement);
int WhichImageSystemCurrentOnesDigit(struct element* theElement);
int WhichImageSystemCurrentTenthsDigit(struct element* theElement);
int WhichImageSystemState(struct element* theElement);
int WhichImageVoltageState(struct element* theElement);
int WhichImageActuatorDown(struct element* theElement);
int WhichImageActuatorUp(struct element* theElement);
int WhichImageActuatorLeft(struct element* theElement);
int WhichImageActuatorRight(struct element* theElement);
int WhichImageActuatorNeutral(struct element* theElement);
int WhichImageIsAutoBrightSelected(struct element* theElement);
int WhichImageIsBrightSelected(struct element* theElement);
int WhichImageIsDimSelected(struct element* theElement);
int WhichImageNightDayImage(struct element* theElement);
int WhichImageIsCalibrateTiltUpInProgress(struct element* theElement);
int WhichImageIsCalibrateTiltDownInProgress(struct element* theElement);
int WhichImageAuxBatteryYes(struct element* theElement);
int WhichImageAuxBatteryNo(struct element* theElement);
int WhichImageSetActuatorButtonModeAuto(struct element* theElement);
int WhichImageSetActuatorButtonModeManual(struct element* theElement);
int WhichImagePatternSelectAllow(struct element* theElement);
int WhichImageTiltFrame180(struct element* theElement);
int WhichImageTiltFrame90(struct element* theElement);
int WhichImageTiltFrameNone(struct element* theElement);
int WhichImageCloneGettingData(struct element* theElement);
int WhichImageMaintModeAllLightsOn(struct element* theElement);
int WhichImageMaintModeAllLightsOff(struct element* theElement);
int WhichImageMaintModeErroredLightsOff(struct element* theElement);
int WhichImagePaired(struct element* theElement);
int WhichImageControllerVersionDigit1(struct element* theElement);
int WhichImageControllerVersionDigit2(struct element* theElement);
int WhichImageControllerVersionDigit3(struct element* theElement);
int WhichImageControllerVersionDigit4(struct element* theElement);
int WhichImageControllerVersionDigit5(struct element* theElement);
int WhichImageDriverVersionDigit1(struct element* theElement);
int WhichImageDriverVersionDigit2(struct element* theElement);
int WhichImageDriverVersionDigit3(struct element* theElement);
int WhichImageDriverVersionDigit4(struct element* theElement);
int WhichImageDriverVersionDigit5(struct element* theElement);
int WhichImageSelectedStandardMain(struct element* theElement);
int WhichImageSelectedSingleMain(struct element* theElement);
int WhichSystemVoltageTensDigit(struct element* theElement);
int WhichSystemVoltageOnesDigit(struct element* theElement);
int WhichSystemVoltageTenthsDigit(struct element* theElement);
int WhichImageRssiDigit1(struct element* theElement);
int WhichImageRssiDigit2(struct element* theElement);

/////
// is enabled functions
/////
int IsEnabledConnect(struct element* theElement);
int IsEnabledModel(struct element* theElement);
int IsEnabledWiredConnection(struct element* theElement);
int IsEnabledSpanishUI(struct element* theElement);
int IsEnabledCustomUI(struct element* theElement);
int IsEnabledActuator(struct element* theElement);
int IsEnabledPatternAllowed(struct element* theElement);
int IsEnabledActuatorAutoMode(struct element* theElement);
int IsAutoActuatorEnabled(struct element* theElement);

/////
// button image offset functions
/////
int ButtonImageOffsetWigWag(struct element* theElement);
#endif		// MENU_FUNCTIONS_H
