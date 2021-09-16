#include <stdlib.h>
#include <stdio.h>
#include "timer.h"
#include "queue.h"
#include "shareddefs.h"
#include "sharedinterface.h"
#include "lpc23xx.h"
#include "irq.h"

static ePLATFORM_TYPE ePlatformType;
static int nLEDCycles;
volatile static unsigned int nCurrentMilliseconds;
volatile static unsigned char nCurrentEpoch;
static BOOL driverBoardToggleLED()
{
#define uP_SYS_LED 15
	static BOOL bSysLED = FALSE;
	if(bSysLED)
	{
		FIO1CLR |= (1<<uP_SYS_LED);
		bSysLED = FALSE;
	}
	else
	{
		FIO1SET |= (1<<uP_SYS_LED);
		bSysLED = TRUE;
	}
	return bSysLED;
}

static BOOL handHeldToggleLED()
{
#define uP_DBG_LED 6
	static BOOL bSysLED = FALSE;
	if(bSysLED)
	{
		FIO0CLR |= (1<<uP_DBG_LED);
		bSysLED = FALSE;
	}
	else
	{
		FIO0SET |= (1<<uP_DBG_LED);
		bSysLED = TRUE;
	}
	return bSysLED;
}

BOOL timerToggleLED()
{
	BOOL bRetVal = FALSE;
	switch(ePlatformType)
	{
		case ePLATFORM_TYPE_HANDHELD:
			bRetVal = handHeldToggleLED();
			break;
		
		case ePLATFORM_TYPE_ARROW_BOARD:
			bRetVal = driverBoardToggleLED();
			break;
		
		case ePLATFORM_TYPE_REPEATER:		
			bRetVal = driverBoardToggleLED();
			break;
		
		default:
			break;
	}
	return bRetVal;
}

void T0Handler(void)  __irq
{

	// acknowledge interrupt
	T0IR = (1 << T0IR_MR3);

	// TOGGLE UNUSED I/O FOR TESTING
	// that the processor is alive.

	// 32b int msecs = 1,192h till rollover ~ 50days
	if(0 == ++nCurrentMilliseconds)
	{
		nCurrentEpoch++;
	}
	if(0 <= nLEDCycles)
	{
		if(0 >= --nLEDCycles)
		{
			timerToggleLED();
			nLEDCycles = 500;
		}
	}
	
	 VICVectAddr = 0;		/* Acknowledge Interrupt */
}

void T1Handler (void) __irq 
{ 	
	T1IR 	=  (1 << T1IR_MR3); // Clear match 1 interrupt 

	VICVectAddr = 0x00000000; // Dummy write to signal end of interrupt 
} 

//*********
//*********
//*********
//*********
//*********
// configure int to fire every x millisecs
void T0_SETUP_PERIODIC_INT(int desired_msecs)
{
	
		// turn on periph power
		PCONP |= (1 << PCTIM0);
			
		// set pclock
		PCLKSEL0 |= (CCLK_OVER_4 << PCLK_TIMER0);
		// set to INT and reset on match
			
		T0MCR |= (1 << MR3I) | (1 << MR3R);
		
		switch(ePlatformType)
		{
			case ePLATFORM_TYPE_ARROW_BOARD:
				// set match value to int 
				T0MR3 = (BASE_CLK_36MHZ)*(desired_msecs)/(4*1000);
				printf("ePLATFORM_TYPE_ARROW_BOARD: BASE_CLK_36MHZ[%d] desired_msecs[%d]\n", BASE_CLK_36MHZ, desired_msecs);
				break;
			
			case ePLATFORM_TYPE_HANDHELD:
				// set match value to int 
				T0MR3 = (BASE_CLK_72MHZ)*(desired_msecs)/(4*1000);
				printf("ePLATFORM_TYPE_HANDHELD: BASE_CLK_72MHZ[%d] desired_msecs[%d]\n", BASE_CLK_72MHZ, desired_msecs);
				break;
			case ePLATFORM_TYPE_REPEATER:
				// set match value to int 
				T0MR3 = (BASE_CLK_36MHZ)*(desired_msecs)/(4*1000);
				printf("ePLATFORM_TYPE_REPEATER: BASE_CLK_36MHZ[%d] desired_msecs[%d]\n", BASE_CLK_36MHZ, desired_msecs);
				break;
			default:
				break;
		}
		
		// install handler
		// not sure why void() (void) __irq is necessary.  NXP demo code called for (void(*))?
		install_irq( TIMER0_INT, (void (*)(void)__irq) T0Handler, LOWEST_PRIORITY );

		// start counter
		T0TCR = (1 << T0_ENA);
	
}

void T1_SETUP_PERIODIC_INT(int rate, int duty)
{	
		// turn on periph power
		PCONP |= (1 << PCTIM1);
		 	
		// set pclock
		PCLKSEL0 |= (CCLK_OVER_8 << PCLK_TIMER1);
		printf("PCLKSEL0[%08lX]\n",PCLKSEL0);
	
		printf("PINSEL4[%08lX]\n",PINSEL4); 

		// set to INT and reset on match			
		T1MCR |= (1 << MR3R) | (1 << MR3I);
		printf("T1MCR[%08lX]\n",T1MCR);
	
		//T1MR0 = (BASE_CLK)/(4*rate);
		T1MR0 = 0; //2*(12000000)/rate;
		printf("T1MR0[%08lX]\n",T1MR0);
	
		// set match value to int 
		T1MR1 |= 50; //(int)T1MR0*(100-duty)/100;
		printf("T1MR1[%08lX]\n",T1MR1);
	
		T1PR |= 50;
		printf("T1PR[%08lX]\n",T1PR);
	
		//T1MR1 = (BASE_CLK)*(desired_msecs)/(4*10000);
		//printf("BASE_CLK[%d] desired_msecs[%d]\n", BASE_CLK, desired_msecs);

		// install handler
		// not sure why void() (void) __irq is necessary.  NXP demo code called for (void(*))?
		install_irq( TIMER1_INT, (void (*)(void)__irq) T1Handler, LOWEST_PRIORITY );

		// start counter
		T1TCR |= (1 << T1_ENA);	
}


void initTimer(TIMERCTL* pTimer)
{
	pTimer->nTimeoutEpoch = 0;
	pTimer->nTimeoutTime = 0;
}

void timerShowLEDHeartbeat(void)
{
	nLEDCycles = 500;
}

void timerStopLEDHeartbeat()
{
	nLEDCycles = -1;
}

uint32_t getTimerNow()
{
	unsigned char nEpoch = nCurrentEpoch;
	unsigned int nMilliseconds = nCurrentMilliseconds;
	if(nEpoch != nCurrentEpoch)
	{
		/////
		// timer rolled over, so grab it again
		/////
		nEpoch = nCurrentEpoch;
		nMilliseconds = nCurrentMilliseconds;
	}
	return nMilliseconds;
}

void startTimer(TIMERCTL *pTimer, unsigned int nTimeoutMS)
{
	volatile unsigned char nEpoch = nCurrentEpoch;
	volatile unsigned int nMilliseconds = nCurrentMilliseconds;
	if(nEpoch != nCurrentEpoch)
	{
		/////
		// timer rolled over, so grab it again
		/////
		nEpoch = nCurrentEpoch;
		nMilliseconds = nCurrentMilliseconds;
	}
	pTimer->nTimeoutEpoch = nEpoch;
  while( nMilliseconds != nCurrentMilliseconds )
  {
    nMilliseconds = nCurrentMilliseconds;
  }
  pTimer->nTimeoutTime = nMilliseconds;
	if(0 < nTimeoutMS)
	{

		pTimer->nTimeoutTime += nTimeoutMS;
		if(pTimer->nTimeoutTime < nMilliseconds)
		{
			/////
			// our timer rolled over
			// so increment to next epoch
			/////
			pTimer->nTimeoutEpoch++;
		}
	}
}

BOOL isTimerExpired(TIMERCTL* pTimer)
{
	BOOL bRetVal = FALSE;
	volatile unsigned char nEpoch = nCurrentEpoch;
	volatile unsigned int nMilliseconds = nCurrentMilliseconds;

	if(nEpoch != nCurrentEpoch)
	{
		nEpoch = nCurrentEpoch;
		nMilliseconds = nCurrentMilliseconds;
	}
	if(pTimer->nTimeoutEpoch < nEpoch)
	{
		bRetVal = TRUE;
	}
	else if((pTimer->nTimeoutEpoch == nEpoch) && (pTimer->nTimeoutTime < nMilliseconds))
	{
		bRetVal = TRUE;
	}
	return bRetVal;
}

void stopTimer(TIMERCTL* pTimer)
{
	pTimer->nTimeoutEpoch = 0;
	pTimer->nTimeoutTime = 0;
}

void timerInit(ePLATFORM_TYPE ePlatform)
{
	ePlatformType = ePlatform;
	nCurrentMilliseconds = 0;
	nCurrentEpoch = 0;
	T0_SETUP_PERIODIC_INT(1);
	//T1_SETUP_PERIODIC_INT(5000,70);
}
/*****************************************************************************
** Function name:		delayMs
**
** Descriptions:		Start the timer delay in milliseconds
**									until elapsed
**
** parameters:			timer number, Delay value in millisecond			 
** 						
** Returned value:		None
** 
*****************************************************************************/
void delayMs(DWORD delayInMs)
{
	TIMERCTL delayTimer;
	initTimer(&delayTimer);
	if(0 >= delayInMs)
	{
		delayInMs = 1;
	}
	startTimer(&delayTimer, delayInMs);
	while(!isTimerExpired(&delayTimer));
}
