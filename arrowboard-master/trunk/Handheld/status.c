#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "status.h"
#include "timer.h"
#include "queue.h"

#include "gen_UI.h"
#include "commboard.h"
#include "spiflash.h"
#include "uidriver.h"
#include "LCD.h"
#include "menufunctions.h"
#include "hdlc.h"


static PACKED struct STATUS
{
	WORD alarms;
	WORD lineVoltage;
	WORD batteryVoltage;
	WORD signDisplay;
	WORD displayError;
	WORD aux;
	WORD auxErrors;
	WORD switches;
	WORD temperature;
	WORD actuator;
	WORD actuatorLimit;
	WORD lineCurrent;
	WORD systemCurrent;
	WORD driverBoardSoftwareVersion;
	WORD handHeldSoftwareVersion;
}status;


void storeStatus(eCOMMANDS eCommand, WORD sData)
{	
	switch(eCommand)
	{
		case eCOMMAND_STATUS_ALARMS:
			status.alarms = sData;
            status.alarms &= ~(ALARM_BITMAP_LVD); // 110
			break;
		case eCOMMAND_STATUS_LINE_VOLTAGE:
			status.lineVoltage = sData;
			break;
		case eCOMMAND_STATUS_BATTERY_VOLTAGE:
			status.batteryVoltage = sData;
			break;
		case eCOMMAND_STATUS_SIGN_DISPLAY:
			status.signDisplay = sData;
			break;
		case eCOMMAND_STATUS_DISPLAY_ERRORS:
			status.displayError = sData;
			break;
		case eCOMMAND_STATUS_AUX:
			status.aux = sData;
			break;
		case eCOMMAND_STATUS_AUX_ERRORS:
			status.auxErrors = sData;
			break;
		case eCOMMAND_STATUS_SWITCHES:
			status.switches = sData;
			break;
		case eCOMMAND_STATUS_TEMPERATURE:
			status.temperature = sData;
			break;
		case eCOMMAND_STATUS_ACTUATOR:
			status.actuator = sData;
			break;
		case eCOMMAND_STATUS_ACTUATOR_LIMIT:
			status.actuatorLimit = sData;
			break;
		case eCOMMAND_STATUS_LINE_CURRENT:
			status.lineCurrent = sData;
			break;
		case eCOMMAND_STATUS_SYSTEM_CURRENT:
			status.systemCurrent = sData;
			break;					
		case eCOMMAND_STATUS_HANDHELD_SOFTWARE_VERSION:
			status.handHeldSoftwareVersion = sData;
			break;
		case eCOMMAND_STATUS_DRIVER_BOARD_SOFTWARE_VERSION:
			status.driverBoardSoftwareVersion = sData;
			break;
		default:
			break;
	}
}
WORD getAlarmBitmap(void)
{
	return status.alarms;
}

WORD getAlarms(void)
{
	WORD alarmLevel;

	if(FALSE == devicesAreConnected() ||
                status.alarms & ALARM_BITMAP_LAMPS_DISABLED ||
				status.alarms & ALARM_BITMAP_OVER_TEMP)
    {
		alarmLevel = ALARM_LEVEL_HIGH;
	}
	else if(status.alarms & ALARM_BITMAP_LOW_BATTERY)
    {
		alarmLevel = ALARM_LEVEL_MEDIUM;
	}
	else
	{	
        alarmLevel = ALARM_LEVEL_NONE;
	}

	return alarmLevel;
}

WORD getBatteryVoltage(void)
{
	return status.batteryVoltage;
}

WORD getSignDisplay(void)
{
	return status.signDisplay;
}

WORD getDisplayError(void)
{
	return status.displayError;
}

WORD getAux(void)
{
	return status.aux;
}

WORD getAuxErrors(void)
{
	return status.auxErrors;
}

WORD getSwitches(void)
{
	return status.switches;
}

WORD getTemperature(void)
{
	return status.temperature;
}

WORD getActuatorStatus(void)
{
	return status.actuator;
}

WORD getActuatorLimit(void)
{
	return status.actuatorLimit;
}
WORD getLineCurrent(void)
{
	return status.lineCurrent;
}
WORD getSystemCurrent(void)
{
	return status.systemCurrent;
}
WORD getHandHeldSoftwareVersion(void)
{
	return status.handHeldSoftwareVersion;
}
WORD getDriverBoardSoftwareVersion(void)
{
	return status.driverBoardSoftwareVersion;	
}

