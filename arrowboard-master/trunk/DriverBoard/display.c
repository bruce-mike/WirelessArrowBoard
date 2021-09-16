#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "shareddefs.h"
#include "sharedinterface.h"
#include "timer.h"
#include "queue.h"
#include "i2c.h"

#include "commands.h"
#include "mischardware.h"
#include "display.h"
#include "leddriver.h"
#include "errorinput.h"
#include "pwm.h"
#include "watchdog.h"
#include "adc.h"


static eMODEL eModel;

static I2C_QUEUE_ENTRY myI2CQueueEntry;
static I2C_TRANSACTION group_1_8_Transaction;
static I2C_TRANSACTION group_9_16_Transaction;

// Rev J hardware uses a single 16 bit driver
static I2C_TRANSACTION group_1_16_Transaction;


static TIMERCTL patternTimer;
static int nCurrentDisplayPatternIndex;
static int nDisplayPatterns;
static int nCurrentIndicatorPatternIndex;
static int nIndicatorPatterns;
static unsigned short nDisplayErrors;
static DISPLAY_PATTERN displayPatterns[MAX_DISPLAY_PATTERNS];
static eLED_CONFIG_BITS indicatorLeft[MAX_DISPLAY_PATTERNS];
static eLED_CONFIG_BITS indicatorRight[MAX_DISPLAY_PATTERNS];
static eDISPLAY_TYPES eCurrentDisplaySelection;
static eDISPLAY_TYPES eInitialDisplaySelection;
static unsigned char bInit = TRUE;
static int nBoardRev;

///////////////////
// the patterns defined here 
// have group bit definitions for 
// group 1-4
// group 5-8
// gtoup 9-12
// group 13-16
////////////////////////

/////////////////
// 25 light sequential
/////////////////
static unsigned char Pattern_Blank[] =
{
	0,
	0,
	0,
	0
};
static unsigned char Pattern_ErroredLights_25_light[] =
{
	0,
	0,
	0,
	0
};
static unsigned char Pattern_AllOn_25_light[] =
{
	DISPLAY_GROUP1|DISPLAY_GROUP2|DISPLAY_GROUP3|DISPLAY_GROUP4,
	DISPLAY_GROUP5|DISPLAY_GROUP6|DISPLAY_GROUP7|DISPLAY_GROUP8,
	DISPLAY_GROUP9|DISPLAY_GROUP10|DISPLAY_GROUP11|DISPLAY_GROUP12,
	DISPLAY_GROUP13|DISPLAY_GROUP14|DISPLAY_GROUP15|DISPLAY_GROUP16
};
static unsigned char Pattern_ErroredLights_15_light[] =
{
	0,
	0,
	0,
	0
};
static unsigned char Pattern_AllOn_15_light[] =
{
	DISPLAY_GROUP1|DISPLAY_GROUP2|DISPLAY_GROUP3|DISPLAY_GROUP4,
	DISPLAY_GROUP5|DISPLAY_GROUP6|DISPLAY_GROUP7|DISPLAY_GROUP8,
	DISPLAY_GROUP9|DISPLAY_GROUP10|DISPLAY_GROUP11|DISPLAY_GROUP12,
	DISPLAY_GROUP13|DISPLAY_GROUP14|DISPLAY_GROUP15|DISPLAY_GROUP16
};
static unsigned char Pattern_DoubleArrow_1_25_light[] =
{
	DISPLAY_GROUP1|DISPLAY_GROUP2|DISPLAY_GROUP3,
	0,
	DISPLAY_GROUP9|DISPLAY_GROUP10|DISPLAY_GROUP12,
	DISPLAY_GROUP13|DISPLAY_GROUP14|DISPLAY_GROUP16
};
static unsigned char Pattern_DoubleArrow_1_15_light[] =
{
	DISPLAY_GROUP1|DISPLAY_GROUP2|DISPLAY_GROUP3,
	DISPLAY_GROUP8,
	DISPLAY_GROUP9|DISPLAY_GROUP10|DISPLAY_GROUP11|DISPLAY_GROUP12,
	DISPLAY_GROUP13|DISPLAY_GROUP14|DISPLAY_GROUP16
};
static unsigned char Pattern_Bar_1_25_light[] = 
{
	DISPLAY_GROUP1|DISPLAY_GROUP4,
	DISPLAY_GROUP5,
	DISPLAY_GROUP9|DISPLAY_GROUP12,
	DISPLAY_GROUP13|DISPLAY_GROUP16
};
static unsigned char Pattern_Bar_1_15_light[] = 
{
	DISPLAY_GROUP1|DISPLAY_GROUP4,
	DISPLAY_GROUP5,
	DISPLAY_GROUP9|DISPLAY_GROUP12,
	DISPLAY_GROUP13|DISPLAY_GROUP16
};
static unsigned char Pattern_FourCorner_1_25_light[] =
{
	DISPLAY_GROUP3,
	0,
	0,
	DISPLAY_GROUP14
};
static unsigned char Pattern_FourCorner_1_15_light[] =
{
	DISPLAY_GROUP3,
	DISPLAY_GROUP8,
	DISPLAY_GROUP11,
	DISPLAY_GROUP14
};
static unsigned char Pattern_FourCorner_1_WigWag[] =
{
	0,
	DISPLAY_GROUP8,
	0,
	DISPLAY_GROUP14
};
static unsigned char Pattern_FourCorner_2_WigWag[] =
{
	DISPLAY_GROUP3,
	0,
	DISPLAY_GROUP11,
	0
};
static unsigned char Pattern_RightArrow_1_25_light[] =
{
	DISPLAY_GROUP1|DISPLAY_GROUP4,
	0,
	DISPLAY_GROUP9|DISPLAY_GROUP10|DISPLAY_GROUP12,
	DISPLAY_GROUP13|DISPLAY_GROUP14|DISPLAY_GROUP16
};
static unsigned char Pattern_RightArrow_1_15_light[] =
{
	DISPLAY_GROUP1|DISPLAY_GROUP4,
	0,
	DISPLAY_GROUP9|DISPLAY_GROUP10|DISPLAY_GROUP11|DISPLAY_GROUP12,
	DISPLAY_GROUP13|DISPLAY_GROUP14|DISPLAY_GROUP16
};
static unsigned char Pattern_LeftArrow_1_25_light[] =
{
	DISPLAY_GROUP1|DISPLAY_GROUP2|DISPLAY_GROUP3,
	DISPLAY_GROUP5,
	DISPLAY_GROUP9|DISPLAY_GROUP12,
	DISPLAY_GROUP13|DISPLAY_GROUP16
};	
static unsigned char Pattern_LeftArrow_1_15_light[] =
{
	DISPLAY_GROUP1|DISPLAY_GROUP2|DISPLAY_GROUP3,
	DISPLAY_GROUP5|DISPLAY_GROUP8,
	DISPLAY_GROUP9|DISPLAY_GROUP12,
	DISPLAY_GROUP13|DISPLAY_GROUP16
};	
static unsigned char Pattern_RightStemArrow_1_25_light[] =
{
	DISPLAY_GROUP1|DISPLAY_GROUP4,
	0,
	0,
	0
};
static unsigned char Pattern_RightStemArrow_2_25_light[] =
{
	DISPLAY_GROUP1|DISPLAY_GROUP4,
	0,
	DISPLAY_GROUP9|DISPLAY_GROUP12,
	DISPLAY_GROUP13
};
static unsigned char Pattern_RightStemArrow_3_25_light[] =
{
	DISPLAY_GROUP1|DISPLAY_GROUP4,
	0,
	DISPLAY_GROUP9|DISPLAY_GROUP10|DISPLAY_GROUP12,
	DISPLAY_GROUP13|DISPLAY_GROUP14|DISPLAY_GROUP16	
};
static unsigned char Pattern_RightStemArrow_1_15_light[] =
{
	DISPLAY_GROUP1|DISPLAY_GROUP4,
	0,
	0,
	0
};
static unsigned char Pattern_RightStemArrow_2_15_light[] =
{
	DISPLAY_GROUP1|DISPLAY_GROUP4,
	0,
	DISPLAY_GROUP9|DISPLAY_GROUP12,
	DISPLAY_GROUP13
};
static unsigned char Pattern_RightStemArrow_3_15_light[] =
{
	DISPLAY_GROUP1|DISPLAY_GROUP4,
	0,
	DISPLAY_GROUP9|DISPLAY_GROUP10|DISPLAY_GROUP11|DISPLAY_GROUP12,
	DISPLAY_GROUP13|DISPLAY_GROUP14|DISPLAY_GROUP16	
};

static unsigned char Pattern_LeftStemArrow_1_25_light[] =
{
	0,
	DISPLAY_GROUP5,
	0,
	DISPLAY_GROUP16	
};
static unsigned char Pattern_LeftStemArrow_2_25_light[] =
{
	0,
	DISPLAY_GROUP5,
	DISPLAY_GROUP9|DISPLAY_GROUP12,
	DISPLAY_GROUP13|DISPLAY_GROUP16
};
static unsigned char Pattern_LeftStemArrow_3_25_light[] =
{
	DISPLAY_GROUP1|DISPLAY_GROUP2|DISPLAY_GROUP3,
	DISPLAY_GROUP5,
	DISPLAY_GROUP9|DISPLAY_GROUP12,
	DISPLAY_GROUP13|DISPLAY_GROUP16
};
static unsigned char Pattern_LeftStemArrow_1_15_light[] =
{
	0,
	DISPLAY_GROUP5,
	0,
	DISPLAY_GROUP16	
};
static unsigned char Pattern_LeftStemArrow_2_15_light[] =
{
	0,
	DISPLAY_GROUP5,
	DISPLAY_GROUP9|DISPLAY_GROUP12,
	DISPLAY_GROUP13|DISPLAY_GROUP16
};
static unsigned char Pattern_LeftStemArrow_3_15_light[] =
{
	DISPLAY_GROUP1|DISPLAY_GROUP2|DISPLAY_GROUP3,
	DISPLAY_GROUP5|DISPLAY_GROUP8,
	DISPLAY_GROUP9|DISPLAY_GROUP12,
	DISPLAY_GROUP13|DISPLAY_GROUP16
};
static unsigned char Pattern_RightWalkingArrow_1_25_light[] =
{
	DISPLAY_GROUP1|DISPLAY_GROUP3|DISPLAY_GROUP4,
	DISPLAY_GROUP8,
	DISPLAY_GROUP12,
	0
};	
static unsigned char Pattern_RightWalkingArrow_2_25_light[] =
{	
	DISPLAY_GROUP1|DISPLAY_GROUP4,
	DISPLAY_GROUP7,
	DISPLAY_GROUP12,
	DISPLAY_GROUP13|DISPLAY_GROUP15
};
static unsigned char Pattern_RightWalkingArrow_3_25_light[] =
{
	DISPLAY_GROUP1|DISPLAY_GROUP4,
	0,
	DISPLAY_GROUP9|DISPLAY_GROUP10|DISPLAY_GROUP12,
	DISPLAY_GROUP13|DISPLAY_GROUP14|DISPLAY_GROUP16
};
static unsigned char Pattern_LeftWalkingArrow_1_25_light[] =
{ 
	0,
	DISPLAY_GROUP5,
	DISPLAY_GROUP11,
	DISPLAY_GROUP13|DISPLAY_GROUP14|DISPLAY_GROUP16
};	
static unsigned char Pattern_LeftWalkingArrow_2_25_light[] =
{
	0,
	DISPLAY_GROUP5|DISPLAY_GROUP6,
	DISPLAY_GROUP12,
	DISPLAY_GROUP13|DISPLAY_GROUP15|DISPLAY_GROUP16
};
static unsigned char Pattern_LeftWalkingArrow_3_25_light[] =
{
	DISPLAY_GROUP1|DISPLAY_GROUP2|DISPLAY_GROUP3,
	DISPLAY_GROUP5,
	DISPLAY_GROUP9|DISPLAY_GROUP12,
	DISPLAY_GROUP13|DISPLAY_GROUP16
};
static unsigned char Pattern_RightChevron_1_25_light[] =
{
	DISPLAY_GROUP3,
	DISPLAY_GROUP8,
	DISPLAY_GROUP12,
	0
};	
static unsigned char Pattern_RightChevron_2_25_light[] =
{ 
	DISPLAY_GROUP3,
	DISPLAY_GROUP7|DISPLAY_GROUP8,
	DISPLAY_GROUP12,
	DISPLAY_GROUP13|DISPLAY_GROUP15
};	
static unsigned char Pattern_RightChevron_3_25_light[] =
{
	DISPLAY_GROUP3,
	DISPLAY_GROUP7|DISPLAY_GROUP8,
	DISPLAY_GROUP10|DISPLAY_GROUP12,
	DISPLAY_GROUP13|DISPLAY_GROUP14|DISPLAY_GROUP15|DISPLAY_GROUP16
};	
static unsigned char Pattern_LeftChevron_1_25_light[] =
{
	0,
	0,
	DISPLAY_GROUP11,
	DISPLAY_GROUP13|DISPLAY_GROUP14	
};
static unsigned char Pattern_LeftChevron_2_25_light[] =
{
	0,
	DISPLAY_GROUP6,
	DISPLAY_GROUP11|DISPLAY_GROUP12,
	DISPLAY_GROUP13|DISPLAY_GROUP14|DISPLAY_GROUP15	
};
static unsigned char Pattern_LeftChevron_3_25_light[] =
{ 
	DISPLAY_GROUP1|DISPLAY_GROUP2|DISPLAY_GROUP3,
	DISPLAY_GROUP6,
	DISPLAY_GROUP11|DISPLAY_GROUP12,
	DISPLAY_GROUP13|DISPLAY_GROUP14|DISPLAY_GROUP15
};
static unsigned char Pattern_DoubleDiamond_1_25_light[] =
{ 
	DISPLAY_GROUP1|DISPLAY_GROUP2|DISPLAY_GROUP3,
	DISPLAY_GROUP8,
	DISPLAY_GROUP12,
	0		
};
static unsigned char Pattern_DoubleDiamond_2_25_light[] =
{ 
	0,
	0,
	DISPLAY_GROUP10|DISPLAY_GROUP11,
	DISPLAY_GROUP13|DISPLAY_GROUP14|DISPLAY_GROUP16
};
#if 0
/////////////////
// 15 light sequential
// note: the grouping is the same
// as for the 25 light sequential
/////////////////
static unsigned char Pattern_AllOn_sequential_15_light[] =
{
	DISPLAY_GROUP1|DISPLAY_GROUP2|DISPLAY_GROUP3|DISPLAY_GROUP4,
	DISPLAY_GROUP5|DISPLAY_GROUP6|DISPLAY_GROUP7|DISPLAY_GROUP8,
	DISPLAY_GROUP9|DISPLAY_GROUP10|DISPLAY_GROUP11|DISPLAY_GROUP12,
	DISPLAY_GROUP13|DISPLAY_GROUP14|DISPLAY_GROUP15|DISPLAY_GROUP16
};
static unsigned char Pattern_FourCorner_1_sequential_15_light[] =
{
	DISPLAY_GROUP3,
	0,
	0,
	DISPLAY_GROUP14
};	

static unsigned char Pattern_DoubleArrow_1_sequential_15_light[] =
{ 
	DISPLAY_GROUP1|DISPLAY_GROUP2|DISPLAY_GROUP3,
	0,
	DISPLAY_GROUP9|DISPLAY_GROUP10|DISPLAY_GROUP12,
	DISPLAY_GROUP13|DISPLAY_GROUP14|DISPLAY_GROUP16,
};	
static unsigned char Pattern_Bar_1_sequential_15_light[] =
{ 
	DISPLAY_GROUP1|DISPLAY_GROUP4,
	DISPLAY_GROUP5,
	DISPLAY_GROUP9|DISPLAY_GROUP12,
	DISPLAY_GROUP13|DISPLAY_GROUP16,
};

static unsigned char Pattern_RightArrow_1_sequential_15_light[] =
{ 
	DISPLAY_GROUP1|DISPLAY_GROUP4,
	0,
	DISPLAY_GROUP9|DISPLAY_GROUP10|DISPLAY_GROUP12,
	DISPLAY_GROUP13|DISPLAY_GROUP14|DISPLAY_GROUP16,
};

static unsigned char Pattern_LeftArrow_1_sequential_15_light[] =
{
	DISPLAY_GROUP1|DISPLAY_GROUP2|DISPLAY_GROUP3,
	DISPLAY_GROUP5,
	DISPLAY_GROUP9|DISPLAY_GROUP12,
	DISPLAY_GROUP13|DISPLAY_GROUP16,
};

static unsigned char Pattern_RightStemArrow_1_sequential_15_light[] =
{ 
	DISPLAY_GROUP1|DISPLAY_GROUP4,
	0,
	0,
	0
};
static unsigned char Pattern_RightStemArrow_2_sequential_15_light[] =
{	
	DISPLAY_GROUP1|DISPLAY_GROUP4,
	0,
	DISPLAY_GROUP9|DISPLAY_GROUP12,
	DISPLAY_GROUP13,
};	
	static unsigned char Pattern_RightStemArrow_3_sequential_15_light[] =
{ 
	DISPLAY_GROUP1|DISPLAY_GROUP4,
	0,
	DISPLAY_GROUP9|DISPLAY_GROUP10|DISPLAY_GROUP12,
	DISPLAY_GROUP13|DISPLAY_GROUP14|DISPLAY_GROUP16,
};

static unsigned char Pattern_LeftStemArrow_1_sequential_15_light[] =
{ 
	0,
	DISPLAY_GROUP5,
	0,
	DISPLAY_GROUP16,
};	
static unsigned char Pattern_LeftStemArrow_2_sequential_15_light[] =
{  
	0,
	DISPLAY_GROUP5,
	DISPLAY_GROUP9|DISPLAY_GROUP12,
	DISPLAY_GROUP13|DISPLAY_GROUP16,
};
static unsigned char Pattern_LeftStemArrow_3_sequential_15_light[] =
{  
	DISPLAY_GROUP1|DISPLAY_GROUP2|DISPLAY_GROUP3,
	DISPLAY_GROUP5,
	DISPLAY_GROUP9|DISPLAY_GROUP12,
	DISPLAY_GROUP13|DISPLAY_GROUP16,
};
/////////////////
// 15 light flashing
// note: the grouping is the same
// as for the 25 light sequential
/////////////////
static unsigned char Pattern_AllOn_flashing_15_light[] =
{
	DISPLAY_GROUP1|DISPLAY_GROUP2|DISPLAY_GROUP3|DISPLAY_GROUP4,
	DISPLAY_GROUP5|DISPLAY_GROUP6|DISPLAY_GROUP7|DISPLAY_GROUP8,
	DISPLAY_GROUP9|DISPLAY_GROUP10|DISPLAY_GROUP11|DISPLAY_GROUP12,
	DISPLAY_GROUP13|DISPLAY_GROUP14|DISPLAY_GROUP15|DISPLAY_GROUP16
};
static unsigned char Pattern_FourCorner_1_flashing_15_light[] =
{ 
	DISPLAY_GROUP3,
	0,
	0,
	DISPLAY_GROUP14
};
static unsigned char Pattern_DoubleArrow_1_flashing_15_light[] =
{ 
	DISPLAY_GROUP1|DISPLAY_GROUP2|DISPLAY_GROUP3,
	0,
	DISPLAY_GROUP9|DISPLAY_GROUP10|DISPLAY_GROUP12,
	DISPLAY_GROUP13|DISPLAY_GROUP14|DISPLAY_GROUP16,
};	
static unsigned char Pattern_Bar_1_flashing_15_light[] =
{ 
	DISPLAY_GROUP1|DISPLAY_GROUP4,
	DISPLAY_GROUP5,
	DISPLAY_GROUP9|DISPLAY_GROUP12,
	DISPLAY_GROUP13|DISPLAY_GROUP16,
};
static unsigned char Pattern_RightArrow_1_flashing_15_light[] =
{ 
	DISPLAY_GROUP1|DISPLAY_GROUP4,
	0,
	DISPLAY_GROUP9|DISPLAY_GROUP10|DISPLAY_GROUP12,
	DISPLAY_GROUP13|DISPLAY_GROUP14|DISPLAY_GROUP16,
};	
static unsigned char Pattern_LeftArrow_1_flashing_15_light[] =
{ 
	DISPLAY_GROUP1|DISPLAY_GROUP2|DISPLAY_GROUP3,
	DISPLAY_GROUP5,
	DISPLAY_GROUP9|DISPLAY_GROUP12,
	DISPLAY_GROUP13|DISPLAY_GROUP16,
};
#endif
typedef enum eIndicatorPatterns
{
	eINDICATOR_PATTERN_OFF,
	eINDICATOR_PATTERN_RIGHT,
	eINDICATOR_PATTERN_LEFT,
	eINCICATOR_PATTERN_BOTH,
	eINDICATOR_PATTERN_MOVING_RIGHT,
	eINDICATOR_PATTERN_MOVING_LEFT,
	eINDICATOR_PATTERN_WIG_WAG,
}eINDICATOR_PATTERNS;

/////
// indicators installed on all trailer signs
// but not on vehicle mount signs
/////
BOOL displayHasIndicators()
{
	BOOL bRetVal = FALSE;
	switch(eModel)
	{
	////////////////////////////////////////
	//// Vehicle Mount
	////////////////////////////////////////
		case eMODEL_VEHICLE_25_LIGHT_SEQUENTIAL:
		case eMODEL_VEHICLE_15_LIGHT_SEQUENTIAL:
		case eMODEL_VEHICLE_15_LIGHT_FLASHING:
		case eMODEL_VEHICLE_15_LIGHT_WIG_WAG:
		case eMODEL_NONE:
		default:
			break;
	
	//////////////////////////////////////////
	//// Solar Trailer
	//////////////////////////////////////////
		case eMODEL_TRAILER_25_LIGHT_SEQUENTIAL:
		case eMODEL_TRAILER_15_LIGHT_SEQUENTIAL:
		case eMODEL_TRAILER_15_LIGHT_FLASHING:
		case eMODEL_TRAILER_15_LIGHT_WIG_WAG:
				/////
				// 051815
				// ignore indicators
				// no one has the kind we provide
				// we don't know how to tell if they are installed
				/////
				//bRetVal = TRUE;
				break;
	}
	return bRetVal;
}
void displaySetIndicatorPatterns(eINDICATOR_PATTERNS eIndicatorPattern)
{
	switch(eIndicatorPattern)
	{
		case eINDICATOR_PATTERN_OFF:
			nIndicatorPatterns = 1;
			indicatorLeft[0] = eLED_OFF;
			indicatorRight[0] = eLED_OFF;
			break;
		case eINDICATOR_PATTERN_RIGHT:
			nIndicatorPatterns = 2;
			indicatorLeft[0] = eLED_OFF;
			indicatorRight[0] = eLED_ON;
			indicatorLeft[1] = eLED_OFF;
			indicatorRight[1] = eLED_OFF;
			break;
		case eINDICATOR_PATTERN_LEFT:
			nIndicatorPatterns = 2;
			indicatorLeft[0] = eLED_ON;
			indicatorRight[0] = eLED_OFF;
			indicatorLeft[1] = eLED_OFF;
			indicatorRight[1] = eLED_OFF;
			break;
		case eINCICATOR_PATTERN_BOTH:
			nIndicatorPatterns = 2;
			indicatorLeft[0] = eLED_ON;
			indicatorRight[0] = eLED_ON;
			indicatorLeft[1] = eLED_OFF;
			indicatorRight[1] = eLED_OFF;
			break;
		case eINDICATOR_PATTERN_MOVING_RIGHT:
			nIndicatorPatterns = 4;
			indicatorLeft[0] = eLED_ON;
			indicatorRight[0] = eLED_OFF;
			indicatorLeft[1] = eLED_OFF;
			indicatorRight[1] = eLED_ON;
			indicatorLeft[2] = eLED_OFF;
			indicatorRight[2] = eLED_ON;
			indicatorLeft[3] = eLED_OFF;
			indicatorRight[3] = eLED_OFF;
			break;
		case eINDICATOR_PATTERN_MOVING_LEFT:
			nIndicatorPatterns = 4;
			indicatorLeft[0] = eLED_OFF;
			indicatorRight[0] = eLED_ON;
			indicatorLeft[1] = eLED_ON;
			indicatorRight[1] = eLED_OFF;
			indicatorLeft[2] = eLED_ON;
			indicatorRight[2] = eLED_OFF;
			indicatorLeft[3] = eLED_OFF;
			indicatorRight[3] = eLED_OFF;
			break;
		case eINDICATOR_PATTERN_WIG_WAG:
			nIndicatorPatterns = 2;
			indicatorLeft[0] = eLED_ON;
			indicatorRight[0] = eLED_OFF;
			indicatorLeft[1] = eLED_OFF;
			indicatorRight[1] = eLED_ON;
			break;
		default:		// This is here to get rid of compiler warnings 
			(void)indicatorLeft;
			(void)indicatorRight;
		break;
	}
}


static void displayAddConfigureI2CQueueEntry()
{
		I2CQueueEntryInitialize(&myI2CQueueEntry);
		
		if(nBoardRev < 6) // pre rev J (note H never released to production)
		{
			I2CQueueInitializeTransaction(&group_1_8_Transaction);
			I2CQueueInitializeTransaction(&group_9_16_Transaction);
	
			group_1_8_Transaction.nSLA_W = PCA9634_G1_8_SLA;
			group_1_8_Transaction.nSLA_R = 0;
			group_1_8_Transaction.cOutgoingData[0] = MODE1_INC;
			group_1_8_Transaction.cOutgoingData[1] = DISPLAY_CONFIG_MODEREG1;
			group_1_8_Transaction.cOutgoingData[2] = DISPLAY_CONFIG_MODEREG2;
			group_1_8_Transaction.nOutgoingDataLength = 3;
			group_1_8_Transaction.nIncomingDataLength = 0;
	
			group_9_16_Transaction.nSLA_W = PCA9634_G9_16_SLA;
			group_9_16_Transaction.nSLA_R = 0;
			group_9_16_Transaction.cOutgoingData[0] = MODE1_INC;
			group_9_16_Transaction.cOutgoingData[1] = DISPLAY_CONFIG_MODEREG1;
			group_9_16_Transaction.cOutgoingData[2] = DISPLAY_CONFIG_MODEREG2;
			group_9_16_Transaction.nOutgoingDataLength = 3;
			group_9_16_Transaction.nIncomingDataLength = 0;
	
			I2CQueueAddTransaction(&myI2CQueueEntry, &group_1_8_Transaction);
			I2CQueueAddTransaction(&myI2CQueueEntry, &group_9_16_Transaction);
	}
	else
	{
			I2CQueueInitializeTransaction(&group_1_16_Transaction);
	
			group_1_16_Transaction.nSLA_W = PCA9635_SLA;
			group_1_16_Transaction.nSLA_R = 0;
			group_1_16_Transaction.cOutgoingData[0] = MODE1_INC;
			group_1_16_Transaction.cOutgoingData[1] = DISPLAY_CONFIG_MODEREG1;
			group_1_16_Transaction.cOutgoingData[2] = DISPLAY_CONFIG_MODEREG2;
			group_1_16_Transaction.nOutgoingDataLength = 3;
			group_1_16_Transaction.nIncomingDataLength = 0;
		
			I2CQueueAddTransaction(&myI2CQueueEntry, &group_1_16_Transaction);
	}
}


static void displayAddI2CQueueEntry()
{
		I2CQueueEntryInitialize(&myI2CQueueEntry);
		
		if(nBoardRev < 6) 
		{
			I2CQueueInitializeTransaction(&group_1_8_Transaction);
			I2CQueueInitializeTransaction(&group_9_16_Transaction);

			group_1_8_Transaction.nSLA_W = PCA9634_G1_8_SLA;
			group_1_8_Transaction.nSLA_R = 0;
			group_1_8_Transaction.cOutgoingData[0] = LEDOUT0;
			group_1_8_Transaction.cOutgoingData[1] = displayPatterns[nCurrentDisplayPatternIndex].nGroup1_4Bits;
			group_1_8_Transaction.cOutgoingData[2] = displayPatterns[nCurrentDisplayPatternIndex].nGroup5_8Bits;
			group_1_8_Transaction.nOutgoingDataLength = 3;
			group_1_8_Transaction.nIncomingDataLength = 0;
	
			group_9_16_Transaction.nSLA_W = PCA9634_G9_16_SLA;
			group_9_16_Transaction.nSLA_R = 0;
			group_9_16_Transaction.cOutgoingData[0] = LEDOUT0;
			group_9_16_Transaction.cOutgoingData[1] = displayPatterns[nCurrentDisplayPatternIndex].nGroup9_12Bits;
			group_9_16_Transaction.cOutgoingData[2] = displayPatterns[nCurrentDisplayPatternIndex].nGroup13_16Bits;
			group_9_16_Transaction.nOutgoingDataLength = 3;
			group_9_16_Transaction.nIncomingDataLength = 0;
//printf("1-displayAddI2CQueueEntry nSLA_W[%X] nSLA_R[%X]\n", group_1_8_Transaction.nSLA_W, group_1_8_Transaction.nSLA_R);
//printf("1A-[%X][%X][%X]\n\r",group_1_8_Transaction.cOutgoingData[0], group_1_8_Transaction.cOutgoingData[1], group_1_8_Transaction.cOutgoingData[2]);
//printf("2-displayAddI2CQueueEntry nSLA_W[%X] nSLA_R[%X]\n", group_9_16_Transaction.nSLA_W, group_9_16_Transaction.nSLA_R);
//printf("2A-[%X][%X][%X]\n\r",group_9_16_Transaction.cOutgoingData[0], group_9_16_Transaction.cOutgoingData[1], group_9_16_Transaction.cOutgoingData[2]);
	
			I2CQueueAddTransaction(&myI2CQueueEntry, &group_1_8_Transaction);
			I2CQueueAddTransaction(&myI2CQueueEntry, &group_9_16_Transaction);
	}
	else	// Rev 6 or better
	{
			I2CQueueInitializeTransaction(&group_1_16_Transaction);

			group_1_16_Transaction.nSLA_W = PCA9635_SLA;
			group_1_16_Transaction.nSLA_R = 0;
			group_1_16_Transaction.cOutgoingData[0] = PCA9635_LEDOUT0;
			group_1_16_Transaction.cOutgoingData[1] = displayPatterns[nCurrentDisplayPatternIndex].nGroup1_4Bits;
			group_1_16_Transaction.cOutgoingData[2] = displayPatterns[nCurrentDisplayPatternIndex].nGroup5_8Bits;
			group_1_16_Transaction.cOutgoingData[3] = displayPatterns[nCurrentDisplayPatternIndex].nGroup9_12Bits;
			group_1_16_Transaction.cOutgoingData[4] = displayPatterns[nCurrentDisplayPatternIndex].nGroup13_16Bits;	
			group_1_16_Transaction.nOutgoingDataLength = 5;
			group_1_16_Transaction.nIncomingDataLength = 0;
			
			I2CQueueAddTransaction(&myI2CQueueEntry, &group_1_16_Transaction);
	}
					
}

static void displayBuildErroredLightsOffPattern()
{
	nDisplayErrors = errorInputGetDriveErrors();
	memset((unsigned char*)Pattern_ErroredLights_25_light, 0, sizeof(Pattern_ErroredLights_25_light));
	//printf("displayBuildErroredLightsOffPattern[%X]\n", nDisplayErrors);
	if(0 == (nDisplayErrors&eDISPLAY_ERROR_GROUP_1))
	{
		Pattern_ErroredLights_25_light[0] |= DISPLAY_GROUP1;
	}
	if(0 == (nDisplayErrors&eDISPLAY_ERROR_GROUP_2))
	{
		Pattern_ErroredLights_25_light[0] |= DISPLAY_GROUP2;
	}
	if(0 == (nDisplayErrors&eDISPLAY_ERROR_GROUP_3))
	{
		Pattern_ErroredLights_25_light[0] |= DISPLAY_GROUP3;
	}
	if(0 == (nDisplayErrors&eDISPLAY_ERROR_GROUP_4))
	{
		Pattern_ErroredLights_25_light[0] |= DISPLAY_GROUP4;
	}
	if(0 == (nDisplayErrors&eDISPLAY_ERROR_GROUP_5))
	{
		Pattern_ErroredLights_25_light[1] |= DISPLAY_GROUP5;
	}
	if(0 == (nDisplayErrors&eDISPLAY_ERROR_GROUP_6))
	{
		Pattern_ErroredLights_25_light[1] |= DISPLAY_GROUP6;
	}
	if(0 == (nDisplayErrors&eDISPLAY_ERROR_GROUP_7))
	{
		Pattern_ErroredLights_25_light[1] |= DISPLAY_GROUP7;
	}
	if(0 == (nDisplayErrors&eDISPLAY_ERROR_GROUP_8))
	{
		Pattern_ErroredLights_25_light[1] |= DISPLAY_GROUP8;
	}
	if(0 == (nDisplayErrors&eDISPLAY_ERROR_GROUP_9))
	{
		Pattern_ErroredLights_25_light[2] |= DISPLAY_GROUP9;
	}
	if(0 == (nDisplayErrors&eDISPLAY_ERROR_GROUP_10))
	{
		Pattern_ErroredLights_25_light[2] |= DISPLAY_GROUP10;
	}
	if(0 == (nDisplayErrors&eDISPLAY_ERROR_GROUP_11))
	{
		Pattern_ErroredLights_25_light[2] |= DISPLAY_GROUP11;
	}
	if(0 == (nDisplayErrors&eDISPLAY_ERROR_GROUP_12))
	{
		Pattern_ErroredLights_25_light[2] |= DISPLAY_GROUP12;
	}
	if(0 == (nDisplayErrors&eDISPLAY_ERROR_GROUP_13))
	{
		Pattern_ErroredLights_25_light[3] |= DISPLAY_GROUP13;
	}
	if(0 == (nDisplayErrors&eDISPLAY_ERROR_GROUP_14))
	{
		Pattern_ErroredLights_25_light[3] |= DISPLAY_GROUP14;
	}
	if(0 == (nDisplayErrors&eDISPLAY_ERROR_GROUP_15))
	{
		Pattern_ErroredLights_25_light[3] |= DISPLAY_GROUP15;
	}
	if(0 == (nDisplayErrors&eDISPLAY_ERROR_GROUP_16))
	{
		Pattern_ErroredLights_25_light[3] |= DISPLAY_GROUP16;
	}
}
static void displaySetPatternData(int nPatternIndex, int nPatternTime, unsigned char* pPatternData)
{
			displayPatterns[nPatternIndex].nGroup1_4Bits		= pPatternData[0];
			displayPatterns[nPatternIndex].nGroup5_8Bits		= pPatternData[1];
			displayPatterns[nPatternIndex].nGroup9_12Bits		= pPatternData[2];
			displayPatterns[nPatternIndex].nGroup13_16Bits	= pPatternData[3];
			displayPatterns[nPatternIndex].nPatternTimeMS		= nPatternTime;
}

static BOOL isPatternBlank(int nIndex)
{
	BOOL bRetVal = TRUE;
	if((displayPatterns[nIndex].nGroup1_4Bits != 0) || 
		(displayPatterns[nIndex].nGroup5_8Bits != 0) ||
		(displayPatterns[nIndex].nGroup9_12Bits	!= 0) ||
		(displayPatterns[nIndex].nGroup13_16Bits != 0))
	{
		bRetVal = FALSE;
	}
	return bRetVal;
}	
static ePACKET_STATUS displayChange25Light(eDISPLAY_TYPES eDisplaySelection)
{
	ePACKET_STATUS eRetVal = ePACKET_STATUS_SUCCESS;

	switch(eDisplaySelection)
	{
		case eDISPLAY_TYPE_BLANK:
			nDisplayPatterns = 1;

			displaySetPatternData(0, DISPLAY_FLASHING_TIME_MS, Pattern_Blank);
		
			displaySetIndicatorPatterns(eINDICATOR_PATTERN_OFF);

			break;
		case eDISPLAY_TYPE_FOUR_CORNER:
			nDisplayPatterns = 2;
		
			displaySetPatternData(0, DISPLAY_FLASHING_TIME_MS, Pattern_FourCorner_1_25_light);
			displaySetPatternData(1, DISPLAY_FLASHING_TIME_MS, Pattern_Blank);
		
			displaySetIndicatorPatterns(eINCICATOR_PATTERN_BOTH);
			break;
		case eDISPLAY_TYPE_DOUBLE_ARROW:
			nDisplayPatterns = 2;

		  displaySetPatternData(0, DISPLAY_FLASHING_TIME_MS, Pattern_DoubleArrow_1_25_light);
		  displaySetPatternData(1, DISPLAY_FLASHING_TIME_MS, Pattern_Blank);
		
			displaySetIndicatorPatterns(eINCICATOR_PATTERN_BOTH);
			break;
		case eDISPLAY_TYPE_BAR:
			nDisplayPatterns = 2;

		  displaySetPatternData(0, DISPLAY_FLASHING_TIME_MS, Pattern_Bar_1_25_light);
		  displaySetPatternData(1, DISPLAY_FLASHING_TIME_MS, Pattern_Blank);
		
			displaySetIndicatorPatterns(eINCICATOR_PATTERN_BOTH);
			break;
		case eDISPLAY_TYPE_RIGHT_ARROW:
			nDisplayPatterns = 2;

		  displaySetPatternData(0, DISPLAY_FLASHING_TIME_MS, Pattern_RightArrow_1_25_light);
		  displaySetPatternData(1, DISPLAY_FLASHING_TIME_MS, Pattern_Blank);
		
			displaySetIndicatorPatterns(eINDICATOR_PATTERN_RIGHT);
			break;
		case eDISPLAY_TYPE_LEFT_ARROW:
			nDisplayPatterns = 2;

		  displaySetPatternData(0, DISPLAY_FLASHING_TIME_MS, Pattern_LeftArrow_1_25_light);
		  displaySetPatternData(1, DISPLAY_FLASHING_TIME_MS, Pattern_Blank);
		
			displaySetIndicatorPatterns(eINDICATOR_PATTERN_LEFT);
			break;
		case eDISPLAY_TYPE_RIGHT_STEM_ARROW:
			nDisplayPatterns = 4;

		  displaySetPatternData(0, DISPLAY_SEQUENCE_4_TIME_MS, Pattern_RightStemArrow_1_25_light);
		  displaySetPatternData(1, DISPLAY_SEQUENCE_4_TIME_MS, Pattern_RightStemArrow_2_25_light);
		  displaySetPatternData(2, DISPLAY_SEQUENCE_4_TIME_MS, Pattern_RightStemArrow_3_25_light);
		  displaySetPatternData(3, DISPLAY_SEQUENCE_4_TIME_MS, Pattern_Blank);
		
			displaySetIndicatorPatterns(eINDICATOR_PATTERN_MOVING_RIGHT);
			break;
		case eDISPLAY_TYPE_LEFT_STEM_ARROW:
			nDisplayPatterns = 4;

		  displaySetPatternData(0, DISPLAY_SEQUENCE_4_TIME_MS, Pattern_LeftStemArrow_1_25_light);
		  displaySetPatternData(1, DISPLAY_SEQUENCE_4_TIME_MS, Pattern_LeftStemArrow_2_25_light);
		  displaySetPatternData(2, DISPLAY_SEQUENCE_4_TIME_MS, Pattern_LeftStemArrow_3_25_light);
		  displaySetPatternData(3, DISPLAY_SEQUENCE_4_TIME_MS, Pattern_Blank);
		
			displaySetIndicatorPatterns(eINDICATOR_PATTERN_MOVING_LEFT);
			break;
		case eDISPLAY_TYPE_RIGHT_WALKING_ARROW:
			nDisplayPatterns = 4;
	
		  displaySetPatternData(0, DISPLAY_SEQUENCE_4_TIME_MS, Pattern_RightWalkingArrow_1_25_light);
		  displaySetPatternData(1, DISPLAY_SEQUENCE_4_TIME_MS, Pattern_RightWalkingArrow_2_25_light);
		  displaySetPatternData(2, DISPLAY_SEQUENCE_4_TIME_MS, Pattern_RightWalkingArrow_3_25_light);
		  displaySetPatternData(3, DISPLAY_SEQUENCE_4_TIME_MS, Pattern_Blank);
		
			displaySetIndicatorPatterns(eINDICATOR_PATTERN_MOVING_RIGHT);
			break;
		case eDISPLAY_TYPE_LEFT_WALKING_ARROW:
			nDisplayPatterns = 4;

		  displaySetPatternData(0, DISPLAY_SEQUENCE_4_TIME_MS, Pattern_LeftWalkingArrow_1_25_light);
		  displaySetPatternData(1, DISPLAY_SEQUENCE_4_TIME_MS, Pattern_LeftWalkingArrow_2_25_light);
		  displaySetPatternData(2, DISPLAY_SEQUENCE_4_TIME_MS, Pattern_LeftWalkingArrow_3_25_light);
		  displaySetPatternData(3, DISPLAY_SEQUENCE_4_TIME_MS, Pattern_Blank);
		
			displaySetIndicatorPatterns(eINDICATOR_PATTERN_MOVING_LEFT);
			break;
		case eDISPLAY_TYPE_RIGHT_CHEVRON:
			nDisplayPatterns = 4;
		
		  displaySetPatternData(0, DISPLAY_SEQUENCE_4_TIME_MS, Pattern_RightChevron_1_25_light);
		  displaySetPatternData(1, DISPLAY_SEQUENCE_4_TIME_MS, Pattern_RightChevron_2_25_light);
		  displaySetPatternData(2, DISPLAY_SEQUENCE_4_TIME_MS, Pattern_RightChevron_3_25_light);
		  displaySetPatternData(3, DISPLAY_SEQUENCE_4_TIME_MS, Pattern_Blank);
		
			displaySetIndicatorPatterns(eINDICATOR_PATTERN_MOVING_RIGHT);
			break;
		case eDISPLAY_TYPE_LEFT_CHEVRON:
			nDisplayPatterns = 4;
		
		  displaySetPatternData(0, DISPLAY_SEQUENCE_4_TIME_MS, Pattern_LeftChevron_1_25_light);
		  displaySetPatternData(1, DISPLAY_SEQUENCE_4_TIME_MS, Pattern_LeftChevron_2_25_light);
		  displaySetPatternData(2, DISPLAY_SEQUENCE_4_TIME_MS	, Pattern_LeftChevron_3_25_light);
		  displaySetPatternData(3, DISPLAY_SEQUENCE_4_TIME_MS, Pattern_Blank);
		
			displaySetIndicatorPatterns(eINDICATOR_PATTERN_MOVING_LEFT);
			break;
		case eDISPLAY_TYPE_DOUBLE_DIAMOND:
			nDisplayPatterns = 2;

		  displaySetPatternData(0, DISPLAY_FLASHING_TIME_MS, Pattern_DoubleDiamond_1_25_light);
		  displaySetPatternData(1, DISPLAY_FLASHING_TIME_MS, Pattern_DoubleDiamond_2_25_light);
		
			displaySetIndicatorPatterns(eINDICATOR_PATTERN_WIG_WAG);
			break;
		case eDISPLAY_TYPE_ALL_LIGHTS_ON:
			nDisplayPatterns = 1;

		  displaySetPatternData(0, DISPLAY_FLASHING_TIME_MS, Pattern_AllOn_25_light);
			displaySetIndicatorPatterns(eINCICATOR_PATTERN_BOTH);
			break;
		
			case eDISPLAY_TYPE_ERRORED_LIGHTS_OFF:
			nDisplayPatterns = 1;
			nDisplayErrors = 0;
			displayBuildErroredLightsOffPattern();
		  displaySetPatternData(0, DISPLAY_FLASHING_TIME_MS, Pattern_ErroredLights_25_light);
			displaySetIndicatorPatterns(eINDICATOR_PATTERN_OFF);
			break;	
	}
	
	/////
	// disable the lamps
	/////
	pwmSetDriver(PWM_SUSPEND);
	
	/////
	// schedule the first pattern
	/////
	nCurrentDisplayPatternIndex = 0;
	nCurrentIndicatorPatternIndex = 0;

/* MJB	
	if(displayHasIndicators())
	{
		ledDriverSetIndR(indicatorRight[nCurrentDisplayPatternIndex]);
		ledDriverSetIndL(indicatorLeft[nCurrentDisplayPatternIndex]);
	}
*/
	displayAddI2CQueueEntry();
	I2CQueueAddEntry(&myI2CQueueEntry);
	startTimer(&patternTimer, displayPatterns[0].nPatternTimeMS);
	return eRetVal;
}
static ePACKET_STATUS displayChange15Light(eDISPLAY_TYPES eDisplaySelection)
{
	ePACKET_STATUS eRetVal = ePACKET_STATUS_SUCCESS;

	switch(eDisplaySelection)
	{
		case eDISPLAY_TYPE_BLANK:
			nDisplayPatterns = 1;

			displaySetPatternData(0, DISPLAY_FLASHING_TIME_MS, Pattern_Blank);
		
			displaySetIndicatorPatterns(eINDICATOR_PATTERN_OFF);

			break;
		case eDISPLAY_TYPE_FOUR_CORNER:
			nDisplayPatterns = 2;
		
			switch(eModel)
			{

				case eMODEL_VEHICLE_15_LIGHT_SEQUENTIAL:
				case eMODEL_TRAILER_15_LIGHT_SEQUENTIAL:
				case eMODEL_VEHICLE_15_LIGHT_FLASHING:
				case eMODEL_TRAILER_15_LIGHT_FLASHING:
					displaySetPatternData(0, DISPLAY_FLASHING_TIME_MS, Pattern_FourCorner_1_15_light);
					displaySetPatternData(1, DISPLAY_FLASHING_TIME_MS, Pattern_Blank);
					break;

				case eMODEL_VEHICLE_15_LIGHT_WIG_WAG:
				case eMODEL_TRAILER_15_LIGHT_WIG_WAG:
					displaySetPatternData(0, DISPLAY_FLASHING_TIME_MS, Pattern_FourCorner_1_WigWag);
					displaySetPatternData(1, DISPLAY_FLASHING_TIME_MS, Pattern_FourCorner_2_WigWag);
					break;
				
				case eMODEL_VEHICLE_25_LIGHT_SEQUENTIAL:
				case eMODEL_TRAILER_25_LIGHT_SEQUENTIAL:
				case eMODEL_NONE:
					nDisplayPatterns = 1;

					displaySetPatternData(0, DISPLAY_FLASHING_TIME_MS, Pattern_Blank);
					break;				
			}
		
			displaySetIndicatorPatterns(eINCICATOR_PATTERN_BOTH);
			break;
		case eDISPLAY_TYPE_DOUBLE_ARROW:
			nDisplayPatterns = 2;

		  displaySetPatternData(0, DISPLAY_FLASHING_TIME_MS, Pattern_DoubleArrow_1_15_light);
		  displaySetPatternData(1, DISPLAY_FLASHING_TIME_MS, Pattern_Blank);
		
			displaySetIndicatorPatterns(eINCICATOR_PATTERN_BOTH);
			break;
		case eDISPLAY_TYPE_BAR:
			nDisplayPatterns = 2;

		  displaySetPatternData(0, DISPLAY_FLASHING_TIME_MS, Pattern_Bar_1_15_light);
		  displaySetPatternData(1, DISPLAY_FLASHING_TIME_MS, Pattern_Blank);
		
			displaySetIndicatorPatterns(eINCICATOR_PATTERN_BOTH);
			break;
		case eDISPLAY_TYPE_RIGHT_ARROW:
			nDisplayPatterns = 2;

		  displaySetPatternData(0, DISPLAY_FLASHING_TIME_MS, Pattern_RightArrow_1_15_light);
		  displaySetPatternData(1, DISPLAY_FLASHING_TIME_MS, Pattern_Blank);
		
			displaySetIndicatorPatterns(eINDICATOR_PATTERN_RIGHT);
			break;
		case eDISPLAY_TYPE_LEFT_ARROW:
			nDisplayPatterns = 2;

		  displaySetPatternData(0, DISPLAY_FLASHING_TIME_MS, Pattern_LeftArrow_1_15_light);
		  displaySetPatternData(1, DISPLAY_FLASHING_TIME_MS, Pattern_Blank);
		
			displaySetIndicatorPatterns(eINDICATOR_PATTERN_LEFT);
			break;
		case eDISPLAY_TYPE_RIGHT_STEM_ARROW:
			nDisplayPatterns = 4;

		  displaySetPatternData(0, DISPLAY_SEQUENCE_4_TIME_MS, Pattern_RightStemArrow_1_15_light);
		  displaySetPatternData(1, DISPLAY_SEQUENCE_4_TIME_MS, Pattern_RightStemArrow_2_15_light);
		  displaySetPatternData(2, DISPLAY_SEQUENCE_4_TIME_MS, Pattern_RightStemArrow_3_15_light);
		  displaySetPatternData(3, DISPLAY_SEQUENCE_4_TIME_MS, Pattern_Blank);
		
			displaySetIndicatorPatterns(eINDICATOR_PATTERN_MOVING_RIGHT);
			break;
		case eDISPLAY_TYPE_LEFT_STEM_ARROW:
			nDisplayPatterns = 4;

		  displaySetPatternData(0, DISPLAY_SEQUENCE_4_TIME_MS, Pattern_LeftStemArrow_1_15_light);
		  displaySetPatternData(1, DISPLAY_SEQUENCE_4_TIME_MS, Pattern_LeftStemArrow_2_15_light);
		  displaySetPatternData(2, DISPLAY_SEQUENCE_4_TIME_MS, Pattern_LeftStemArrow_3_15_light);
		  displaySetPatternData(3, DISPLAY_SEQUENCE_4_TIME_MS, Pattern_Blank);
		
			displaySetIndicatorPatterns(eINDICATOR_PATTERN_MOVING_LEFT);
			break;
		case eDISPLAY_TYPE_RIGHT_WALKING_ARROW:
		case eDISPLAY_TYPE_LEFT_WALKING_ARROW:
		case eDISPLAY_TYPE_RIGHT_CHEVRON:
		case eDISPLAY_TYPE_LEFT_CHEVRON:
		case eDISPLAY_TYPE_DOUBLE_DIAMOND:
			nDisplayPatterns = 1;

		  displaySetPatternData(0, DISPLAY_FLASHING_TIME_MS, Pattern_Blank);
		
			displaySetIndicatorPatterns(eINDICATOR_PATTERN_MOVING_RIGHT);
			break;

		case eDISPLAY_TYPE_ALL_LIGHTS_ON:
			nDisplayPatterns = 1;

		  displaySetPatternData(0, DISPLAY_FLASHING_TIME_MS, Pattern_AllOn_15_light);
			displaySetIndicatorPatterns(eINCICATOR_PATTERN_BOTH);
			break;
		
			case eDISPLAY_TYPE_ERRORED_LIGHTS_OFF:
			nDisplayPatterns = 1;
			nDisplayErrors = 0;
			displayBuildErroredLightsOffPattern();
		  displaySetPatternData(0, DISPLAY_FLASHING_TIME_MS, Pattern_ErroredLights_15_light);
			displaySetIndicatorPatterns(eINDICATOR_PATTERN_OFF);
			break;	
	}
	
	/////
	// disable the lamps
	/////
	pwmSetDriver(PWM_SUSPEND);
	
	/////
	// schedule the first pattern
	/////
	nCurrentDisplayPatternIndex = 0;
	nCurrentIndicatorPatternIndex = 0;
/* MJB	
	if(displayHasIndicators())
	{
		ledDriverSetIndR(indicatorRight[nCurrentDisplayPatternIndex]);
		ledDriverSetIndL(indicatorLeft[nCurrentDisplayPatternIndex]);
	}
*/	
	displayAddI2CQueueEntry();
	I2CQueueAddEntry(&myI2CQueueEntry);
	startTimer(&patternTimer, displayPatterns[0].nPatternTimeMS);
	return eRetVal;
}
static ePACKET_STATUS displayChange15LightWigWag(eDISPLAY_TYPES eDisplaySelection)
{
	ePACKET_STATUS eRetVal = ePACKET_STATUS_SUCCESS;

	switch(eDisplaySelection)
	{
		case eDISPLAY_TYPE_BLANK:
			nDisplayPatterns = 1;

			displaySetPatternData(0, DISPLAY_FLASHING_TIME_MS, Pattern_Blank);
		
			displaySetIndicatorPatterns(eINDICATOR_PATTERN_OFF);

			break;
		case eDISPLAY_TYPE_FOUR_CORNER:
			nDisplayPatterns = 2;
		
			displaySetPatternData(0, DISPLAY_FLASHING_TIME_MS, Pattern_FourCorner_1_WigWag);
			displaySetPatternData(1, DISPLAY_FLASHING_TIME_MS, Pattern_FourCorner_2_WigWag);
		
			displaySetIndicatorPatterns(eINCICATOR_PATTERN_BOTH);
			break;
		case eDISPLAY_TYPE_DOUBLE_ARROW:
			nDisplayPatterns = 2;

		  displaySetPatternData(0, DISPLAY_FLASHING_TIME_MS, Pattern_DoubleArrow_1_15_light);
		  displaySetPatternData(1, DISPLAY_FLASHING_TIME_MS, Pattern_Blank);
		
			displaySetIndicatorPatterns(eINCICATOR_PATTERN_BOTH);
			break;
		case eDISPLAY_TYPE_BAR:
			nDisplayPatterns = 2;

		  displaySetPatternData(0, DISPLAY_FLASHING_TIME_MS, Pattern_Bar_1_15_light);
		  displaySetPatternData(1, DISPLAY_FLASHING_TIME_MS, Pattern_Blank);
		
			displaySetIndicatorPatterns(eINCICATOR_PATTERN_BOTH);
			break;
		case eDISPLAY_TYPE_RIGHT_ARROW:
			nDisplayPatterns = 2;

		  displaySetPatternData(0, DISPLAY_FLASHING_TIME_MS, Pattern_RightArrow_1_15_light);
		  displaySetPatternData(1, DISPLAY_FLASHING_TIME_MS, Pattern_Blank);
		
			displaySetIndicatorPatterns(eINDICATOR_PATTERN_RIGHT);
			break;
		case eDISPLAY_TYPE_LEFT_ARROW:
			nDisplayPatterns = 2;

		  displaySetPatternData(0, DISPLAY_FLASHING_TIME_MS, Pattern_LeftArrow_1_15_light);
		  displaySetPatternData(1, DISPLAY_FLASHING_TIME_MS, Pattern_Blank);
		
			displaySetIndicatorPatterns(eINDICATOR_PATTERN_LEFT);
			break;
		case eDISPLAY_TYPE_RIGHT_STEM_ARROW:
			nDisplayPatterns = 4;

		  displaySetPatternData(0, DISPLAY_SEQUENCE_4_TIME_MS, Pattern_RightStemArrow_1_15_light);
		  displaySetPatternData(1, DISPLAY_SEQUENCE_4_TIME_MS, Pattern_RightStemArrow_2_15_light);
		  displaySetPatternData(2, DISPLAY_SEQUENCE_4_TIME_MS, Pattern_RightStemArrow_3_15_light);
		  displaySetPatternData(3, DISPLAY_SEQUENCE_4_TIME_MS, Pattern_Blank);
		
			displaySetIndicatorPatterns(eINDICATOR_PATTERN_MOVING_RIGHT);
			break;
		case eDISPLAY_TYPE_LEFT_STEM_ARROW:
			nDisplayPatterns = 4;

		  displaySetPatternData(0, DISPLAY_SEQUENCE_4_TIME_MS, Pattern_LeftStemArrow_1_15_light);
		  displaySetPatternData(1, DISPLAY_SEQUENCE_4_TIME_MS, Pattern_LeftStemArrow_2_15_light);
		  displaySetPatternData(2, DISPLAY_SEQUENCE_4_TIME_MS, Pattern_LeftStemArrow_3_15_light);
		  displaySetPatternData(3, DISPLAY_SEQUENCE_4_TIME_MS, Pattern_Blank);
		
			displaySetIndicatorPatterns(eINDICATOR_PATTERN_MOVING_LEFT);
			break;
		case eDISPLAY_TYPE_RIGHT_WALKING_ARROW:
		case eDISPLAY_TYPE_LEFT_WALKING_ARROW:
		case eDISPLAY_TYPE_RIGHT_CHEVRON:
		case eDISPLAY_TYPE_LEFT_CHEVRON:
		case eDISPLAY_TYPE_DOUBLE_DIAMOND:
					nDisplayPatterns = 1;
				  displaySetPatternData(1, DISPLAY_FLASHING_TIME_MS, Pattern_Blank);
					displaySetIndicatorPatterns(eINDICATOR_PATTERN_OFF);
			break;
		case eDISPLAY_TYPE_ALL_LIGHTS_ON:
			nDisplayPatterns = 1;

		  displaySetPatternData(0, DISPLAY_FLASHING_TIME_MS, Pattern_AllOn_15_light);
			displaySetIndicatorPatterns(eINCICATOR_PATTERN_BOTH);
			break;
		
			case eDISPLAY_TYPE_ERRORED_LIGHTS_OFF:
			nDisplayPatterns = 1;
			nDisplayErrors = 0;
			displayBuildErroredLightsOffPattern();
		  displaySetPatternData(0, DISPLAY_FLASHING_TIME_MS, Pattern_ErroredLights_15_light);
			displaySetIndicatorPatterns(eINDICATOR_PATTERN_OFF);
			break;	
	}
	
	/////
	// disable the lamps
	/////
	pwmSetDriver(PWM_SUSPEND);
	
	/////
	// schedule the first pattern
	/////
	nCurrentDisplayPatternIndex = 0;
	nCurrentIndicatorPatternIndex = 0;
/* MJB
	if(displayHasIndicators())
	{
		ledDriverSetIndR(indicatorRight[nCurrentDisplayPatternIndex]);
		ledDriverSetIndL(indicatorLeft[nCurrentDisplayPatternIndex]);
	}
*/
	displayAddI2CQueueEntry();
	I2CQueueAddEntry(&myI2CQueueEntry);
	startTimer(&patternTimer, displayPatterns[0].nPatternTimeMS);
	return eRetVal;
}
static ePACKET_STATUS displayChange15LightSequential(eDISPLAY_TYPES eDisplaySelection)
{
	ePACKET_STATUS eRetVal = ePACKET_STATUS_SUCCESS;
#if 1
	switch(eDisplaySelection)
	{
		case eDISPLAY_TYPE_BLANK:
		case eDISPLAY_TYPE_FOUR_CORNER:
		case eDISPLAY_TYPE_DOUBLE_ARROW:
		case eDISPLAY_TYPE_BAR:
		case eDISPLAY_TYPE_RIGHT_ARROW:
		case eDISPLAY_TYPE_LEFT_ARROW:
		case eDISPLAY_TYPE_RIGHT_STEM_ARROW:
		case eDISPLAY_TYPE_LEFT_STEM_ARROW:
		case eDISPLAY_TYPE_ALL_LIGHTS_ON:
		case eDISPLAY_TYPE_ERRORED_LIGHTS_OFF:
			/////
			// all of these signs have the same groupings
			// so treat them the same here
			/////
			eRetVal = displayChange15Light(eDisplaySelection);
			break;
		case eDISPLAY_TYPE_RIGHT_WALKING_ARROW:
		case eDISPLAY_TYPE_LEFT_WALKING_ARROW:
		case eDISPLAY_TYPE_RIGHT_CHEVRON:
		case eDISPLAY_TYPE_LEFT_CHEVRON:
		case eDISPLAY_TYPE_DOUBLE_DIAMOND:
			eRetVal = ePACKET_STATUS_NOT_SUPPORTED;
			break;
	}
#else
	switch(eDisplaySelection)
	{
		case eDISPLAY_TYPE_BLANK:
			nDisplayPatterns = 1;
	
			displaySetPatternData(0, DISPLAY_FLASHING_TIME_MS, Pattern_Blank);
		
			displaySetIndicatorPatterns(eINDICATOR_PATTERN_OFF);
			break;
		case eDISPLAY_TYPE_FOUR_CORNER:
			nDisplayPatterns = 2;

			displaySetPatternData(0, DISPLAY_FLASHING_TIME_MS, Pattern_FourCorner_1_sequential_15_light);
			displaySetPatternData(1, DISPLAY_FLASHING_TIME_MS, Pattern_Blank);
		
			displaySetIndicatorPatterns(eINCICATOR_PATTERN_BOTH);
			break;
		case eDISPLAY_TYPE_DOUBLE_ARROW:
			nDisplayPatterns = 2;
		
			displaySetPatternData(0, DISPLAY_FLASHING_TIME_MS, Pattern_DoubleArrow_1_sequential_15_light);
			displaySetPatternData(1, DISPLAY_FLASHING_TIME_MS, Pattern_Blank);
		
			displaySetIndicatorPatterns(eINCICATOR_PATTERN_BOTH);
			break;
		case eDISPLAY_TYPE_BAR:
			nDisplayPatterns = 2;
		
			displaySetPatternData(0, DISPLAY_FLASHING_TIME_MS, Pattern_Bar_1_sequential_15_light);
			displaySetPatternData(1, DISPLAY_FLASHING_TIME_MS, Pattern_Blank);
		
			displaySetIndicatorPatterns(eINCICATOR_PATTERN_BOTH);
			break;
		case eDISPLAY_TYPE_RIGHT_ARROW:
			nDisplayPatterns = 2;
		
			displaySetPatternData(0, DISPLAY_FLASHING_TIME_MS, Pattern_RightArrow_1_sequential_15_light);
			displaySetPatternData(1, DISPLAY_FLASHING_TIME_MS, Pattern_Blank);
		
			displaySetIndicatorPatterns(eINDICATOR_PATTERN_RIGHT);
			break;
		case eDISPLAY_TYPE_LEFT_ARROW:
			nDisplayPatterns = 2;
		
			displaySetPatternData(0, DISPLAY_FLASHING_TIME_MS, Pattern_LeftArrow_1_sequential_15_light);
			displaySetPatternData(1, DISPLAY_FLASHING_TIME_MS, Pattern_Blank);
		
			displaySetIndicatorPatterns(eINDICATOR_PATTERN_LEFT);
			break;
		case eDISPLAY_TYPE_RIGHT_STEM_ARROW:
			nDisplayPatterns = 4;
		
			displaySetPatternData(0, DISPLAY_SEQUENCE_4_TIME_MS, Pattern_RightStemArrow_1_sequential_15_light);
			displaySetPatternData(1, DISPLAY_SEQUENCE_4_TIME_MS, Pattern_RightStemArrow_2_sequential_15_light);
			displaySetPatternData(2, DISPLAY_SEQUENCE_4_TIME_MS, Pattern_RightStemArrow_3_sequential_15_light);
			displaySetPatternData(3, DISPLAY_SEQUENCE_4_TIME_MS, Pattern_Blank);
		
			displaySetIndicatorPatterns(eINDICATOR_PATTERN_MOVING_RIGHT);
			break;
		case eDISPLAY_TYPE_LEFT_STEM_ARROW:
			nDisplayPatterns = 4;
		
			displaySetPatternData(0, DISPLAY_SEQUENCE_4_TIME_MS, Pattern_LeftStemArrow_1_sequential_15_light);
			displaySetPatternData(1, DISPLAY_SEQUENCE_4_TIME_MS, Pattern_LeftStemArrow_2_sequential_15_light);
			displaySetPatternData(2, DISPLAY_SEQUENCE_4_TIME_MS, Pattern_LeftStemArrow_3_sequential_15_light);
			displaySetPatternData(3, DISPLAY_SEQUENCE_4_TIME_MS, Pattern_Blank);
		
			displaySetIndicatorPatterns(eINDICATOR_PATTERN_MOVING_LEFT);
			break;
		case eDISPLAY_TYPE_ALL_LIGHTS_ON:
			nDisplayPatterns = 1;

		  displaySetPatternData(0, DISPLAY_FLASHING_TIME_MS, Pattern_AllOn_sequential_15_light);
			displaySetIndicatorPatterns(eINCICATOR_PATTERN_BOTH);
			break;
		case eDISPLAY_TYPE_RIGHT_WALKING_ARROW:
		case eDISPLAY_TYPE_LEFT_WALKING_ARROW:
		case eDISPLAY_TYPE_RIGHT_CHEVRON:
		case eDISPLAY_TYPE_LEFT_CHEVRON:
		case eDISPLAY_TYPE_DOUBLE_DIAMOND:
			eRetVal = ePACKET_STATUS_NOT_SUPPORTED;
			break;
	}
	
	if(ePACKET_STATUS_SUCCESS == eRetVal)
	{
		/////
		// disable the lamps
		/////
		pwmSetDriver(PWM_SUSPEND);
	
		/////
		// schedule the first pattern
		/////
		nCurrentDisplayPatternIndex = 0;
		nCurrentIndicatorPatternIndex = 0;
/* MJB		
		if(displayHasIndicators())
		{
			ledDriverSetIndR(indicatorRight[nCurrentDisplayPatternIndex]);
			ledDriverSetIndL(indicatorLeft[nCurrentDisplayPatternIndex]);
		}
*/
		displayAddI2CQueueEntry();
		I2CQueueAddEntry(&myI2CQueueEntry);
		startTimer(&patternTimer, displayPatterns[0].nPatternTimeMS);
	}
#endif
	return eRetVal;
}
static ePACKET_STATUS displayChange15LightFlashing(eDISPLAY_TYPES eDisplaySelection)
{
	ePACKET_STATUS eRetVal = ePACKET_STATUS_SUCCESS;
#if 1
	switch(eDisplaySelection)
	{
		case eDISPLAY_TYPE_BLANK:
		case eDISPLAY_TYPE_FOUR_CORNER:
		case eDISPLAY_TYPE_DOUBLE_ARROW:
		case eDISPLAY_TYPE_BAR:
		case eDISPLAY_TYPE_RIGHT_ARROW:
		case eDISPLAY_TYPE_LEFT_ARROW:
		case eDISPLAY_TYPE_ALL_LIGHTS_ON:
		case eDISPLAY_TYPE_ERRORED_LIGHTS_OFF:
			/////
			// all of these signs have the same groupings
			// so treat them the same here
			/////
			eRetVal = displayChange15Light(eDisplaySelection);
			break;
		case eDISPLAY_TYPE_RIGHT_STEM_ARROW:
		case eDISPLAY_TYPE_LEFT_STEM_ARROW:
		case eDISPLAY_TYPE_RIGHT_WALKING_ARROW:
		case eDISPLAY_TYPE_LEFT_WALKING_ARROW:
		case eDISPLAY_TYPE_RIGHT_CHEVRON:
		case eDISPLAY_TYPE_LEFT_CHEVRON:
		case eDISPLAY_TYPE_DOUBLE_DIAMOND:
			eRetVal = ePACKET_STATUS_NOT_SUPPORTED;
			break;
	}
#else
	switch(eDisplaySelection)
	{
		case eDISPLAY_TYPE_BLANK:
			nDisplayPatterns = 1;			

			displaySetPatternData(0, DISPLAY_FLASHING_TIME_MS, Pattern_Blank);
		
			displaySetIndicatorPatterns(eINDICATOR_PATTERN_OFF);
			break;
		case eDISPLAY_TYPE_FOUR_CORNER:
			nDisplayPatterns = 2;
		
			displaySetPatternData(0, DISPLAY_FLASHING_TIME_MS, Pattern_FourCorner_1_flashing_15_light);
			displaySetPatternData(1, DISPLAY_FLASHING_TIME_MS, Pattern_Blank);
		
			displaySetIndicatorPatterns(eINCICATOR_PATTERN_BOTH);
			break;
		case eDISPLAY_TYPE_DOUBLE_ARROW:
			nDisplayPatterns = 2;
		
		  displaySetPatternData(0, DISPLAY_FLASHING_TIME_MS, Pattern_DoubleArrow_1_flashing_15_light);
		  displaySetPatternData(1, DISPLAY_FLASHING_TIME_MS, Pattern_Blank);
		
			displaySetIndicatorPatterns(eINCICATOR_PATTERN_BOTH);
			break;
		case eDISPLAY_TYPE_BAR:
			nDisplayPatterns = 2;
		
		  displaySetPatternData(0, DISPLAY_FLASHING_TIME_MS, Pattern_Bar_1_flashing_15_light);
		  displaySetPatternData(1, DISPLAY_FLASHING_TIME_MS, Pattern_Blank);
		
			displaySetIndicatorPatterns(eINCICATOR_PATTERN_BOTH);
			break;
		case eDISPLAY_TYPE_RIGHT_ARROW:
			nDisplayPatterns = 2;
		
		  displaySetPatternData(0, DISPLAY_FLASHING_TIME_MS, Pattern_RightArrow_1_flashing_15_light);
		  displaySetPatternData(1, DISPLAY_FLASHING_TIME_MS, Pattern_Blank);
		
			displaySetIndicatorPatterns(eINDICATOR_PATTERN_RIGHT);
			break;
		case eDISPLAY_TYPE_LEFT_ARROW:
			nDisplayPatterns = 2;
		
		  displaySetPatternData(0, DISPLAY_FLASHING_TIME_MS, Pattern_LeftArrow_1_flashing_15_light);
		  displaySetPatternData(1, DISPLAY_FLASHING_TIME_MS, Pattern_Blank);
		
			displaySetIndicatorPatterns(eINDICATOR_PATTERN_LEFT);
			break;
		case eDISPLAY_TYPE_ALL_LIGHTS_ON:
			nDisplayPatterns = 1;

		  displaySetPatternData(0, DISPLAY_FLASHING_TIME_MS, Pattern_AllOn_flashing_15_light);
			displaySetIndicatorPatterns(eINCICATOR_PATTERN_BOTH);
			break;
		case eDISPLAY_TYPE_RIGHT_STEM_ARROW:
		case eDISPLAY_TYPE_LEFT_STEM_ARROW:
		case eDISPLAY_TYPE_RIGHT_WALKING_ARROW:
		case eDISPLAY_TYPE_LEFT_WALKING_ARROW:
		case eDISPLAY_TYPE_RIGHT_CHEVRON:
		case eDISPLAY_TYPE_LEFT_CHEVRON:
		case eDISPLAY_TYPE_DOUBLE_DIAMOND:
			eRetVal = ePACKET_STATUS_NOT_SUPPORTED;
			break;
	}
	if(ePACKET_STATUS_SUCCESS == eRetVal)
	{
		/////
		// disable the lamps
		/////
		pwmSetDriver(PWM_SUSPEND);
	
		/////
		// schedule the first pattern
		/////
		nCurrentDisplayPatternIndex = 0;
		nCurrentIndicatorPatternIndex = 0;
/* MJB
		ledDriverSetIndR(indicatorRight[nCurrentDisplayPatternIndex]);
		ledDriverSetIndL(indicatorLeft[nCurrentDisplayPatternIndex]);
*/
		displayAddI2CQueueEntry();
		I2CQueueAddEntry(&myI2CQueueEntry);
		startTimer(&patternTimer, displayPatterns[0].nPatternTimeMS);
	}
#endif
	return eRetVal;
}


void displayInit(eDISPLAY_TYPES eInitialType)
{
	eInitialDisplaySelection = eInitialType;
	
	nBoardRev = hwReadBoardRev(); 
	
	/////
	// remember the model number
	/////
	eModel = hwReadModel();

	//////////////////////////////////////
	//// Set the Lamp Rail According to
	//// the Model Number (Solar/Vehicle)
	//////////////////////////////////////
	switch(eModel)
	{
		///////////////////////////////////////
		//// Vehicle Mount Arrow Boards use
		//// 12 volt bulbs so set the lamp
		//// rail to 12 volts
		///////////////////////////////////////
		case eMODEL_VEHICLE_25_LIGHT_SEQUENTIAL:
		case eMODEL_VEHICLE_15_LIGHT_SEQUENTIAL:
		case eMODEL_VEHICLE_15_LIGHT_FLASHING:
		case eMODEL_VEHICLE_15_LIGHT_WIG_WAG:
			hwLampRailSelect(LAMP_RAIL_12V0);
			printf("displayInit: LAMP_RAIL_12V0 Chosen\n");
			break;
		
		///////////////////////////////////////
		//// Solar Trailer Arrow Boards use
		//// 8.4 volt bulbs so set the lamp
		//// rail to 8.4 volts
		///////////////////////////////////////
		case eMODEL_TRAILER_25_LIGHT_SEQUENTIAL:
		case eMODEL_TRAILER_15_LIGHT_SEQUENTIAL:
		case eMODEL_TRAILER_15_LIGHT_FLASHING:
		case eMODEL_TRAILER_15_LIGHT_WIG_WAG:
			hwLampRailSelect(LAMP_RAIL_8V4);
			printf("displayInit: LAMP_RAIL_8V4 Chosen\n");
			break;	

		default:
			break;
	} 
	/////
	// initialize the pattern timer
	/////
	initTimer(&patternTimer);	
	
	/////
	// schedule I2C transaction to configure the driver
	// leave all drivers off for now
	/////
	pwmSetDriver(PWM_SUSPEND);
	displayAddConfigureI2CQueueEntry();
	I2CQueueAddEntry(&myI2CQueueEntry);
	
	nDisplayPatterns = 0;
	nIndicatorPatterns = 0;
	nDisplayErrors = 0;
}
ePACKET_STATUS displayChange(unsigned short nDisplaySelection)
{
	ePACKET_STATUS eRetVal = ePACKET_STATUS_SUCCESS;
	eDISPLAY_TYPES eDisplayType = (eDISPLAY_TYPES)nDisplaySelection;

	printf("1-DisplayChange nDisplaySelection[%d] eModel[%d]\n", nDisplaySelection, eModel);
	switch(eModel)
	{
		case eMODEL_VEHICLE_25_LIGHT_SEQUENTIAL:
		case eMODEL_TRAILER_25_LIGHT_SEQUENTIAL:
			eRetVal = displayChange25Light(eDisplayType);
			break;
		case eMODEL_VEHICLE_15_LIGHT_SEQUENTIAL:
		case eMODEL_TRAILER_15_LIGHT_SEQUENTIAL:
			eRetVal = displayChange15LightSequential(eDisplayType);
			break;
		case eMODEL_VEHICLE_15_LIGHT_FLASHING:
		case eMODEL_TRAILER_15_LIGHT_FLASHING:
			eRetVal = displayChange15LightFlashing(eDisplayType);
			break;
		case eMODEL_VEHICLE_15_LIGHT_WIG_WAG:
		case eMODEL_TRAILER_15_LIGHT_WIG_WAG:
			eRetVal = displayChange15LightWigWag(eDisplayType);
			break;
		case eMODEL_NONE:
			break;
	}

	if(ePACKET_STATUS_SUCCESS == eRetVal)
	{
		eCurrentDisplaySelection = eDisplayType;
		*((unsigned short*)pLastDisplayType) = eDisplayType;
	}
	else
	{
		eCurrentDisplaySelection = eDISPLAY_TYPE_BLANK;	
		*((unsigned short*)pLastDisplayType) = eDisplayType;
	}
	return eRetVal;
}

unsigned short displayGetCurrentPattern()
{

	return (unsigned short)eCurrentDisplaySelection;
}

unsigned short displayGetErrors()
{
	return errorInputGetDriveErrors();
}

static BOOL bDisplayIsBlank = TRUE;
BOOL displayIsBlank()
{
	return bDisplayIsBlank;
}

static BOOL bLampShutDown = FALSE;
BOOL displayIsShutDown()
{
	return bLampShutDown;
}

void displayDoWork()
{
	BOOL bUpdateIndex = TRUE;
	
	switch(myI2CQueueEntry.eState)
	{
		case eI2C_TRANSFER_STATE_COMPLETE:
			if(TRUE == bInit)
			{
				/////
				// just booted up,
				// so set display to blank
				/////
				bInit = FALSE;
				displayChange(eInitialDisplaySelection);
			}
			else
			{
				//printf("1-\n\r");
				/////
				// transfer is complete
				// enable the lamps (if necessary)
				// and start the timer
				/////

				if(isPatternBlank(nCurrentDisplayPatternIndex) || bLampShutDown == TRUE)
				{
					bDisplayIsBlank = TRUE;
					pwmSetDriver(PWM_SUSPEND);
				}
				else
				{
					bDisplayIsBlank = FALSE;
					pwmSetDriver(PWM_RESUME);
				}
	
								
				if(bLampShutDown == TRUE) // MJB
				{
						if(ADCGetSystemVoltage() > lowVoltageLampRecovery)
						{
							printf("Lamps[1]\r\n");
							bDisplayIsBlank = FALSE;
							pwmSetDriver(PWM_RESUME);
							bLampShutDown = FALSE;
						}
				}
				else if(ADCGetSystemVoltage() <= lowVoltageLampLimit) 
				{
					bDisplayIsBlank = TRUE;
					pwmSetDriver(PWM_SUSPEND);
					bLampShutDown = TRUE;
				}

				if(eDISPLAY_TYPE_ERRORED_LIGHTS_OFF == eCurrentDisplaySelection)
				{
					/////
					// trying to show errors
					// so re-generate the pattern
					/////
					if(nDisplayErrors != errorInputGetDriveErrors())
					{
						displayBuildErroredLightsOffPattern();
					}
				}
				startTimer(&patternTimer, displayPatterns[nCurrentDisplayPatternIndex].nPatternTimeMS);
				myI2CQueueEntry.eState = eI2C_TRANSFER_STATE_IDLE;
			}
			break;
		case eI2C_TRANSFER_STATE_FAILED:
			//printf("2-\n\r");
			/////
			// transfer failed, so resend this one
			/////
			bUpdateIndex = FALSE;
		
			/////
			// setup the transfer state
			// and expire the timer
			// so we will start this pattern now
			/////
			myI2CQueueEntry.eState = eI2C_TRANSFER_STATE_IDLE;
			stopTimer(&patternTimer);
			break;
		case eI2C_TRANSFER_STATE_IDLE:
			//printf("3-\n\r");
			break;
		default:
			//printf("4-\n\r");
			break;
	}
	if((eI2C_TRANSFER_STATE_IDLE == myI2CQueueEntry.eState) && (isTimerExpired(&patternTimer)))
	{
		//printf("5-\n");
		/////
		// timer expired
		// disable lamps
		// and send the next pattern
		//////
		pwmSetDriver(PWM_SUSPEND);
		if(bUpdateIndex)
		{
			/////
			// update indicator pattern index
			/////
			nCurrentIndicatorPatternIndex++;
			if(nCurrentIndicatorPatternIndex >= nIndicatorPatterns)
			{
				nCurrentIndicatorPatternIndex = 0;
			}
			
			/////
			// update display pattern index
			/////		
			nCurrentDisplayPatternIndex++;
			if(nCurrentDisplayPatternIndex >= nDisplayPatterns)
			{
				nCurrentDisplayPatternIndex = 0;
			}
		}
		
// MJB		ledDriverSetIndR(indicatorRight[nCurrentDisplayPatternIndex]);
// MJB		ledDriverSetIndL(indicatorLeft[nCurrentDisplayPatternIndex]);

		/////
		// update the display
		/////

		displayAddI2CQueueEntry();
		I2CQueueAddEntry(&myI2CQueueEntry);
	}
}
