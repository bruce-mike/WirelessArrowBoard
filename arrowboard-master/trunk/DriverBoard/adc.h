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

#define ADC_MAX_CHANS	8					/* for LPC23xx */
#define ADC_CLK				1000000		/* set to 1Mhz */

#define ADVREF				(3.0)
#define VS_REF        (3.3) 	// REFERENCE FOR 'VBAT MONITOR' DIFF AMP 
#define NUMBITS				(10)	 	// conversion 'width' N bits (aka 10 bits /8 bits)

#define SOLAR_FACTOR	(11) 		// Multiplier to account for R divider on solar

#define SOLAR_CHARGE_MAX_THRESHOLD (1350) // MAX Solar Charge On Battery
#define SOLAR_CHARGE_MIN_THRESHOLD (1300) // MIN Solar Charge On Battery

static const float ADC_SCALE_FACTOR = (ADVREF/(1 << NUMBITS));
#define VB_CONSTANT			5.52
#define IL_CONSTANT			0.01621   	// full scale 16.6Amp:  16.6A/1024bits = 16.21mA/bit 
#define IS_CONSTANT			0.00859375  // full scale 8.8Amp    8.8A/1024bits = 8.59mA/bit
#define IACT_CONSTANT		0.03223			// full scale 33Amp			33A/1024bits = 32.3mA/bit

//jgrfloat  CONVERT_TO_VS   (int);							// system voltage from diff amp
float  CONVERT_TO_VPCELL (int);						// photo cell
//float  CONVERT_TO_VD   (int);							// lamp voltage 8V4/12V0

void  ADCInit(int boardRev);
void  ADCDoWork(void);
//jgrDWORD ADCGetVs(void);
DWORD ADCGetVl(void);
DWORD ADCGetVb(void);
DWORD ADCGetPr(void);



unsigned short ADCGetSystemVoltage(void);
unsigned short ADCGetLineCurrent(void);
unsigned short ADCGetSystemCurrent(void);
uint16_t ADCGetRssiVoltage( void );

#endif /* end __ADC_H */
/*****************************************************************************
**                            End Of File
******************************************************************************/
