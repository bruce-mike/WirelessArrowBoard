// support functions particlar to the driver board
// GPIO config, control of periphs and status bits
// connected straight to uP.

// UART/I2C/ADC etc handled in respective .c and .h files

#include <lpc23xx.h>
#include <stdio.h>

// hardware specs
#include "shareddefs.h"
#include "sharedinterface.h"
#include "timer.h"
#include "queue.h"
#include "wireless.h"
#include "mischardware.h"
#include "adc.h"

#define LVD_TIMER_MS 120000

typedef enum eLVDControlStates
{
	eLVD_MONITOR_VB,
	eLVD_VB_AT_SHUTDOWN_VOLTAGE, 
}eLVD_CONTROL_STATES;



// GPIO CONFIG
// set up I/Os as in/out, set default states
// peripheral pins (UART/ADC/SPI/ETC) are *NOT* configured here.

// NB: LAST STEP IS TO DISABLE ETM IN SW
// it is configured on boot by sampling RTCK.
// The value written to the config reg is xRTCK.  
// As drawn, this enables ETM wand hoses some PORT2 ops

// *********************
// *********************
// *********************
// DISABLE ETM (embedded trace - dorks up PORT2 pins 9:0)
static void hwDisableETM(void) {
	
	//FIXME MAGIC NUMBER
	PINSEL10 = 0x00000000;

}
/*
 * Repeater pin functionality
 * P0.0 & P0.1 = Aux Tx3 & Rx3
 * P0.2 & P0.3 = Debug Tx0 & Rx0
 * P0.4 & P0.5 = 485 Enb & Rec
 * P0.6 - P0.9 = Not used
 * P0.10 & P0.11 = 485 Tx2 & Rx2
 * P0.12 - P0.14 = Reserved
 * P0.15 & P0.16 = Xbee Tx1 & Rx1
 * P0.17         = Xbee CTS1
 * P0.18         = Not used
 * P0.19         = Xbee reset
 * P0.20 - P0.21 = Not used
 * P0.22         = Xbee RTS1
 * P0.23         = RSSI inpit digital
 * P0.24         = Not used
 * P0.25         = RSSI analog AD0.2
 * P0.26 - P0.30 = Not used 
 * 
 * P1.0          = Blow out brains
 * P1.15         = Green LED
 * P1.16         = Red LED
 * P1.27 - P1.29 = Rev dip swt's
 */
void hwGPIOConfig(void)
{

	// enable FIO on ports 0 & 1
	SCS = 0x1;	

	//**************
	//******* PORT 0

	// plain I/O setup
	// chip default is input w/pull-ups ENABLED
	
	// special functions						  
	// ADC, UART, I2C pins set up in respective
	// peripheral init routines
	// should they be included here?


	// explicitly configure port as GPIO, each pin has 2 bits of info, 00 = GPIO
	PINSEL0 = 0x00000000; 	// P0.0-15
	PINSEL1 = 0x00000000;		// P0.16-31

  PINMODE0 |= 0x00000008; // P0.0-15 pu/pd/hi-z  =  00/11/10

	// set directions 0 = input   1 = output
	FIO0DIR =	0x00000000;
	FIO0DIR |= (1 << REPEATER_RF0_xRST);
	FIO0SET =	0x00000000;
	
	//**************
	//******* PORT 1

	// peripherals are set up in their respective
	// config routines (ADC, UART, SPI)

	// set as GPIO
	PINSEL2 = 0x00000000;
	PINSEL3 = 0x00000000;

	// set directions	
	FIO1DIR = 0x00000000;			// Set up all as inputs
	FIO1DIR |= (OUT << uP_SYS_LED ) |	// Green System LED
						 (OUT << uP_REPEATER_RED_LED );	// Red System LED
										
	FIO1SET = (1 << uP_SYS_LED );

	FIO1CLR = (1 << uP_REPEATER_RED_LED );	// Turn on red led
	
	//**************
	//******* PORT 2

	// set as GPIO except for interrupt pins
	PINSEL4 = (EXT_INT << EXTINT1) |  
						(EXT_INT << EXTINT2) | 
						(EXT_INT << EXTINT3);

	FIO2DIR = (IN  << 10);		/* uP_xISP */
						//(IN  << 12	) | /* uP_INT_xDIN */
						//(IN  << uP_INT_xERR ) ;
									
	// SET INITIAL VALUES ON OUTPUTS

	FIO2SET = 0x00000000;

	//**************
	//******* PORT 3

	// port 3 pwm line
	// is configured in PWM module setup
	//#define uP_3_PWM			26
	// set as GPIO
	PINSEL6 = 0x00000000;		
	PINSEL7 = 0x00000000;
	switch(hwReadBoardRev())
	{
		case 0:
			//FIO1DIR |= (OUT << uP_USB_LED );
			break;
		default:
			
		printf("set port 3\n");
			break;
	}
	//**************
	//******* PORT 4
	//**************
	// the only port4 pins are SPI1.
	PINSEL9 = 0x00000000;
	
	FIO4DIR = 0x00000000;		

	FIO4SET = 0x00000000;
						
	// the last step - disable ETM
	hwDisableETM();
}


// enable various periphs connected straight to uP

void hwSetSysLED()
{
	FIO1SET = (1<<uP_SYS_LED);						// Green LED
}
void hwClrSysLED()
{
	FIO1CLR = (1<<uP_SYS_LED);
}
void hwSetRedLED( void )
{
	FIO1CLR = (1 << uP_REPEATER_RED_LED);	// Red LED
}
void hwClrRedLED( void )
{
	FIO1SET = (1 << uP_REPEATER_RED_LED);	// Red LED
}
void hwPatWatchDog(void)
{
	static BOOL bHigh = FALSE;
	/////
	// provide a transition
	// to the external watchdog
	/////
	if(bHigh)
	{
		
		bHigh = FALSE;
	}
	else
	{

		bHigh = TRUE;
	}
}

int hwReadBoardRev()
{
	int port;
	int nRev = 0;
		// model select on port1
	port = FIO1PIN;			// Port1.8-15
	nRev |= ((port & (1<<27))?1:0);
	nRev |= ((port & (1<<28))?2:0);
	nRev |= ((port & (1<<29))?4:0);
	switch(nRev)
	{
		case 0:
			// REV0=0, REV1=0, REV2=0
			nRev = 0;
			break;

		default:
			nRev = 255;
			break;
	}
	return (eMODEL)nRev;
}

// *********************
// *********************
// *********************
// Read model select
eMODEL hwReadModel(void)
{
	char nModel = 0;
	//int nRev;
	
	//nRev = hwReadBoardRev();
				
	return (eMODEL)nModel;

}


