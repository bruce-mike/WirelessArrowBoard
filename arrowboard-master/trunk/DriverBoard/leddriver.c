#include <string.h>
#include <stdio.h>
#include "shareddefs.h"
#include "sharedinterface.h"
#include "timer.h"
#include "queue.h"
#include "i2c.h"

#include "leddriver.h"
#include "mischardware.h"

static I2C_QUEUE_ENTRY myI2CQueueEntry;
static I2C_TRANSACTION myTransaction;

static TIMERCTL sampleTimer;

/////
// set state of LEDs
// variables are PCA9634 LEDOUT bits
/////
static eLED_CONFIG_BITS nStateIndR;
static eLED_CONFIG_BITS nStateIndL;
static eLED_CONFIG_BITS nStateAux;

static eLED_CONFIG_BITS nStateLedVlow;
static eLED_CONFIG_BITS nStateLedChrgr;
static eLED_CONFIG_BITS nStateLedSyst;
static eLED_CONFIG_BITS nStateLedAlarm;



static eLED_CONFIG_BITS nNewIndR;
static eLED_CONFIG_BITS nNewIndL;
static eLED_CONFIG_BITS nNewAux;
static eLED_CONFIG_BITS nNewLedVlow;
static eLED_CONFIG_BITS nNewLedChrgr;
static eLED_CONFIG_BITS nNewLedSyst;
static eLED_CONFIG_BITS nNewLedAlarm;

/////
// limit switch power is switchable
// it sits with the LED control PCA9634
// so manage it here
/////
static eLED_CONFIG_BITS nStateLimitSW;
static eLED_CONFIG_BITS nNewLimitSW;

static int nBoardRev;
static void ledDriverAddConfigureI2CQueueEntry()
{
		I2CQueueEntryInitialize(&myI2CQueueEntry);
		I2CQueueInitializeTransaction(&myTransaction);
		myTransaction.nSLA_W = PCA9634_SYS_LEDS_AND_PWM;
		myTransaction.nSLA_R = 0;
		myTransaction.cOutgoingData[0] = MODE1_INC;
		myTransaction.cOutgoingData[1] = SYS_LEDS_CONFIG_MODEREG1;
		myTransaction.cOutgoingData[2] = SYS_LEDS_CONFIG_MODEREG2;
		myTransaction.nOutgoingDataLength = 3;
		myTransaction.nIncomingDataLength = 0;
		I2CQueueAddTransaction(&myI2CQueueEntry, &myTransaction);
}

static void ledDriverAddLedControlI2CQueueEntry()
{
		I2CQueueEntryInitialize(&myI2CQueueEntry);
		I2CQueueInitializeTransaction(&myTransaction);
		myTransaction.nSLA_W = PCA9634_SYS_LEDS_AND_PWM;
		myTransaction.nSLA_R = 0;
		myTransaction.cOutgoingData[0] = LEDOUT0;								// auto inc bit is set
		myTransaction.cOutgoingData[1] = nStateIndR|(nStateIndL<<2)|(nStateAux<<4)|(nStateLimitSW<<6);
		myTransaction.cOutgoingData[2] = nStateLedVlow|(nStateLedChrgr<<2)|(nStateLedSyst<<4)|(nStateLedAlarm<<6);
		myTransaction.nOutgoingDataLength = 3;
		myTransaction.nIncomingDataLength = 0;
		I2CQueueAddTransaction(&myI2CQueueEntry, &myTransaction);
}
static BOOL bInit;
void ledDriverInit()
{
	nBoardRev = hwReadBoardRev();
	
	nStateIndR = eLED_ON;
	nStateIndL = eLED_ON;
	nStateAux = eLED_ON;
	nStateLedVlow = eLED_ON;
	nStateLedChrgr = eLED_ON;
	nStateLedSyst = eLED_ON;
	nStateLedAlarm = eLED_ON;
	nStateLimitSW = eLED_ON;
	
	
	nNewIndR = eLED_OFF;
	nNewIndL = eLED_OFF;
	nNewAux = eLED_OFF;
	nNewLedVlow = eLED_OFF;
	nNewLedChrgr = eLED_OFF;
	nNewLedSyst = eLED_OFF;
	nNewLedAlarm = eLED_OFF;
	nNewLimitSW = eLED_OFF;
	
	bInit = TRUE;
	initTimer(&sampleTimer);
	startTimer(&sampleTimer, LEDDRIVER_SAMPLE_TIME_MS);
	/////
	// schedule I2C transaction to configure the driver
	// leave all drivers off for now
	/////
	ledDriverAddConfigureI2CQueueEntry();
	I2CQueueAddEntry(&myI2CQueueEntry);
}

void ledDriverDoWork()
{

	switch(myI2CQueueEntry.eState)
	{

		case eI2C_TRANSFER_STATE_COMPLETE:
		case eI2C_TRANSFER_STATE_FAILED:
			{
				myI2CQueueEntry.eState = eI2C_TRANSFER_STATE_IDLE;
			}
			break;
		case eI2C_TRANSFER_STATE_IDLE:
			break;
		default:
			break;
	}
	if((eI2C_TRANSFER_STATE_IDLE == myI2CQueueEntry.eState) && isTimerExpired(&sampleTimer))
	{
				/////
				// previous I2C queue entry is complete
				// if anything has changed
				// then schedule another
				/////
				BOOL bNew = FALSE;
				if(bInit)
				{
					hwEnableSystemLedsAndIndicators();
					bInit = FALSE;
				}
				if(nNewIndR != nStateIndR)
				{
					nStateIndR = nNewIndR;
					bNew = TRUE;
				}
				if(nNewIndL != nStateIndL)
				{
					nStateIndL = nNewIndL;
					bNew = TRUE;
				}
				if(nBoardRev < 6)
				{
					if(nNewAux != nStateAux)
					{
							nStateAux = nNewAux;
							bNew = TRUE;
					}
   			}

				if(nNewLedVlow != nStateLedVlow)
				{
					nStateLedVlow = nNewLedVlow;
					bNew = TRUE;
				}
				if(nNewLedChrgr != nStateLedChrgr)
				{
					nStateLedChrgr = nNewLedChrgr;
					bNew = TRUE;
				}
				if(nNewLedSyst != nStateLedSyst)
				{
					nStateLedSyst = nNewLedSyst;
					bNew = TRUE;
				}
				if(nNewLedAlarm != nStateLedAlarm)
				{
					nStateLedAlarm = nNewLedAlarm;
					bNew = TRUE;
				}
				if(nNewLimitSW != nStateLimitSW)
				{
					nStateLimitSW = nNewLimitSW;
					bNew = TRUE;
				}

				if(bNew)
				{
					ledDriverAddLedControlI2CQueueEntry();
					I2CQueueAddEntry(&myI2CQueueEntry);
					startTimer(&sampleTimer, LEDDRIVER_SAMPLE_TIME_MS);
				}
	}
}

void ledDriverSetIndR(eLED_CONFIG_BITS eNew)
{
	nNewIndR = eNew;
}
void ledDriverSetIndL(eLED_CONFIG_BITS eNew)
{
	nNewIndL = eNew;
}
void ledDriverSetAux(eLED_CONFIG_BITS eNew)
{
	nNewAux = eNew;
}
void ledDriverSetLedVlow(eLED_CONFIG_BITS eNew)
{
	nNewLedVlow = eNew;
}
void ledDriverSetLedChrgr(eLED_CONFIG_BITS eNew)
{
	nNewLedChrgr = eNew;
}
void ledDriverSetLedSyst(eLED_CONFIG_BITS eNew)
{
	nNewLedSyst = eNew;
}
void ledDriverSetLedAlarm(eLED_CONFIG_BITS eNew)
{
	nNewLedAlarm = eNew;
}
void ledDriverSetLimitSWPower(eLED_CONFIG_BITS eNew)
{
	nNewLimitSW = eNew;
}
eLED_CONFIG_BITS ledDriverGetLimitSWPower()
{
	return nStateLimitSW;
}

unsigned short ledDriverGetAuxStatus()
{
	unsigned short nStatus = 0;
	if(eLED_OFF != nNewIndR)
	{
		nStatus |= AUX_STATUS_INDR;
	}
	if(eLED_OFF != nNewIndL)
	{
		nStatus |= AUX_STATUS_INDL;
	}
	if(nBoardRev < 6)
	{
		if(eLED_OFF != nNewAux)
		{
			nStatus |= AUX_STATUS_AUX;
		}
	}
	return nStatus;
}
