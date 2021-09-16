#ifndef WIRELESS_H
#define WIRELESS_H

typedef enum eRadioType
{
	eINTERNAL_RADIO = 0,
	eEXTERNAL_RADIO,
	eUNKNOWN_RADIO = 0xFF
}eRADIOTYPE;

uint32_t wirelessGetOurESN(void);
void wirelessInit(ePLATFORM_TYPE ePlatformType);

#ifdef RADIO_V2
//#define RADIO_RX_TIMEOUT 1125UL
#define RADIO_RX_TIMEOUT 500UL
#define ENTER_AT_MODE_RETRIES 3
BOOL wirelessSetWancoRadio( uint32_t inputSeed, uint32_t dstAddr );
BOOL wirelessSetWancoRepeaterRadio( uint32_t inputSeed, uint32_t dstAddr );
void wirelessDumpRadioRegs( void );
uint32_t wirelessReadOutRadiosESN( void );
int8_t getRssi( void );

//BOOL wirelessNetworkDiso( void );
//void doWirelessState( void );
#else
BOOL wirelessDoBondLearn(eRADIOTYPE radioType);
#endif

// Coefficients in Q16 format
#define RSSI_SLOPE 5824L
#define RSSI_INTERCEPT 6619136L

DWORD wirelessSwapDword( DWORD Data ); 
ePLATFORM_TYPE whatPlatformTypeAmI( void );

/////
// wireless port definitions
// arrow board controller use port 0
// handheld used port 1
/////
#define ARROW_BOARD_RF_M0											17
#define ARROW_BOARD_RF_M1											18
#define ARROW_BOARD_RF_M2											19
//#define ARROW_BOARD_RF_IO										20  // NOTE: For REV 5 Board
#define ARROW_BOARD_RF1_xRST									20	// NOTE: For REV 6 Board
#define ARROW_BOARD_RF_LRN										21
#define ARROW_BOARD_RF0_xRST									22
#define REPEATER_RF0_xRST											19
//////
// PINMODE Register 0, port 0 pins 16-26
//  0  0  0  0  0  1  1  1  1  1  2  2  2  2  2  3
//  1  3  5  7  9  1  3  5  7  9  1  3  5  7  9  1
//  1  1  1  1  2  2  2  2  2  2  2
//  6  7  8  9  0  1  2  3  4  5  6
// 00 00 00 00 II LL 00 00 00 00 00 rr rr rr rr rr
// 0000 0000 IILL 0000 0000 00rr rrrr rrrr
/////
#define ARROW_BOARD_RF_LRN_PINMODE_MASK				0x00300000
#define ARROW_BOARD_RF_LRN_PULLUP_ENABLE			0x00000000
#define ARROW_BOARD_RF_LRN_PULLDOWN_ENABLE		0x00300000
#define ARROW_BOARD_RF_LRN_PULLUP_OFF					0x00100000

#define ARROW_BOARD_RF_IO_PINMODE_MASK				0x00C00000
#define ARROW_BOARD_RF_IO_PULLUP_ENABLE				0x00000000
#define ARROW_BOARD_RF_IO_PULLDOWN_ENABLE			0x00C00000
#define ARROW_BOARD_RF_IO_PULLUP_OFF					0x00400000

#define ARROW_BOARD_RF_RST_PINMODE_MASK				0x00003000
#define ARROW_BOARD_RF_RST_PULLUP_ENABLE			0x00000000
#define ARROW_BOARD_RF_RST_PULLDOWN_ENABLE		0x00003000

#define HANDHELD_RF_M0					19
#define HANDHELD_RF_M1					22
#define HANDHELD_RF_M2					25
#define HANDHELD_RF_IO					27
#define HANDHELD_RF_LRN					28
#define HANDHELD_RF_xRST				29
//////
// PINMODE Register 3, port 1 pins 16-31
//  0  0  0  0  0  1  1  1  1  1  2  2  2  2  2  3
//  1  3  5  7  9  1  3  5  7  9  1  3  5  7  9  1
//  1  1  1  1  2  2  2  2  2  2  2  2  2  2  3  3
//  6  7  8  9  0  1  2  3  4  5  6  7  8  9  0  1
// 00 00 00 00 00 00 00 00 00 00 00 II LL 00 00 00
// 0000 0000 0000 0000 0000 00II LL00 0000
/////
#define HANDHELD_RF_LRN_PINMODE_MASK				0x000000c0
#define HANDHELD_RF_LRN_PULLUP_ENABLE				0x00000000
#define HANDHELD_RF_LRN_PULLDOWN_ENABLE			0x000000c0
#define HANDHELD_RF_LRN_PULLUP_OFF					0x00000080

#define HANDHELD_RF_IO_PINMODE_MASK					0x00000300
#define HANDHELD_RF_IO_PULLUP_ENABLE				0x00000000
#define HANDHELD_RF_IO_PULLDOWN_ENABLE			0x00000300
#define HANDHELD_RF_IO_PULLUP_OFF						0x00000200

#define HANDHELD_RF_RST_PINMODE_MASK				0x03000000
#define HANDHELD_RF_RST_PULLUP_ENABLE				0x00000000
#define HANDHELD_RF_RST_PULLDOWN_ENABLE			0x03000000

#ifdef RADIO_V2
#define TRANSMIT_OPTIONS_READ "ATTO\r"
#define TRANSMIT_OPTIONS_REPLY "C4\r"
#define TRANSMIT_OPTION_WRITE "ATTO 0xC4\r"

#define DEFAULT_TRANSMIT_OPTION "ATTO 0xC0\r"
#define REPEATER_TRANSMIT_OPTION "ATTO 0x80\r"
#define DEFAULT_PREAMBLE_VALUE "ATHP 0x04\r"
#define DEFAULT_NETWORK_ID_VALUE "ATID 0x0A37\r"
#define DEFAULT_DST_ADDR_HIGH "ATDH 0013A200\r"
#define DEFAULT_DST_ADDR_LOW "ATDL DEADBEEF\r"



typedef struct 
{
	uint8_t 	preambleID;		// Unique ID for packet filtering, derived from handheld
	uint16_t	networkID;		// Unique ID for packet filtering, derived from handheld 
	uint32_t	key[2];				// Encryption key for network
	uint32_t  srcAddrsHigh;	// Upper 32 
	uint32_t  srcAddrsLow;	// Lower 32 
	uint32_t  dstAddrsHigh; // Upper 32 
	uint32_t  dstAddrsLow; 	// Lower 32 
} radioParms_t;

typedef struct
{
	char bytesTransmitted[6];
	char rssi[4];
	char receivedErrors[6];
	char receivedGoodPackets[6];
	char macAckErrors[6];
	char txFailures[6];
	char txAttempts[6];
} radioDiagnostics_t;

#endif 	// RADIO_V2

#endif		// WIRELESS_H
