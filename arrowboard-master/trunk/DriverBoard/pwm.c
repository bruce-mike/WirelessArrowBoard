// pwm.c setup and config routines for PWM peripheral

#include <LPC23xx.H>
#include <stdio.h>
#include "shareddefs.h"
#include "sharedinterface.h"
#include "ArrowBoardDefs.h"
#include "pwm.h"
#include "actuator.h"
#include "adc.h"

#define PHOTOCELL_MAX_SAMPLES 100
#define MAX_PHOTOCELL_VALUE 1024

static BOOL autoBrightnessModeOn = FALSE;
static DWORD nPhotocellData[PHOTOCELL_MAX_SAMPLES];
static int nPhotocellSampleIndex, i;

//*******
//*******
//*******
// rate = Hz base freq.  Duty = %.  e.g SETUP...(100, 50) is 100Hz, 50% duty
void pwmLampDriverSetup(int rate, int duty) 
{

	// power-up device
	PCONP |= (1 << PCPWM1);

	// set up peripheral clock = CCLK (12MHz)
	PCLKSEL0 |= (1 << PCLK_PWM1);
		
	// set PWM pins and modes
	// we would like to use the PWM peripheral to generate 
	// the control signal for driver I2C chips.  This will be
	// a 50% duty, 100Hz square wave for dim, or steady-on for full-bright.
	// this signal is modulated by timer0 to produce blinking with
	// period of 1.875s	
		
	// set pin function for P2.1 (pwm1 channel 2) to pwm
	PINSEL4 |= (PWM1_2 << P2_1_PIN_SEL);	
	printf("pwmLampDriverSetup: PINSEL4[%08X]\n",PINSEL4);
		
	// TURN ON PULL UP unnecessary - pin is output	

	PINMODE4 |= (PULL_UP << P2_1_MODE_SEL);
		
	// set up match register.  
	// mr0 sets repetition rate.  MR2 sets pulse width if MR1 = 0.  Otherwise
	// MR1 sets leading edge and MR2 sets falling edge
	PWM1MR0 = 2*PWMCLK_36MHZ/rate;
	PWM1MR1 = 0x00; 
	
	// sense of output is inverted, so correct with (100 - duty)
	PWM1MR2 = (int)PWM1MR0*(100-duty)/100;  // = MR0/2

	// set up match control reg to reset on match with MR0
	// mr0 sets repetition rate.  MR2 sets pulse width
	PWM1MCR = (1 << PWMMR0R);

	// set match value latching
	PWM1LER |= (1 << PWM_MATCH2_LATCH);

	// enable the output p2.1
	PWM1PCR |= (1 << PWMENA2);

}

void pwmActuatorDriverSetup(int rate, int duty) 
{

	// power-up device
	PCONP |= (1 << PCPWM1);

	// set up peripheral clock = CCLK (12MHz)
	PCLKSEL0 |= (1 << PCLK_PWM1);
		
	// set PWM pins and modes
	// we would like to use the PWM peripheral to generate 
	// the control signal for driver I2C chips.  This will be
	// a 50% duty, 100Hz square wave for dim, or steady-on for full-bright.
	// this signal is modulated by timer0 to produce blinking with
	// period of 1.875s	
	/* New rev K PWM may or may not be used. */
	#if 0
	// set pin function for P2.5 (pwm1 channel 6) to pwm
	PINSEL4 |= (PWM1_6 << P2_5_PIN_SEL);
	#else
	PINSEL4 |= (GPIO << P2_5_PIN_SEL);
	#endif

	//printf("pwmActuatorDriverSetup: PINSEL4[%08X]\n",PINSEL4);
	
	// set up match register.  
	// mr0 sets repetition rate.  MR2 sets pulse width if MR1 = 0.  Otherwise
	// MR1 sets leading edge and MR2 sets falling edge
	PWM1MR0 = 2*PWMCLK_36MHZ/rate;
	PWM1MR1 = 0x00; 
	
	// sense of output is inverted, so correct with (100 - duty)
	PWM1MR5 = (int)PWM1MR0*(100-duty)/100;

	// set up match control reg to reset on match with MR0
	// mr0 sets repetition rate.  MR2 sets pulse width
	PWM1MCR = (1 << PWMMR0R);

	// set match value latching
	PWM1LER |= (1 << PWM_MATCH5_LATCH);

	// enable the output p2.4 (pwm1 channel 5)
	PWM1PCR |= (1 << PWMENA5);  
}

void pwmLampDriverSetDutyCycle(int duty)
{
	if(duty < 1)
	{
		duty = 1;
	}
	PWM1MR2 = (int)PWM1MR0*(100-duty)/100;
	//printf("PWM1MR2[%08X] duty[%d]\n",PWM1MR2,duty);
	// After assigning the new duty cycle to PWM1MR2, you must set the PWM1LER bit (PWM_MATCH2_LATCH)
  // to latch the newvalue into the PWM counter register upon the next period elapsing.	
	PWM1LER |= (1 << PWM_MATCH2_LATCH);
}

void pwmActuatorDriverSetDutyCycle(int duty)
{
	if(duty < 1)
	{
		duty = 1;
	}
	PWM1MR5 = (int)PWM1MR0*(100-duty)/100;
	// After assigning the new duty cycle to PWM1MR5, you must set the PWM1LER bit (PWM_MATCH5_LATCH)
  // to latch the newvalue into the PWM counter register upon the next period elapsing.	
	PWM1LER |= (1 << PWM_MATCH5_LATCH);
}

void pwmInit()
{
	////////////////////////////////////////////////////////////////
	//// Config PWM Peripheral for 'rate' (Hz) and 'duty' cycle (%) 
	////////////////////////////////////////////////////////////////
	pwmLampDriverSetup(100, 50);
	pwmActuatorDriverSetup(5000,50);
	
	////////////////////////////////////////
	//// Set Master PWM Counter and Enable
	////////////////////////////////////////
	PWM1TCR = (1 << COUNTER_ENA) | (1 << PWM_ENA);
	pwmSetDriver(PWM_ON);
	
	/////////////////////////////////////////
	//// Initialize the Photocell Data 
	/////////////////////////////////////////
	for(i=0; i<PHOTOCELL_MAX_SAMPLES; i++)
	{
		nPhotocellData[i] = MAX_PHOTOCELL_VALUE;
	}
	nPhotocellSampleIndex = 0;
}

//*******
//*******
//*******
static int eCurrentAction = PWM_OFF;
void pwmSetDriver(int action)
{
		switch(action)
		{
			case PWM_ON:
				{
					//printf("\npwmSetDriver ON\n");//jgr
					// set pin function for P2.1 (pwm1 channel 2) to pwm
					PINSEL4 |= (PWM1_2 << P2_1_PIN_SEL);
					// enable counter and timer 
					eCurrentAction = action;
				}	
				break;
			case PWM_OFF:
				{
										//printf("\npwmSetDriver OFF\n");//jgr

					// raise xOE
					// set pin function to GPIO
					PINSEL4 &= ~(PWM1_2 << P2_1_PIN_SEL);
					FIO2SET = ( 1 << uP_PWM_DRV ) ;		// disable I2C driver for system LEDs
					eCurrentAction = action;
				}
				break;
			case PWM_FULL_BRIGHT:
				{
										printf("\npwmSetDriver FULL ON\n");//jgr

					// set pin to GPIO
					PINSEL4 &= ~(GPIO << P2_1_PIN_SEL);
					// lower xOE
					FIO2CLR = (1 << uP_PWM_DRV);
					eCurrentAction = action;
				}
				break;
			case PWM_TOGGLE:
				{
										printf("\npwmSetDriver TOGGLE\n");//jgr

					FIO2PIN ^= (1 << uP_PWM_DRV);
				}
				break;
			case PWM_SUSPEND:
									//printf("\npwmSetDriver SUSPEND\n");//jgr

				PINSEL4 &= ~(PWM1_2 << P2_1_PIN_SEL);
			#if 1  //jgr
				FIO2SET = ( 1 << uP_PWM_DRV ) ;		// disable I2C driver for system LEDs
			#else
			FIO2CLR = ( 1 << uP_PWM_DRV ) ;//jgr
			#endif
				break;
			case PWM_RESUME:
									//printf("\npwmSetDriver RESUME\n");//jgr

				pwmSetDriver(eCurrentAction);
				break;
		}
}

void pwmSetLampBrightnessControl(eBRIGHTNESS_CONTROL brightness)
{
	autoBrightnessModeOn = FALSE;

	switch(brightness)
	{
		case eBRIGHTNESS_CONTROL_AUTO:
		case eBRIGHTNESS_CONTROL_NONE:	
		default:
			autoBrightnessModeOn = TRUE;
			break;
		case eBRIGHTNESS_CONTROL_MANUAL_BRIGHT:
			pwmLampDriverSetDutyCycle(PWM_FULL_BRIGHT_DUTY);
			break;
		case eBRIGHTNESS_CONTROL_MANUAL_MEDIUM:
			pwmLampDriverSetDutyCycle(PWM_MEDIUM_BRIGHT_DUTY);
			break;
		case eBRIGHTNESS_CONTROL_MANUAL_DIM:
			pwmLampDriverSetDutyCycle(PWM_DIM_BRIGHT_DUTY);
			break;
	}
}

void pwmDoWork(void)
{
	int i;
	int nAverage = 0;
	int nPercent = 0;
	
	if(autoBrightnessModeOn)
	{		
		/////
		// process photocell data
		/////
		nPhotocellData[nPhotocellSampleIndex++] = ADCGetPr();
		
		if(PHOTOCELL_MAX_SAMPLES <= nPhotocellSampleIndex)
		{
			nPhotocellSampleIndex = 0;
		}
		
		for(i=0; i<PHOTOCELL_MAX_SAMPLES; i++)
		{
				nAverage += nPhotocellData[i];
		}
		
		nAverage /= PHOTOCELL_MAX_SAMPLES;

		nPercent = 100-((100*(MAX_PHOTOCELL_VALUE-nAverage))/MAX_PHOTOCELL_VALUE);

		if(PWM_MINIMUM_BRIGHT_PERCENT > nPercent)
		{
			nPercent = PWM_MINIMUM_BRIGHT_PERCENT;
		}

		pwmLampDriverSetDutyCycle(nPercent);
	}		
}



