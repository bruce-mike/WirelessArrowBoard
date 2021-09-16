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
#include "i2c.h"
#include "serial.h"
#include "pwm.h"
#include "adc.h"
#include "ADS7828.h"
#include "errorinput.h"
#include "leddriver.h"
#include "mischardware.h"
#include "actuator.h"
#include "lm73.h"
#include "display.h"
#include "flash_nvol.h"
#include "power.h"
#include "pll.h"
#include "irq.h"
#include "ssp.h"
#include "spiFlash.h"
#include "commands.h"
#include "storedconfig.h"
#include "watchdog.h"
#include "accelerometer.h"

#define WATCHDOG_FEEDING_TIME_MS 100
static TIMERCTL watchdogFeedingTimer;

#define MILLISECONDS_PER_SECOND 1000
static TIMERCTL oneSecondDoWorkTimer;

    
unsigned int nTheirESN;
static int nBoardRev;
static BOOL isExternalLedOn = TRUE;  
unsigned short nAlarms;
static eMODEL eModel;


static void ourWatchDogInit()
{
	switch(nBoardRev)
	{
		default:
			// Rev 1 (Rev H)
			// uses external watchdog
			// just pat it here
			hwPatWatchDog();
            // fall through so we initialize the internal watchdog too
	
		case 0:
			// Rev 0 used internal watchdog
			// initialize this here
			watchdogInit((WATCHDOG_FEEDING_TIME_MS*8)+1);
			break;
	}
	// initialize the watchdog timer
	// timeout in 2 seconds
	initTimer(&watchdogFeedingTimer);
	startTimer(&watchdogFeedingTimer, WATCHDOG_FEEDING_TIME_MS);
}

static void ourWatchDogFeed()
{
	switch(nBoardRev)
	{

		default:
			// Rev 1 (Rev H)
			// uses external watchdog
			// just pat it here
			hwPatWatchDog();
			// fall through so we pat the internal watchdog too
		case 0:
			watchdogFeed();
			break;
	}
}
void actuatorAndSolarSetup(void)
{
    // AUX battery is always ON - the line and battery voltages are now OR'd in the hardware
    hwSetLT4365_BATTERY(ON);
    
	switch(eModel)
	{
//========================================================
// actuator power taken from line or battery
// set this up here
//========================================================
		case eMODEL_VEHICLE_25_LIGHT_SEQUENTIAL:
		case eMODEL_VEHICLE_15_LIGHT_SEQUENTIAL:
		case eMODEL_VEHICLE_15_LIGHT_FLASHING:
		case eMODEL_VEHICLE_15_LIGHT_WIG_WAG:
			/////
			// configure Actuator GPIO
			// moved it here so we could init the limit switch power
			// vehicle mount, so use line volts for actuator
			/////
			actuatorInit(TRUE);
		
			break;
		
		case eMODEL_TRAILER_25_LIGHT_SEQUENTIAL:
		case eMODEL_TRAILER_15_LIGHT_SEQUENTIAL:
		case eMODEL_TRAILER_15_LIGHT_FLASHING:
		case eMODEL_TRAILER_15_LIGHT_WIG_WAG:
			//=============================================
			// When GPIO is first initialized, Solar 
			// Charge is turned on. If an Aux Battery is
			// Installed, turn off Solar Charging and let 
			// the Solar Charging State Machine Control it.
			// If an Aux Battery is not installed, leave 
			// Solar Charging on and disable the AUX
			// Battery Input (LT4365).
			//==============================================
			hwSetSolarCharge(OFF);
			/////
			// August 29, 2016
			// let all sign types use the actuator
			// if they are wired correctly and have sufficient battery
			// then it will work. Otherwise it will not.
			// this should work with Rev 1(Rev H) and beyond
			// anything earlier is a crapshoot
			/////
			actuatorInit(FALSE);
			break;
		
		case eMODEL_NONE:
		default:
			break;
	}

}


static void doWorkOneSecond()
{    
    hwLVDControl(); 
    
    nAlarms = errorInputGetAlarms();
		       
    if(ALARM_BITMAP_CHARGER_ON & nAlarms)
    {
        ledDriverSetLedChrgr(eLED_ON);	
        nAlarms &= ~ALARM_BITMAP_CHARGER_ON;
    }
    else
    {
        ledDriverSetLedChrgr(eLED_OFF);
    }
		
    if(ALARM_NONE != nAlarms)
    {
        ledDriverSetLedAlarm(eLED_ON);
			
        // have an active alarm - blink external GREEN LED
        if(isExternalLedOn == TRUE)
        {
            ledDriverSetIndR(eLED_OFF);
            isExternalLedOn = FALSE;
        }
        else
        {
            ledDriverSetIndR(eLED_ON);
            isExternalLedOn = TRUE;
        }

        if(ALARM_BITMAP_LOW_BATTERY & nAlarms)
        {
            ledDriverSetLedVlow(eLED_ON);
        }
        else
        {
            ledDriverSetLedVlow(eLED_OFF);
        }
    }
    else // ALARMS_NONE == nAlarms
    {
        ledDriverSetLedAlarm(eLED_OFF);
        ledDriverSetLedVlow(eLED_OFF);
			
        // no alarms - set external GREEN LED
        ledDriverSetIndR(eLED_ON);	
        isExternalLedOn = TRUE;			
    }
				
    lm73DoWork();
		
    pwmDoWork();
}  
 
    
static void doWork()
{     	
	if(isTimerExpired(&watchdogFeedingTimer))
	{
		//static int bInit = 0;
		
		/////
		// feed the watchdog
		/////
		ourWatchDogFeed();
		
		/////
		// and restart the feeding timer
		/////
		startTimer(&watchdogFeedingTimer, WATCHDOG_FEEDING_TIME_MS);
	}
	
	I2CQueueDoWork();
    
	errorInputDoWork();

	ledDriverDoWork();

	serialDoWork();
	
	//jgrhdlcDoWork();
	
	commandDoWork();
		
	displayDoWork();
    
    if(nBoardRev < 6)
    {
        // handle pre-RevJ board ADC and error input
        ADS7828DoWork();
    }
	
	//===============================================
	// RJH_07_14_16 NOTE: This is for Traffic Safety
	// because they are using Morning Star for Solar
	// Charge Contol. Uncomment if we are using 
	// Native Solar Charge Control
	//===============================================
	//hwSolarChargeControl();
	
	switch(eModel)
	{
//============================================================
// RJH 09/01/15: It is ASSUMED for NOW that SOLAR CHARGING is 
// 							 only used for TRAILER MOUNTED ARROW BOARD.
//============================================================
		case eMODEL_TRAILER_25_LIGHT_SEQUENTIAL:
		case eMODEL_TRAILER_15_LIGHT_SEQUENTIAL:
		case eMODEL_TRAILER_15_LIGHT_FLASHING:
		case eMODEL_VEHICLE_15_LIGHT_WIG_WAG:
			
				hwSolarChargeControl();
		
				break;				
		
//========================================================
// RJH 09/01/15: It is ASSUMED for NOW that the ACTUATOR
// 				       is only used on VEHICLE MOUNTED ARROW
//							 BOARDS.
//========================================================
		case eMODEL_VEHICLE_25_LIGHT_SEQUENTIAL:
		case eMODEL_VEHICLE_15_LIGHT_SEQUENTIAL:
		case eMODEL_VEHICLE_15_LIGHT_FLASHING:
		case eMODEL_TRAILER_15_LIGHT_WIG_WAG:

				break;
		
		case eMODEL_NONE:
		default:
			break;
	}
	
    ADCDoWork();
    
	// August 29, 2016
	// let all sign types use the actuator
	// if they are wired correctly and have sufficient battery
	// then it will work. Otherwise it will not.
	actuatorDoWork();
	
	accelerometerDoWork( );
		
}


int main(void)
{
	int k;  
	
    // delay for JTAG to catch the processor and put it in reset	
	for(k = 0; k < 10000; k++)
	{}

	// setup the clocks
	PLL_CLOCK_SETUP(ePLATFORM_TYPE_ARROW_BOARD);
		
	// initialize miscellaneous hardware
	hwGPIOConfig();

	// turn VB on
	hwSetLT4365_BATTERY(ON);
		
	// get the board revision
    nBoardRev = hwReadBoardRev();
		
	// initialize the VIC
	init_VIC();
			
	// initialize serial port
	serialInit(ePLATFORM_TYPE_ARROW_BOARD);	

	hwSetSysLED();

	// initialize the timer system
	timerInit(ePLATFORM_TYPE_ARROW_BOARD);
	
	// initialize SPI Flash
	if(!SSPInit(ePLATFORM_TYPE_ARROW_BOARD))
	{	
		printf("SSPInit Failed!\n");
	}
	
	// initialize stored configuration
	storedConfigInit(ePLATFORM_TYPE_ARROW_BOARD);
	
	// initialize PWM
	pwmInit();
	
	// set our defaults
	pwmSetLampBrightnessControl(storedConfigGetArrowBoardBrightnessControl());
	
	// Read the ESN for the Driver Board	
	nTheirESN = storedConfigGetSendersESN();	
	
	// initialize the wireless subsystem
	wirelessInit(ePLATFORM_TYPE_ARROW_BOARD);
	printf("nOurESN[%08X] nSendersESN[%08X]\n", wirelessGetOurESN(), nTheirESN);

	// initialize command processor
	commandInit();
	
	// initialize I2C hardware and queue
	I2CInitialize();

	
	// handle board rev dependant initialization
	ADCInit(nBoardRev);

	// initialize the error input PCA9555/PCA9554
	errorInputInit();
	
	// initialize the led driver PCA9634
	ledDriverInit();

	eModel = hwReadModel();
	printf("Board Rev[%d] Model[%d]\n", nBoardRev,eModel);

	// do what we need to do
	// to set up actuator and solar
	actuatorAndSolarSetup();

	// initialize the display subsystem
	// make it display the last selected pattern
	displayInit(storedConfigGetDisplayType());

	// initialize the temperature sensor
	lm73Init();
	
	// show heartbeat for debug
	timerShowLEDHeartbeat();
	
	// initialize the watchdog
	ourWatchDogInit();
	
	// initialize accelerometer
	accelerometerInit( );

    initTimer(&oneSecondDoWorkTimer);
    startTimer(&oneSecondDoWorkTimer, MILLISECONDS_PER_SECOND);

	printf("Ready\n");

	while(1)
	{
        doWork();
        
        if(isTimerExpired(&oneSecondDoWorkTimer))
        {
            doWorkOneSecond();
            startTimer(&oneSecondDoWorkTimer, MILLISECONDS_PER_SECOND);
        }
 	}
}
