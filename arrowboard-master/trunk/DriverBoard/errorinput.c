#include <string.h>
#include <stdio.h>
#include "shareddefs.h"
#include "sharedinterface.h"
#include "timer.h"
#include "queue.h"

#include "i2c.h"
#include "errorinput.h"
#include "mischardware.h"
#include "actuator.h"
#include "adc.h"
#include "lm73.h"
#include "display.h"
#include "storedconfig.h"

static eMODEL eModel;

static TIMERCTL sampleTimer;

static I2C_QUEUE_ENTRY myI2CQueueEntry;
static I2C_TRANSACTION miscErrorTransaction;
static I2C_TRANSACTION driveErrorTransaction;

static unsigned char nMiscErrorData;
static unsigned short nDriveErrorData;
static int nDriveErrorErroredCounts;
#define DRIVE_ERROR_REPORT_COUNTS 20
static int nBoardRev;

static void errorInputAddI2CQueueEntry(unsigned char bConfig)
{
		I2CQueueEntryInitialize(&myI2CQueueEntry);

		I2CQueueInitializeTransaction(&driveErrorTransaction);
		driveErrorTransaction.nSLA_W = PCA9555_DRV_ERRS;
	
		I2CQueueInitializeTransaction(&miscErrorTransaction);
		miscErrorTransaction.nSLA_W = PCA9554_MISC_FLT_N_IO;
		if(bConfig)
		{
			miscErrorTransaction.nSLA_R = 0;
			driveErrorTransaction.nSLA_R = 0;			
			miscErrorTransaction.cOutgoingData[0] = CONP0;
			miscErrorTransaction.cOutgoingData[1] = ALL_INPUT;
			miscErrorTransaction.nOutgoingDataLength = 2;
			miscErrorTransaction.nIncomingDataLength = 0;
			
			if(nBoardRev < 6)
			{
				driveErrorTransaction.cOutgoingData[0] = CONP0;
				driveErrorTransaction.cOutgoingData[1] = ALL_INPUT;
				driveErrorTransaction.cOutgoingData[2] = ALL_INPUT;
				driveErrorTransaction.nOutgoingDataLength =3;
				driveErrorTransaction.nIncomingDataLength = 0;
			}
		}
		else
		{			
			miscErrorTransaction.nSLA_R = PCA9554_MISC_FLT_N_IORD;
			miscErrorTransaction.cOutgoingData[0] = INP0;
			miscErrorTransaction.nOutgoingDataLength = 1;
			miscErrorTransaction.nIncomingDataLength = 1;
			
			if(nBoardRev < 6)
			{
				driveErrorTransaction.nSLA_R = PCA9555_DRV_ERRS_RD;			
				driveErrorTransaction.cOutgoingData[0] = INP0;
				driveErrorTransaction.nOutgoingDataLength = 1;
				driveErrorTransaction.nIncomingDataLength = 2;
			}
		}
		I2CQueueAddTransaction(&myI2CQueueEntry, &miscErrorTransaction);
		if(nBoardRev < 6)
		{
			I2CQueueAddTransaction(&myI2CQueueEntry, &driveErrorTransaction);
		}
}

void errorInputInit()
{
		/////
	// get the board revision
	////	
	nBoardRev = hwReadBoardRev();
	
	/////
	// clear the received data
	/////
	nMiscErrorData = 0xff;
	nDriveErrorData = 0x0;
	nDriveErrorErroredCounts = 0;

	/////
	// remember the model number
	/////
	eModel = hwReadModel();
	
}

void errorInputDoWork()
{
	static int bConfig = 1;
	
	switch(myI2CQueueEntry.eState)
	{
		case eI2C_TRANSFER_STATE_COMPLETE:
			{	
				/////
				// previous I2C queue entry is complete
				// grab the data
				//(first transfer is configuration)
				/////
				if(0 == bConfig)
				{
					int nDriveErrorTemp;
					nMiscErrorData = miscErrorTransaction.cIncomingData[0];
					if(nBoardRev < 6)
					{
						nDriveErrorTemp = driveErrorTransaction.cIncomingData[1];
						nDriveErrorTemp <<= 8;
						nDriveErrorTemp |= driveErrorTransaction.cIncomingData[0];

						//printf("[%X][%X]\n", driveErrorTransaction.cIncomingData[1], driveErrorTransaction.cIncomingData[0]);
						//nDriveErrorData &= (((driveErrorTransaction.cIncomingData[1])<<8)|driveErrorTransaction.cIncomingData[0]);
						/////
						// errors are active low
						// so flip them
						// to be active high
						/////
						nDriveErrorTemp ^= 0xFFFF;
						if(0 == nDriveErrorTemp)
						{
							if(0 != nDriveErrorData)
							{
								if(0 >= nDriveErrorErroredCounts--)
								{
									nDriveErrorData = 0;
									nDriveErrorErroredCounts = 0;
								}
							}
						}
						else
						{
							nDriveErrorData = nDriveErrorTemp;
							nDriveErrorErroredCounts = DRIVE_ERROR_REPORT_COUNTS;
						}
					}
				}
				bConfig = 0;
				myI2CQueueEntry.eState = eI2C_TRANSFER_STATE_IDLE;
			}
			break;

		case eI2C_TRANSFER_STATE_FAILED:
			/////
			// transfer failed for some reason
			// we will schedule another when the timer expires
			/////
			myI2CQueueEntry.eState = eI2C_TRANSFER_STATE_IDLE;
			break;
		case eI2C_TRANSFER_STATE_IDLE:
			break;
		case eI2C_TRANSFER_STATE_TRANSFERRING:
			break;
		default:
			break;
	}

	if((eI2C_TRANSFER_STATE_IDLE == myI2CQueueEntry.eState) && isTimerExpired(&sampleTimer))
	{
		/////
		// schedule another
		/////
		errorInputAddI2CQueueEntry(FALSE);
		I2CQueueAddEntry(&myI2CQueueEntry);

		/////
		// and restart the timer
		/////
		startTimer(&sampleTimer, ERROR_INPUT_SAMPLE_TIME_MS);
	}
}

unsigned short errorInputGetSwitchData()
{
	unsigned short nBitmap = 0;
	int nModeSwitches  = hwReadSwitches();
	
	if(!(nModeSwitches&0x01))
	{
		nBitmap |= SWITCH_BITMAP_MODE_1;
	}
	
	if(!(nModeSwitches&0x02))
	{
		nBitmap |= SWITCH_BITMAP_MODE_2;
	}
	
	if(!(nModeSwitches&0x04))
	{
		nBitmap |= SWITCH_BITMAP_MODE_4;
	}
	
	if(!(nModeSwitches&0x08))
	{
		nBitmap |= SWITCH_BITMAP_MODE_8;
	}
	
	return nBitmap;
}

int errorInputGetActSwitch()
{
	int nRetVal = 0;
	if(!(nMiscErrorData&0x20))
	{
		nRetVal = SWITCH_BITMAP_ACT_UP;
	}
	else if(!(nMiscErrorData&0x10))
	{
		nRetVal = SWITCH_BITMAP_ACT_DOWN;
	}	
	return nRetVal;
}
unsigned short errorInputGetMiscErrors()
{
	/////
	// low is active error
	// so flip this data
	/////
	return ((nMiscErrorData)&0xff)^0xFF;
}


unsigned short errorInputGetDriveErrors()
{
	/////
	// this data has been flipped,
	// so high is active error
	/////
	return nDriveErrorData;
}

unsigned short errorInputGetAlarms()
{
	unsigned short nAlarms = ALARM_NONE;
	unsigned short nDriveErrors = errorInputGetDriveErrors();
	unsigned char nMiscErrors = errorInputGetMiscErrors();
	int nLineVoltageLowLimit = VEHICLE_MOUNT_LINE_VOLTAGE_LOW_LIMIT;	
	
	switch(eModel)
	{
		case eMODEL_VEHICLE_25_LIGHT_SEQUENTIAL:
		case eMODEL_VEHICLE_15_LIGHT_SEQUENTIAL:
		case eMODEL_VEHICLE_15_LIGHT_FLASHING:
			if(ADCGetSystemVoltage() < nLineVoltageLowLimit)		
			{ 
				nAlarms |= ALARM_BITMAP_LOW_LINE_VOLTAGE;
	
			}
			break;
		default:
			break;
	
	//////////////////////////////////////////
	//// Solar Trailer
	//////////////////////////////////////////
		case eMODEL_TRAILER_25_LIGHT_SEQUENTIAL:
		case eMODEL_TRAILER_15_LIGHT_SEQUENTIAL:
		case eMODEL_TRAILER_15_LIGHT_FLASHING:
		case eMODEL_VEHICLE_15_LIGHT_WIG_WAG:
			nLineVoltageLowLimit = TRAILER_MOUNT_LINE_VOLTAGE_LOW_LIMIT;
			break;
	}


    if(ADCGetSystemVoltage() < batteryVoltageLowLimit)
    {
        nAlarms |= ALARM_BITMAP_LOW_BATTERY;
    }
	
	if(lm73GetDegreesC(TRUE) > temperatureHighLimit || lm73GetDegreesC(TRUE) < temperatureLowLimit)
	{
		nAlarms |= ALARM_BITMAP_OVER_TEMP;
	}
	
	if(hwGetLVDStatus())
	{
		nAlarms |= ALARM_BITMAP_LVD;
	}
	
	if(hwGetSolarChargeStatus())
	{
		nAlarms |= ALARM_BITMAP_CHARGER_ON;
	}
	
	if(displayIsShutDown()) 
	{
		nAlarms |= ALARM_BITMAP_LAMPS_DISABLED;				
	}
	
	return nAlarms;
}

BOOL errorInputIsVBOK()
{
	BOOL bRetVal = FALSE;
	unsigned char nErrors = errorInputGetMiscErrors();
	
	/////
	// 0 is good
	/////
	//printf("errorInputIsVBOK()[%X][%X][%d][%d]\n", nErrors, (1<<IN_VOK_VB), ADCGetLineVoltage(), ADCGetBatteryVoltage());
	if(0 == ((1<<IN_VOK_VB) & nErrors))
	{
		bRetVal = TRUE;
	}
	return bRetVal;
}

BOOL errorInputIsVLOK( void )
{
	BOOL bRetVal = FALSE;
	unsigned char nErrors = errorInputGetMiscErrors();
	/////
	// 0 is good
	/////
	//printf("errorInputIsVLOK()[%X][%X][%d][%d]\n", nErrors, (1<<IN_VOK_VL), ADCGetLineVoltage(), ADCGetBatteryVoltage());
	if(0 == ((1<<IN_VOK_VL) &  nErrors))
	{
		bRetVal = TRUE;
	}
	return bRetVal;
}

unsigned short errorInputGetAuxErrors()
{
	unsigned short nErrors = 0;
	unsigned short nAllErrors = errorInputGetMiscErrors();
	
	if(nAllErrors& (1<<xERR_INDR))
	{
		nErrors |= AUX_ERROR_INDR;
	}
	if(nAllErrors&(1<<xERR_INDL))
	{
		nErrors |= AUX_ERROR_INDL;
	}
	if(nAllErrors&(1<<xERR_AUX))
	{
		nErrors |= AUX_ERROR_AUX;
	}
	if(nAllErrors&(1<<xERR_LIMSW))
	{
		nErrors |= AUX_ERROR_LIMIT_SW;
	}
	
	
	return nErrors;
}
