// commboard.h - design specific uC pin defs
// this file can be reused and names changed to match
// the schematic
#include "shareddefs.h"
#include "sharedinterface.h"


//DATA DIRECTIONS/pin modes
#define IN			0
#define OUT			1


// MISC DEFS
#define OFF 				0
#define ON 					1
#define TOGGLE 			2
#define FULL_BRIGHT 3
#define BLINK       4


// ADC CHANNEL DEFS
#define ADC_VS			0



// **********************
// PORT 0 BIT DEFS
// **********************
// 0 UNUSED
// 1 UNUSED
#define uP_DBG_TXD 		2		// UART 0
#define uP_DBG_RXD		3   // UART0
#define uP_W85_DEN 		4		// RS-485 UART
#define uP_W85_xRE		5		// RS-485 UART
#define uP_DBG_LED		6
// 7 UNUSED
// 8 UNUSED
// 9 UNUSED
#define uP_W85_TXD		10	// RJ-45 RS-485 UART2
#define uP_W85_RXD		11
// UNAVAILABLE (P.104)
// UNAVAILABLE (P.104)
// UNAVAILABLE (P.104)
#define uP_SPI0_SCK		 15		
#define uP_SPI0_CS		 16		
#define uP_SPI0_MISO	 17
#define uP_SPI0_MOSI	 18
#define uP_SPI0_WP		 19
#define uP_SPI0_HOLD	 20
#define uP_PUSH_BUTTON 21 // Standby pushbutton
#define uP_PWR_xSHDN   22 // Power ON/OFF (3.3 volts and uP)
#define uP_ADC_VS			 23	// Battery Voltage Monitor
#define uP_ADC_ALS		 24
// 25 UNUSED
// 26 UNUSED
// 27 UNUSED				
// 28 UNUSED			
#define uP_USB_DP			29		// 
// 30 UNUSED 		
// 31 UNAVAILABLE (P.104)

// PORT 0 NAMES VERIFIED.






// **********************
// PORT 1 BIT DEFS
// **********************
#define LCD_WR				0
#define LCD_RD				1
#define LCD_CS				4
#define LCD_DC				8				
#define LCD_WAIT      9
#define LCD_RST 			10

// 2 UNAVAILABLE (P.107)
// 3 UNAVAILABLE (P.107)
// 5 UNAVAILABLE (P.107)
// 6 UNAVAILABLE (P.107)
// 7 UNAVAILABLE (P.107)
// 11 UNAVAILABLE (P.107)
// 12 UNAVAILABLE (P.107)
// 13 UNAVAILABLE (P.107)
// 17 UNUSED
// 18	UNUSED	
#define RF_M0						19		// RFD wireless mode select
// 20 unused
#define uP_PWM_KEY			21	// BEEPER
#define RF_M1						22		//RFD mode
// 23 unused
// 24 unused
#define RF_M2						25		// RFD MODE
// 26 UNUSED	
#define RF_IO						27		// gpio RFD
#define RF_LRN					28		// learn
#define RF_xRST       	29		// reset
// 30 UNUSED
// 31 UNUSED


// **********************
// PORT 2 BIT DEFS
// **********************
#define D0 							0
#define D1 							1
#define D2 							2
#define D3 							3
#define D4 							4
#define D5 							5
#define D6 							6
#define D7      				7
// 8 UNUSED
// 9 UNUSED
#define uP_xISP				10		
#define LCD_INT		    11		// LCD Touch Panel Int
// 13 UNUSED
// 14 UNAVAILABLE (P.109)
// 15 UNAVAILABLE (P.109)
#define DIP_xB				16
#define DIP_xA				17		// MODE SELECT
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
#define xWIRE_PRESENT 25 
// 26 UNUSED		
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
#define uP_RFD_TXD	28			//RFD UART3
#define uP_RFD_RXD	29			// RFD UART3
// 30 UNAVAILABLE (P.111)
// 31 UNAVAILABLE (P.111)


// prototypes
BOOL isRS485Power		(void);
BOOL powerButtonPressed(void);
void powerDownHandHeld(void);
void commboardDoWork(void);
BOOL commboardInit(void);
void DISABLE_ETM    (void);
void GPIO_CONFIG    (void);
char READ_MODEL     (void);
void ENABLE_LED_PWM (void);
void DISABLE_LED_PWM(void);
WORD SCAN_KEYS      (void);
BOOL readDipSwitchB( void );





