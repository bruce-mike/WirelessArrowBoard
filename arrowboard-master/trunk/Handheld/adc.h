/*****************************************************************************
 *   adc.h:  Header file for NXP LPC23xx Family Microprocessors
 *
 *   Copyright(C) 2006, NXP Semiconductor
 *   All rights reserved.
 *
 *   History
 *   2006.09.20  ver 1.00    Prelimnary version, first Release
 *
******************************************************************************/
#ifndef __ADC_H 
#define __ADC_H

#define ADC_INTERRUPT_FLAG	1	/* 1 is interrupt driven, 0 is polling */

#define ADC_OFFSET		0x10
#define ADC_INDEX			4

#define ADC_DONE			0x80000000
#define ADC_OVERRUN		0x40000000
#define ADC_ADINT			0x00010000

#define ADC_MAX_CHANS	2				
#define ADC_CLK				1000000		/* set to 1Mhz */

#define ADVREF				(3.0)
#define VS_REF        (3.3) 	// REFERENCE FOR 'VBAT MONITOR' DIFF AMP 
#define NUMBITS				(10)	 	// conversion 'width' N bits (aka 10 bits /8 bits)

void  ADCInit(int boardRev);
void  ADCDoWork(void);
uint16_t ADCGetRssiVoltage( void );

#endif /* end __ADC_H */
/*****************************************************************************
**                            End Of File
******************************************************************************/
