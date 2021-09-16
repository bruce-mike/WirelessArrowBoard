#include <string.h>
#include <stdio.h>
#include "shareddefs.h"
#include "sharedinterface.h"
#include "timer.h"
#include "queue.h"

#include "i2c.h"
#include "display.h"

#include "ADS7828.h"

// these are ref'd to 'by name' via #define's in ads7828.h.
//                            ch0   ch1   ch2   ch3   ch4   ch5   ch6   ch7
// ch0 = ID1
// ch1 = ID2 etc\
// power down mode and single-ended comversion mode is encoded here in channel cmd
const char ADS7828_CMDS[] = {ADC_ID1, ADC_ID2, ADC_IB, ADC_IX, ADC_GND, ADC_IA, ADC_IL, ADC_IS};	

//single-ended, internal ref ON, ADC ON.
//#define ADC_ID1			0x8C 	// Unused
//#define ADC_ID2 		0xCC	// ditto, drivers 1-16
//#define ADC_IB 			0x9C	// Unused
//#define ADC_IX 			0xDC	// Unused
//#define ADC_GND 		0xAC	// ground
//#define ADC_IA 			0xEC	// Actuator
//#define ADC_IL 			0xBC	// battery
//#define ADC_IS 			0xFC	// 12v subsystem (lamp drivers, beacon, indicator, user, hour, lbar, strobe
static TIMERCTL sampleTimer;
static TIMERCTL currentAverageTimer;

static I2C_QUEUE_ENTRY myI2CQueueEntry;
static I2C_TRANSACTION myTransaction;

static int nADC_ID2 = 0;
static int nADC_IA = 0;
static int nADC_IL = 0;
static int nADC_IS = 0;

static BOOL bActuatorLimitReached;
static int nPreviousActuatorSample;
static int nUpwardSamples;
static int nOverSamples;
#define MIN_ACTUATOR_UPWARD_DIFFERENCE 100
//#define MIN_ACTUATOR_UPWARD_DIFFERENCE 500
#define MIN_ACTUATOR_UPWARD_SAMPLES 3
#define MIN_ACTUATOR_OVER_SAMPLES 10
static void ADS7828AddI2CQueueEntry(int nChannelNum)
{
		I2CQueueEntryInitialize(&myI2CQueueEntry);
		I2CQueueInitializeTransaction(&myTransaction);
		myTransaction.nSLA_W = ADS7828_BASE;
		myTransaction.nSLA_R = ADS7828_RD;
		myTransaction.cOutgoingData[0] = ADS7828_CMDS[nChannelNum];
		myTransaction.nOutgoingDataLength = 1;
		myTransaction.nIncomingDataLength = 2;

		I2CQueueAddTransaction(&myI2CQueueEntry, &myTransaction);
}
void ADS7828Init()
{
	nADC_ID2 = 0;
	nADC_IA = 0;
	nADC_IL = 0;
	nADC_IS = 0;
	bActuatorLimitReached = FALSE;
	ADS7828AddI2CQueueEntry(1);
	I2CQueueAddEntry(&myI2CQueueEntry);
	startTimer(&sampleTimer, ADS7828_SAMPLE_TIME_MS);
	startTimer(&currentAverageTimer, ADS7828_CURRENT_AVERAGE_TIME_MS);
}


static int nActuatorStopCurrent = ACT_DEFAULT_UP_STOP_CURRENT;
void ads7828SetActuatorStopCurrent(int nStopCurrent)
{
	nActuatorStopCurrent = nStopCurrent;
}
static void ads7828ActuatorLimitCalculation(int nSample)
{
	int nDiff = nSample-nPreviousActuatorSample;
	if(nSample > nActuatorStopCurrent)
	{
		nOverSamples++;
		if(MIN_ACTUATOR_OVER_SAMPLES <= nOverSamples)
		{
				bActuatorLimitReached = TRUE;
		}
	}
	else
	{
		nOverSamples = 0;
	}
	if(nDiff > MIN_ACTUATOR_UPWARD_DIFFERENCE)
	{
		nUpwardSamples++;
	}
	else
	{
		if(MIN_ACTUATOR_UPWARD_SAMPLES < nUpwardSamples)
		{
			bActuatorLimitReached = TRUE;
		}
		nUpwardSamples = 0;
	}
	nPreviousActuatorSample = nSample;
}
void ads7828BeginActuatorLimitCalculations()
{
	bActuatorLimitReached = FALSE;
	nPreviousActuatorSample = 0;
	nUpwardSamples = 0;
	nOverSamples = 0;
}
void ads7827ResetCurrentLimitReached()
{
	bActuatorLimitReached = FALSE;
}
BOOL ads7827IsActuatorCurrentLimitReached()
{
	return bActuatorLimitReached;
}
void ADS7828DoWork()
{
	#define MAX_ACTUATOR_SAMPLES_FOR_AVERAGE 10
	static int nActuatorSampleArray[MAX_ACTUATOR_SAMPLES_FOR_AVERAGE];
	static int nActuatorSampleIndex = 0;

	static unsigned long nLineCurrentSum = 0;
	static unsigned long nSystemCurrentSum = 0;
	static unsigned long nLineCurrentSamples = 0;
	static unsigned long nSystemCurrentSamples = 0;
		
	static BOOL bInit = TRUE;
	int nAverage;
	static int nChannelNum = 1;
	int nADCValue = 0;
	int i;
	if(bInit)
	{
		for(i=0;i<MAX_ACTUATOR_SAMPLES_FOR_AVERAGE;i++)
		{
			nActuatorSampleArray[i] = 0;
		}
		bInit = FALSE;
	}
	
	/////
	// update current average logic
	/////
	if(isTimerExpired(&currentAverageTimer))
	{
			/////
			// use the one timer
			// to average both the line current and system current
			//////
			nADC_IL = 0;
			if(0 < nLineCurrentSamples)
			{
				nADC_IL = (int)(nLineCurrentSum/nLineCurrentSamples);
			}
			nLineCurrentSamples = 0;
			nLineCurrentSum = 0;
	
			nADC_IS = 0;
			if(0 < nSystemCurrentSamples)
			{
				nADC_IS = (int)(nSystemCurrentSum/nSystemCurrentSamples);
			}
			nSystemCurrentSamples = 0;
			nSystemCurrentSum = 0;

			startTimer(&currentAverageTimer, ADS7828_CURRENT_AVERAGE_TIME_MS);
	}
	switch(myI2CQueueEntry.eState)
	{
		case eI2C_TRANSFER_STATE_COMPLETE:
			{
				nADCValue = (myTransaction.cIncomingData[0])<<8|myTransaction.cIncomingData[1];
				/////
				// previous I2C queue entry is complete
				// grab the data
				// and schedule another
				/////
				switch(nChannelNum)
				{
					case 0:
						nChannelNum = 1;
						break;
					case 1:
						nADC_ID2 = nADCValue;
						//printf("1-nADCValue[%d]\n", nADCValue);
						nChannelNum = 5;
						break;
					case 2:
					case 3:
					case 4:
						nChannelNum = 5;
						break;
					case 5:
						{
							nChannelNum++;
							if(!displayIsBlank())
							{
								/////
								// only want to look at the line current
								// when the display is blank
								/////
								nChannelNum = 5;
							}
							//printf("5-nADCValue[%d]\n", nADCValue);
							if(4095 <= nADCValue)
							{
								break;
							}
							nActuatorSampleArray[nActuatorSampleIndex++] = nADCValue;
							if(MAX_ACTUATOR_SAMPLES_FOR_AVERAGE <= nActuatorSampleIndex)
							{
								nActuatorSampleIndex = 0;
							}
							nAverage = 0;
							for(i=0;i<MAX_ACTUATOR_SAMPLES_FOR_AVERAGE;i++)
							{
								nAverage += nActuatorSampleArray[i];
							}
							nAverage /= MAX_ACTUATOR_SAMPLES_FOR_AVERAGE;
							nADC_IA = nAverage;
							ads7828ActuatorLimitCalculation(nADC_IA);
							//printf("ADS7828DoWork: nADCValue[%d]\n", nADCValue);
							//printf("A[%d]\tB[%d]\n", nAverage, nADCValue);
						}

						break;
					case 6:
						/////
						// skip back to Actuator current
						// don't look at system current here
						/////
						nChannelNum = 5;
						if(4095 <= nADCValue)
						{
							break;
						}
						nLineCurrentSum += nADCValue;
						nLineCurrentSamples++;	
						//printf("nADC_IL[%d] nADCValue[%d] nLineCurrentSum[%ld] nLineCurrentSamples[%ld]\n", nADC_IL, nADCValue, nLineCurrentSum, nLineCurrentSamples);						
						//printf("6-nADCValue[%d]\n", nADCValue);
						break;
					case 7:
						nChannelNum = 1;
						if(4095 <= nADCValue)
						{
							break;
						}
						nSystemCurrentSum += nADCValue;
						nSystemCurrentSamples++;
						//printf("nADC_IS[%d] nADCValue[%d] nSystemCurrentSum[%ld] nSystemCurrentSamples[%ld]\n", nADC_IS, nADCValue, nSystemCurrentSum, nSystemCurrentSamples);
						//printf("7-nADCValue[%d]\n", nADCValue);
						break;
				}
				myI2CQueueEntry.eState = eI2C_TRANSFER_STATE_IDLE;
				break;
			case eI2C_TRANSFER_STATE_FAILED:
				myI2CQueueEntry.eState = eI2C_TRANSFER_STATE_IDLE;
				break;
			default:
				break;
		}
	}
	
	/////
	// if sample time expired
	// then schedule another transfer
	/////
	if((eI2C_TRANSFER_STATE_IDLE == myI2CQueueEntry.eState) && (TRUE == isTimerExpired(&sampleTimer)))
	{
		ADS7828AddI2CQueueEntry(nChannelNum);
		I2CQueueAddEntry(&myI2CQueueEntry);
		startTimer(&sampleTimer, ADS7828_SAMPLE_TIME_MS);
	}
}

int nADS7828GetID2()
{
	return nADC_ID2;
}
int nADS7828GetIA()
{
	return nADC_IA;
}

/////
// line current
/////
unsigned short nADS7828GetIL(void)
{
	unsigned short nAmps = 0;

	float fAmps = AMPS_PER_BIT_ALT_AND_BATT * nADC_IL;
	nAmps = fAmps *100;
	//printf("nADC_IL[%04X] nAmps[%d]\n",nADC_IL, nAmps);
	//printf("\n");
	return nAmps;
}

/////
// system current
/////
unsigned short nADS7828GetIS(void)
{
	unsigned short nAmps = 0;

	float fAmps = AMPS_PER_BIT_ALT_AND_BATT * nADC_IS;
	nAmps = fAmps *100;
	//printf("nADC_IS[%04X] nAmps[%d]\n",nADC_IS, nAmps);
	//printf("\n");
	return nAmps;
}
