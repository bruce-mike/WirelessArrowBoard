// COMM_BD_V2_BOARD_MAIN.C

#include <lpc23xx.h>
#include <stdio.h>
#include "sharedinterface.h"
#include "shareddefs.h"
#include "commboard.h"
#include "irq.h"
#include "timer.h"
#include "queue.h"

#include "hdlc.h"
#include "serial.h"
#include "ssp.h"
#include "wireless.h"
#include "commands.h"
#include "flash_nvol.h"
#include "uidriver.h"
#include "spiflash.h"
#include "power.h"
#include "pll.h"
#include "gen_UI.h"
#include "LCD.h"
#include "UI.h"
#include "pwm.h"
#include "storedconfig.h"
#include "watchdog.h"
#include "status.h"
#include "adc.h"

//============================
// HandHeld Software Version
//============================
#define HANDHELD_SOFTWARE_VERSION 20600

//==========================================
// Default Driver Board Software Version.
// This is used if we are connected to a
// Driver Board with Older Revision Software
// for Backwards Compatibility.
//==========================================
#define DEFAULT_DRIVER_BOARD_SOFTWARE_VERSION 000

#define WATCHDOG_FEEDING_TIME_MS 250
#define SAMPLE_TIME_MS 1000

#define RADIO_XMIT 0		// test code to continously xmit
#define RADIO_RX   0		// test code to echo rx radio chars to UART and 485

// global timekeeper wrap in ~49 days
int MILLISECONDS;
TIMERCTL sampleTimer;
static TIMERCTL watchdogFeedingTimer;
static void doWork(void);
static long nTheirESN;
static eINTERFACE eInterfaceToConnect = eINTERFACE_UNKNOWN;
static int nPreviousNewConnections;
static uint32_t sampleTimerCounter = 0UL;

void getWirelessDiagnostics( BOOL delay );

int main(void) {


long k; 
//char key;  
	
// disable ints
	//necessary??
//IDISABLE;


	
for(k = 0; k < 100000; k++)
{}

    
	/////
	// setup the clocks
	/////
	PLL_CLOCK_SETUP(ePLATFORM_TYPE_HANDHELD);

    
	// // init non-peripheral I/O	
	// NB: LAST STEP IS TO DISABLE ETM IN SW
	// it is configured on boot by sampling RTCK.
	// The value written to the config reg is ~RTCK.  
	// As drawn, this enables ETM and hoses some PORT2 ops
	GPIO_CONFIG();	


	/////
	// initialize VIC
	/////
	init_VIC();

	// Board rev dependant initialization
	ADCInit( 1 );
	
	/////
	// initialize Handheld Serial
	/////
	serialInit(ePLATFORM_TYPE_HANDHELD);
	
	printf("COMMBD V2 SERIAL0 INIT..\n\r");

//=========================================================================
	/////
	// initialize timer subsystem
	/////
	timerInit(ePLATFORM_TYPE_HANDHELD);

	
	/////
	// initialize the spi flash subsystem
	/////
	SSPInit(ePLATFORM_TYPE_HANDHELD);
	
	/////
	// initialize stored configuration
	/////
	storedConfigInit(ePLATFORM_TYPE_HANDHELD);
	

	//////
	// Store HandHeld Software Version 
	// in a Static Structure for Future
	// use.
	//////
	storeStatus(eCOMMAND_STATUS_HANDHELD_SOFTWARE_VERSION,(WORD)HANDHELD_SOFTWARE_VERSION);
	
	//////
	// Store Default Driver Board Software Version 
	// in a Static Structure. This is so that the
	// Version Number is not Zero for older Revision
	// Software that does not know about sending back
	// the Version Number.
	//////
	storeStatus(eCOMMAND_STATUS_DRIVER_BOARD_SOFTWARE_VERSION,(WORD)DEFAULT_DRIVER_BOARD_SOFTWARE_VERSION);

	/////
	// Read the ESN for the Driver Board
	/////		
	nTheirESN = storedConfigGetSendersESN();	
	
	/////
	// initialize the wireless subsystem
	/////
	wirelessInit(ePLATFORM_TYPE_HANDHELD);
	printf("nOurESN[%08X] nSendersESN[%08X]\n", wirelessGetOurESN(), nTheirESN);
	
	//eInterfaceToConnect = eINTERFACE_WIRELESS;

	if(isRS485Power())
	{
		printf("Using RS485 power\n");
		eInterfaceToConnect = eINTERFACE_RS485;
	}
	else
	{
		printf("Using external power\n");
		eInterfaceToConnect = eINTERFACE_WIRELESS;
	}
	nPreviousNewConnections = 0;
	setInterface( eInterfaceToConnect );

	/////
	// initialize command processor
	/////
	commandInit();

	initTimer(&sampleTimer);	
	startTimer(&sampleTimer, SAMPLE_TIME_MS);
    sampleTimerCounter = 0UL;
	
	/////
	// initialize the graphics display subsystem
	/////
	LCD_Set_CS();
	LCD_Init();

	/////
	// initialize the user interface
	/////
	initUI();
    
	uiDriverInit();

	/////
	// initialize the PWM for Touch Feedback
	/////
	
	pwmInit(4000, 50);

	/////
	// show heartbeat for debug
	/////
	timerShowLEDHeartbeat();


	/////
	// initialize the watchdog timer
	// timeout in 2 seconds
	/////
	watchdogInit((WATCHDOG_FEEDING_TIME_MS*8)+1);
	initTimer(&watchdogFeedingTimer);
	startTimer(&watchdogFeedingTimer, WATCHDOG_FEEDING_TIME_MS);


	// Initialize Brown Out Detection (BOD)
    // v2.01.01
    // moved down the initialization chain as brown out detect was
    // firing before rail completely came up
	commboardInit(); 
    
	while(1)
	{
		doWork();
	}

}


int nLed = 0;
int i,j;
static void doWork()
{
	if(isTimerExpired(&watchdogFeedingTimer))
	{
		/////
		// feed the watchdog
		/////
		watchdogFeed();
		
		/////
		// and restart the feeding timer
		/////
		startTimer(&watchdogFeedingTimer, WATCHDOG_FEEDING_TIME_MS);
	}
	
	serialDoWork();
	
	//jgrhdlcDoWork();
	
	if(TRUE == areWeConnectedTo(eInterfaceToConnect))
	{
		if(nPreviousNewConnections != getNewConnectionsCount(eInterfaceToConnect))
		{
			nPreviousNewConnections = getNewConnectionsCount(eInterfaceToConnect);
			printf("uiDriverSendGetConfig\n");
			uiDriverSendGetConfig(eInterfaceToConnect);
		}
	}
	else
	{
		uiDriverSetModel(eMODEL_NONE);
	}
	
	commandDoWork();

	uiDriverDoWork();

	
	// Handle Power Button Press
	commboardDoWork();
	
	if(isTimerExpired(&sampleTimer))
	{
		ADCDoWork( );
		startTimer(&sampleTimer, SAMPLE_TIME_MS);
		sampleTimerCounter++;		// Incs every 1 second
	}
	
	/****************** jgr ********************************/
	#if 0
	{
		if( (0 == (sampleTimerCounter % 60))  && sampleTimerCounter )
		{
			//state = FALSE;
			getWirelessDiagnostics( TRUE );
		}
	}
	#endif
	/****************** jgr  *******************************/
}

