// actuator routines
// presumes pindefs elsewhere
// presumes GPIO set up elsewhere
#include <stdlib.h>
#include <stdio.h>
#include <lpc23xx.h>
#include "shareddefs.h"
#include "ArrowBoardDefs.h"		// hardware 
#include "sharedinterface.h"
#include "timer.h"
#include "queue.h"

#include "mischardware.h"
#include "commands.h"
#include "actuator.h"	// actuator specifics
#include "errorinput.h"
#include "ADS7828.h"
#include "leddriver.h"
#include "pwm.h"
#include "storedconfig.h"
#include "watchdog.h"
#include "adc.h"
#include "accelerometer.h"

#include "i2c.h"
#include "mischardware.h"

static I2C_QUEUE_ENTRY myI2CQueueEntry;
static I2C_TRANSACTION myTransaction;
static uint8_t presentActuatorIO = 0;
static TIMERCTL sampleTimer;
static BOOL bInit = FALSE;
static uint8_t nNew = 0;

static TIMERCTL commandTimer;
static TIMERCTL inrushTimer;
static TIMERCTL limitSwitchTimer;
static eACTUATOR_COMMANDS eReceivedCommand;
static eACTUATOR_INTERNAL_STATE eInternalState;
static eACTUATOR_INTERNAL_STATE ePreviousInternalState;

/////
// define current limits
// for actuator motion
/////
static int nActuatorUpStopCurrent = ACT_DEFAULT_UP_STOP_CURRENT;
static int nActuatorDownStopCurrent = ACT_DEFAULT_DOWN_STOP_CURRENT;
static BOOL bCalibratingLimitCurrent = FALSE;
static TIMERCTL calibratingLimitCurrentTimer;
static int nBoardRev;

/*
 * Configure 9634 for H-Bridge
 */ 
static void actuatorDriverAddConfigureI2CQueueEntry( void )
{
		I2CQueueEntryInitialize(&myI2CQueueEntry);
		I2CQueueInitializeTransaction(&myTransaction);
		myTransaction.nSLA_W = PCA9634_ACT_CONTROL;
		myTransaction.nSLA_R = 0;
		myTransaction.cOutgoingData[0] = ACT_CONTROL_AUTO_INC_ADDR_0;	// Auto inc 
		myTransaction.cOutgoingData[1] = ACT_CONFIG_MODEREG1;					// Awake
		myTransaction.cOutgoingData[2] = ACT_CONFIG_MODEREG2;					// Invert output, totem pole output
		myTransaction.cOutgoingData[3] = 0;
		myTransaction.cOutgoingData[4] = 0;
		myTransaction.cOutgoingData[5] = 0;
		myTransaction.cOutgoingData[6] = 0;
		myTransaction.cOutgoingData[7] = 0;
		myTransaction.cOutgoingData[8] = 0;
		myTransaction.cOutgoingData[9] = 0;
		myTransaction.cOutgoingData[10] = 0;
		myTransaction.cOutgoingData[11] = 0xFF;
		myTransaction.cOutgoingData[12] = 0;
		myTransaction.cOutgoingData[13] = 0x54;												// Initial in Brake
		myTransaction.cOutgoingData[14] = 0x55;												// IO 7-4 all 1's
		presentActuatorIO = 0xF6;
	
		myTransaction.nOutgoingDataLength = 14;
		myTransaction.nIncomingDataLength = 0;
		I2CQueueAddTransaction(&myI2CQueueEntry, &myTransaction);
}
/*
 * Input parameter is bit map of output I/O from i2c expander
 */
static void actuatorDriverAddOutputControlI2CQueueEntry( uint8_t newOutput )
{
	//static uint8_t crap = 0x00;
	uint8_t tempOutput = 0;
	
	if(!bInit )
	{
		return;
	}
	I2CQueueEntryInitialize(&myI2CQueueEntry);
	I2CQueueInitializeTransaction(&myTransaction);
	myTransaction.nSLA_W = PCA9634_ACT_CONTROL;
	myTransaction.nSLA_R = 0;

	myTransaction.cOutgoingData[0] = H_BRIDGE_IO_ADDR;
	// Determine control values for pins LEDOUT 0-3
	tempOutput |= ((1 << 0) & newOutput) ? (1 << 0) : 0;
	tempOutput |= ((1 << 1) & newOutput) ? (1 << 2) : 0;
	tempOutput |= ((1 << 2) & newOutput) ? (1 << 4) : 0;	
	tempOutput |= ((1 << 3) & newOutput) ? (1 << 6) : 0;
	myTransaction.cOutgoingData[1] = tempOutput;
	tempOutput = 0;
	// Determine control values for pins LEDOUT 4-7
	tempOutput |= ((1 << 4) & newOutput) ? (1 << 0) : 0;
	tempOutput |= ((1 << 5) & newOutput) ? (1 << 2) : 0;
	tempOutput |= ((1 << 6) & newOutput) ? (1 << 4) : 0;	
	tempOutput |= ((1 << 7) & newOutput) ? (1 << 6) : 0;
	myTransaction.cOutgoingData[2] = tempOutput;
	
	myTransaction.nOutgoingDataLength = 3;
	myTransaction.nIncomingDataLength = 0;
	I2CQueueAddTransaction(&myI2CQueueEntry, &myTransaction);
	
	presentActuatorIO = newOutput;
}

static int actuatorRun(int direction)
{
	uint8_t newOutput = presentActuatorIO;
	static int lastDirection = 0xFF;	// jgr DEBUG 
		
	switch(direction) {		

		case UP:
			if( direction != lastDirection ) { printf("actuatorRun UP\n"); }//jgr
			newOutput |= (ACT_ENA | ACT_A | ACT_PWM );	// Enable Actuator, Turn ON A input, 100% PWM
			newOutput &= ~(ACT_B);											// Turn OFF B input
			actuatorDriverAddOutputControlI2CQueueEntry( newOutput );
			nNew++;
			//I2CQueueAddEntry(&myI2CQueueEntry);
			break;
				

		case DOWN:
			if( direction != lastDirection ) { printf("actuatorRun DOWN\n"); }//jgr
			newOutput |= (ACT_ENA | ACT_B | ACT_PWM );	// Enable Actuator, Turn ON B input, 100% PWM
			newOutput &= ~(ACT_A);											// Turn OFF A input
			actuatorDriverAddOutputControlI2CQueueEntry( newOutput );
			nNew++;
			//I2CQueueAddEntry(&myI2CQueueEntry);
			break;

		
		case OFF: 
		default:
			if( direction != lastDirection ) { printf("actuatorRun OFF\n"); }//jgr
			// These settings will cause the H-Bridge to cause the Actuator to 
			// Brake to Vcc. It can also be set to break to Ground.
			newOutput &= ~(ACT_PWM);							// 0% PWM
			newOutput |= ACT_ENA | ACT_A | ACT_B;	// Turn ON A & B, Eable H-bridge, input, PWM = 0%
			actuatorDriverAddOutputControlI2CQueueEntry( newOutput );
			nNew++;
			//I2CQueueAddEntry(&myI2CQueueEntry);
			ads7827ResetCurrentLimitReached();	
			break;
		
	}
	lastDirection = direction;
	return TRUE;
}


// *********************
// *********************
// *********************
static void actuatorConfigGPIO(unsigned char bUseLineVolts) 
{
	
//set up directions
FIO2DIR |= (OUT << uP_PWM_xACT ) |	/* Either PWM or OE for actuator */
					 (IN << uP_PWR_xACTA)  |
	         (IN << uP_PWR_xACTB)  |
	         (IN << uP_ENA_xACT )  |	
	         (IN  << uP_IOK_xACTA) |
	         (IN  << uP_IOK_xACTB) ;
	
	
// make sure actuator doesn't run, 
FIO2CLR =  (1 << uP_PWM_xACT );

//printf("1-actuatorConfigGPIO bUseLineVolts[%d]\n", bUseLineVolts);	
	if(bUseLineVolts)
	{
		FIO3SET |= (1 << ACT_VL_ENA);
//		printf("2-actuatorConfigGPIO bUseLineVolts[%d]\n", bUseLineVolts);	
	}
	else
	{
		FIO3CLR |= (1 << ACT_VL_ENA);
//		printf("3-actuatorConfigGPIO bUseLineVolts[%d]\n", bUseLineVolts);	
	}
}

void actuatorInit(unsigned char bUseLineVolts)
{
	int nLimit;
	nBoardRev = hwReadBoardRev();
	
	printf("Act Init\n");
	// Init mode registers
	bInit = FALSE;
	actuatorDriverAddConfigureI2CQueueEntry( );
	I2CQueueAddEntry(&myI2CQueueEntry);
	initTimer(&sampleTimer);
	startTimer(&sampleTimer, 20/*LEDDRIVER_SAMPLE_TIME_MS*/);
	// Initial output lines
	actuatorConfigGPIO(bUseLineVolts);
	//actuatorRun(OFF);
	eInternalState = eACTUATOR_INTERNAL_STATE_IDLE;
	ePreviousInternalState = eACTUATOR_INTERNAL_STATE_IDLE;
	eReceivedCommand = eACTUATOR_COMMAND_STOP;
	initTimer(&commandTimer);
	initTimer(&inrushTimer);
	initTimer(&limitSwitchTimer);
	initTimer(&calibratingLimitCurrentTimer);
	/////
	// then turn limit switch power on
	/////
	ledDriverSetLimitSWPower(eLED_ON);
	startTimer(&limitSwitchTimer, LIMIT_SWITCH_POWER_WAIT_TIMEOUT);
	
	nLimit = storedConfigGetActuatorUpLimit();
	if(0 < nLimit)
	{	
		//nActuatorUpStopCurrent = nLimit;
	}
	//printf("nActuatorUpStopCurrent[%d]\n", nActuatorUpStopCurrent);
	nLimit = storedConfigGetActuatorDownLimit();
	if(0 < nLimit)
	{	
		//nActuatorDownStopCurrent = nLimit;
	}
	//printf("nActuatorDownStopCurrent[%d]\n", nActuatorDownStopCurrent);

	// This !OE to i/o ic. enable output
	FIO2CLR = (1 << uP_PWM_xACT);
}
void actuatorDoState(eACTUATOR_EVENTS eEvent)
{
	if( eACTUATOR_INTERNAL_STATE_IDLE != eInternalState )
	{
		//printf("1-actuatorDoState eEvent[%d] eInternalState[%d]\n", eEvent, eInternalState);
	}
	switch(eInternalState)
	{
		case eACTUATOR_INTERNAL_STATE_IDLE:
			switch(eEvent)
			{
				case eACTUATOR_EVENT_NOOP:
				case eACTUATOR_EVENT_COMMAND_STOP:
				case eACTUATOR_EVENT_COMMAND_TIMEOUT:
				case eACTUATOR_EVENT_INRUSH_TIMER_EXPIRY:
				case eACTUATOR_EVENT_SWITCH_OFF:
				case eACTUATOR_EVENT_OVERCURRENT:
				case eACTUATOR_EVENT_UP_LIMIT_SWITCH_CLOSURE:
				case eACTUATOR_EVENT_DOWN_LIMIT_SWITCH_CLOSURE:
					//printf("1-DoState\n\r");
					actuatorRun(OFF);
					break;
				
				case eACTUATOR_EVENT_COMMAND_UP:
					ePreviousInternalState = eInternalState;
					eInternalState = eACTUATOR_INTERNAL_STATE_UP_BEFORE_CURRENT_CHECK;
					//printf("2-DoState\n\r");
					ads7828SetActuatorStopCurrent(nActuatorUpStopCurrent);
					actuatorRun(UP);
					if(nBoardRev < 6)
					{
						 startTimer(&inrushTimer, ACTUATOR_INRUSH_TIMEOUT);
					}
					startTimer(&commandTimer, ACTUATOR_COMMAND_TIMEOUT);
					break;
				case eACTUATOR_EVENT_COMMAND_DOWN:
					ePreviousInternalState = eInternalState;
					eInternalState = eACTUATOR_INTERNAL_STATE_DOWN_BEFORE_CURRENT_CHECK;
					//printf("3-DoState\n\r");
					ads7828SetActuatorStopCurrent(nActuatorDownStopCurrent);
					actuatorRun(DOWN);
					if(nBoardRev < 6)
					{
						 startTimer(&inrushTimer, ACTUATOR_INRUSH_TIMEOUT);
					}
					startTimer(&commandTimer, ACTUATOR_COMMAND_TIMEOUT);
					break;

				case eACTUATOR_EVENT_SWITCH_UP:
					ePreviousInternalState = eInternalState;
					eInternalState = eACTUATOR_INTERNAL_STATE_UP_BEFORE_CURRENT_CHECK;
					//printf("4-DoState\n\r");
					ads7828SetActuatorStopCurrent(nActuatorUpStopCurrent);
					actuatorRun(UP);
					if(nBoardRev < 6)
					{
						 startTimer(&inrushTimer, ACTUATOR_INRUSH_TIMEOUT);
					}
					break;
				case eACTUATOR_EVENT_SWITCH_DOWN:
					ePreviousInternalState = eInternalState;
					eInternalState = eACTUATOR_INTERNAL_STATE_DOWN_BEFORE_CURRENT_CHECK;
					//printf("5-DoState\n\r");
					ads7828SetActuatorStopCurrent(nActuatorDownStopCurrent);
					actuatorRun(DOWN);
					if(nBoardRev < 6)
					{
						startTimer(&inrushTimer, ACTUATOR_INRUSH_TIMEOUT);						
					}
					break;
			}
			break;
		case eACTUATOR_INTERNAL_STATE_UP_BEFORE_CURRENT_CHECK:
				switch(eEvent)
			{
				case eACTUATOR_EVENT_NOOP:
					break;
				case eACTUATOR_EVENT_COMMAND_STOP:
				case eACTUATOR_EVENT_SWITCH_OFF:
				case eACTUATOR_EVENT_COMMAND_TIMEOUT:
				case eACTUATOR_EVENT_UP_LIMIT_SWITCH_CLOSURE:
					eInternalState = eACTUATOR_INTERNAL_STATE_IDLE;
					actuatorRun(OFF);
					break;
				case eACTUATOR_EVENT_COMMAND_UP:
					startTimer(&commandTimer, ACTUATOR_COMMAND_TIMEOUT);
					break;
				case eACTUATOR_EVENT_COMMAND_DOWN:
					ePreviousInternalState = eInternalState;
					eInternalState = eACTUATOR_INTERNAL_STATE_IDLE;
					actuatorRun(OFF);
					break;
				case eACTUATOR_EVENT_SWITCH_UP:
					break;
				case eACTUATOR_EVENT_SWITCH_DOWN:
					ePreviousInternalState = eInternalState;
					eInternalState = eACTUATOR_INTERNAL_STATE_IDLE;
					actuatorRun(OFF);
					break;
				case eACTUATOR_EVENT_INRUSH_TIMER_EXPIRY:
					ePreviousInternalState = eInternalState;
					eInternalState = eACTUATOR_INTERNAL_STATE_UP_WITH_CURRENT_CHECK;
					break;
				case eACTUATOR_EVENT_OVERCURRENT:
					printf("eACTUATOR_INTERNAL_STATE_UP_BEFORE_CURRENT_CHECK: eACTUATOR_EVENT_OVERCURRENT\n");
					break;
				case eACTUATOR_EVENT_DOWN_LIMIT_SWITCH_CLOSURE:
					break;
			}
			break;
		case eACTUATOR_INTERNAL_STATE_DOWN_BEFORE_CURRENT_CHECK:
			switch(eEvent)
			{
				case eACTUATOR_EVENT_NOOP:
					break;
				case eACTUATOR_EVENT_COMMAND_STOP:
				case eACTUATOR_EVENT_SWITCH_OFF:
				case eACTUATOR_EVENT_COMMAND_TIMEOUT:
				case eACTUATOR_EVENT_DOWN_LIMIT_SWITCH_CLOSURE:
					ePreviousInternalState = eInternalState;
					eInternalState = eACTUATOR_INTERNAL_STATE_IDLE;
					actuatorRun(OFF);
					break;
				case eACTUATOR_EVENT_COMMAND_UP:
					ePreviousInternalState = eInternalState;
					eInternalState = eACTUATOR_INTERNAL_STATE_IDLE;
					actuatorRun(OFF);
					break;
				case eACTUATOR_EVENT_COMMAND_DOWN:
					startTimer(&commandTimer, ACTUATOR_COMMAND_TIMEOUT);
					break;
				case eACTUATOR_EVENT_SWITCH_UP:
					ePreviousInternalState = eInternalState;
					eInternalState = eACTUATOR_INTERNAL_STATE_IDLE;
					actuatorRun(OFF);
					break;
				case eACTUATOR_EVENT_SWITCH_DOWN:
					break;
				case eACTUATOR_EVENT_INRUSH_TIMER_EXPIRY:
					ePreviousInternalState = eInternalState;
					eInternalState = eACTUATOR_INTERNAL_STATE_UP_WITH_CURRENT_CHECK;
					break;
				case eACTUATOR_EVENT_OVERCURRENT:
					printf("eACTUATOR_INTERNAL_STATE_DOWN_BEFORE_CURRENT_CHECK: eACTUATOR_EVENT_OVERCURRENT\n");
					break;
				case eACTUATOR_EVENT_UP_LIMIT_SWITCH_CLOSURE:
					break;
			}
			break;
		case eACTUATOR_INTERNAL_STATE_UP_WITH_CURRENT_CHECK:
			switch(eEvent)
			{
				case eACTUATOR_EVENT_NOOP:
					break;
				case eACTUATOR_EVENT_COMMAND_STOP:
				case eACTUATOR_EVENT_SWITCH_OFF:
				case eACTUATOR_EVENT_COMMAND_TIMEOUT:
				case eACTUATOR_EVENT_UP_LIMIT_SWITCH_CLOSURE:
					ePreviousInternalState = eInternalState;
					eInternalState = eACTUATOR_INTERNAL_STATE_IDLE;
					actuatorRun(OFF);
					break;
				case eACTUATOR_EVENT_COMMAND_UP:
					startTimer(&commandTimer, ACTUATOR_COMMAND_TIMEOUT);
					break;
				case eACTUATOR_EVENT_COMMAND_DOWN:
					ePreviousInternalState = eInternalState;
					eInternalState = eACTUATOR_INTERNAL_STATE_IDLE;
					actuatorRun(OFF);
					break;
				case eACTUATOR_EVENT_SWITCH_UP:

					break;
				case eACTUATOR_EVENT_SWITCH_DOWN:
					ePreviousInternalState = eInternalState;
					eInternalState = eACTUATOR_INTERNAL_STATE_IDLE;
					actuatorRun(OFF);
					break;
				case eACTUATOR_EVENT_INRUSH_TIMER_EXPIRY:
					break;
				case eACTUATOR_EVENT_OVERCURRENT:
					ePreviousInternalState = eInternalState;
					eInternalState = eACTUATOR_INTERNAL_STATE_ERROR;
					actuatorRun(OFF);
					printf("eACTUATOR_INTERNAL_STATE_UP_WITH_CURRENT_CHECK: eACTUATOR_EVENT_OVERCURRENT->actuatorRun(OFF)\n");
					break;
				case eACTUATOR_EVENT_DOWN_LIMIT_SWITCH_CLOSURE:
					break;
			}
			break;
		case eACTUATOR_INTERNAL_STATE_DOWN_WITH_CURRENT_CHECK:
			switch(eEvent)
			{
				case eACTUATOR_EVENT_NOOP:
					break;
				case eACTUATOR_EVENT_COMMAND_STOP:
				case eACTUATOR_EVENT_SWITCH_OFF:
				case eACTUATOR_EVENT_COMMAND_TIMEOUT:
				case eACTUATOR_EVENT_DOWN_LIMIT_SWITCH_CLOSURE:
					ePreviousInternalState = eInternalState;
					eInternalState = eACTUATOR_INTERNAL_STATE_IDLE;
					actuatorRun(OFF);
					break;
				case eACTUATOR_EVENT_COMMAND_UP:
					ePreviousInternalState = eInternalState;
					eInternalState = eACTUATOR_INTERNAL_STATE_IDLE;
					actuatorRun(OFF);
					break;
				case eACTUATOR_EVENT_COMMAND_DOWN:
					startTimer(&commandTimer, ACTUATOR_COMMAND_TIMEOUT);
					break;
				case eACTUATOR_EVENT_SWITCH_UP:
					ePreviousInternalState = eInternalState;
					eInternalState = eACTUATOR_INTERNAL_STATE_IDLE;
					actuatorRun(OFF);
					break;
				case eACTUATOR_EVENT_SWITCH_DOWN:
					break;
				case eACTUATOR_EVENT_INRUSH_TIMER_EXPIRY:
					break;
				case eACTUATOR_EVENT_OVERCURRENT:
					ePreviousInternalState = eInternalState;
					eInternalState = eACTUATOR_INTERNAL_STATE_ERROR;
					actuatorRun(OFF);
					printf("eACTUATOR_INTERNAL_STATE_DOWN_WITH_CURRENT_CHECK: eACTUATOR_EVENT_OVERCURRENT->actuatorRun(OFF)\n");
					break;
				case eACTUATOR_EVENT_UP_LIMIT_SWITCH_CLOSURE:
					break;
			}
			break;
		case eACTUATOR_INTERNAL_STATE_ERROR:
			switch(eEvent)
			{
				case eACTUATOR_EVENT_COMMAND_STOP:
				case eACTUATOR_EVENT_COMMAND_TIMEOUT:
				case eACTUATOR_EVENT_SWITCH_OFF:
					ePreviousInternalState = eInternalState;
					eInternalState = eACTUATOR_INTERNAL_STATE_IDLE;
					break;
				
				case eACTUATOR_EVENT_COMMAND_UP:
				case eACTUATOR_EVENT_COMMAND_DOWN:
					startTimer(&commandTimer, ACTUATOR_COMMAND_TIMEOUT);
					break;
				
				case eACTUATOR_EVENT_NOOP:
				case eACTUATOR_EVENT_SWITCH_UP:
				case eACTUATOR_EVENT_SWITCH_DOWN:
				case eACTUATOR_EVENT_INRUSH_TIMER_EXPIRY:
				case eACTUATOR_EVENT_OVERCURRENT:
				case eACTUATOR_EVENT_UP_LIMIT_SWITCH_CLOSURE:
				case eACTUATOR_EVENT_DOWN_LIMIT_SWITCH_CLOSURE:
					printf("eACTUATOR_INTERNAL_STATE_ERROR: eACTUATOR_EVENT_OVERCURRENT\n");
					break;
			}
			break;
	}
}

void actuatorDoWork()
{
	static int nPrevSwitchBitmap = 0;
	BOOL bComplete = FALSE;

	switch(myI2CQueueEntry.eState)
	{

		case eI2C_TRANSFER_STATE_COMPLETE:
			myI2CQueueEntry.eState = eI2C_TRANSFER_STATE_IDLE;
		break;
		case eI2C_TRANSFER_STATE_FAILED:
			myI2CQueueEntry.eState = eI2C_TRANSFER_STATE_IDLE;
			printf("\nERROR****\n");
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
		bInit = TRUE;

		if( nNew )
		{
		//			ledDriverAddLedControlI2CQueueEntry();
			I2CQueueAddEntry(&myI2CQueueEntry);
			startTimer(&sampleTimer, LEDDRIVER_SAMPLE_TIME_MS);
			nNew--;
		}
	}
	
	/////
	// have we reached the current limit?
	/////
	if(bCalibratingLimitCurrent)
	{
		/////
		// we don't want to do this forever
		// if we lose communication
		/////
		if(isTimerExpired(&calibratingLimitCurrentTimer))
		{
			bCalibratingLimitCurrent = FALSE;
			actuatorCalibrate(eACTUATOR_LIMIT_CALIBRATE_CANCEL);
		}
	}
	else
	{
#if (0)
		if(ads7827IsActuatorCurrentLimitReached())
		{
			/////
			// issue an overcurrent event
			/////;
			actuatorDoState(eACTUATOR_EVENT_OVERCURRENT);
		}
#endif
	}

	do
	{		
		/////
		// switches override commands
		// so handle them first
		/////
		int nSwitchBitmap = errorInputGetActSwitch();

		switch(nSwitchBitmap)
		{
			case SWITCH_BITMAP_ACT_UP:
				if(errorInputIsVLOK())
				{
					printf("Act Switch up[%d]\n", ADCGetSystemVoltage());
					actuatorDoState(eACTUATOR_EVENT_SWITCH_UP);
				}
				/////
				// reset received command
				// so we don't act on it again
				/////
				eReceivedCommand = eACTUATOR_COMMAND_NOOP;
			
				/////
				// show that we acted on this information
				/////
				bComplete = TRUE;
				break;
			
			case SWITCH_BITMAP_ACT_DOWN:
				if(errorInputIsVLOK())
				{
					printf("Act Switch down[%d]\n", ADCGetSystemVoltage());
					actuatorDoState(eACTUATOR_EVENT_SWITCH_DOWN);
				}
			
				/////
				// reset received command
				// so we don't act on it again
				/////
				eReceivedCommand = eACTUATOR_COMMAND_NOOP;
			
				/////
				// show that we acted on this information
				/////
				bComplete = TRUE;
				break;
			
			case 0:
				if(0 != nPrevSwitchBitmap)
				{
					/////
					// switch just went off
					/////
					printf("Act Switch off\n");
					actuatorDoState(eACTUATOR_EVENT_SWITCH_OFF);
			
					/////
					// reset received command
					// so we don't act on it again
					/////
					eReceivedCommand = eACTUATOR_COMMAND_NOOP;
			
					/////
					// show that we acted on this information
					/////
					bComplete = TRUE;
				}
				break;	
				
			default:
				break;
		}
		nPrevSwitchBitmap = nSwitchBitmap;
		if(bComplete)
		{
			break;
		}

		/////
		// now handle any commands
		/////
		switch(eReceivedCommand)
		{
			case eACTUATOR_COMMAND_STOP:
				//printf("Do Work-Act Command stop[%d]\n", ADCGetSystemVoltage() );
				actuatorDoState(eACTUATOR_EVENT_COMMAND_STOP);
				break;
			
			case eACTUATOR_COMMAND_MOVE_UP:
				if(errorInputIsVLOK())
				{
					//printf("Do Work-Act Command up[%d]\n", ADCGetSystemVoltage() );
					actuatorDoState(eACTUATOR_EVENT_COMMAND_UP);
				}
				break;
			
			case eACTUATOR_COMMAND_MOVE_DOWN:
				if(errorInputIsVLOK())
				{
					//printf("Do Work-Act Command down[%d]\n", ADCGetSystemVoltage() );
					actuatorDoState(eACTUATOR_EVENT_COMMAND_DOWN);
				}
				break;
			
			default:
				if(isTimerExpired(&commandTimer))
				{
					actuatorDoState(eACTUATOR_EVENT_COMMAND_TIMEOUT);
					/////
					// DEH DEBUG 060614
					/////
					startTimer(&commandTimer, ACTUATOR_COMMAND_TIMEOUT);
				}
				if(nBoardRev < 6)
				{
						if(isTimerExpired(&inrushTimer))
						{
							switch(eInternalState)
							{
								case eACTUATOR_INTERNAL_STATE_UP_BEFORE_CURRENT_CHECK:
									eInternalState = eACTUATOR_INTERNAL_STATE_UP_WITH_CURRENT_CHECK;
									ads7828BeginActuatorLimitCalculations();
									break;
								case eACTUATOR_INTERNAL_STATE_DOWN_BEFORE_CURRENT_CHECK:
									eInternalState = eACTUATOR_INTERNAL_STATE_DOWN_WITH_CURRENT_CHECK;
									ads7828BeginActuatorLimitCalculations();
									break;
								default:
									break;
							}					
						}
				}
				break;
		}
		
		
		/////
		// reset received command
		// so we don't act on it again
		/////
		eReceivedCommand = eACTUATOR_COMMAND_NOOP;
	}while(0);
	if(isTimerExpired(&limitSwitchTimer))
	{
		switch(eInternalState)
		{

			case eACTUATOR_INTERNAL_STATE_UP_BEFORE_CURRENT_CHECK:
			case eACTUATOR_INTERNAL_STATE_DOWN_BEFORE_CURRENT_CHECK:
			case eACTUATOR_INTERNAL_STATE_UP_WITH_CURRENT_CHECK:
			case eACTUATOR_INTERNAL_STATE_DOWN_WITH_CURRENT_CHECK:
				break;
			case eACTUATOR_INTERNAL_STATE_IDLE:
			case eACTUATOR_INTERNAL_STATE_ERROR:
				{
					/////
					// read the limit switches and record the state
					/////
					eACTUATOR_LIMITS eLimitState = actuatorGetLimitState();
					switch(eLimitState)
					{
						case eACTUATOR_LIMIT_ERROR:
						case eACTUATOR_LIMIT_NONE:
							accelerometerActuatorIsDown(FALSE);
							break;
						case eACTUATOR_LIMIT_TOP:
							actuatorDoState(eACTUATOR_EVENT_UP_LIMIT_SWITCH_CLOSURE);
							break;
						case eACTUATOR_LIMIT_BOTTOM:
							actuatorDoState(eACTUATOR_EVENT_DOWN_LIMIT_SWITCH_CLOSURE);
							/////
							// limit switch shows that we are down
							// so tell the accelerometer to make a note of initial position
							/////
							accelerometerActuatorIsDown(TRUE);
							break;
					}
					/////
					// wait for the next cycle
					/////
					startTimer(&limitSwitchTimer, LIMIT_SWITCH_POWER_IDLE_TIMEOUT);
				}
				break;
		}
	}
}
// need a helper routine to configure PWM for actuator

ePACKET_STATUS actuatorCommand(unsigned short nsActuatorControl)
{
	ePACKET_STATUS eRetVal = ePACKET_STATUS_SUCCESS;
	//printf("1-actuatorCommand[%d]\n", nsActuatorControl);//old jgr

	switch(nsActuatorControl)
	{
		case eACTUATOR_COMMAND_STOP:
			accelerometerActuatorStopping( eACTUATOR_COMMAND_STOP );
			eReceivedCommand = eACTUATOR_COMMAND_STOP;
			printf("1-Actuator Command STOP[%d]\n", ADCGetSystemVoltage() );
			break;
		
		case eACTUATOR_COMMAND_MOVE_UP:
			if(errorInputIsVLOK())
			{
				printf("2-Actuator Command UP[%d]\n", ADCGetSystemVoltage() );
				if(eACTUATOR_LIMIT_TOP == actuatorGetLimitState())
				{
					eRetVal = ePACKET_STATUS_OUT_OF_RANGE;
					accelerometerActuatorStopping( eACTUATOR_COMMAND_MOVE_UP );
					eReceivedCommand = eACTUATOR_COMMAND_STOP;
					printf("  2A-Act limit top[%d]\n", ADCGetSystemVoltage() );
				}
				else
				{
					accelerometerActuatorStarting( eACTUATOR_COMMAND_MOVE_UP );
					eReceivedCommand = eACTUATOR_COMMAND_MOVE_UP;
					startTimer(&commandTimer, ACTUATOR_COMMAND_TIMEOUT);
					////printf("  2B-Act Command up[%d]\n", ADCGetSystemVoltage() );
				}
			}
			else
			{
				/////
				// voltage too low
				// don't start actuator
				/////
				printf("errorInputIsVLOK[%d][%ld] errorInputIsVBOK[%d][%ld]\n", errorInputIsVLOK(),  ADCGetVl(), errorInputIsVBOK(), ADCGetVb());
				printf("  2C-Act low voltage\n"); //jgr
				eRetVal = ePACKET_STATUS_GENERAL_ERROR;
			}
			break;
		
		case eACTUATOR_COMMAND_MOVE_DOWN:
			if(errorInputIsVLOK())
			{
				printf("3-Actuator Command DOWN[%d]\n", ADCGetSystemVoltage() );
				if(eACTUATOR_LIMIT_BOTTOM == actuatorGetLimitState())
				{
					eRetVal = ePACKET_STATUS_OUT_OF_RANGE;
					accelerometerActuatorStopping( eACTUATOR_COMMAND_MOVE_DOWN );
					eReceivedCommand = eACTUATOR_COMMAND_STOP;
					//printf("  3A-Act limit bottom\n"); //jgr
				}
				else
				{
					accelerometerActuatorStarting( eACTUATOR_COMMAND_MOVE_DOWN );
					eReceivedCommand = eACTUATOR_COMMAND_MOVE_DOWN;
					startTimer(&commandTimer, ACTUATOR_COMMAND_TIMEOUT);
					//printf("  3B-Act Command down[%d]\n", ADCGetSystemVoltage() );
				}
			}
			else
			{
				/////
				// voltage too low
				// don't start actuator
				/////
				printf("errorInputIsVLOK[%d][%ld] errorInputIsVBOK[%d][%ld]\n", errorInputIsVLOK(),  ADCGetVl(), errorInputIsVBOK(), ADCGetVb());
				printf("3C-Act low voltage\n"); //jgr
				eRetVal = ePACKET_STATUS_GENERAL_ERROR;
			}
			break;
		
		default:
			eRetVal = ePACKET_STATUS_NOT_SUPPORTED;
			printf("4-Actuator unknown\n"); //jgr
			break;
	}

	return eRetVal;
}
#ifdef ACCEL_BMA255
eACTUATOR_LIMITS actuatorGetLimitState( void )
{
	eACTUATOR_LIMITS nLimitState = eACTUATOR_LIMIT_NONE;
	
	if( isArrowBoardUp( ) )
	{
		nLimitState = eACTUATOR_LIMIT_TOP;
		ledDriverSetLedSyst(eLED_ON);
		//printf("LIMIT AT TOP\n");
	}
	else if( isArrowBoardDown( ) )
	{
		nLimitState = eACTUATOR_LIMIT_BOTTOM;
		ledDriverSetLedSyst(eLED_ON);
				//printf("LIMIT AT BOTTOM\n");
	}
	else
	{
		ledDriverSetLedSyst(eLED_OFF);
	}
	
	return nLimitState;
}

#else 	// ACCEL_BMA255

eACTUATOR_LIMITS actuatorGetLimitState( void )
{
	eACTUATOR_LIMITS nLimitState = eACTUATOR_LIMIT_NONE;
	/////
	// read the limit switches and record the state
	/////
	BOOL bActuatorLimitSwitchA = FALSE;
	BOOL bActuatorLimitSwitchB = FALSE;

	readActuatorLimitSwitch(&bActuatorLimitSwitchA, &bActuatorLimitSwitchB);

	if(bActuatorLimitSwitchA && bActuatorLimitSwitchB)
	{
		nLimitState = eACTUATOR_LIMIT_ERROR;
		//printf("eACTUATOR_LIMIT_ERROR: BOTH SWITCHES A&B ACTIVATED!\n");
	}
	else if(bActuatorLimitSwitchA)
	{
		nLimitState = eACTUATOR_LIMIT_TOP;
		ledDriverSetLedSyst(eLED_ON);
	}
	else if(bActuatorLimitSwitchB)
	{
		nLimitState = eACTUATOR_LIMIT_BOTTOM;
		ledDriverSetLedSyst(eLED_ON);
	}
	else
	{
		ledDriverSetLedSyst(eLED_OFF);
	}		
	return nLimitState;
}
#endif 	// ACCEL_BMA255

unsigned short actuatorGetStatus()
{
	unsigned short nStatus = 0;
	switch(eInternalState)
	{
		case eACTUATOR_INTERNAL_STATE_IDLE:
			nStatus = eACTUATOR_STATE_IDLE;
			break;
		case eACTUATOR_INTERNAL_STATE_UP_BEFORE_CURRENT_CHECK:
		case eACTUATOR_INTERNAL_STATE_UP_WITH_CURRENT_CHECK:
			nStatus = eACTUATOR_STATE_MOVING_UP;		
			break;
		case	eACTUATOR_INTERNAL_STATE_DOWN_BEFORE_CURRENT_CHECK:
		case eACTUATOR_INTERNAL_STATE_DOWN_WITH_CURRENT_CHECK:
			nStatus = eACTUATOR_STATE_MOVING_DOWN;		
			break;
		case eACTUATOR_INTERNAL_STATE_ERROR:
			//printf("1-[%d] [%d]\n", eInternalState, ePreviousInternalState);
			nStatus = eACTUATOR_STATE_STALLED_MOVING_UP;	
			switch(ePreviousInternalState)
			{
				case eACTUATOR_INTERNAL_STATE_UP_BEFORE_CURRENT_CHECK:
				case eACTUATOR_INTERNAL_STATE_UP_WITH_CURRENT_CHECK:
					nStatus = eACTUATOR_STATE_STALLED_MOVING_UP;		
					break;
				case	eACTUATOR_INTERNAL_STATE_DOWN_BEFORE_CURRENT_CHECK:
				case eACTUATOR_INTERNAL_STATE_DOWN_WITH_CURRENT_CHECK:
					nStatus = eACTUATOR_STATE_STALLED_MOVING_DOWN;		
					break;
				default:
					break;
			}
			break;
	}
	return nStatus;
}

void actuatorCalibrate(eACTUATOR_LIMIT_CALIBRATE eStage)
{
	static eACTUATOR_LIMIT_CALIBRATE ePreviousStage;
	switch(eStage)
	{
		case eACTUATOR_LIMIT_CALIBRATE_CANCEL:
			bCalibratingLimitCurrent = FALSE;
			actuatorCommand(eACTUATOR_COMMAND_STOP);
			break;
		case eACTUATOR_LIMIT_CALIBRATE_BEGIN_UP:
			bCalibratingLimitCurrent = TRUE;
			startTimer(&calibratingLimitCurrentTimer, CALIBRATING_LIMIT_MAX_TIMEOUT);
			actuatorCommand(eACTUATOR_COMMAND_MOVE_UP);
			break;
		case eACTUATOR_LIMIT_CALIBRATE_BEGIN_DOWN:
			bCalibratingLimitCurrent = TRUE;
			startTimer(&calibratingLimitCurrentTimer, CALIBRATING_LIMIT_MAX_TIMEOUT);
			actuatorCommand(eACTUATOR_COMMAND_MOVE_DOWN);
			break;
		case eACTUATOR_LIMIT_CALIBRATE_GRAB_LIMIT:
			bCalibratingLimitCurrent = FALSE;
			switch(ePreviousStage)
			{
				case eACTUATOR_LIMIT_CALIBRATE_BEGIN_UP:
					nActuatorUpStopCurrent = nADS7828GetIA();
					storedConfigSetActuatorUpLimit(nActuatorUpStopCurrent);
					break;
				case eACTUATOR_LIMIT_CALIBRATE_BEGIN_DOWN:
					nActuatorDownStopCurrent = nADS7828GetIA();
					storedConfigSetActuatorDownLimit(nActuatorDownStopCurrent);
					break;
				case eACTUATOR_LIMIT_CALIBRATE_CANCEL:
				case eACTUATOR_LIMIT_CALIBRATE_GRAB_LIMIT:
					break;
			}
			actuatorCommand(eACTUATOR_COMMAND_STOP);
			break;
	}
	ePreviousStage = eStage;
}

