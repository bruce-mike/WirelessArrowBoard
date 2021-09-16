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
static volatile BYTE ADC2IntDone = 0;
static volatile BYTE ADC3IntDone = 0;
static volatile BYTE ADC4IntDone = 0;
static volatile BYTE ADC5IntDone = 0;
static volatile BYTE ADC6IntDone = 0;
static volatile BYTE ADC7IntDone = 0;

#define ADC_Fpclk 48000000

#define SIZE_OF_MEDIAN_FILTER   5
#define MEDIAN_VALUE_INDEX      2

#define SIZE_OF_AVG_FILTER      8
#define DIVIDE_BY_EIGHT_SHIFT   3

#define DEFAULT_VOLTAGE         1200


float CONVERT_TO_VL(int adreturn);
float CONVERT_TO_IL(int adreturn);
float CONVERT_TO_IS(int adreturn);
float CONVERT_TO_VPCELL(int adreturn);

typedef struct {
  BYTE medianIndex;
  BYTE avgIndex;
  UINT avgValues[SIZE_OF_AVG_FILTER];
  UINT medianValues[SIZE_OF_MEDIAN_FILTER];
  UINT filteredValue;
} ADC_FILTER;


static ADC_FILTER lineVoltageFilter;
static ADC_FILTER photoCellVoltageFilter;


static void initAdcFilter(ADC_FILTER *filter)
{
   int i;
    
   filter->medianIndex = 0;
   filter->avgIndex = 0;
    
   for(i=0; i<SIZE_OF_MEDIAN_FILTER; i++)
   {
       filter->medianValues[i] = DEFAULT_VOLTAGE;
   }
   
   for(i=0; i<SIZE_OF_AVG_FILTER; i++)
   {
       filter->avgValues[i] = DEFAULT_VOLTAGE;
   }
}


static UINT getMedianValue(UINT medianValues[])
// sort array of values in ascending order
// return median value
{
    int i, j;
    UINT swapValue, numbers[SIZE_OF_MEDIAN_FILTER];

    for(i=0; i<SIZE_OF_MEDIAN_FILTER; i++)
    {
      numbers[i] = medianValues[i];
    }

    for (i=0; i<SIZE_OF_MEDIAN_FILTER; i++)
    {
        for (j=1; j<(SIZE_OF_MEDIAN_FILTER-1-i); j++)
        {
            if(numbers[j] > numbers[j+1])
            {
                swapValue = numbers[j];
                numbers[j] = numbers[j+1];
                numbers[j+1] = swapValue;
           }
        }
    }

    return numbers[MEDIAN_VALUE_INDEX];
}


static UINT adcFilter(ADC_FILTER *filter, UINT newValue)
// 1st stage - median filter (noise rejection)
// 2nd stage - avg values (smoothing)
{
    unsigned long sum = 0;
    int i;

    filter->medianValues[filter->medianIndex] = newValue;
    if(++filter->medianIndex > SIZE_OF_MEDIAN_FILTER)
    {
			filter->medianIndex = 0;
    }

    filter->avgValues[filter->avgIndex] = getMedianValue(&filter->medianValues[0]);   
    if(++filter->avgIndex >= SIZE_OF_AVG_FILTER)
    { 
       filter->avgIndex = 0;
    }

    for(i=0; i<SIZE_OF_AVG_FILTER; i++)
    {
      sum += filter->avgValues[i];
    }
		
    sum = sum/SIZE_OF_AVG_FILTER;
 
    return ((UINT)sum);  
}

unsigned short ADCGetLineVoltage()
{
	return lineVoltageFilter.filteredValue;
}

unsigned short ADCGetBatteryVoltage()
{
    return lineVoltageFilter.filteredValue;
}
	
unsigned short ADCGetLineCurrent()
{
	static unsigned short nAmps;

	float fAmps = CONVERT_TO_IL(ADC0Value[0]);
	nAmps = fAmps*100;
  //printf("System current %.2f\n",fAmps);

	return nAmps;
}
// +12V current
unsigned short ADCGetSystemCurrent()
{
	static unsigned short nAmps;

	float fAmps = CONVERT_TO_IS(ADC0Value[4]);
	nAmps = fAmps*100;
  //printf("System current %.2f\n",fAmps);

	return nAmps;
}



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
	
			case 0x04:
			regVal = AD0DR2;
			break;
	
			case 0x08:
			regVal = AD0DR3;
			break;
	
			case 0x10:
			regVal = AD0DR4;
			break;
	
			case 0x20:
			regVal = AD0DR5;
			break;
	
			case 0x40:
			regVal = AD0DR6;
			break;
	
			case 0x80:
			regVal = AD0DR7;
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
	
			case 0x04:
			ADC0Value[2] = ( AD0DR2 >> 6 ) & 0x3FF;
			ADC2IntDone = 1;
			break;
	
			case 0x08:
			ADC0Value[3] = ( AD0DR3 >> 6 ) & 0x3FF;
			ADC3IntDone = 1;
			break;
	
			case 0x10:
			ADC0Value[4] = ( AD0DR4 >> 6 ) & 0x3FF;
			ADC4IntDone = 1;
			break;
	
			case 0x20:
			ADC0Value[5] = ( AD0DR5 >> 6 ) & 0x3FF;
			ADC5IntDone = 1;
			break;
	
			case 0x40:
			ADC0Value[6] = ( AD0DR6 >> 6 ) & 0x3FF;
			ADC6IntDone = 1;
			break;
	
			case 0x80:
			ADC0Value[7] = ( AD0DR7 >> 6 ) & 0x3FF;
			ADC7IntDone = 1;
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
#ifdef PCBA_REV_K
	/* PINSEL0 is for port 0 pins 0:15
	 * PINSEL1 for port 0 pins 16:32, PINSEL2 for port 1 pins 0:15...etc.
	 * PCBA_REV_K has the following analog h/w set up 
	 * ADC0.0 = P0.23 = uP_ADC_Il		(Line current)
	 * ADC0.3 = P0.26 = uP_ADC_Pr  	(Photo cell voltage)
	 * ADC0.4 = P1.30 = uP_ADC_Is  	(+12 current) 
	 * ADC0.5 = P1.31 = uP_ADC_VS		(System voltage, all input voltage supplies)
	 */
	
	// Clear/GPIO P0.23, 24, 25, 26 = ADC0.0 .1 .2 .3
	PINSEL1 &= ~((3UL << 14) | (3UL << 16) | (3UL << 18) | (3UL << 20));	
	// Clear/GPIO P1.30, 31 = ADC0.4 .5
	PINSEL3 &= ~((3UL << 28) | (3UL << 30));		// Clear

	// Set pins for ADC
	PINSEL1 |= ((1UL << 14) | (1UL << 20));		// P0.23 = ADC0.0  P0.26 = ADC0.3
	PINSEL3 |= ((3UL << 28) | (3UL << 30));		// P1.30 = ADC0.4  P0.31 = ADC0.5

#else
  PINSEL0 |= 0x0F00 0000;	/* P0.12~13, A0.6~7, function 11 */	
  PINSEL1 &= ~0x003FC000;	/* P0.23~26, A0.0~3, function 01 */
  PINSEL1 |= 0x00154000;
  PINSEL3 |= 0xF0000000;	/* P1.30~31, A0.4~5, function 11 */
#endif	// PCBA_REV_K

  AD0CR = ( 0x01 << 0 ) | 	/* SEL=1,select channel 0~7 on ADC0 */
		( ( ADC_Fpclk / ADC_Clk - 1 ) << 8 ) |  /* CLKDIV = ADC_Fpclk / 1000000 - 1 */ 
		( 0 << 16 ) | 		/* BURST = 0, no BURST, software controlled */
		( 0 << 17 ) |  		/* CLKS = 0, 11 clocks/10 bits */
		( 1 << 21 ) |  		/* PDN = 1, normal operation */
		( 0 << 22 ) |  		/* TEST1:0 = 00 */
		( 0 << 24 ) |  		/* START = 0 A/D conversion stops */
		( 0 << 27 );		/* EDGE = 0 (CAP/MAT singal falling,trigger A/D conversion) */ 


  /* If POLLING, no need to do the following */
#if ADC_INTERRUPT_FLAG
  AD0INTEN = 0x0FF;		/* Enable all interrupts */
  if ( install_irq( ADC0_INT, ADC0Handler, HIGHEST_PRIORITY ) == FALSE )
  {
	return (FALSE);
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

float CONVERT_TO_VPCELL(int adreturn)
{
	float v;
	
	//Vb is fed to 4.02k/18.2k voltage divider = div 4.02/(4.02+18.2)
	//float K1 = 11.1;	 //FIXME: Magic #'s.  Belong in .h?
	float K1 = VB_CONSTANT;

	v = adreturn*K1*ADC_SCALE_FACTOR;

	return v;
}


// CONVERT TO LINE VOLTAGE
float CONVERT_TO_VL(int adreturn)
{
	float v;
	
	#if 1	/* Use linear regression of measured points */
	float K1 = .01605;
	v = K1*adreturn + 0.05594;
	#else
	//Vb is fed to 4.02k/18.2k voltage divider 
	float K1 = 11.2;	 //FIXME: Magic #'s.  Belong in .h?

	v = adreturn*K1*ADC_SCALE_FACTOR;
	#endif
	
	return v;
}


float CONVERT_TO_IL(int adreturn)
{
	float v = 0.0;
	
	#if 1	/* Use linear regresion */
	float K1 = 0.01836;
	#else	
	// 16.6A/1024bits = 16.21mA/bit 
	float K1 = IL_CONSTANT;
	#endif
	
	v = adreturn*K1;

	return v;
}
float CONVERT_TO_IS(int adreturn)
{
	float v = 0.0;
	
	#if 1	/* Use linear regresion */
	float K1 = 0.03049;
	#else
	//max 8.8 amp: 8.8A/1024bits = 8.59ma/bit
	float K1 = IS_CONSTANT;
	#endif
	
	v = adreturn*K1;

	return v;
}


//============================================
void ADCInit(int boardRev)
{

	
	memset((unsigned char*)ADC0Value, 0, sizeof(ADC0Value));
	ADCHWInit( ADC_CLK );
	

    initAdcFilter(&lineVoltageFilter);
    initAdcFilter(&photoCellVoltageFilter);

}


unsigned int adcReadCounter = 0;
/*
 * This is called every 50mSecs from the main loop.
 * Read ADC status register, determine if an ADC conversion is complete
 * If so read it, optionally filter/average and store it.
 * Determine next channel, then start the next conversion.
 */
void ADCDoWork()
{
	static BYTE nChannelNum = 0;
  float fValue;
	unsigned short nValue;
    

	DWORD regVal = *(volatile unsigned long *)(AD0_BASE_ADDR 
			+ ADC_OFFSET + ADC_INDEX * nChannelNum);
	
	/* read result of A/D conversion */
	if ( !(regVal & ADC_DONE) )	// NOTE: ADC_DONE is cleared in the interrupt after saving data
	{
		adcReadCounter++;
		
		if(1 == ADC0IntDone)
    {
			ADC0IntDone = 0;
			//fValue = CONVERT_TO_DB( ADC0Value[0] );
      //nValue = fValue*100;
      //lineVoltageFilter.filteredValue = adcFilter(&lineVoltageFilter, nValue);
			//printf("\n\t\t\tADC.0 %d\n\n",ADC0Value[0]);//jgr
    }
		
		if(1 == ADC1IntDone)
    {
			ADC1IntDone = 0;
    }
        
    if(1 == ADC2IntDone)
    {
			ADC2IntDone = 0;
    }
		
    if(1 == ADC3IntDone)
    {
			ADC3IntDone = 0;
      fValue = CONVERT_TO_VPCELL( ADC0Value[3] );
      nValue = fValue*100;
      photoCellVoltageFilter.filteredValue = adcFilter(&photoCellVoltageFilter, nValue);
    }
    
		if(1 == ADC4IntDone)
    {
			ADC4IntDone = 0;
    }		
		
		if(1 == ADC5IntDone)
    {
			ADC5IntDone = 0;
			fValue = CONVERT_TO_VL( ADC0Value[5] );
      nValue = (int32_t)(fValue*100.0);
      lineVoltageFilter.filteredValue = adcFilter(&lineVoltageFilter, nValue);
  		//printf("\n\nADC.5 [%d %d %d]\n\n",ADC0Value[5], nValue, lineVoltageFilter.filteredValue);//jgr
    }

		if(1 == ADC6IntDone)
    {
			ADC6IntDone = 0;
    }

		if(1 == ADC7IntDone)
    {
			ADC7IntDone = 0;
    }

		/////
		// previous conversion is complete
		// data was placed in ADC0Value[nChannelNum]
		/////

		/////
		// figure out which is the next channel to read
		// we have only 3 channels that are used, Vline, Vbattery, Photo cell
		/////
        
    switch(nChannelNum)
    {
			case 0:
				nChannelNum = 3;
        break;
			case 3:
				nChannelNum = 4;
        break;
			case 4:
				nChannelNum = 5;
        break;
      case 5:
				nChannelNum = 0;               
        break;
      default: 
				nChannelNum = 0;
        break;
    }
        
		/////
		// start the current conversion
		/////
		ADC0Read(nChannelNum);
		
	}
}

/////
// Supplt voltage, battery, alternator, etc.
/////
DWORD ADCGetVl()
{
//printf("ADC[1]: %ld\r\n", ADC0Value[1]);
	return ADC0Value[5];
}

DWORD ADCGetVb()
{
//printf("ADC[2]: %ld\r\n", ADC0Value[2]);
	return ADC0Value[5];
}


DWORD ADCGetIs( void )
{
  //printf("ADC[4]: %ld\r\n", ADC0Value[4]);
	
	return ADC0Value[0];
}
DWORD ADCGetPr()
{
	return ADC0Value[3];
}

unsigned short ADCGetSystemVoltage()
{
	return lineVoltageFilter.filteredValue;
}
uint16_t ADCGetRssiVoltage( void )
{
	return( ADC0Value[0] );
}
//============================================
/*********************************************************************************
**                            End Of File
*********************************************************************************/

