// pwm.c setup and config routines for PWM peripheral

#include <LPC23xx.H>
#include <stdio.h>
#include "ArrowBoardDefs.h"
#include "pwm.h"
#include "commboard.h"


//*******
//*******
//*******
// rate = Hz (100 = 100Hz, duty = %, 30 = 30%
void pwmInit(int rate, int duty) {

	// set up peripheral clock = CCLK (12MHz)
	PCLKSEL0 |= (1 << PCLK_PWM1);
		
	// Set PWM pins
	// We want to use the PWM peripheral to generate 
	// the control signal for driving the piezo speaker.
	// This will produce a 50% duty, 4000Hz square wave. 	
		
	// set pin function for P1.21 (pwm1 channel 3) to pwm
	PINSEL3 |= (PWM1_3 << P1_21_PIN_SEL);	
		
	// set up match register.  
	// mr0 sets repetition rate.  MR2 sets pulse width if MR1 = 0.  Otherwise
	// MR1 sets leading edge and MR2 sets falling edge
	PWM1MR0 = 2*PWMCLK_72MHZ/rate;
	PWM1MR1 = 0x00; 
	
	// sense of output is inverted, so correct with (100 - duty)
	PWM1MR3 = (int)PWM1MR0*(100-duty)/100;

	// set up match control reg to reset on match with MR0
	// mr0 sets repetition rate.  MR3 sets pulse width
	PWM1MCR = (1 << PWMMR0R);

	// set match value latching
	PWM1LER = (1 << PWM_MATCH0_LATCH);

	// enable the output p1.21
	PWM1PCR = (1 << PWMENA1);
}


//*******
//*******
//*******
void pwmControl(int action)
{
	if(ON == action) 
	{
		// enable counter and timer 
		PWM1TCR = (1 << COUNTER_ENA) | (1 << PWM_ENA);
	}	
		
	if(OFF == action) 
	{
		// disable counter and timer 		
		PWM1TCR &= ~((1 << COUNTER_ENA) | (1 << PWM_ENA));
	}
}
