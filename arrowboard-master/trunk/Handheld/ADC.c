/*****************************************************************************
 *   adc.c:  ADC module file for NXP LPC23xx Family Microprocessors
 *
 *   Copyright(C) 2006, NXP Semiconductor
 *   All rights reserved.
 *
 *   History
 *   2006.08.15  ver 1.00    Prelimnary version, first Release
 *
******************************************************************************/
#include "LPC23xx.h"                        /* LPC23xx definitions */
#include <stdio.h>
#include <string.h>
#include "shareddefs.h"
#include "sharedinterface.h"
#include "timer.h"
#include "queue.h"

#include "adc.h"
#include <irq.h>


volatile DWORD ADC0Value[ADC_MAX_CHANS];

static volatile BYTE ADC0IntDone = 0;
static volatile BYTE ADC1IntDone = 0;

#define ADC_CHANNEL_RSSI_VOLTAGE    1 


#define ADC_SAMPLE_TIME_MS  1000


//static int nBoardRev;

#define ADC_Fpclk 48000000

static TIMERCTL adcSampleTimer;


#if ADC_INTERRUPT_FLAG
/******************************************************************************
** Function name:		ADC0Handler
**
** Descriptions:		ADC0 interrupt handler
**
** parameters:			None
** Returned value:		None
** 
******************************************************************************/
static void ADC0Handler (void) __irq 
{
  DWORD regVal;
  
  regVal = AD0STAT;		/* Read ADC will clear the interrupt */
	
  if ( regVal & 0x0000FF00 )	/* check OVERRUN error first */
  {
		printf("ADC Overrun\n");
		regVal = (regVal & 0x0000FF00) >> 0x08;
		/* if overrun, just read ADDR to clear */
		/* regVal variable has been reused. */
		switch ( regVal )
		{
			case 0x01:
			regVal = AD0DR0;
			break;
	
			case 0x02:
			regVal = AD0DR1;
			break;
	
			default:
			break;
		}
  }
    
  if ( regVal & ADC_ADINT )
  {
		switch ( regVal & 0xFF )	/* check DONE bit */
		{
			case 0x01:
			ADC0Value[0] = ( AD0DR0 >> 6 ) & 0x3FF;
			ADC0IntDone = 1;
			break;
	
			case 0x02:
			ADC0Value[1] = ( AD0DR1 >> 6 ) & 0x3FF;
			ADC1IntDone = 1;
			break;
	
			default:
			break;
		}
	}
	
  AD0CR &= 0xF8FFFFFF;	/* stop ADC now */ 
  VICVectAddr = 0;		/* Acknowledge Interrupt */
}
#endif

/*****************************************************************************
** Function name:		ADCHWInit
**
** Descriptions:		initialize ADC channel
**
** parameters:			ADC clock rate
** Returned value:		true or false
** 
*****************************************************************************/

static DWORD ADCHWInit( DWORD ADC_Clk )
{
  /* Enable CLOCK into ADC controller */
  PCONP |= (1 << 12);

	/* PINSEL0 is for port 0 pins 0:15
	 * PINSEL1 for port 0 pins 16:32, PINSEL2 for port 1 pins 0:15...etc.
	 * PCBA_REV_K & above has the following analog h/w set up 
	 * ADC0.0 = P0.23 
	 * ADC0.1 - P0.24 - RSSI
	 * ADC0.2 - P0.25
	 * ADC0.3 = P0.26 
	 * ADC0.4 = P1.30 
	 * ADC0.5 = P1.31 
	 */
	
	// Clear/GPIO P0.23, 24, 25, 26 = ADC0.0 .1 .2 .3
	PINSEL1 &= ~((3UL << 14) | (3UL << 16) | (3UL << 18) | (3UL << 20));	
	// Clear/GPIO P1.30, 31 = ADC0.4 .5
	PINSEL3 &= ~((3UL << 28) | (3UL << 30));		// Clear

	// Set pins for ADC inputs/function
	// P0.24 = ADC0.1  
	PINSEL1 |= (1UL << 16);		
	
  AD0CR = ( 0x01 << 0 ) | 	/* SEL=1,select channel 0~7 on ADC0 */
		( ( ADC_Fpclk / ADC_Clk - 1 ) << 8 ) |  /* CLKDIV = ADC_Fpclk / 1000000 - 1 */ 
		( 0 << 16 ) | 		/* BURST = 0, no BURST, software controlled */
		( 0 << 17 ) |  		/* CLKS = 0, 11 clocks/10 bits */
		( 1 << 21 ) |  		/* PDN = 1, normal operation */
		( 0 << 22 ) |  		/* TEST1:0 = 00 */
		( 0 << 24 ) |  		/* START = 0 A/D conversion stops */
		( 0 << 27 );			/* EDGE = 0 (CAP/MAT singal falling,trigger A/D conversion) */ 


  /* If POLLING, no need to do the following */
#if ADC_INTERRUPT_FLAG
  AD0INTEN = 0x0FF;		/* Enable all interrupts */
  if ( install_irq( ADC0_INT, ADC0Handler, HIGHEST_PRIORITY ) == FALSE )
  {
		printf("ADC_INT: Failed to Install!\n");
		return (FALSE);
  }
	else
	{
		printf("ADC_INT: Installed Successfully!\n");
	}
#endif
  return (TRUE);
}

/*****************************************************************************
** Function name:		ADC0Read
**
** Descriptions:		If ADC_INTERRUPT_FLAG = 0 this will start ADC0 on
**									the passed in channel, then wait for completion of
**									the conversion, returns the ADC data
** 									If ADC_INTERRUPT_FLAG = 1 this will start ADC0 on
**									the passed in channel, then return.  Interrupt
**									handler will fill in ADC0Value[X] &	ADCXIntDone
**
** parameters:			Channel number
** Returned value:		Value read, if interrupt driven, return channel #
** 
*****************************************************************************/
static DWORD ADC0Read( BYTE channelNum )
{
#if !ADC_INTERRUPT_FLAG
  DWORD regVal, ADC_Data;
#endif

  /* channel number is 0 through 7 */
  if ( channelNum >= ADC_MAX_CHANS )
  {
		channelNum = 0;		/* reset channel number to 0 */
  }
  AD0CR &= 0xFFFFFF00;											// Clear out channel mask
  AD0CR |= (1 << 24) | (1 << channelNum);		// Select next channel, Start ADC conversion

#if !ADC_INTERRUPT_FLAG
  while ( 1 )			/* wait until end of A/D convert */
  {
	regVal = *(volatile unsigned long *)(AD0_BASE_ADDR 
			+ ADC_OFFSET + ADC_INDEX * channelNum);
	/* read result of A/D conversion */
	if ( regVal & ADC_DONE )
	{
	  break;
	}
  }	
        
  AD0CR &= 0xF8FFFFFF;	/* stop ADC now */    
  if ( regVal & ADC_OVERRUN )	/* save data when it's not overrun, otherwise, return zero */
  {
	return ( 0 );
  }

  ADC_Data = ( regVal >> 6 ) & 0x3FF;
  return ( ADC_Data );    	/* return A/D conversion value */
#else
  return ( channelNum );	/* if it's interrupt driven, the ADC reading is 
							done inside the handler. so, return channel number */
#endif
}

//============================================
void ADCInit(int boardRev)
{
//	nBoardRev = boardRev;
    
  initTimer(&adcSampleTimer);
	
	memset((unsigned char*)ADC0Value, 0, sizeof(ADC0Value));
	ADCHWInit( ADC_CLK );
	
  startTimer(&adcSampleTimer, ADC_SAMPLE_TIME_MS);
}
/*
 * This is called every xxmSecs from the main loop.
 * Read ADC status register, determine if an ADC conversion is complete
 * If so read it, optionally filter/average and store it.
 * Determine next channel, then start the next conversion.
 */
void ADCDoWork()
{
	static BYTE nChannelNum = 1;
    

	DWORD regVal = *(volatile unsigned long *)(AD0_BASE_ADDR 
			+ ADC_OFFSET + ADC_INDEX * nChannelNum);
	
	/* read result of A/D conversion */
    if(!(regVal & ADC_DONE))    // NOTE: ADC_DONE is cleared in the interrupt after saving data
    {	
        if(1 == ADC0IntDone)
        {
					ADC0IntDone = 0;
        }
        else if(1 == ADC1IntDone)
        {
					ADC1IntDone = 0;
        }
				else
				{
					printf("Unknown done 0x%X\n", regVal);
				}
        
        // Previous conversion is complete and
        // data was placed in ADC0Value[nChannelNum]
	
        // Determine which channel to start conversion
				nChannelNum = ADC_CHANNEL_RSSI_VOLTAGE;
    }
    
    if(isTimerExpired(&adcSampleTimer))
    {
			ADC0Read(nChannelNum);	// This starts ADC conversion on nChannelNum
      startTimer(&adcSampleTimer, ADC_SAMPLE_TIME_MS);
    }
}

uint16_t ADCGetRssiVoltage( void )
{
    return( ADC0Value[ADC_CHANNEL_RSSI_VOLTAGE] );
}

//============================================
/*********************************************************************************
**                            End Of File
*********************************************************************************/

