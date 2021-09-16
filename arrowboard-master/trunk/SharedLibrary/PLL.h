//PLL #defines


//PLLCFG
#define MSEL 0
#define NSEL 16


//PLLCON (These define PLL mode (off/active-no connect/active-connected)
#define PLLE 0	
#define PLLC 1

//PLLSTAT
#define PLLSTAT_PLLE 24
#define PLLSTAT_PLLC 25
#define PLLSTAT_PLOCK 26

///PLL mult = 35 = 0x23.  gives fcco=(2 x (M+1) x Fin)/N = (2*36*4000000)/1 = 288000000
#define PLL_MULT 0x23
// PLL div N = 4, reg holds N-1 = 3
// 288000000/3 = 96000000
// 288000000/4 = 72000000
#define PLL_DIV_72MHZ  0x03			// for 72000000
#define CCLKSEL_72MHZ  0x00																			
#define CCLKSEL_36MHZ  0x01			// This divides the PLL Output Signal in Half
																// In this case 72000000/(CCLKSEL + 1) = 36000000


void PLL_CONFIG(ePLATFORM_TYPE ePlatformType);
void PLL_CLOCK_SETUP(ePLATFORM_TYPE ePlatformType);
