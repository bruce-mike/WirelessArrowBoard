#include <lpc23xx.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "shareddefs.h"
#include "sharedinterface.h"
#include "timer.h"
#include "queue.h"

#include "wireless.h"
#include "hdlc.h"
#include "serial.h"
#include "adc.h"
#include "mischardware.h"
#include "flash_nvol.h"
#include "pll.h"
#include "irq.h"
#include "commands.h"
#include "storedconfig.h"
#include "watchdog.h"

extern void commandSendRepeaterCommand( eCOMMANDS eCommand );


#define WATCHDOG_FEEDING_TIME_MS 100
#define SAMPLE_TIME_MS 50

static void repeaterState( void );
static void doWork(void);
static TIMERCTL sampleTimer;
static TIMERCTL watchdogFeedingTimer;
unsigned int nTheirESN;
static int nBoardRev;
BYTE *data;

static eMODEL eModel;

static void ourWatchDogInit()
{
	switch(nBoardRev)
	{

		default:
			/////
			// Rev 1 (Rev H)
			// uses external watchdog
			//
			// just pat it here
			/////
			hwPatWatchDog();

			/////
			// fall through so we initialize the internal watchdog too
			/////
		case 0:
			/////
			// Rev 0 used internal watchdog
			// initialize this here
			/////
			watchdogInit((WATCHDOG_FEEDING_TIME_MS*8)+1);
			break;
	}
	/////
	// initialize the watchdog timer
	// timeout in 2 seconds
	/////
	initTimer(&watchdogFeedingTimer);
	startTimer(&watchdogFeedingTimer, WATCHDOG_FEEDING_TIME_MS);
}

static void ourWatchDogFeed()
{
		switch(nBoardRev)
	{

		default:
			/////
			// Rev 1 (Rev H)
			// uses external watchdog
			//
			// just pat it here
			/////
			hwPatWatchDog();
			
			/////
			// fall through so we pat the internal watchdog too
			/////
		case 0:
			/////

			/////
			watchdogFeed();
			break;
	}
}

int main(void)
{
	int k;					// no C program complete without int
	
// wait f‹r Keil.  Heil Keil?
// delay for JTAG	 to catch the processor and put it in reset	
	for(k = 0; k < 10000; k++)
	{}

	/////
	// setup the clocks
	/////
	PLL_CLOCK_SETUP( ePLATFORM_TYPE_REPEATER );
		
	/////
	// initialize miscellaneous hardware
	/////
	hwGPIOConfig();

	/////
	// get the board revision
	////	
	nBoardRev = hwReadBoardRev();
		
	/////
	// initialize the VIC
	/////
	init_VIC();
			
	/////
	// initialize serial port
	/////
	serialInit( ePLATFORM_TYPE_REPEATER );	

	hwSetSysLED();

	/////
	// initialize the timer system
	/////
	timerInit( ePLATFORM_TYPE_REPEATER );
	
	/////
	// Read the ESN for the Driver Board
	/////		
	nTheirESN = storedConfigGetSendersESN();	
	/////
	// initialize the wireless subsystem
	/////
	wirelessInit(ePLATFORM_TYPE_REPEATER);
	printf("nOurESN[%08X] nSendersESN[%08X]\n", wirelessGetOurESN(), nTheirESN);

	/////
	// initialize command processor
	/////
	commandInit();
	
	/////
	// handle board rev dependant initialization
	////
	ADCInit(nBoardRev);

	/////
	// initialize the error input PCA9555/PCA9554
	/////
	//errorInputInit();

	
	eModel = hwReadModel();
	printf("Board Rev[%d] Model[%d]\n", nBoardRev,eModel);

	/////
	// initialize the Sample Timer
	/////
	initTimer(&sampleTimer);		
	startTimer(&sampleTimer, SAMPLE_TIME_MS);
	
	/////
	// show heartbeat for debug
	/////
	timerShowLEDHeartbeat();
	
	/////
	// initialize the watchdog
	/////
	ourWatchDogInit();
	
	printf("Ready\n");
	
	while(1)
	{
		doWork();
	}
}


static void doWork()
{
  static unsigned int delayCounter = 0;
    
	static unsigned int counter = 0;
	extern unsigned int adcReadCounter;
	extern unsigned int adcOverruns;
	
	if(isTimerExpired(&watchdogFeedingTimer))
	{
		/////
		// feed the watchdog
		/////
		ourWatchDogFeed();
		
		/////
		// and restart the feeding timer
		/////
		startTimer(&watchdogFeedingTimer, WATCHDOG_FEEDING_TIME_MS);
	}
	
	//errorInputDoWork();

	serialDoWork();
	
	commandDoWork();
		
	/*** 
	 *** This section executes every 50mSecs, Mike Bruce 
	 ***/
	
	if(isTimerExpired(&sampleTimer)) 
	{	
		ADCDoWork();
    counter++;
    
		
		/***
	   *** This section executes every 1 second 
		 ***/
		
    if(++delayCounter >= 20)  // one second has elapsed
    {
			delayCounter = 0;
            
			//////////////////////////////////////////////////
			counter = 0;
			adcReadCounter = 0;
			/////////////////////////////////////////////////////        
      
			repeaterState( );
			
    }  // End of 1 sec section

		startTimer(&sampleTimer, SAMPLE_TIME_MS);
	}		// End of 50mS section
	
}
/*
 * This deterines what the repeater should do. 1.) pair or
 * 2.) go into repeat mode.  It does this by always sending
 * out a ping at power up, via 485.  If it doesn't return 
 * then go into repeater mode, else, 485 active, so attempt 
 * to pair.
 */
static void repeaterState( void )
{
	static BOOL state = TRUE;
	static uint32_t mySecondCounter = 0;
	
	if( state )
	{
		hwSetRedLED( );
		if( mySecondCounter == 0UL ) 	// 1st time thru
		{
			printf("Send PING to driver board\n");
			commandSendRepeaterCommand( eCOMMAND_DO_PING );
		}
		else if( mySecondCounter <= 6UL ) // Wait for reply
		{
			if( eINTERFACE_RS485 == whichInterfaceIsActive( ) )
			{
				// Got here means that 485 active, so try to pair
				printf("Send REPATER CONFIG to driver board\n");
				commandSendRepeaterCommand( eCOMMAND_REPEATER_CONFIG );
				hwClrRedLED( );
				state = FALSE;
			}
		}
		else	// Got here means timed out waiting for ping, so no 485
		{
			printf("Timed out waiting for reply from driver board\n");
			hwClrRedLED( );
			state = FALSE;
		}
		mySecondCounter++;
	}
}

