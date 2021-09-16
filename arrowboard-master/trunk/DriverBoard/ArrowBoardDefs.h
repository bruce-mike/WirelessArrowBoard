#ifndef ARROWBOARD_DEFS_H
#define ARROWBOARD_DEFS_H


#define ARROW_BOARD_CONTROLLER

/////
// nvdata ids
/////
#define FLASH_SENDERS_ESN_ID 1

//DATA DIRECTIONS/pin modes
#define IN			0
#define OUT			1


// **********************
// PORT 0 BIT DEFS
// **********************
#define LIMIT_SW_A		0
#define LIMIT_SW_B		1
#define uP_DBG_TXD 		2
#define uP_DBG_RXD		3
#define uP_W85_DEN 		4
#define uP_W85_xRE		5
#define uP_SPI1_xCS 	6
#define uP_SPI1_SCK		7
#define uP_SPI1_MISO 	8
#define uP_SPI1_MOSI	9
#define uP_W85_TXD		10
#define uP_W85_RXD		11
// UNAVAILABLE (P.104)
// UNAVAILABLE (P.104)
// UNAVAILABLE (P.104)
#define uP_1_TXD			15
#define uP_1_RXD			16


// 22 UNUSED/NC
#define uP_ADC_VS			23
#define uP_ADC_VL			24
#define uP_ACD_VB			25
#define uP_ADC_VD			26
#define I2C_SDA				27
#define I2C_SCL				28
#define uP_USB_DP			29
#define uP_USB_DN			30
// 31 UNAVAILABLE (P.104)

// **********************
// PORT 1 BIT DEFS
// **********************
#define REV0 0
#define REV1 1
// 2 UNAVAILABLE (P.107)
// 3 UNAVAILABLE (P.107)
#define REV2 4
// 5 UNAVAILABLE (P.107)
// 6 UNAVAILABLE (P.107)
// 7 UNAVAILABLE (P.107)
// 8 UNUSED/NC
// 9 UNUSED/NC
// 10 UNUSED/NC
// 11 UNAVAILABLE (P.107)
// 12 UNAVAILABLE (P.107)
// 13 UNAVAILABLE (P.107)
// 14 UNUSED/NC
#define uP_SYS_LED		15
// 16 UNUSED/NC
// 17 UNUSED/NC
#define uP_USB_LED		18		// kind of unused output Rev 0
#define WD_KICK				18		// hardware watchdog Rev 1(Rev H) and beyond
#define uP_MOD_X1			19		// model select
#define uP_MOD_X2			22		// ditto
#define uP_MOD_X4			25		// ditto
#define uP_MOD_X8			27		// ditto
#define ISP_xRST			26
//#define uP_PWR_USER		19		// user/aux power
#define uP_POS_VL			20		// solar polarity
#define uP_POS_VB			21		// battery polarity
//#define uP_PWR_HOUR		22		// hourmeter power
#define uP_OVP_VL			23		// force 4363 on out of OVLO
//24 UNUSED/NC
//#define uP_PWR_LBAR		25		// lightbar
// 26 UNUSED/NC
//#define uP_PWR_STRB		27		// strobe
#define uP_PWR_VL			28		// solar charger 4363 enable
#define uP_PWR_VB			29		// battery 4365 enable
#define uP_ADC_PF			30
#define uP_ADC_PR			31
// **********************
// PORT 2 BIT DEFS
// **********************

#define uP_PWM_LED		0
#define uP_PWM_DRV		1
#define uP_IOK_xACTA	2
#define uP_IOK_xACTB	3
#define uP_PWM_xACT		5
#define uP_ENA_xACT		4

#define uP_PWR_xACTA	6
#define uP_PWR_xACTB	7
#define uP_PWR_x8V4		8			// 8.4/12V rail select
#define uP_PWR_xSHDN	9			// LVD 
#define uP_xISP				10		// unused (except for ISP)
#define uP_INT_xTMP		11		//temp sensor interrupt (if programmed)
#define uP_INT_xDIN		12		// misc error and actuator
#define uP_INT_xERR		13		// output channel errors
// 14 UNAVAILABLE (P.109)
// 15 UNAVAILABLE (P.109)
// 16 UNAVAILABLE (P.109)
// 17 UNAVAILABLE (P.109)
// 18 UNAVAILABLE (P.109)
// 19 UNAVAILABLE (P.109)
// 20 UNAVAILABLE (P.109)
// 21 UNAVAILABLE (P.109)
// 22 UNAVAILABLE (P.109)
// 23 UNAVAILABLE (P.109)
// 24 UNAVAILABLE (P.109)
// 25 UNAVAILABLE (P.109)
// 26 UNAVAILABLE (P.109)
// 27 UNAVAILABLE (P.109)
// 28 UNAVAILABLE (P.109)
// 29 UNAVAILABLE (P.109)
// 30 UNAVAILABLE (P.109)
// 31 UNAVAILABLE (P.109)


// **********************
// PORT 3
// **********************

// 0  UNAVAILABLE (P.111)
// 1  UNAVAILABLE (P.111)
// 2  UNAVAILABLE (P.111)
// 3  UNAVAILABLE (P.111)
// 4  UNAVAILABLE (P.111)
// 5  UNAVAILABLE (P.111)
// 6  UNAVAILABLE (P.111)
// 7  UNAVAILABLE (P.111)
// 8  UNAVAILABLE (P.111)
// 9  UNAVAILABLE (P.111)
// 10 UNAVAILABLE (P.111)
// 11 UNAVAILABLE (P.111)
// 12 UNAVAILABLE (P.111)
// 13 UNAVAILABLE (P.111)
// 14 UNAVAILABLE (P.111)
// 15 UNAVAILABLE (P.111)
// 16 UNAVAILABLE (P.111)
// 17 UNAVAILABLE (P.111)
// 18 UNAVAILABLE (P.111)
// 19 UNAVAILABLE (P.111)
// 20 UNAVAILABLE (P.111)
// 21 UNAVAILABLE (P.111)
// 22 UNAVAILABLE (P.111)
// 23 UNAVAILABLE (P.111)
// 25 UNAVAILABLE (P.111)
#define ACT_VL_ENA		25
#define uP_3_PWM			26
// 27 UNAVAILABLE (P.111)
// 28 UNAVAILABLE (P.111)
// 29 UNAVAILABLE (P.111)
// 30 UNAVAILABLE (P.111)
// 31 UNAVAILABLE (P.111)

// **********************
// PORT 4
// **********************

// 0  UNAVAILABLE (P.111)
// 1  UNAVAILABLE (P.111)
// 2  UNAVAILABLE (P.111)
// 3  UNAVAILABLE (P.111)
// 4  UNAVAILABLE (P.111)
// 5  UNAVAILABLE (P.111)
// 6  UNAVAILABLE (P.111)
// 7  UNAVAILABLE (P.111)
// 8  UNAVAILABLE (P.111)
// 9  UNAVAILABLE (P.111)
// 10 UNAVAILABLE (P.111)
// 11 UNAVAILABLE (P.111)
// 12 UNAVAILABLE (P.111)
// 13 UNAVAILABLE (P.111)
// 14 UNAVAILABLE (P.111)
// 15 UNAVAILABLE (P.111)
// 16 UNAVAILABLE (P.111)
// 17 UNAVAILABLE (P.111)
// 18 UNAVAILABLE (P.111)
// 19 UNAVAILABLE (P.111)
// 20 UNAVAILABLE (P.111)
// 21 UNAVAILABLE (P.111)
// 22 UNAVAILABLE (P.111)
// 23 UNAVAILABLE (P.111)
// 25 UNAVAILABLE (P.111)
// 26 UNAVAILABLE (P.111)
// 27 UNAVAILABLE (P.111)
#define uP_SPI1_HOLD 28
#define uP_SPI1_WP   29
//#define uP_3_TXD			28
//#define uP_3_RXD			29
// 30 UNAVAILABLE (P.111)
// 31 UNAVAILABLE (P.111)
#endif	// ARROWBOARD_DEFS_H
