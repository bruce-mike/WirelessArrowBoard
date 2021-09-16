// support functions particlar to the driver board
// GPIO config, control of periphs and status bits
// connected straight to uP.

// UART/I2C/ADC etc handled in respective .c and .h files

#include <lpc23xx.h>
#include <stdio.h>

// hardware specs
#include "shareddefs.h"
#include "ArrowBoardDefs.h"
#include "sharedinterface.h"
#include "timer.h"
#include "queue.h"

#include "mischardware.h"
#include "errorinput.h"
#include "adc.h"

#define LVD_TIMER_MS 120000

typedef enum eLVDControlStates
{
	eLVD_MONITOR_VB,
	eLVD_VB_AT_SHUTDOWN_VOLTAGE, 
}eLVD_CONTROL_STATES;


static eLVD_CONTROL_STATES eLVDNextState = eLVD_MONITOR_VB;

static TIMERCTL lvdTimer; // LVD


// GPIO CONFIG
// set up I/Os as in/out, set default states
// peripheral pins (UART/ADC/SPI/ETC) are *NOT* configured here.

// NB: LAST STEP IS TO DISABLE ETM IN SW
// it is configured on boot by sampling RTCK.
// The value written to the config reg is xRTCK.  
// As drawn, this enables ETM wand hoses some PORT2 ops
static BOOL solarChargerOn, lvdOn = FALSE;

// *********************
// *********************
// *********************
// DISABLE ETM (embedded trace - dorks up PORT2 pins 9:0)
static void hwDisableETM(void) {
	
	//FIXME MAGIC NUMBER
	PINSEL10 = 0x00000000;

}

void hwGPIOConfig(void)
{

	// 1 = output, 0 = input


	// enable FIO
	SCS = 0x1;	

	// SET DIRECTIONS


	// should port inits be individualized?
	// PORT1_INIT()
	// PORT1_INIT(), etc?
	
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

  PINMODE0 |= 0x0000000A; // Limit Switch Inputs (PORT0.0 and PORT0.1) are pulled up externally

	// set directions
	FIO0DIR =	(IN << LIMIT_SW_A   ) |
						(IN << LIMIT_SW_B   ) |
						(OUT << uP_SPI1_xCS ) |
						(OUT << uP_SPI1_SCK ) |	
						(IN << uP_SPI1_MISO ) |
						(OUT << uP_SPI1_MOSI) |
						(IN << uP_USB_DP    ) | // UNUSED
						(IN << uP_USB_DP    ) ; // UNUSED
						
	FIO0SET =	(1 << uP_SPI1_xCS ) |
						(1 << uP_SPI1_SCK ) |	
						(1 << uP_SPI1_MOSI) ;

	//**************
	//******* PORT 1

	// peripherals are set up in their respective
	// config routines (ADC, UART, SPI)

	// set as GPIO
	PINSEL2 = 0x00000000;
	PINSEL3 = 0x00000000;

	// set directions	
	FIO1DIR = (IN  << uP_POS_VL  ) |
						(IN  << uP_POS_VB  ) |
						(IN << ISP_xRST    ) |
						(OUT << uP_OVP_VL  ) |  //4363 overvoltage lockout override ("force on" by defeating auto-retry)
						(IN << uP_MOD_X1	 ) |	// Dip Switch Settings
						(IN << uP_MOD_X2 	 ) |	//
						(IN << uP_MOD_X4 	 ) |	//
						(IN << uP_MOD_X8 	 ) |	//				
						(OUT << uP_PWR_VL  ) |	// SOLAR CHARGE ENABLE
						(OUT << uP_PWR_VB  ) |	// BATTERY POWER ENABLE
						(OUT << uP_SYS_LED ) |	// Green System LED
						(IN << REV0) |					// Board Revision
						(IN << REV1) |
						(IN << REV2);

					
	// WRITE INITIAL VALUES TO OUTPUT PINS
	FIO1SET = (1 << uP_SYS_LED ) |
						(1 << uP_OVP_VL  );	// 4363 over-voltage shutdown override(manual control)
																  // pull low to force part out of OVLO/defeat auto-retry																	

	hwSetSolarCharge(ON);						// Enable Solar Charge
	hwSetLT4365_BATTERY(ON);				// Enable LT4365/Battery Power
	//FIO1SET = (1 << uP_PWR_VB  ) ;  // Enable LT4365/BATTERY POWER


	//**************
	//******* PORT 2

	// set as GPIO except for interrupt pins
	PINSEL4 = (EXT_INT << EXTINT1) |  
						(EXT_INT << EXTINT2) | 
						(EXT_INT << EXTINT3);

	FIO2DIR = (OUT << uP_PWM_LED  ) |		// system LEDs driver xOE
						(OUT << uP_PWM_DRV  ) |		// driver I2C xOE
						(OUT << uP_PWR_x8V4 ) | 	// 8.4/12V RAIL SELECT for lamps	
						(OUT << uP_PWR_xSHDN) |		// LVVD
						(IN  << uP_xISP     ) |
						(IN  << uP_INT_xDIN	) |
						(IN  << uP_INT_xERR ) ;
				
					
	// SET INITIAL VALUES ON OUTPUTS

	FIO2SET = (1 << uP_PWR_x8V4 ) |		// select 12V default until told otherwise
						(1 << uP_PWR_xSHDN) |		// disable LVD
						(1 << uP_PWM_LED  ) |		// disable I2C system LEDs driver
						(1 << uP_PWM_DRV  ) ;		// disable I2C driver for system LEDs

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
			FIO1DIR |= (OUT << WD_KICK);
			FIO3DIR |= (OUT << ACT_VL_ENA);
			FIO3SET |= (1 << ACT_VL_ENA );
		printf("set port 3\n");
			break;
	}
	//**************
	//******* PORT 4
	//**************
	// the only port4 pins are SPI1.
	PINSEL9 = 0x00000000;
	
	FIO4DIR = (OUT << uP_SPI1_HOLD) |
						(OUT << uP_SPI1_WP  ) ;

	FIO4SET = (1 << uP_SPI1_HOLD) |
						(1 << uP_SPI1_WP  ) ;
						
	// the last step - disable ETM
	hwDisableETM();
}


// enable various periphs connected straight to uP

void hwSetSysLED()
{
	FIO1SET = (1<<uP_SYS_LED);
}
void hwClrSysLED()
{
	FIO1CLR = (1<<uP_SYS_LED);
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
		FIO1CLR |= (1<<WD_KICK);
		bHigh = FALSE;
	}
	else
	{
		FIO1SET |= (1<<WD_KICK);
		bHigh = TRUE;
	}
}

// *********************
// *********************
// *********************
// turn on/off solar charge
void hwSetSolarCharge(int action)
{
	if (ON == action) 
	{
		FIO1SET = (1 << uP_PWR_VL);
		solarChargerOn = TRUE;
	}
	else if (OFF == action)
	{
		FIO1CLR = (1 << uP_PWR_VL);
		solarChargerOn = FALSE;
	}
}

BOOL hwGetSolarChargeStatus(void)
{
	return solarChargerOn;
}

// *********************
// *********************
// *********************
// turn on/off LVD
void hwSetLVD(int action)
{	
	if (ON == action)
	{	
		FIO2CLR = (1 << uP_PWR_xSHDN);
		lvdOn = TRUE;
	}
	else if (OFF == action)
	{	
		FIO2SET = (1 << uP_PWR_xSHDN);
		lvdOn = FALSE;
	}	
}

BOOL hwGetLVDStatus(void)
{
	return lvdOn;
}

// *********************
// *********************
// *********************

// check solar voltage polarity
// return TRUE if positive/correct
// FALSE if neg/reverse pol
BOOL hwIsVLPosPolarity(void)
{
	int port;

	// read port
	port = FIO1PIN;

	// mask off POS_VL bit
	port = port & (1 << uP_POS_VL);

	if(port == 0x00000000) {

			return FALSE;
		
		} 

// COMPARISON FAIL - polarity is correct.

return TRUE;
	
}

// *********************
// *********************
// *********************

// check battery voltage polarity
// return TRUE if positive/correct polarity
// FALSE if neg/reverse pol
BOOL hwIsVBPosPolarity(void)
{
	
	int port;

		// read port
		port = FIO1PIN;

		// mask off POS_VL bit
		port = port & (1 << uP_POS_VB);

		if(port == 0x00000000) {
		// polarity is incorrect
			return FALSE;

			} 
			
		// COMPARISON FAIL
		// polarity is correct
		return TRUE;

}



// *********************
// *********************
// *********************
// Select 8V4 or 12V0 for lamp supply
// measure and ensure rail selection was as intended???

// call with arg _8V4 or _12v0
void hwLampRailSelect(int rail)
{
	if (LAMP_RAIL_8V4 == rail) {
		
		FIO2CLR = (1 << uP_PWR_x8V4);

		} else if(LAMP_RAIL_12V0 == rail) {
		
		FIO2SET = (1 << uP_PWR_x8V4);

		}
}

int hwReadBoardRev()
{
	int port;
	int nRev = 0;
		// model select on port1
	port = FIO1PIN;	
	nRev |= ((port & (1<<REV0))?1:0);
	nRev |= ((port & (1<<REV1))?2:0);
	nRev |= ((port & (1<<REV2))?4:0);
	switch(nRev)
	{
		case 4:
			/////
			// first rev board was 100 because REV2 was added later
			// REV2 pulled high internally and REV0 and REV1 were pulled low externally
			// REV0=0, REV1=0, REV2=1
			/////
			nRev = 0;
			break;
		case 2:
			/////
			// second rev is rev 1
			// REV0=0, REV1=1, REV2=0
			/////
			nRev = 1;
			break;
		case 1:
			// REV0=1, REV1=0, REV2=0
			nRev = 2;
			break;
		case 0:
			// REV0=0, REV1=0, REV2=0
			nRev = 3;
			break;
		case 3:
			// REV0=1, REV1=1, REV2=0
			nRev = 4;
			break;
		case 5:
			// REV0=1, REV1=0, REV2=1
			nRev = 5;
			break;
		case 6:
			// REV0=0, REV1=1, REV2=1
			nRev = 6;
			break;
		case 7:
			// REV0=1, REV1=1, REV2=1
			nRev = 7;
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
	int port;
	char nModel = 0;
	int nRev;
	
	nRev = hwReadBoardRev();
	

	// model select on port1
	port = FIO1PIN;	
	
	// mask off model bits
	port = port & ((1 << uP_MOD_X1) |
                 (1 << uP_MOD_X2) |
							   (1 << uP_MOD_X4) |
							   (1 << uP_MOD_X8) );

	if(nRev < 6) {	// earlier board (than rev J) with dip switches
			if(port & (1 << uP_MOD_X1))
			{
				nModel |= 0x01;
			}
			if(port & (1 << uP_MOD_X2))
			{
				nModel |= 0x02;
			}
			if(port & (1 << uP_MOD_X4))
			{
				nModel |= 0x04;
			}
			if(port & (1 << uP_MOD_X8))
			{
				nModel |= 0x08;
			}
			
			nModel &= 0x07;
	}
	else  // rotary switch - logic inverted
	{
			if((port & (1 << uP_MOD_X1)) == 0)
			{
				nModel |= 0x01;
			}
			if((port & (1 << uP_MOD_X2)) == 0)
			{
				nModel |= 0x02;
			}
			if((port & (1 << uP_MOD_X4)) == 0)
			{
				nModel |= 0x04;
			}
			if((port & (1 << uP_MOD_X8)) == 0)
			{
				nModel |= 0x08;
			}
			
			// rotary switch has 10 positions - if extra 2 dupicate first 2 models
			if(nModel == 0x08)
			{
				nModel = 0;
			}
			else if(nModel == 0x9)
			{
				nModel = 1;
			}
	}
		
	return (eMODEL)nModel;

}

/////
// get the 80 degree tilt indicator
/////
BOOL is80DegreeTilt()
{
	BOOL b80Degrees = (0 != (hwReadModel() & 0x08)); // 0001 0010 0011 0100 0111 1000
	return b80Degrees;
}

/////
// read all switches
// TBD
/////
int hwReadSwitches(void)
{
	int port;

	// model select on port0
	port = FIO0PIN;	

	// mask off model bits
	port = port & ((1 << uP_MOD_X1) |
                 (1 << uP_MOD_X2) |
							   (1 << uP_MOD_X4) |
							   (1 << uP_MOD_X8) );

	// shift model nibble into LSB
	port = port >> (uP_MOD_X1);
	return port;
}
// *********************
// *********************
// *********************
// shutdown/enable 4365 OV/UV/Rev BATT controller
void hwSetLT4365_BATTERY (int action) 
{
	
	if(ON == action) 
	{
		FIO1SET = (1 << uP_PWR_VB);
	}
	else 
	{
		FIO1CLR = (1 << uP_PWR_VB);	
	}
		
// does this need some sort of error/fault detection?  How
// to accomplish?		
}	
#if 0
// *********************
// *********************
// *********************
void hwSetLT4363_ALT_SLR(int action) 
{
		// shutdonw/enable 4363 solar surge stopper
	if(action == ON)
	{		
		FIO1SET = (1 << uP_PWR_VL);
	}
	else
	{	
		FIO1CLR = (1 << uP_PWR_VL);	
	}	
}
#endif
void readActuatorLimitSwitch(BOOL *pbLimitSwitchA, BOOL *pbLimitSwitchB)
{
	*pbLimitSwitchA = FIO0PIN & (1<<LIMIT_SW_A)?FALSE:TRUE;
	*pbLimitSwitchB = FIO0PIN & (1<<LIMIT_SW_B)?FALSE:TRUE;
}

// *********************
// *********************
// *********************
// ENABLE PCA9634 connected to system LEDs 
// and aux/indicator
// (lower xOE on PCA9634)
void hwEnableSystemLedsAndIndicators(void)
{
	
	FIO2CLR = (1 << uP_PWM_LED);			// xOE = 0 ENABLES PCA9436.
	
}

//======================
// Solar Charge Control
//======================
void hwSolarChargeControl(void)
{	
	if(ADCGetSystemVoltage() <= chargerOnVoltage)
	{
		hwSetSolarCharge(ON);
	}
	else if(ADCGetSystemVoltage() >= chargerOffVoltage)
	{
		hwSetSolarCharge(OFF);
	}					
}
 
unsigned int checkLvd = 0;

// check LVD condition every second
// if LVD condition true for 20 seconds then LVD is TRUE
// if LVD condition is TRUE then sample again every 30 seconds
//   for 3 minutes - if all samples show LVD condition then shutdown

void hwLVDControl(void)
{
    #define LVD_LOW_VOLTAGE_READS	10 //  filtered values checked once per second (consective readings to be considered LVD TRUE)
    #define LVD_PERIODS              6
    #define LVD_TIMER_PERIOD        30000	// 30 seconds
    
    static int lvdActiveStates = 0;
    static int lvdActivePeriods = 0;
    static BOOL isLvdVoltage = FALSE;
    
    
    lvdActiveStates ++;
    

    if((ADCGetSystemVoltage() > inputVoltageShutdownLimit))
    {
        // voltage condition good
        lvdActiveStates = 0;
        isLvdVoltage = FALSE;
    }
    else
    {
        if(FALSE == isLvdVoltage)
        {
            if(lvdActiveStates >= LVD_LOW_VOLTAGE_READS)
            {
                isLvdVoltage = TRUE;
                //printf("lvdStates[%d]\r\n", lvdActiveStates);
            }
        }
    }

    switch(eLVDNextState)
    {
        case eLVD_MONITOR_VB:
            if(isLvdVoltage == TRUE)
            {
                //printf("LVD TIMER STARTED\r\n");
                startTimer(&lvdTimer, LVD_TIMER_PERIOD);  // search and remove LVD_TIMER_MS
                eLVDNextState = eLVD_VB_AT_SHUTDOWN_VOLTAGE;
            }
            break;

        case eLVD_VB_AT_SHUTDOWN_VOLTAGE:
            if(isTimerExpired(&lvdTimer))
            {
                if(FALSE == isLvdVoltage)
                {
//printf("LVD TIMER CLEARED\r\n");
                    lvdActivePeriods = 0;
                    eLVDNextState = eLVD_MONITOR_VB;
                }
                else
                {
                    lvdActivePeriods ++;
//printf("LVD_Periods[%d]\r\n", lvdPeriods);
                    if(lvdActivePeriods >= LVD_PERIODS)
                    {
 //                       printf("LVD ACTIVATED\r\n");
                        hwSetLVD(ON);
                    }
                    else
                    {
                      startTimer(&lvdTimer, LVD_TIMER_PERIOD);
                    }                        
                }
            }
            break;

            default:
                eLVDNextState = eLVD_MONITOR_VB;
   }
}
