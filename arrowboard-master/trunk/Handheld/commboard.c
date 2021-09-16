// support functions particlar to the driver board
// GPIO config, control of periphs and status bits
// connected straight to uP.

// UART/I2C/ADC etc handled in respective .c and .h files
#include <stdlib.h>
#include <stdio.h>

#include "lpc23xx.h"
#include "shareddefs.h"
#include <irq.h>
#include "commboard.h"
#include "timer.h"
#include "LCD.h"
#include "watchdog.h"

#define POWER_BUTTON_TIME_MS 2000
#define DEBUG_SPEW 0

#define WORK_LOOP_DELAY_TIME_MS 1
static TIMERCTL workLoopDelayTimer;

static TIMERCTL powerButtonTimer;

typedef enum ePowerButtonStates
{
    ePB_FIRST_POWER_BUTTON,
	ePB_IDLE,
	ePB_PRESSED_START_TIMER,
	ePB_PRESSED_WAIT_FOR_TIMER_TO_EXPIRE,
	ePB_PRESSED_TIMER_EXPIRED,
	ePB_SLEEP_MODE,
    ePB_SLEEP_ACTIVE,
    ePB_SLEEP_WAKE
}ePOWERBUTTONSTATES;

static ePOWERBUTTONSTATES ePBNextState = ePB_FIRST_POWER_BUTTON;

// GPIO CONFIG
// set up I/Os as in/out, set default states
// peripheral pins (UART/ADC/SPI/ETC) are *NOT* configured here.

// NB: LAST STEP IS TO DISABLE ETM IN SW
// it is configured on boot by sampling RTCK.
// The value written to the config reg is xRTCK.  
// As drawn, this enables ETM and hoses some PORT2 ops

void GPIO_CONFIG(void)
{


// enable FIO
SCS = 0x1;	

// SET DIRECTIONS

	
//**************
//******* PORT 0

// plain I/O setup
// chip default is input w/pull-ups ENABLED
	
// special functions						  
// ADC, UART, I2C pins set up in respective
// peripheral init routines
// should they be included here?


// explicitly configure port as GPIO
PINSEL0 = 0x00000000; 
PINSEL1 = 0x00000000;

/*
// **********************
// PORT 0 BIT DEFS
// **********************
*/

// set directions
// Pins default to input, called here for explicitness
FIO0DIR =	(OUT << uP_DBG_LED		) |
				  (OUT << uP_W85_DEN		) |
					(OUT << uP_W85_xRE		)	|	
					(OUT << uP_SPI0_SCK		) |
					(OUT << uP_SPI0_CS		) |
					(IN  << uP_SPI0_MISO	) |
					(OUT << uP_SPI0_MOSI	) |
					(OUT << uP_SPI0_WP 		) |
					(OUT << uP_SPI0_HOLD	) |
					(IN  << uP_PUSH_BUTTON) |
					(OUT << uP_PWR_xSHDN	) |
					(IN  << uP_ADC_VS  		) |
					(IN  << uP_ADC_ALS 		);

// SET INITIAL VALUES. 
FIO0SET = (1 << uP_DBG_LED  ) |
				  (1 << uP_W85_DEN  ) |
					(1 << uP_W85_xRE  )	|	
					(1 << uP_SPI0_SCK ) |
					(1 << uP_SPI0_CS  ) |
					(1 << uP_SPI0_MOSI) |
					(1 << uP_SPI0_WP  ) |
					(1 << uP_SPI0_HOLD) |
					(1 << uP_PWR_xSHDN)	;
					


// //***************************************
// //***************************************
// //******* PORT 1
// //***************************************
// //***************************************

// // peripherals are set up in their respective
// // config routines (ADC, UART, SPI)

// // set as GPIO
 PINSEL2 = 0x00000000;
 PINSEL3 = 0x00000000;



// // set directions

FIO1DIR = (OUT << RF_M0      ) |
 					(OUT << RF_M1      ) |
					(OUT << RF_M2      ) |
 					(OUT << RF_LRN     ) |
 					(OUT << RF_IO      ) |
					(OUT << RF_xRST    ) |
					(OUT << uP_PWM_KEY ) |
					(IN  << DIP_xA     ) |
					(IN  << DIP_xB     ) |
					(OUT << LCD_WR     ) |
					(OUT << LCD_RD     ) |
					(OUT << LCD_CS     ) |
					(OUT << LCD_DC     ) |
					(OUT << LCD_WAIT   ) |
   				(OUT << LCD_RST    );
					
// SET INITIAL VALUES

FIO1SET = (1 << RF_M0      ) |
 					(1 << RF_M1      ) |
					(1 << RF_M2      ) |
 					(1 << RF_LRN     ) |
 					(1 << RF_IO      ) |
					(1 << RF_xRST    ) |
					(1 << uP_PWM_KEY ) |
					(1 << LCD_WR     ) |
					(1 << LCD_RD     ) |
					(1 << LCD_CS     ) |
					(1 << LCD_DC     ) |
					(1 << LCD_RST    ) ;
					
FIO1CLR = (1 << LCD_WAIT  );		

// // RF DIGITAL MODULE CONFIG'D ELSEWHERE

					
 

// //***************************************
// //***************************************
// //******* PORT 2
// //***************************************
// //***************************************

// // set as GPIO except for interrupt pins
PINSEL4 = 0x00000000 | (EXT_INT << EXTINT1); // This for the LCD_INT 

// Pins default to input, called here for explicitness
 FIO2DIR = (OUT << D0         ) |
				   (OUT << D1         ) |
  		 	   (OUT << D2         ) |
				   (OUT << D3         ) |
				   (OUT << D4         ) |
				   (OUT << D5         ) |
				   (OUT << D6         ) |
				   (OUT << D7         ) |
					 (IN  << LCD_INT    ) |
 					 (IN  << uP_xISP    ) ; 

      	
// // SET INITIAL VALUES ON OUTPUTS

	FIO2SET = (1 << D0         ) |
						(1 << D1         ) |
						(1 << D2         ) |
						(1 << D3         ) |
						(1 << D4         ) |
						(1 << D5         ) |
						(1 << D6         ) |
						(1 << D7         ) ;

	FIO1CLR = (1 << LCD_CS    );

//  

// //***************************************
// //***************************************
// //******* PORT 3
// //***************************************
// //***************************************


// // set as GPIO
// // 
 PINSEL6 = 0x00000000;		 
 PINSEL7 = 0x00000000;

FIO3DIR = (IN << xWIRE_PRESENT);

//FIO3SET = 0xFFFFFFFF;


// //***************************************
// //***************************************
// //******* PORT 4
// //***************************************
// //***************************************

// SET AS GPIO
PINSEL9 = 0x00000000;
// // the only port4 pins are UART.
// // they are configured in UART peripheral setup
#define uP_3_TXD			28
#define uP_3_RXD			29

//FIO4SET = 0xFFFFFFFF;

// // last step of setup - disable ETM
// // (ETM hoses some PORT2 ops (P2[9:0])
// // should fix this in HW - change pull
DISABLE_ETM();

//=============================================
// Initialize and Start the Power Button Timer
// Timeout is 2 Seconds
//=============================================
initTimer(&powerButtonTimer);
initTimer(&workLoopDelayTimer);
}

//====================================================================
// Function name:		BODHandler
//
// Descriptions:		Brown Out Detect (BOD) interrupt request handler
//
// parameters:			  None
// Returned value:		None
// 
//====================================================================
static void BODHandler (void) __irq 
{
	powerDownHandHeld(); // Power off the Handheld

	VICVectAddr = 0;		 // Acknowledge Interrupt
}

//=======================================================================
// Function name:		commboardInit
//
// Descriptions:		Initialize the Brown Out Detect (BOD) of the LPC2387
//
// parameters:			None
// Returned value:  None
// 
//========================================================================
BOOL commboardInit(void)
{
  if ( install_irq( BOD_INT, BODHandler, HIGHEST_PRIORITY ) == FALSE )
  {
		printf("BOD_INT: Failed to Install!\n");
		return(FALSE);
  }
	else
	{
		printf("BOD_INT: Installed Successfully!\n");
		return(TRUE);
	}
}

void commboardDoWork(void)
{   
	if(isTimerExpired(&workLoopDelayTimer))
	{
		switch(ePBNextState)
		{
            case ePB_FIRST_POWER_BUTTON:
                // do not check for power off or standby on first
                // power up keypress            
                if(FALSE == powerButtonPressed())
                {
                    ePBNextState = ePB_IDLE;
                }
                break;
                
			case ePB_IDLE:               
				if(powerButtonPressed())
				{
                    
					ePBNextState = ePB_PRESSED_START_TIMER;
				}
				
				break;
			
			case ePB_PRESSED_START_TIMER:
				if(powerButtonPressed())
				{
                    startTimer(&powerButtonTimer, POWER_BUTTON_TIME_MS);	
                    ePBNextState = ePB_PRESSED_WAIT_FOR_TIMER_TO_EXPIRE;
				}
				else
				{
					ePBNextState = ePB_SLEEP_MODE;
				}
				
				break;
			
			case ePB_PRESSED_WAIT_FOR_TIMER_TO_EXPIRE:
				if(powerButtonPressed())
				{
					if(isTimerExpired(&powerButtonTimer))
					{
						ePBNextState = ePB_PRESSED_TIMER_EXPIRED;
					}
				}
				else
				{
					ePBNextState = ePB_SLEEP_MODE;
				}	
								
				break;
		
			case ePB_PRESSED_TIMER_EXPIRED:
				powerDownHandHeld();
			
				ePBNextState = ePB_IDLE;				
				break;
		
			case ePB_SLEEP_MODE:
				if(!LCD_Sleep_Mode_Enabled())
				{				
					LCD_Enable_Sleep_Mode(TRUE);
				}
				
				ePBNextState = ePB_SLEEP_ACTIVE;			
				break;
                
            case ePB_SLEEP_ACTIVE:
                if(powerButtonPressed())
                {
                    LCD_Enable_Sleep_Mode(FALSE);
                    ePBNextState = ePB_SLEEP_WAKE;
                }
                break;
                
            case ePB_SLEEP_WAKE:
                if(!powerButtonPressed())
                {
                   ePBNextState = ePB_IDLE;
                }
             break;
                
		}
        watchdogFeed();
		startTimer(&workLoopDelayTimer, WORK_LOOP_DELAY_TIME_MS);
	}
}

BOOL powerButtonPressed(void)
{
	BOOL powerButtonPressed = FALSE;
	
	if(!(FIO0PIN & (1 << uP_PUSH_BUTTON)))
	{
		powerButtonPressed = TRUE;
	}
	
	return powerButtonPressed;
}

void powerDownHandHeld(void)
{
	FIO0CLR = (1 << uP_PWR_xSHDN); // Powered Off	
}

BOOL isRS485Power()
{
	BOOL bRetVal = TRUE;
	int port = FIO3PIN;
	if(port & (1 << xWIRE_PRESENT))
	{
		bRetVal = FALSE;
	}
	return bRetVal;
}


// *********************
// *********************
// *********************
// Read model select
// return printable char '0' (0x30), '1' (0x31), etc
char READ_MODEL(void)
{
#if 1
	uint32_t port = FIO1PIN;	

	return( (1UL << DIP_xA) == (port & (1UL << DIP_xA)) );
#else	// Don't believe this ever worked??
	int port;

	// model select on port0
	port = FIO2PIN;	

	// mask off model bit and map return val to 'A' (0) OR 'B' (1)
	// to match silk
	//FIXME: magic numbers
	port = ((port >> DIP_xA) & 0x01) + 0x41; // add 0x41 to make printable 'A' (0x41) or 'B' (0x42)

	return (char) port;
#endif
}
BOOL readDipSwitchB( void )
{
	uint32_t port = FIO1PIN;	

	return( (1UL << DIP_xB) == (port & (1UL << DIP_xB)) );
}
// *********************
// *********************
// *********************
// DISABLE ETM (embedded trace - dorks up PORT2 pins use
void DISABLE_ETM(void) {
	
	//FIXME MAGIC NUMBER
	PINSEL10 = 0x00000000;

}
