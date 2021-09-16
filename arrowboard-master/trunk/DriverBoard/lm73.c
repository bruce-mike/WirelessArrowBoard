#include <string.h>
#include <stdio.h>
#include "shareddefs.h"
#include "sharedinterface.h"
#include "timer.h"
#include "queue.h"
#include "i2c.h"

#include "lm73.h"
static TIMERCTL sampleTimer;

static I2C_QUEUE_ENTRY myI2CQueueEntry;
static I2C_TRANSACTION myTransaction;
static volatile BYTE LM73DataReceived = 0;
	
static union 
{
	WORD wTemperatureCounts;
	BYTE bTemperatureCounts[2];
}TEMP_COUNTS;

static void lm73AddI2CConfigQueueEntry()
{
		I2CQueueEntryInitialize(&myI2CQueueEntry);
		I2CQueueInitializeTransaction(&myTransaction);
		myTransaction.nSLA_W = LM73_ADDR;
		myTransaction.nSLA_R = 0;
		myTransaction.cOutgoingData[0] = CONFIG_REG;
		myTransaction.cOutgoingData[1] = LM73_RESOLUTION_11_BITS;
		myTransaction.nOutgoingDataLength = 2;
		myTransaction.nIncomingDataLength = 0;
		//printf("lm73AddI2CConfigQueueEntry myTransaction[%X] [%X][%X][%X][%X] nSLA_W[%X] nSLA_R[%X]\n\r", (int)&myTransaction, myTransaction.cOutgoingData[0], myTransaction.cOutgoingData[1], myTransaction.cOutgoingData[2], myTransaction.cOutgoingData[3], myTransaction.nSLA_W, myTransaction.nSLA_R);
		I2CQueueAddTransaction(&myI2CQueueEntry, &myTransaction);
}
static void lm73AddI2CReadQueueEntry()
{
		I2CQueueEntryInitialize(&myI2CQueueEntry);
		I2CQueueInitializeTransaction(&myTransaction);
		myTransaction.nSLA_W = LM73_ADDR;
		myTransaction.nSLA_R = LM73_RD;
		myTransaction.cOutgoingData[0] = TEMP_REG;
		myTransaction.nOutgoingDataLength = 1;
		myTransaction.nIncomingDataLength = 2;
		//printf("lm73AddI2CQueueEntry myTransaction[%X] [%X][%X][%X][%X]\n\r", (int)&myTransaction, myTransaction.cOutgoingData[0], myTransaction.cOutgoingData[1], myTransaction.cOutgoingData[2], myTransaction.cOutgoingData[3]);
		I2CQueueAddTransaction(&myI2CQueueEntry, &myTransaction);
}
void lm73Init()
{
	TEMP_COUNTS.wTemperatureCounts = 0;
	lm73AddI2CConfigQueueEntry();
	//printf("lm73Init myI2CQueueEntry[%X]\n\r", (int)&myI2CQueueEntry);
	I2CQueueAddEntry(&myI2CQueueEntry);
	startTimer(&sampleTimer, LM73_SAMPLE_TIME_MS);
}

void lm73DoWork()
{
	switch(myI2CQueueEntry.eState)
	{
		case eI2C_TRANSFER_STATE_COMPLETE:
			{
				/////
				// previous I2C queue entry is complete
				// grab the data
				// and schedule another
				/////
				TEMP_COUNTS.wTemperatureCounts = (myTransaction.cIncomingData[0]<<8)|myTransaction.cIncomingData[1];
			}
			myI2CQueueEntry.eState = eI2C_TRANSFER_STATE_IDLE;
            LM73DataReceived = 1;
			break;

		case eI2C_TRANSFER_STATE_FAILED:
			myI2CQueueEntry.eState = eI2C_TRANSFER_STATE_IDLE;
			break;
		case eI2C_TRANSFER_STATE_IDLE:
		default:
			break;
	}
	if((eI2C_TRANSFER_STATE_IDLE == myI2CQueueEntry.eState) && isTimerExpired(&sampleTimer))
	{
		lm73AddI2CReadQueueEntry();
		I2CQueueAddEntry(&myI2CQueueEntry);
		startTimer(&sampleTimer, LM73_SAMPLE_TIME_MS);
	}
}

int lm73GetDegreesC(BOOL bDegreesC)
{
	static float tempC = 0; // always save last C value
    float tempF;
	int temperature;
    
	// 1 degree == 0000 0000 1000 0000
	
    if(1 == LM73DataReceived)
    {
        LM73DataReceived = 0;
        
		// Check to see if the Negative Temperature Bit is Set
		if(TEMP_COUNTS.wTemperatureCounts & 0x8000)
		{	
			TEMP_COUNTS.wTemperatureCounts = ( ( ~(TEMP_COUNTS.wTemperatureCounts) ) >> 7 );
			
			// Negative Degrees Celecius
			tempC = -((float)TEMP_COUNTS.bTemperatureCounts[0]);
		}
		else
		{
			// Positive Degrees Celecius
			tempC = (float)(TEMP_COUNTS.wTemperatureCounts/128);
		}

    }        

    if(TRUE == bDegreesC)
    {
        temperature = (int)tempC;
    }
    else
    {
        tempF = (1.8 * tempC) + 32;       
        temperature = (int)tempF;
    }
    
    return temperature;
}    


