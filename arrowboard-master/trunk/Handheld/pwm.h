// PWM.H

//LPC2366 PWM setup and defs

#define PCPWM1	       6			// power control reg bit location
#define PCLK_PWM1     12			// clock control reg bit location (2-bits)
#define PWM1_3        10			// select pin function PWM
#define GPIO					00      // select pin function GPIO
#define P1_21_PIN_SEL 10			// pin function bit location in PINSEL4
#define P1_21_MODE_SEL  2			// pin mode function bit location in PINMODE3
#define PULL_UP				00		  // mode select bit(s) value in PINMODE
#define PWMMR0R        1		  // bit location in pWM1MCR
#define PWMENA1				 11			// bit location in PWM1PCR
#define PWM_MATCH0_LATCH 0		// bit location in PWM1LER


#define COUNTER_ENA      0    // PWM1TCR bit location
#define PWM_ENA					 3    // ditto


// this should be def'd elsewhere? With CCLK and PLL dividers?
#define PWMCLK_96MHZ		BASE_CLK_96MHZ // for MR0/rate configuration
#define PWMCLK_72MHZ		BASE_CLK_72MHZ // for MR0/rate configuration
#define PWMCLK_36MHZ		BASE_CLK_36MHZ // for MR0/rate configuration



// *******
// func defs

// rate = Hz base freq.  Duty = %.  e.g SETUP...(100, 50) is 100Hz, 50% duty
void pwmInit(int rate, int duty);
void pwmControl(int);


