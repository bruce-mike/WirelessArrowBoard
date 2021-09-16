#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <lpc23xx.h>
#include "shareddefs.h"
#include "sharedinterface.h"
#include "timer.h"
#include "queue.h"

#include "wireless.h"
#include "serial.h"
#include "spiflash.h"
#include "watchdog.h"
#include "storedconfig.h"
#include "hdlc.h"		// crc16

#include <assert.h>

uint16_t ADCGetRssiVoltage( void );	

// Use this for quick way to enter command mode
#define USE_RADIO_CONFIG_PIN

// These are used for debugging ONLY, 
// NOTE: For production ALWAYS comment ALL defines 
//#define ENCRYPTION_OFF
//#define DISPLAY_KEY
//#define LOW_TX_PWR
//#define DUMP_RADIO_REGS

// These are used for debugging radio comms
//#define DEBUG_RADIO_1					/* High level printf for wireless */
//#define DEBUG_RADIO_SERIAL	  /* Raw serial radio Tx & Rx, NOTE: Robust, buffer may fill */
//#define DEBUG_PIN_ENABLED			/* Use LRN pin as an output for debug, else input to factory reset */

static radioParms_t ourRadioParms = {0};
static ePLATFORM_TYPE eMyPlatformType;
static uint32_t nOurESN;
static BOOL inATCmdMode = FALSE;
static radioDiagnostics_t radioDiagnostics;

// Fwd declarations
static void atDumpRadioRegs( void );
static BOOL atRestoreFactoryDefaults( void );
static void hex2ascii( uint8_t *input, char *output, uint8_t nBytes );
static BOOL setPortPinFunction( char *cmdStr, uint8_t length );
static BOOL wirelessResetRadio( void );
static BOOL wirelessRestoreFactoryDefaults( void );
static uint8_t ascii2hex( char *input );

/*
 * Support for long time outs and delays
 */
static delayMSecs( uint32_t delayMSecs )
{
	TIMERCTL timerWait;
	
	initTimer(&timerWait);
	startTimer(&timerWait, delayMSecs);
	do
	{	
		watchdogFeed();
	} while( !isTimerExpired(&timerWait) );
}
/*
 * This will write the passed in string to the radio, assume
 * that we're in AT Cmd mode.
 * Will not return anything.  It is up to the calling routine
 * to determine/read the returning string.
 */
static void wirelessWriteStringToRadio( char *input, uint8_t length )
{
	uint8_t i;
	
	for( i = 0; i < length; i++)
	{
		serialWriteWireless(input[i]);
	}
	#ifdef DEBUG_RADIO_SERIAL
	printf(">%s\n",input);
	#endif
}
/*
 * This function will read in the radio's output.  Reads in one character 
 * at a time till 1.) buffer is reached, or 2.)  an error occurs(time out), 
 * or 3.) the delimiter character is read in, (carraige return, \r).  A CR  
 * means the radio is done spewing shit.
 * Stores new received message in passed in *returnString.  Also returns
 * number of characters read.
 */
static uint8_t wirelessReadStringFromRadio( char *returnString, uint8_t maxLength )
{
	uint8_t nCharsRead = 0;
	unsigned char rtnChar;
	TIMERCTL timerWait;
	
	initTimer(&timerWait);
	startTimer(&timerWait, RADIO_RX_TIMEOUT);	
	
	while( nCharsRead < maxLength )
	{
		if(serialReadWireless(&rtnChar))
		{
			returnString[nCharsRead++] = rtnChar;
			if( '\r' == rtnChar )
			{
				returnString[nCharsRead] = NULL;
				delayMSecs( 2UL );		// Keep from commands coming down to quick
				break;
			}
		}
		if( isTimerExpired(&timerWait) )
		{
			#ifdef DEBUG_RADIO_SERIAL
			printf("READ T.O.\n");
			#endif
			break;
		}
		watchdogFeed();
	}
	#ifdef DEBUG_RADIO_SERIAL
	printf("<%s\n",returnString);
	#endif
	return( nCharsRead );
}
/*
 * Some of the Xbee AT commands acknowledge the passed in commands 
 * have been executed by returning the string "OK\r".  Real high tech.
 * The calling routine typically will call wirelessReadStringFromRadio()
 * which captures the returned string from a command.  This function 
 * checks to see if the passed in string, *reply, of a command, contains
 * the "OK\r" pattern.   
 * Returns TRUE or FALSE.
 */
BOOL gotOKResponse( char *reply )
{
	BOOL status = FALSE;
	
	if( ('O' == reply[0]) && ('K' == reply[1]) && ('\r' == reply[2]) )
	{
		status = TRUE;
	}
	else
	{
		printf("No OK response <%c> <%c>\n",reply[0],reply[1]);
	}
	return( status );
}
/*
 * Place radio in CMD MODE.  Must enter Cmd mode to execute any 
 * of the AT commands.  In this mode, during the reset function,
 * the CONFIG pin is used to enter CMD mode.
 */
static BOOL wirelessEnterATCmdMode( void )
{
	if( inATCmdMode )
	{
		return( TRUE );
	}
	
	if( wirelessResetRadio( ) )
	{
		inATCmdMode = TRUE;
		return( TRUE );
	}
	else if( wirelessResetRadio( ) )
	{
		inATCmdMode = TRUE;
		return( TRUE );
	}
	printf("ENTER AT MODE FAILED\n");
	inATCmdMode = FALSE;
	return( FALSE );
}
/*
 * Place radio in CMD MODE.  Must enter Cmd mode to execute any 
 * of the AT commands.  Takes ~ 1 second for radio to respond.
 * Spec says no serial activity for at least 1 second, before and 
 * after issuing this command, time is programable.
 * Assumes that a radio reset occured prior to calling this function.
 * After 1 second of issuing a reset, send down "+++", waits another 
 * 1 second for "OK\r" reply.  This will try 2 times to get a proper
 * response cause sometimes there's crap in the Rx FIFO that needs to
 * be cleared.
 * We reset radio on front end to save the 1st 1 second delay.
 */
static BOOL wirelessEnterATCmdMode3Plus( void )
{
	char cmd[] = {"+++"};
	char retn[6];
	BOOL passed  = FALSE;
	uint8_t retries = ENTER_AT_MODE_RETRIES;
	
	if( inATCmdMode )
	{
		return( TRUE );
	}
	else 
	{
		inATCmdMode = FALSE;	// This is redundent, but be complete?
	}
	
	delayMSecs( 1100UL );
	
	while( retries-- )
	{
		// Send down '+++'
		wirelessWriteStringToRadio( cmd, strlen(cmd) );
	
		// Expect 'OK\r'
		memset( retn, 0, sizeof(retn) );
		wirelessReadStringFromRadio( retn, sizeof(retn) );
	
		if( gotOKResponse( retn ) )
		{
			delayMSecs( 100UL );
			passed = TRUE;
			inATCmdMode = TRUE;
			return( passed );
		}
	}

	printf("ENTER AT MODE FAILED\n");
	return( passed );
}
/*
 * Get radio out of CMD MODE.  Assumed that we are already in 
 * command mode.  If not no harm.
 */
BOOL wirelessExitATCmdMode( void )
{
	char cmd[] = {"ATCN\r"};
	char retn[6];
	BOOL passed  = FALSE;
	uint8_t retries = ENTER_AT_MODE_RETRIES;
	
	while( retries-- )
	{
		wirelessWriteStringToRadio( cmd, strlen(cmd) );
	
		// Expect 'OK\r'
		memset( retn, 0, sizeof(retn) );
		if( 0 != wirelessReadStringFromRadio( retn, sizeof(retn) ) )
		{
			if( gotOKResponse( retn ) )
			{
				delayMSecs( 250UL );
				passed = TRUE;
				inATCmdMode = FALSE;
				return( passed );
			}
		}
	}
	printf("EXIT CMD MODE FAILED\n");
	inATCmdMode = FALSE;

	return( passed );
}
/*
 * Local function that helps toggle the hardware reset line
 * to the radio.
 */
static void wirelessAssertReset(BOOL bState)
{

	switch(eMyPlatformType)
	{
		case ePLATFORM_TYPE_ARROW_BOARD:
			if(bState)
			{
				FIO0CLR = (1<<ARROW_BOARD_RF0_xRST);
			}
			else
			{
				FIO0SET = (1<<ARROW_BOARD_RF0_xRST);
			}
			break;
		case ePLATFORM_TYPE_HANDHELD:	
			if(bState)
			{
				FIO1CLR = (1<<HANDHELD_RF_xRST);
			}
			else
			{
				FIO1SET = (1<<HANDHELD_RF_xRST);
			}		
			break;
		case ePLATFORM_TYPE_REPEATER:	//P0.19
			if(bState)
			{
				FIO0CLR = (1<<REPEATER_RF0_xRST);
			}
			else
			{
				FIO0SET = (1<<REPEATER_RF0_xRST);
			}
			break;
	}	
}
#ifdef USE_RADIO_CONFIG_PIN
/* 
 * Reset the radio AND place it in command mode.  It takes over
 * the Tx line and holds it low while the reset pin is toggled.
 * After reset line is released, we wait to read "OK\r" and then
 * releases the Tx/!CONFIG pin.  Returns TRUE if successful.
 */
static BOOL wirelessResetRadio( void )
{
	char crap[6];
	BOOL bPassed = FALSE; 
	
	wirelessAssertReset(TRUE);						// Place radio in reset
	
	// Make Tx/!CONFIG pin a GPIO output
	// Then assert low to !CONFIG pin
	switch(eMyPlatformType)
	{
		case ePLATFORM_TYPE_ARROW_BOARD:		// P0.15
			PINSEL0 &= ~(UART1_SEL << TXD1_SEL); 	
			FIO0CLR = (1 << 15);									
			FIO0DIR |= (OUT << 15);
			break;
		case ePLATFORM_TYPE_HANDHELD:				// P4.28
			PINSEL9 &= ~(UART3_SEL << TXD3_SEL); 
			FIO4CLR = (1 << 28);	
			FIO4DIR |= (OUT << 28);
			break;
		case ePLATFORM_TYPE_REPEATER:		// P0.15
			PINSEL0 &= ~(UART1_SEL << TXD1_SEL); 	
			FIO0CLR = (1 << 15);									
			FIO0DIR |= (OUT << 15);
			break;
		default:
			assert( FALSE );
			break;
	}
	
	serialReadWireless( (unsigned char *)&crap[0] );	// Clear crap from Rx FIFO
	
	delayMSecs(5);
	wirelessAssertReset(FALSE);						// Pull radio out of reset

	serialReadWireless( (unsigned char *)&crap[0] );	// Clear crap from Rx FIFO
	
	// Wait for repsonse or time out
	memset( crap, 0, sizeof(crap) );
	wirelessReadStringFromRadio( crap, sizeof(crap) );

	if( gotOKResponse( crap ) )
	{
		bPassed = TRUE;
	}
	
	// Set pin high and place pin mode back to Tx/UART	
	switch(eMyPlatformType)
	{
		case ePLATFORM_TYPE_ARROW_BOARD:
			FIO0SET = (1 << 15);									// !CONFIG pin high
			PINSEL0 |= (UART1_SEL << TXD1_SEL); 	// Make Tx again
			break;
		case ePLATFORM_TYPE_HANDHELD:	
			FIO4SET = (1 << 28);	
			PINSEL9 |= (UART3_SEL << TXD3_SEL); 
			break;
		case ePLATFORM_TYPE_REPEATER:
			FIO0SET = (1 << 15);									// !CONFIG pin high
			PINSEL0 |= (UART1_SEL << TXD1_SEL); 	// Make Tx again
			break;
		default:
			assert( FALSE );
			break;
	}

	serialReadWireless( (unsigned char *)&crap[0] );	// Clear crap from Rx FIFO

	delayMSecs(10);

	serialReadWireless( (unsigned char *)&crap[0] );	// Clear crap from Rx FIFO
	
	return( bPassed );
}
#else	// USE_RADIO_CONFIG_PIN
/* 
 * Reset the radio.  We also issue a read from the radio.
 * This does 2 things 1.) Clears out Rx FIFO and 2.) Does
 * the XBee required delay before issuing any commands.
 */
static void wirelessResetRadio( void )
{
	char resetReply[] ="Blow Me";
	unsigned char crap;
	uint8_t counter = 0;
	
	wirelessAssertReset(TRUE);						// Place radio in reset
	
	delayMSecs(5);
	wirelessAssertReset(FALSE);						// Pull radio out of reset
	
	#ifdef DEBUG_RADIO_1
	printf("Reset Radio\n");
	#endif

	inATCmdMode = FALSE;

	delayMSecs(80);
	wirelessWriteStringToRadio( resetReply, sizeof(resetReply) );
	
	// XBee spec says no serial activity for 1 second, We use the 
	// low level read function to clear out any rx crap while we
	// wait 1.2 seconds
	do
	{
		serialReadWireless( &crap );
		delayMSecs(100);
	} while( 12 > counter++ );
}
#endif	// USE_RADIO_CONFIG_PIN
/*
 * Reads in external port pin LRN.  Used for DEBUG
 */
static unsigned char wirelessGetLRNPin()
{
	BOOL bRetVal = FALSE;
	switch(eMyPlatformType)
	{
		case ePLATFORM_TYPE_ARROW_BOARD:
			if(0 != (FIO0PIN & (1<<ARROW_BOARD_RF_LRN)))
			{
				bRetVal = TRUE;
			}
			break;
		case ePLATFORM_TYPE_HANDHELD:
			if(0 != (FIO1PIN & (1<<HANDHELD_RF_LRN)))
			{
				bRetVal = TRUE;
			}
			break;
		case ePLATFORM_TYPE_REPEATER:
			if(0 != (FIO1PIN & (1<<0)))	// P1.0
			{
				bRetVal = TRUE;
			}
			break;
		default:
			assert(FALSE);
			break;
	}
	return bRetVal;
}
/*
 * This reads the physical radio's internal register to get
 * the radio's ESN.  It resets the radio, delay, place radio 
 * in command mode, read ESN, leave command mode, save ESN,
 * all 64 bits, returns the lower 32 bits.  
 * If successful, stores lower 32 bits in nonVolatile memory.
 * Not yet on storing.....
 */
uint32_t wirelessReadOutRadiosESN( void )
{
	uint32_t nESNLow = 0UL;
	uint32_t nESNHigh = 0UL;
	BOOL bFound = FALSE;
	char szReadESNHigh[16];
	char szReadESNLow[16];
	uint8_t nLenLow;
	uint8_t nLenHigh;
	uint8_t retries = ENTER_AT_MODE_RETRIES;
	uint8_t i;

	serialDisableInterface(eINTERFACE_WIRELESS);
	
	#ifdef DEBUG_RADIO_1	
	printf("<wirelessReadOutRadiosESN>\n");
	#endif

	// Place in command mode, read our address low and high
		
	if( wirelessEnterATCmdMode( ) )
	{
		char readSrcAddr[] = "ATSH\r";
		
		while( retries-- )		// Read out high ESN
		{
			wirelessWriteStringToRadio( readSrcAddr, strlen( readSrcAddr ) );
			memset( szReadESNHigh, 0, sizeof(szReadESNHigh) );
			nLenHigh = wirelessReadStringFromRadio( szReadESNHigh, sizeof(szReadESNHigh) );
			if( 3 < nLenHigh )
			{
				break;
			}
		}
		
		if( retries )	// If retries left then got high ESN
		{
			retries = ENTER_AT_MODE_RETRIES;		
			readSrcAddr[3] = 'L';		// Cmd = "ATSL\r"
			while( retries-- )			// Get low ESN
			{
				wirelessWriteStringToRadio( readSrcAddr, strlen( readSrcAddr ) );
				memset( szReadESNLow, 0, sizeof(szReadESNLow) );
				nLenLow = wirelessReadStringFromRadio( szReadESNLow, sizeof(szReadESNLow) );
				if( 3 < nLenLow )
				{
					bFound = TRUE;			// Got both high and low ESN
					break;
				}
			}
		}			


		#ifdef LOW_TX_PWR	/*** For testing repeater, reduce Tx power ***/
		{
			char ATCmd[] = "ATPL 0\r";
			char ATReply[6];
			memset( ATReply, 0, sizeof(ATReply) );
			wirelessWriteStringToRadio( ATCmd, strlen( ATCmd ) );
			wirelessReadStringFromRadio( ATReply, sizeof(ATReply) );
			if( gotOKResponse( ATReply ) ) 
			{
				printf("\nWARNING: TX PWR 5mW\n\n");
			}
		}		
		#endif	// LOW_TX_PWR
		
		#ifdef DUMP_RADIO_REGS
		atDumpRadioRegs( );
		#endif
		
		wirelessExitATCmdMode( );
	}
	else
	{
		bFound = FALSE;
		nESNHigh = 0UL;
		nESNLow = 0UL;
	
		#ifdef DEBUG_RADIO_1
		printf("wirelessReadOutRadiosESN FAILED\n");
		#endif
	}

	if(bFound)
	{
		/////
		// grab the ESN
		// Keil strtol can't handle this large value directly
		// so break it into two peices
		/////
		for( i = 0; i < 8; i++ )
		{
			if( (('0' <= szReadESNHigh[i]) && ('9' >= szReadESNHigh[i])) || (('A' <= szReadESNHigh[i]) && ('F' >= szReadESNHigh[i])) )
			{
				nESNHigh <<= 4;
				nESNHigh |= (szReadESNHigh[i] <= '9') ? (uint32_t)(szReadESNHigh[i] - '0') : (uint32_t)(szReadESNHigh[i] - '7');
				i++;
				if( (('0' <= szReadESNHigh[i]) && ('9' >= szReadESNHigh[i])) || (('A' <= szReadESNHigh[i]) && ('F' >= szReadESNHigh[i])) )
				{
					nESNHigh <<= 4;
					nESNHigh |= (szReadESNHigh[i] <= '9') ? (uint32_t)(szReadESNHigh[i] - '0') : (uint32_t)(szReadESNHigh[i] - '7');
				}
			}
		}
				
		for( i = 0; i < 8; i++ )
		{
			if( (('0' <= szReadESNLow[i]) && ('9' >= szReadESNLow[i])) || (('A' <= szReadESNLow[i]) && ('F' >= szReadESNLow[i])) )
			{
				nESNLow <<= 4;
				nESNLow |= (szReadESNLow[i] <= '9') ? (uint32_t)(szReadESNLow[i] - '0') : (uint32_t)(szReadESNLow[i] - '7');
				i++;
				if( (('0' <= szReadESNLow[i]) && ('9' >= szReadESNLow[i])) || (('A' <= szReadESNLow[i]) && ('F' >= szReadESNLow[i])) )
				{
					nESNLow <<= 4;
					nESNLow |= (szReadESNLow[i] <= '9') ? (uint32_t)(szReadESNLow[i] - '0') : (uint32_t)(szReadESNLow[i] - '7');
				}
			}
		}
	}
	
	#ifdef DEBUG_RADIO_SERIAL
	printf("OurESN: 0x%X %X\n",nESNHigh,nESNLow);
	#endif
	
	serialEnableInterface(eINTERFACE_WIRELESS);
	
	storedConfigSetOurESN( nESNLow );
	
	ourRadioParms.srcAddrsHigh = nESNHigh;
	ourRadioParms.srcAddrsLow = nESNLow;
	
	return( ourRadioParms.srcAddrsLow );
}
/*
 * nOurESN should be loaded from wirelessInit()
 */
uint32_t wirelessGetOurESN( void )
{
	return nOurESN;
}
/*
 * On XBee radios, reset is asserted low.  There is no
 * difference between internal or external radio on new 
 * PCBA's.  Presntly !RESET is on the following pins/ports:
 * Arrow board Reset: pin 56, P0.22
 * Handheld board Reset: pin 45, P1.29
 *
 */
void wirelessInit(ePLATFORM_TYPE ePlatformType)
{
	eMyPlatformType = ePlatformType;
	
	
#ifndef DEBUG_PIN_ENABLED	// Normal LRN pin is input to factory reset
	switch(eMyPlatformType)
	{
		case ePLATFORM_TYPE_ARROW_BOARD:
		
			// Make all old radio pins inputs
			FIO0DIR &= ~((OUT<<ARROW_BOARD_RF0_xRST)| // NOTE: For Rev 6 Board
									(OUT<<ARROW_BOARD_RF1_xRST)|
									(OUT<<ARROW_BOARD_RF_M0	 )|
									(OUT<<ARROW_BOARD_RF_M1	 )|
									(OUT<<ARROW_BOARD_RF_M2  )|
									(OUT<<ARROW_BOARD_RF_LRN));
			
			FIO0DIR |= ((OUT<<ARROW_BOARD_RF0_xRST)| // NOTE: For Rev 6 Board
									(IN<<ARROW_BOARD_RF1_xRST)|
									(IN<<ARROW_BOARD_RF_M0	 )|
									(IN<<ARROW_BOARD_RF_M1	 )|
									(IN<<ARROW_BOARD_RF_M2   )|
									(IN<<ARROW_BOARD_RF_LRN  ));
		
			PINMODE1 &= (~ARROW_BOARD_RF_RST_PINMODE_MASK);
			PINMODE1 |=  ARROW_BOARD_RF_RST_PULLUP_ENABLE;

					
			FIO0CLR = (1<<ARROW_BOARD_RF0_xRST);	// Hold radio in reset
			break;

		case ePLATFORM_TYPE_HANDHELD:
			FIO1DIR &= ~((OUT<<HANDHELD_RF_xRST)|
									(OUT<<HANDHELD_RF_M0)|
									(OUT<<HANDHELD_RF_M1)|
									(OUT<<HANDHELD_RF_M2)|
									(OUT<<HANDHELD_RF_LRN));
		
			FIO1DIR |=  ((OUT<<HANDHELD_RF_xRST)|
									(IN<<HANDHELD_RF_M0)|
									(IN<<HANDHELD_RF_M1)|
									(IN<<HANDHELD_RF_M2)|
									(IN<<HANDHELD_RF_LRN));
		
			PINMODE3 &= (~HANDHELD_RF_RST_PINMODE_MASK);
			PINMODE3 |=  HANDHELD_RF_RST_PULLUP_ENABLE;				
			break;
		case ePLATFORM_TYPE_REPEATER:
			/* Done in mischardware */
			break;
		default:
			assert( FALSE );
			break;
	}	
#else  //LRN pin is output for debug
		switch(eMyPlatformType)
	{
		case ePLATFORM_TYPE_ARROW_BOARD:
		
			// Make all old radio pins inputs
			FIO0DIR &= ~((OUT<<ARROW_BOARD_RF0_xRST)| // NOTE: For Rev 6 Board
									(OUT<<ARROW_BOARD_RF1_xRST)|
									(OUT<<ARROW_BOARD_RF_M0	 )|
									(OUT<<ARROW_BOARD_RF_M1	 )|
									(OUT<<ARROW_BOARD_RF_M2  )|
									(OUT<<ARROW_BOARD_RF_LRN));
			
			FIO0DIR |= ((OUT<<ARROW_BOARD_RF0_xRST)| // NOTE: For Rev 6 Board
									(OUT<<ARROW_BOARD_RF1_xRST)|
									(IN<<ARROW_BOARD_RF_M0	 )|
									(IN<<ARROW_BOARD_RF_M1	 )|
									(IN<<ARROW_BOARD_RF_M2   )|
									(OUT<<ARROW_BOARD_RF_LRN  ));
		
			PINMODE1 &= (~ARROW_BOARD_RF_LRN_PINMODE_MASK);
			PINMODE1 |=  ARROW_BOARD_RF_LRN_PULLUP_ENABLE;

			
			FIO0CLR = (1<<ARROW_BOARD_RF0_xRST);
			FIO0CLR = (1<<ARROW_BOARD_RF1_xRST);
			FIO0SET = (1<<ARROW_BOARD_RF_LRN);
			break;

		case ePLATFORM_TYPE_HANDHELD:
			FIO1DIR &= ~((OUT<<HANDHELD_RF_xRST)|
									(OUT<<HANDHELD_RF_M0)|
									(OUT<<HANDHELD_RF_M1)|
									(OUT<<HANDHELD_RF_M2)|
									(OUT<<HANDHELD_RF_LRN));
		
			FIO1DIR |=  ((OUT<<HANDHELD_RF_xRST)|
									(IN<<HANDHELD_RF_M0)|
									(IN<<HANDHELD_RF_M1)|
									(IN<<HANDHELD_RF_M2)|
									(OUT<<HANDHELD_RF_LRN));
		
			PINMODE3 &= (~HANDHELD_RF_LRN_PINMODE_MASK);
			PINMODE3 |=  HANDHELD_RF_LRN_PULLUP_ENABLE;				
			/////
			// io pin output
			// hold it low
			/////	
			FIO1DIR |= (1<<HANDHELD_RF_IO);	
			FIO1CLR = (1<<HANDHELD_RF_IO);
			FIO0SET = (1<<ARROW_BOARD_RF_LRN);
			break;
		case ePLATFORM_TYPE_REPEATER:
			/* Done in mischardware */
			break;
		default:
			assert( FALSE );
			break;
	}	
#endif
	/////
	// read our ESN
	/////
	nOurESN = wirelessReadOutRadiosESN( ); // Read out of radio
	//nOurESN = storedConfigGetOurESN( );

	// DEBUG ONLY
	// If external pin is pulled low and is input, then blow out our brains
	if( (0 == ((1<<HANDHELD_RF_LRN) & FIO1DIR)) && ( 0 == wirelessGetLRNPin( ) ) )
	{
		printf("\n!!! LRN Pin is %x !!!\n", (wirelessGetLRNPin()) ? 1 : 0 );
		wirelessRestoreFactoryDefaults( );
		storedConfigSetSendersESN( 0x00000000 );
	}
}
/*
 * Will set radio parameters to WanCo defined parameters.
 * Used for initial communication during pairing.  Resets
 * radio to factory defaults and then sets to Wanco unique 
 * parameters.  Parameters are unique to each pairing.
 * Parameter inputSeed is unique to each pairing and is the 
 * hand held device's ESN.
 *
 * Transmit options = DEFAULT_TRANSMIT_OPTION = DigiMesh
 * Preamble ID = Lower nibble seed % 8.   Valid values 0 - 7
 * Network ID = crc( seed )  Valid values 0 - 0x7FFF
 * Destination address
 * AES = crc( seed | crc(seed) | ..)
 */
BOOL wirelessSetWancoRadio( uint32_t inputSeed, uint32_t dstAddr )
{
	char ATCmd[42];
	char ATReply[24];
	uint16_t nwkID;
	uint8_t nLen;
	uint8_t failureCount = 1;
	uint32_t temp32;
	uint8_t i;
	
	serialDisableInterface(eINTERFACE_WIRELESS);
	
	#ifdef DEBUG_RADIO_1	
	printf("<wirelessSetWancoRadio %4X %4X>\n", inputSeed, dstAddr);
	#endif
	
	// Enter command mode and send down options
	
	if( wirelessEnterATCmdMode( ) )
	{	
		failureCount = 0;
		
		/*** Restore to factory defaults ***/
		if( !atRestoreFactoryDefaults( ) )
		{
			failureCount++;
		}
		
		/*** Transmit options ***/
		memset( ATReply, 0, sizeof(ATReply) );
		strcpy( ATCmd, DEFAULT_TRANSMIT_OPTION );
		wirelessWriteStringToRadio( ATCmd, strlen( ATCmd ) );
		delayMSecs( 1 );
		nLen = wirelessReadStringFromRadio( ATReply, sizeof(ATReply) );
		if( (0 == nLen) || (!gotOKResponse( ATReply )) )
		{
			failureCount++;
			#ifdef DEBUG_RADIO_1	
			printf("Fail xmit option\n");
			#endif
		}

		/*** For testing repeater, reduce Tx power ***/
		#ifdef LOW_TX_PWR	
		memset( ATReply, 0, sizeof(ATReply) );
		strcpy( ATCmd, "ATPL 0\r" );
		wirelessWriteStringToRadio( ATCmd, strlen( ATCmd ) );
		delayMSecs( 1 );
		nLen = wirelessReadStringFromRadio( ATReply, sizeof(ATReply) );
		if( (0 == nLen) || (!gotOKResponse( ATReply )) )
		{
			failureCount++;
			#ifdef DEBUG_RADIO_1	
			printf("Fail Tx pwr option\n");
			#endif
		}	
		else
		{
			#ifdef DEBUG_RADIO_1	
			printf("\nWARNING: TX PWR 5mW\n\n");
			#endif
		}
		#endif	// LOW_TX_PWR
		
		/*** Set Preamble option ***/
		memset( ATReply, 0, sizeof(ATReply) );
		strcpy( ATCmd, "ATHP " );
		ATCmd[5] = (char)('0' + ((inputSeed & 0x00000000F) % 8 ));	// '0' - '7'
		ATCmd[6] = '\r';
		ATCmd[7] = 0;
		wirelessWriteStringToRadio( ATCmd, strlen( ATCmd ) );
		delayMSecs( 1 );
		nLen = wirelessReadStringFromRadio( ATReply, sizeof(ATReply) );
		if( (0 == nLen) || (!gotOKResponse( ATReply )) )
		{
			failureCount++;
			#ifdef DEBUG_RADIO_1	
			printf("Fail Preamble %s\n", ATCmd);
			#endif
		}
		
		/*** Set Network ID ***/
		ATReply[0] = (uint8_t)((inputSeed >> 0) & 0x000000FF);
		ATReply[1] = (uint8_t)((inputSeed >> 8) & 0x000000FF);
		ATReply[2] = (uint8_t)((inputSeed >> 16) & 0x000000FF);
		ATReply[3] = (uint8_t)((inputSeed >> 24) & 0x000000FF);
		temp32 = crc16( (uint8_t *)ATReply, 4 ) & 0x00007FFFUL;
		nwkID = (uint16_t)temp32;
		strcpy( ATCmd, "ATID " );
		nLen = strlen( ATCmd );
		do
		{
			uint8_t nextDigit = (uint8_t)((temp32 & (0x0FUL << 12)) >> 12);
			ATCmd[nLen++] = (nextDigit < 0x0A) ? (char)(nextDigit + '0') : (char)(nextDigit + '7');
			temp32 <<= 4;
		} while( temp32 & 0x0000FFFFUL );
		ATCmd[nLen++] = '\r';
		ATCmd[nLen++] = 0;
		wirelessWriteStringToRadio( ATCmd, strlen( ATCmd ) );
		delayMSecs( 1 );
		memset( ATReply, 0, sizeof(ATReply) );
		nLen = wirelessReadStringFromRadio( ATReply, sizeof(ATReply) );
		if( (0 == nLen) || (!gotOKResponse( ATReply )) )
		{
			failureCount++;
			#ifdef DEBUG_RADIO_1	
			printf("Fail NwkID %s\n",ATCmd);
			#endif
		}
		
		/*** Destination address ***/
    // NOTE: Destination high 32 defaults to 0x13A200
		memset( ATCmd, 0, sizeof(ATCmd) );		// Fill with ascii NULL
		temp32 = dstAddr;
		strcpy( ATCmd, "ATDL " );
		nLen = strlen( ATCmd );
		do
		{
			uint8_t nextDigit = (uint8_t)((temp32 & (0x0FUL << 28)) >> 28);
			ATCmd[nLen++] = (nextDigit < 0x0A) ? (char)(nextDigit + '0') : (char)(nextDigit + '7');
			temp32 <<= 4;
		} while( temp32 );
		ATCmd[nLen++] = '\r';
		ATCmd[nLen] = 0;
		//printf("\n%s\n",ATCmd);
		wirelessWriteStringToRadio( ATCmd, strlen( ATCmd ) );
		delayMSecs( 1 );
		nLen = wirelessReadStringFromRadio( ATReply, sizeof(ATReply) );
		if( (0 == nLen) || (!gotOKResponse( ATReply )) )
		{
			failureCount++;
			#ifdef DEBUG_RADIO_1	
			printf("Fail Dst low %s\n", ATCmd);
			#endif
		}

		/*** Set AES Key ***/
		memset( ATCmd, 0, sizeof(ATCmd) );		// Fill with ascii NULL
		memset( ATReply, 0, sizeof(ATReply) );	// Fill with ascii NULL

		// Set up binary vector to perform hash
		ATReply[0] = (uint8_t)((inputSeed >> 0) & 0x000000FF);
		ATReply[1] = (uint8_t)((inputSeed >> 8) & 0x000000FF);
		ATReply[2] = (uint8_t)((inputSeed >> 16) & 0x000000FF);
		ATReply[3] = (uint8_t)((inputSeed >> 24) & 0x000000FF);
		ATReply[4] = (uint8_t)((nwkID >> 0) & 0x000000FF);
		ATReply[5] = (uint8_t)((nwkID >> 8) & 0x000000FF);
		ATReply[6] = (uint8_t)(((inputSeed >> 0) & 0x000000FF) ^ 0x1A);
		ATReply[7] = (uint8_t)(((inputSeed >> 8) & 0x000000FF) ^ 0x73);
		ATReply[8] = (uint8_t)(((inputSeed >> 16) & 0x000000FF) ^ 0xD4);
		ATReply[9] = (uint8_t)(((inputSeed >> 24) & 0x000000FF) ^ 0x59);
		ATReply[10] = (uint8_t)(((nwkID >> 0) & 0x000000FF) ^ 0x68);
		ATReply[11] = (uint8_t)(((nwkID >> 8) & 0x000000FF) ^ 0xBC);
		ATReply[12] = (uint8_t)(((inputSeed >> 0) & 0x000000FF) ^ (nwkID & 0xFF));
		ATReply[13] = (uint8_t)(((inputSeed >> 8) & 0x000000FF) ^ ((nwkID >> 8) & 0xFF));
		ATReply[14] = (uint8_t)(((inputSeed >> 16) & 0x000000FF) ^ (nwkID & 0xFF));
		ATReply[15] = (uint8_t)(((inputSeed >> 24) & 0x000000FF) ^ ((nwkID >> 8) & 0xFF));

		// Each loop adds, 2 binary bytes, = 4 ascii bytes,(1 ascii byte/binary nibble),
		// to the key.  The key is 128 bits/16 bytes
		// Run hash over ATReply  at differnt entry points
		strcpy( ATCmd, "ATKY " );
		for( i = 0; i < 8; i++ )
		{
			nLen = strlen( ATCmd );
			temp32 = crc16( (uint8_t *)&ATReply[i], 16-i );
			hex2ascii( (uint8_t *)&temp32, &ATCmd[nLen], 2 );
		}
		// Set up to send down command
		nLen = strlen( ATCmd );
		ATCmd[nLen++] = '\r';
		ATCmd[nLen] = 0;
		#ifdef DISPLAY_KEY
		printf("%s\n", ATCmd );		
		#endif
		wirelessWriteStringToRadio( ATCmd, strlen( ATCmd ) );
		delayMSecs( 1 );
		nLen = wirelessReadStringFromRadio( ATReply, sizeof(ATReply) );
		if( (0 == nLen) || (!gotOKResponse( ATReply )) )
		{
			failureCount++;
			#ifdef DEBUG_RADIO_1	
			printf("Fail AES key %s\n", ATCmd);
			#endif
		}
		// Set up to enable encryption
		memset( ATCmd, 0, sizeof(ATCmd) );		// Fill with ascii NULL
		
		#ifndef ENCRYPTION_OFF
		strcpy( ATCmd, "ATEE 1\r" );	// Turn on encryption
		#else
		strcpy( ATCmd, "ATEE 0\r" );	// Turn off encryption...debug only
		#endif
		delayMSecs( 1 );
		wirelessWriteStringToRadio( ATCmd, strlen( ATCmd ) );
		delayMSecs( 1 );
		nLen = wirelessReadStringFromRadio( ATReply, sizeof(ATReply) );
		if( (0 == nLen) || (!gotOKResponse( ATReply )) )
		{
			failureCount++;
			#ifdef DEBUG_RADIO_1	
			printf("Fail Enabling Encryption\n");
			#endif
		}
		
		/*** Set I/O ports ***/
		strcpy( ATCmd, "ATD0 4\r" );					// Make DIO0 low, pin 20
		setPortPinFunction( ATCmd, strlen(ATCmd) );
		
		strcpy( ATCmd, "ATD8 0\r" );					// Make DIO8 Disable, was sleep input, pin 9 
		setPortPinFunction( ATCmd, strlen(ATCmd) );
		
		strcpy( ATCmd, "ATD9 4\r" );					// Make DIO9 Low, was on/~sleep, pin 13 
		setPortPinFunction( ATCmd, strlen(ATCmd) );

		strcpy( ATCmd, "ATD7 0\r" );					// Make DIO7 disabled, was ~CTS, pin 12 
		setPortPinFunction( ATCmd, strlen(ATCmd) );

		/*** Node options for disco/find neighbor ***/
		memset( ATReply, 0, sizeof(ATReply) );		
		strcpy( ATCmd, "ATNO 4\r" );
		delayMSecs( 1 );
		wirelessWriteStringToRadio( ATCmd, strlen( ATCmd ) );
		delayMSecs( 1 );
		nLen = wirelessReadStringFromRadio( ATReply, sizeof(ATReply) );
		if( (0 == nLen) || (!gotOKResponse( ATReply )) )
		{
			failureCount++;
			#ifdef DEBUG_RADIO_1	
			printf("Fail NO\n");
			#endif
		}
		
		/*** Write new values into radio non-volatile ***/
		memset( ATReply, 0, sizeof(ATReply) );		
		strcpy( ATCmd, "ATWR\r" );
		delayMSecs( 1 );
		wirelessWriteStringToRadio( ATCmd, strlen( ATCmd ) );
		delayMSecs( 1 );
		nLen = wirelessReadStringFromRadio( ATReply, sizeof(ATReply) );
		if( (0 == nLen) || (!gotOKResponse( ATReply )) )
		{
			failureCount++;
			#ifdef DEBUG_RADIO_1	
			printf("Fail WR\n");
			#endif
		}
		
		if( 0 == failureCount )
		{
			#ifdef DEBUG_RADIO_1
			printf("Wanco Radio Setup Passed\n");
			atDumpRadioRegs( );
			#endif
		}
		else
		{
			printf("Wanco Radio FAIL\n");
		}
		wirelessExitATCmdMode( );
	}
		
	serialEnableInterface(eINTERFACE_WIRELESS);	
	
	return( 0 == failureCount );
}
/*
 * Will set radio parameters to WanCo defined parameters.
 * THis is unique for the REPEATER.
 * Used for initial communication during pairing.  Resets
 * radio to factory defaults and then sets to Wanco unique 
 * parameters.  Parameters are unique to each pairing.
 * Parameter inputSeed is unique to each pairing and is the 
 * hand held device's ESN.
 *
 * Transmit options = DEFAULT_TRANSMIT_OPTION = DigiMesh
 * Preamble ID = Lower nibble seed % 8.   Valid values 0 - 7
 * Network ID = crc( seed )  Valid values 0 - 0x7FFF
 * Destination address
 * AES = crc( seed | crc(seed) | ..)
 */
BOOL wirelessSetWancoRepeaterRadio( uint32_t inputSeed, uint32_t dstAddr )
{
	char ATCmd[42];
	char ATReply[24];
	uint16_t nwkID;
	uint8_t nLen;
	uint8_t failureCount = 1;
	uint32_t temp32;
	uint8_t i;
	
	serialDisableInterface(eINTERFACE_WIRELESS);
	
	#ifdef DEBUG_RADIO_1	
	printf("<wirelessSetWancoRepeaterRadio %4X %4X>\n", inputSeed, dstAddr);
	#endif
	
	// Enter command mode and send down options
	
	if( wirelessEnterATCmdMode( ) )
	{	
		failureCount = 0;
		
		/*** Restore to factory defaults ***/
		if( !atRestoreFactoryDefaults( ) )
		{
			failureCount++;
		}
		
		/*** Transmit options ***/
		memset( ATReply, 0, sizeof(ATReply) );
		strcpy( ATCmd, REPEATER_TRANSMIT_OPTION );
		wirelessWriteStringToRadio( ATCmd, strlen( ATCmd ) );
		delayMSecs( 1 );
		nLen = wirelessReadStringFromRadio( ATReply, sizeof(ATReply) );
		if( (0 == nLen) || (!gotOKResponse( ATReply )) )
		{
			failureCount++;
			#ifdef DEBUG_RADIO_1	
			printf("Fail xmit option\n");
			#endif
		}

		/*** For testing repeater, reduce Tx power ***/
		#if 0		// Repeater never has low power
		#ifdef LOW_TX_PWR	
		memset( ATReply, 0, sizeof(ATReply) );
		strcpy( ATCmd, "ATPL 0\r" );
		wirelessWriteStringToRadio( ATCmd, strlen( ATCmd ) );
		delayMSecs( 1 );
		nLen = wirelessReadStringFromRadio( ATReply, sizeof(ATReply) );
		if( (0 == nLen) || (!gotOKResponse( ATReply )) )
		{
			failureCount++;
			#ifdef DEBUG_RADIO_1	
			printf("Fail Tx pwr option\n");
			#endif
		}	
		else
		{
			#ifdef DEBUG_RADIO_1	
			printf("\nWARNING: TX PWR 5mW\n\n");
			#endif
		}
		#endif	// LOW_TX_PWR
		#endif
		
		/*** Set Preamble option ***/
		memset( ATReply, 0, sizeof(ATReply) );
		strcpy( ATCmd, "ATHP " );
		ATCmd[5] = (char)('0' + ((inputSeed & 0x00000000F) % 8 ));	// '0' - '7'
		ATCmd[6] = '\r';
		ATCmd[7] = 0;
		wirelessWriteStringToRadio( ATCmd, strlen( ATCmd ) );
		delayMSecs( 1 );
		nLen = wirelessReadStringFromRadio( ATReply, sizeof(ATReply) );
		if( (0 == nLen) || (!gotOKResponse( ATReply )) )
		{
			failureCount++;
			#ifdef DEBUG_RADIO_1	
			printf("Fail Preamble %s\n", ATCmd);
			#endif
		}
		
		/*** Set Network ID ***/
		ATReply[0] = (uint8_t)((inputSeed >> 0) & 0x000000FF);
		ATReply[1] = (uint8_t)((inputSeed >> 8) & 0x000000FF);
		ATReply[2] = (uint8_t)((inputSeed >> 16) & 0x000000FF);
		ATReply[3] = (uint8_t)((inputSeed >> 24) & 0x000000FF);
		temp32 = crc16( (uint8_t *)ATReply, 4 ) & 0x00007FFFUL;
		nwkID = (uint16_t)temp32;
		strcpy( ATCmd, "ATID " );
		nLen = strlen( ATCmd );
		do
		{
			uint8_t nextDigit = (uint8_t)((temp32 & (0x0FUL << 12)) >> 12);
			ATCmd[nLen++] = (nextDigit < 0x0A) ? (char)(nextDigit + '0') : (char)(nextDigit + '7');
			temp32 <<= 4;
		} while( temp32 & 0x0000FFFFUL );
		ATCmd[nLen++] = '\r';
		ATCmd[nLen++] = 0;
		wirelessWriteStringToRadio( ATCmd, strlen( ATCmd ) );
		delayMSecs( 1 );
		memset( ATReply, 0, sizeof(ATReply) );
		nLen = wirelessReadStringFromRadio( ATReply, sizeof(ATReply) );
		if( (0 == nLen) || (!gotOKResponse( ATReply )) )
		{
			failureCount++;
			#ifdef DEBUG_RADIO_1	
			printf("Fail NwkID %s\n",ATCmd);
			#endif
		}
		
		/*** Destination address ***/
    // NOTE: Destination high 32 defaults to 0x13A200
		memset( ATCmd, 0, sizeof(ATCmd) );		// Fill with ascii NULL
		temp32 = dstAddr;
		strcpy( ATCmd, "ATDL " );
		nLen = strlen( ATCmd );
		do
		{
			uint8_t nextDigit = (uint8_t)((temp32 & (0x0FUL << 28)) >> 28);
			ATCmd[nLen++] = (nextDigit < 0x0A) ? (char)(nextDigit + '0') : (char)(nextDigit + '7');
			temp32 <<= 4;
		} while( temp32 );
		ATCmd[nLen++] = '\r';
		ATCmd[nLen] = 0;
		//printf("\n%s\n",ATCmd);
		wirelessWriteStringToRadio( ATCmd, strlen( ATCmd ) );
		delayMSecs( 1 );
		nLen = wirelessReadStringFromRadio( ATReply, sizeof(ATReply) );
		if( (0 == nLen) || (!gotOKResponse( ATReply )) )
		{
			failureCount++;
			#ifdef DEBUG_RADIO_1	
			printf("Fail Dst low %s\n", ATCmd);
			#endif
		}

		/*** Set AES Key ***/
		memset( ATCmd, 0, sizeof(ATCmd) );		// Fill with ascii NULL
		memset( ATReply, 0, sizeof(ATReply) );	// Fill with ascii NULL

		// Set up binary vector to perform hash
		ATReply[0] = (uint8_t)((inputSeed >> 0) & 0x000000FF);
		ATReply[1] = (uint8_t)((inputSeed >> 8) & 0x000000FF);
		ATReply[2] = (uint8_t)((inputSeed >> 16) & 0x000000FF);
		ATReply[3] = (uint8_t)((inputSeed >> 24) & 0x000000FF);
		ATReply[4] = (uint8_t)((nwkID >> 0) & 0x000000FF);
		ATReply[5] = (uint8_t)((nwkID >> 8) & 0x000000FF);
		ATReply[6] = (uint8_t)(((inputSeed >> 0) & 0x000000FF) ^ 0x1A);
		ATReply[7] = (uint8_t)(((inputSeed >> 8) & 0x000000FF) ^ 0x73);
		ATReply[8] = (uint8_t)(((inputSeed >> 16) & 0x000000FF) ^ 0xD4);
		ATReply[9] = (uint8_t)(((inputSeed >> 24) & 0x000000FF) ^ 0x59);
		ATReply[10] = (uint8_t)(((nwkID >> 0) & 0x000000FF) ^ 0x68);
		ATReply[11] = (uint8_t)(((nwkID >> 8) & 0x000000FF) ^ 0xBC);
		ATReply[12] = (uint8_t)(((inputSeed >> 0) & 0x000000FF) ^ (nwkID & 0xFF));
		ATReply[13] = (uint8_t)(((inputSeed >> 8) & 0x000000FF) ^ ((nwkID >> 8) & 0xFF));
		ATReply[14] = (uint8_t)(((inputSeed >> 16) & 0x000000FF) ^ (nwkID & 0xFF));
		ATReply[15] = (uint8_t)(((inputSeed >> 24) & 0x000000FF) ^ ((nwkID >> 8) & 0xFF));

		// Each loop adds, 2 binary bytes, = 4 ascii bytes,(1 ascii byte/binary nibble),
		// to the key.  The key is 128 bits/16 bytes
		// Run hash over ATReply  at differnt entry points
		strcpy( ATCmd, "ATKY " );
		for( i = 0; i < 8; i++ )
		{
			nLen = strlen( ATCmd );
			temp32 = crc16( (uint8_t *)&ATReply[i], 16-i );
			hex2ascii( (uint8_t *)&temp32, &ATCmd[nLen], 2 );
		}
		// Set up to send down command
		nLen = strlen( ATCmd );
		ATCmd[nLen++] = '\r';
		ATCmd[nLen] = 0;
		#ifdef DISPLAY_KEY
		printf("%s\n", ATCmd );		
		#endif
		wirelessWriteStringToRadio( ATCmd, strlen( ATCmd ) );
		delayMSecs( 1 );
		nLen = wirelessReadStringFromRadio( ATReply, sizeof(ATReply) );
		if( (0 == nLen) || (!gotOKResponse( ATReply )) )
		{
			failureCount++;
			#ifdef DEBUG_RADIO_1	
			printf("Fail AES key %s\n", ATCmd);
			#endif
		}
		// Set up to enable encryption
		memset( ATCmd, 0, sizeof(ATCmd) );		// Fill with ascii NULL
		
		#ifndef ENCRYPTION_OFF
		strcpy( ATCmd, "ATEE 1\r" );	// Turn on encryption
		#else
		strcpy( ATCmd, "ATEE 0\r" );	// Turn off encryption...debug only
		#endif
		delayMSecs( 1 );
		wirelessWriteStringToRadio( ATCmd, strlen( ATCmd ) );
		delayMSecs( 1 );
		nLen = wirelessReadStringFromRadio( ATReply, sizeof(ATReply) );
		if( (0 == nLen) || (!gotOKResponse( ATReply )) )
		{
			failureCount++;
			#ifdef DEBUG_RADIO_1	
			printf("Fail Enabling Encryption\n");
			#endif
		}
		
		/*** Set I/O ports ***/
		strcpy( ATCmd, "ATD0 4\r" );					// Make DIO0 low, pin 20
		setPortPinFunction( ATCmd, strlen(ATCmd) );
		
		strcpy( ATCmd, "ATD8 0\r" );					// Make DIO8 Disable, was sleep input, pin 9 
		setPortPinFunction( ATCmd, strlen(ATCmd) );
		
		strcpy( ATCmd, "ATD9 4\r" );					// Make DIO9 Low, was on/~sleep, pin 13 
		setPortPinFunction( ATCmd, strlen(ATCmd) );

		strcpy( ATCmd, "ATD7 0\r" );					// Make DIO7 disabled, was ~CTS, pin 12 
		setPortPinFunction( ATCmd, strlen(ATCmd) );

		/*** Node options for disco/find neighbor ***/
		memset( ATReply, 0, sizeof(ATReply) );		
		strcpy( ATCmd, "ATNO 4\r" );
		delayMSecs( 1 );
		wirelessWriteStringToRadio( ATCmd, strlen( ATCmd ) );
		delayMSecs( 1 );
		nLen = wirelessReadStringFromRadio( ATReply, sizeof(ATReply) );
		if( (0 == nLen) || (!gotOKResponse( ATReply )) )
		{
			failureCount++;
			#ifdef DEBUG_RADIO_1	
			printf("Fail NO\n");
			#endif
		}
		
		/*** Write new values into radio non-volatile ***/
		memset( ATReply, 0, sizeof(ATReply) );		
		strcpy( ATCmd, "ATWR\r" );
		delayMSecs( 1 );
		wirelessWriteStringToRadio( ATCmd, strlen( ATCmd ) );
		delayMSecs( 1 );
		nLen = wirelessReadStringFromRadio( ATReply, sizeof(ATReply) );
		if( (0 == nLen) || (!gotOKResponse( ATReply )) )
		{
			failureCount++;
			#ifdef DEBUG_RADIO_1	
			printf("Fail WR\n");
			#endif
		}
		
		if( 0 == failureCount )
		{
			#ifdef DEBUG_RADIO_1
			printf("Wanco Radio Setup Passed\n");
			atDumpRadioRegs( );
			#endif
		}
		else
		{
			printf("Wanco Radio FAIL\n");
		}
		wirelessExitATCmdMode( );
	}
		
	serialEnableInterface(eINTERFACE_WIRELESS);	
	
	return( 0 == failureCount );
}
/*
 * Helper function that programs the I/O ports/pins
 * for the XBee module.  Assumes in AT Cmd mode.
 */
static BOOL setPortPinFunction( char *cmdStr, uint8_t length )
{
	uint8_t nLen;
	char ATReply[6] = {0,};
	//memset( ATCmd, 0, sizeof(ATCmd) );		// Fill with ascii NULL
	//strcpy( ATCmd, "ATD0 4\r" );					// Make DIO0 low, pin 20
	//delayMSecs( 1 );
	wirelessWriteStringToRadio( cmdStr, length );
	delayMSecs( 1 );
	nLen = wirelessReadStringFromRadio( ATReply, sizeof(ATReply) );
	if( (0 == nLen) || (!gotOKResponse( ATReply )) )
	{
		#ifdef DEBUG_RADIO_1	
		printf("Fail <%s>\n",cmdStr);
		#endif
		return( FALSE );
	}
	return( TRUE );
}
/*
 * Assumes already in AT mode
 */
static void atDumpRadioRegs( void )
{
	char ATCmd[16];
	char ATReply[16];
	uint8_t nLen;
	
	#ifdef DEBUG_RADIO_1
	printf("<wirelessDumpRadioRegs>\n");
	#endif
	
	if( !inATCmdMode )
	{
		return;
	}
	
	// Preamble option
	memset( ATReply, 0, sizeof(ATReply) );
	strcpy( ATCmd, "ATHP\r" );
	wirelessWriteStringToRadio( ATCmd, strlen( ATCmd ) );
	delayMSecs( 1 );
	nLen = wirelessReadStringFromRadio( ATReply, sizeof(ATReply) );
	if( nLen )
	{
		printf("Preamble %s\n", ATReply);
	}
	
	// Transmit option
	memset( ATReply, 0, sizeof(ATReply) );
	strcpy( ATCmd, "ATTO\r" );
	wirelessWriteStringToRadio( ATCmd, strlen( ATCmd ) );
	delayMSecs( 1 );
	nLen = wirelessReadStringFromRadio( ATReply, sizeof(ATReply) );
	if( nLen )
	{
		printf("Tx options %s\n", ATReply);
	}
	
	// Network ID
	memset( ATReply, 0, sizeof(ATReply) );
	strcpy( ATCmd, "ATID\r" );
	wirelessWriteStringToRadio( ATCmd, strlen( ATCmd ) );
	delayMSecs( 1 );
	nLen = wirelessReadStringFromRadio( ATReply, sizeof(ATReply) );
	if( nLen )
	{
		printf("Nwk ID %s\n", ATReply);
	}
	
	// Destination high
	memset( ATReply, 0, sizeof(ATReply) );
	strcpy( ATCmd, "ATDH\r" );
	wirelessWriteStringToRadio( ATCmd, strlen( ATCmd ) );
	delayMSecs( 1 );
	nLen = wirelessReadStringFromRadio( ATReply, sizeof(ATReply) );
	if( nLen )
	{
		printf("Dest high %s\n", ATReply);
	}
	
	// Destination low
	memset( ATReply, 0, sizeof(ATReply) );
	strcpy( ATCmd, "ATDL\r" );
	wirelessWriteStringToRadio( ATCmd, strlen( ATCmd ) );
	delayMSecs( 1 );
	nLen = wirelessReadStringFromRadio( ATReply, sizeof(ATReply) );
	if( nLen )
	{
		printf("Dest low %s\n", ATReply);
	}
	
	// Source high
	memset( ATReply, 0, sizeof(ATReply) );
	strcpy( ATCmd, "ATSH\r" );
	wirelessWriteStringToRadio( ATCmd, strlen( ATCmd ) );
	delayMSecs( 1 );
	nLen = wirelessReadStringFromRadio( ATReply, sizeof(ATReply) );
	if( nLen )
	{
		printf("Src high %s\n", ATReply);
	}
	
	// Source low
	memset( ATReply, 0, sizeof(ATReply) );
	strcpy( ATCmd, "ATSL\r" );
	wirelessWriteStringToRadio( ATCmd, strlen( ATCmd ) );
	delayMSecs( 1 );
	nLen = wirelessReadStringFromRadio( ATReply, sizeof(ATReply) );
	if( nLen )
	{
		printf("Src low %s\n", ATReply);
	}

	// Encryption enabled?
	memset( ATReply, 0, sizeof(ATReply) );
	strcpy( ATCmd, "ATEE\r" );
	wirelessWriteStringToRadio( ATCmd, strlen( ATCmd ) );
	delayMSecs( 1 );
	nLen = wirelessReadStringFromRadio( ATReply, sizeof(ATReply) );
	if( nLen )
	{
		printf("Encryption %s\n", ('1' == ATReply[0]) ? "ON" : "OFF" );
	}	
	
	// Tx power level
	memset( ATReply, 0, sizeof(ATReply) );
	strcpy( ATCmd, "ATPL\r" );
	wirelessWriteStringToRadio( ATCmd, strlen( ATCmd ) );
	delayMSecs( 1 );
	nLen = wirelessReadStringFromRadio( ATReply, sizeof(ATReply) );
	if( nLen )
	{
		printf("Tx Pwr ");
		switch( ATReply[0] )
		{
			case '0':
				printf("5mW\n");
				break;
			case '1':
				printf("32mW\n");
				break;
			case '2':
				printf("63mW\n");
				break;
			case '3':
				printf("125mW\n");
				break;
			case '4':
				printf("250mW\n");
				break;
			default:
				printf("??\n");
		}	
	}
	
	// RSSI
	memset( ATReply, 0, sizeof(ATReply) );
	strcpy( ATCmd, "ATDB\r" );
	wirelessWriteStringToRadio( ATCmd, strlen( ATCmd ) );
	delayMSecs( 1 );
	nLen = wirelessReadStringFromRadio( ATReply, sizeof(ATReply) );
	if( nLen )
	{
		printf("RSSI dBm -%s\n", ATReply);
	}	
}
/*
 * Output to debug serial port internal radio registers.
 * Used for debug
 */
void wirelessDumpRadioRegs( void )
{
		
	serialDisableInterface(eINTERFACE_WIRELESS);
	
	if( !wirelessEnterATCmdMode( ) )
	{	
		printf("Failed debug dump\n");
		serialEnableInterface(eINTERFACE_WIRELESS);	
		return;
	}
	
	atDumpRadioRegs( );

	wirelessExitATCmdMode( );

	serialEnableInterface(eINTERFACE_WIRELESS);	
}
/*
 * NOTE: Assumes in AT cmd mode already.
 * Restore radio to factory default values.  Modifies the destination
 * address because default is the broadcast address.  So any other factory
 * virgins would reply to any transmits.
 */
static BOOL atRestoreFactoryDefaults( void )
{
	BOOL bStatus = FALSE;
	char ATCmd[16];
	char ATReply[16];
	uint8_t nLen;
	
	if( !inATCmdMode )
	{
		return( bStatus );
	}
	
	// Send cmd's down to radio 
	memset( ATReply, 0, sizeof(ATReply) );
	strcpy( ATCmd, "ATRE\r" );
	wirelessWriteStringToRadio( ATCmd, strlen( ATCmd ) );
	delayMSecs( 1 );
	nLen = wirelessReadStringFromRadio( ATReply, sizeof(ATReply) );
	if( (0 < nLen) || gotOKResponse( ATReply ) )
	{
		// Destination address is broardcast address, need to change
		memset( ATReply, 0, sizeof(ATReply) );		
		strcpy( ATCmd, DEFAULT_DST_ADDR_HIGH );
		wirelessWriteStringToRadio( ATCmd, strlen( ATCmd ) );
		delayMSecs( 2 );
		nLen = wirelessReadStringFromRadio( ATReply, sizeof(ATReply) );
		if( (0 < nLen) && (gotOKResponse( ATReply )) )
		{
			memset( ATReply, 0, sizeof(ATReply) );		
			strcpy( ATCmd, DEFAULT_DST_ADDR_LOW );
			wirelessWriteStringToRadio( ATCmd, strlen( ATCmd ) );
			delayMSecs( 2 );
			nLen = wirelessReadStringFromRadio( ATReply, sizeof(ATReply) );
			if( (0 < nLen) && (gotOKResponse( ATReply )) )
			{
				memset( ATReply, 0, sizeof(ATReply) );
				strcpy( ATCmd, "ATWR\r" );
				wirelessWriteStringToRadio( ATCmd, strlen( ATCmd ) );
				delayMSecs( 1 );
				nLen = wirelessReadStringFromRadio( ATReply, sizeof(ATReply) );
				if( (0 < nLen) && (gotOKResponse( ATReply )) )
				{
					bStatus = TRUE;
					#ifdef DEBUG_RADIO_1
					printf("Factory Reset Passed\n");
					//atDumpRadioRegs( );
					#endif
				}
			}
		}
	}
		
	if( !bStatus )
	{
		printf("Failed factory reset\n");
	}

	return( bStatus );
}		

/*
 * Restore radio to factory default values.  Modifies the destination
 * address because default is the broadcast address.  So any other factory
 * virgins would reply to any transmits.
 */
static BOOL wirelessRestoreFactoryDefaults( void )
{
	BOOL bStatus = FALSE;
	
	serialDisableInterface(eINTERFACE_WIRELESS);
	
	#ifdef DEBUG_RADIO_1
	printf("<wirelessRestoreFactoryDefaults>\n");
	#endif
	
	// Send cmd's down to radio 
	
	if( wirelessEnterATCmdMode( ) )
	{	
		bStatus = atRestoreFactoryDefaults( );
	}
	
	wirelessExitATCmdMode( );

	serialEnableInterface(eINTERFACE_WIRELESS);	
	
	if( !bStatus )
	{
		printf("Failed factory reset\n");
	}
	return( bStatus );
}	

ePLATFORM_TYPE whatPlatformTypeAmI( void )
{
	return( eMyPlatformType );
}
/*
 * Takes in pointer *input to a binary vector, returns 2 ascii characters
 * per each binary byte, and writes them into *output.  No NULL is added.
 */
static void hex2ascii( uint8_t *input, char *output, uint8_t nBytes )
{
	uint8_t presentNibble;
	while( nBytes )
	{
		presentNibble = (*input >> 4) & 0x0F;
		*output++ = (presentNibble <= 9) ? (char)(presentNibble + '0') : (char)(presentNibble + '7');
		presentNibble = *input++ & 0x0F;
		*output++ = (presentNibble <= 9) ? (char)(presentNibble + '0') : (char)(presentNibble + '7');
		nBytes--;
	} 
}
/*
 * Input is 2 characters, (2 bytes), representing a hex number.
 * 0x00 - 0xFF, input must be upper case.
 * Returns a binray byte.
 */
static uint8_t ascii2hex( char *input )
{
	uint8_t number = 0;
	uint8_t loop = 1;
	
	while( loop < 17 )
	{
		if( *input < '9' )
		{
			number += ((uint8_t)(*input++ - '0')) * loop;
		}
		else
		{
			number += ((uint8_t)(*input++ - '7')) * loop;
		}
		loop += 15;
	}
	return( number );
}
/*
 * Converts ADC reading into RSSI value. Assumes that pin 6
 * of the radio module is outputing a PWM signal porportional 
 * to the rssi.  Also assumes that it's filtered and an analog
 * value that's going into the ADC0.  Input is that ADC value.
 * Returns the RSSI value in dBm,  0 to -101
 */
int8_t getRssi( void )
{
	int32_t temp32;
	
	temp32 = (int32_t)ADCGetRssiVoltage( ) * RSSI_SLOPE - RSSI_INTERCEPT;
	return( (int8_t)(temp32 >> 16) );
}
/*
 * Read internal register that contains the RSSI.
 * NOTE: Putting the radio in AT Cmd mode while Rx/Tx is 
 * occurring is NOT recommended.  Use this for diagnostics
 * or debug only!  After receiving a transmission then call
 * this, wait for reply then send again.
 * 
 * Returns byte info 0x00 - 0xFF in -dBm...we have to add
 * the -.
 */
uint8_t readRssi( void )
{
	char ATString[8];
	uint8_t rssi = 0;
	
	serialDisableInterface(eINTERFACE_WIRELESS);
		
	memset( &ATString, 0, sizeof(ATString) );
	
	if( wirelessEnterATCmdMode3Plus( ) )
	{	
		memset( ATString, 0, sizeof(ATString) );
		strcpy( ATString, "ATDB\r" );
		wirelessWriteStringToRadio( ATString, strlen(ATString) );
		delayMSecs( 1 );
		memset( ATString, 0, sizeof(ATString) );
		wirelessReadStringFromRadio( ATString, sizeof(ATString) );
		
		wirelessExitATCmdMode( );
		
		printf("rssi %s\n",ATString);
		// Return from radio is a hex string, convert
		rssi = ascii2hex( ATString );
	}
	else
	{
		printf("Fail reading RSSI\n");
	}
	
	serialEnableInterface(eINTERFACE_WIRELESS);		
	return ( rssi );
}
/*
 * Read out radio's statistics, then clear out counters.
 * If already in AT Cmd mode then don't need the delay,
 * Else should have the delay. 
 */
void getWirelessDiagnostics( BOOL delay )
{
	char ATCmd[8];
	
	
	if( delay )
	{
		delayMSecs( 1100UL );
	}
	
	#ifdef DEBUG_RADIO_1
	printf("\n\n<getWirelessDiagnostics>\n");
	#endif
	
	serialDisableInterface(eINTERFACE_WIRELESS);
		
	if( wirelessEnterATCmdMode( ) )
	{	
		memset( &radioDiagnostics, 0, sizeof(radioDiagnostics_t) );
		// Bytes Tx
		//memset( radioDiagnostics.bytesTransmitted, 0, sizeof(radioDiagnostics.bytesTransmitted) );
		strcpy( ATCmd, "ATBC\r" );
		wirelessWriteStringToRadio( ATCmd, strlen( ATCmd ) );
		delayMSecs( 1 );
		wirelessReadStringFromRadio( radioDiagnostics.bytesTransmitted, sizeof(radioDiagnostics.bytesTransmitted) );
		#ifdef DEBUG_RADIO_1	
		printf("Bytes tx 0x%s\n", radioDiagnostics.bytesTransmitted );
		#endif
		// Rssi
		//memset( radioDiagnostics.rssi, 0, sizeof(radioDiagnostics.rssi) );
		strcpy( ATCmd, "ATDB\r" );
		wirelessWriteStringToRadio( ATCmd, strlen( ATCmd ) );
		delayMSecs( 1 );
		wirelessReadStringFromRadio( radioDiagnostics.rssi, sizeof(radioDiagnostics.rssi) );
		#ifdef DEBUG_RADIO_1	
		printf("RSSI -0x%s\n", radioDiagnostics.rssi );
		#endif
		// Good Rx
		//memset( radioDiagnostics.receivedGoodPackets, 0, sizeof(radioDiagnostics.receivedGoodPackets) );
		strcpy( ATCmd, "ATGD\r" );
		wirelessWriteStringToRadio( ATCmd, strlen( ATCmd ) );
		delayMSecs( 1 );
		wirelessReadStringFromRadio( radioDiagnostics.receivedGoodPackets, sizeof(radioDiagnostics.receivedGoodPackets) );
		#ifdef DEBUG_RADIO_1	
		printf("Rx Good count 0x%s\n", radioDiagnostics.receivedGoodPackets );
		#endif
		// Rx errors....integrity issues
		//memset( radioDiagnostics.receivedErrors, 0, sizeof(radioDiagnostics.receivedErrors) );
		strcpy( ATCmd, "ATER\r" );
		wirelessWriteStringToRadio( ATCmd, strlen( ATCmd ) );
		delayMSecs( 1 );
		wirelessReadStringFromRadio( radioDiagnostics.receivedErrors, sizeof(radioDiagnostics.receivedErrors) );
		#ifdef DEBUG_RADIO_1	
		printf("Rx integrity error 0x%s\n", radioDiagnostics.receivedErrors );
		#endif
		// Bad Mac acks
		//memset( radioDiagnostics.macAckErrors, 0, sizeof(radioDiagnostics.macAckErrors) );
		strcpy( ATCmd, "ATEA\r" );
		wirelessWriteStringToRadio( ATCmd, strlen( ATCmd ) );
		delayMSecs( 1 );
		wirelessReadStringFromRadio( radioDiagnostics.macAckErrors, sizeof(radioDiagnostics.macAckErrors) );
		#ifdef DEBUG_RADIO_1	
		printf("Mac ack t.o. 0x%s\n", radioDiagnostics.macAckErrors );
		#endif		
		// Tx attempts
		//memset( radioDiagnostics.txAttempts, 0, sizeof(radioDiagnostics.txAttempts) );
		strcpy( ATCmd, "ATUA\r" );
		wirelessWriteStringToRadio( ATCmd, strlen( ATCmd ) );
		delayMSecs( 1 );
		wirelessReadStringFromRadio( radioDiagnostics.txAttempts, sizeof(radioDiagnostics.txAttempts) );
		#ifdef DEBUG_RADIO_1	
		printf("Tx attempts 0x%s\n", radioDiagnostics.txAttempts );
		#endif		
		// Tx failures
		//memset( radioDiagnostics.txFailures, 0, sizeof(radioDiagnostics.txFailures) );
		strcpy( ATCmd, "ATTR\r" );
		wirelessWriteStringToRadio( ATCmd, strlen( ATCmd ) );
		delayMSecs( 1 );
		wirelessReadStringFromRadio( radioDiagnostics.txFailures, sizeof(radioDiagnostics.txFailures) );
		#ifdef DEBUG_RADIO_1	
		printf("Tx failures 0x%s\n\n", radioDiagnostics.txFailures );
		#endif		
	}
	else
	{
		
	}
	wirelessExitATCmdMode( );
	
	serialEnableInterface(eINTERFACE_WIRELESS);	

}



#if 0	/************************* Not yet  *********************************/

/*
 * These functions support the simple state machine that is
 * called continously called from the main while loop.  It's
 * here because the XBee radio takes large chunks of time, in
 * the seconds to 2 second time.
 */
void doWirelessState( void )
{
	if( NULL != wirelessState )
	{
		wirelessState( );
	}
}
static void setWirelessState( void(*statePtr)(void) )
{
	wirelessState = statePtr;
}
static void wirelessNopState( void ) { }
/*
 * Issue network discovery command and parse the data stream 
 * for other node(s) on this nwk.  Grab ESN of destination
 * node.
 */
BOOL wirelessNetworkDiso( void )
{
	char ATCmd[16];
	char ATResult[16];
	char ATReply[64];
	char upperESN[16];
	char lowerESN[16];
	uint8_t nLen;
	uint8_t replyIndex = 0;
	uint8_t runningIndex = 0;
	unsigned char rtnChar;
	
	TIMERCTL timerWait;
	initTimer(&timerWait);
	
	serialDisableInterface(eINTERFACE_WIRELESS);
	
	#ifdef DEBUG_RADIO_1
	printf("<wirelessNetworkDiso>\n");
	#endif
	
	// Enter command mode
	
	if( !wirelessEnterATCmdMode( ) )
	{	
		serialEnableInterface(eINTERFACE_WIRELESS);	
		return( FALSE );
	}
	
	// Issue long cmd mode time out, 25 seconds
	startTimer(&timerWait, 25*1000);	
	
	memset( ATReply, 0, sizeof(ATReply) );
	strcpy( ATCmd, "ATCT 0x1000\r" );
	wirelessWriteStringToRadio( ATCmd, strlen( ATCmd ) );
	delayMSecs( 1 );
	nLen = wirelessReadStringFromRadio( ATReply, sizeof(ATReply) );
	if( !gotOKResponse( ATReply ) )
	{
		printf("Disco error 1\n");
		wirelessExitATCmdMode( );
		serialEnableInterface(eINTERFACE_WIRELESS);	
		return( FALSE );
	}
	// Issue nwk disco cmd, takes long time for reply
	memset( ATReply, 0, sizeof(ATReply) );
	strcpy( ATCmd, "ATND\r" );
	wirelessWriteStringToRadio( ATCmd, strlen( ATCmd ) );
	delayMSecs( 1 );
	
	
	// This cmd takes a long time, so keep looking for reply
	memset( ATReply, 0, sizeof(ATReply) );
	do
	{
		if(serialReadWireless(&rtnChar) )
		{
			ATReply[replyIndex++] = rtnChar;
		}	
		if(isTimerExpired(&timerWait))
		{
			break;
		}
		// Look for triple D's, yea baby, which signals end of return data
		// Should be ~ 48 bytes
		if( ('\r' == ATReply[replyIndex-1]) && ('\r' == ATReply[replyIndex-2]) && 
			('\r' == ATReply[replyIndex-3]) && (3 < replyIndex) )
		{
			break;
		}
		watchdogFeed();
	} while( sizeof(ATReply) > replyIndex );
	
	//printf("RI %d\n",replyIndex);
	// Check if exhausted time waiting, didn't find any nodes
	if( 4 >= replyIndex )
	{
		printf("Failed disco\n");
		serialEnableInterface(eINTERFACE_WIRELESS);	
		return( FALSE );
	}
	// Parse; 1.) Look for 1st string returned = FFFE,  2.) 2nd string returned should be
	// the upper 32 bits of the ESN 0013A200.  Which only 0013AXX is guaranteed. 3.) 3ed
	// string is lower 32 bits of ESN	
	do
	{
		memset(ATResult, 0, sizeof(ATResult));
		runningIndex += parseDiscoString( &ATReply[runningIndex], ATResult );
		if( 0 == strcmp( ATResult, "FFFE\r" ) )  // Get 1st string?
		{
			//printf("%s\n",ATResult);
			
			memset(ATResult, 0, sizeof(ATResult));
			runningIndex += parseDiscoString( &ATReply[runningIndex], ATResult );
			
			// Get 2nd string
			if( ('0' == ATResult[0]) && ('0' == ATResult[1]) && ('1' == ATResult[2]) && ('3' == ATResult[3]) )
			{
				strcpy( upperESN, ATResult);	// Save upper ESN in string
				//printf("%s\n",upperESN);
				
				memset(ATResult, 0, sizeof(ATResult));
				runningIndex += parseDiscoString( &ATReply[runningIndex], ATResult );
				strcpy( lowerESN, ATResult);	// Save upper ESN in string
				//printf("%s\n",lowerESN);
			}
			break;
		}
	} while( runningIndex < replyIndex );
	
	// Save destination ESN
	strcpy( ATCmd, "ATDH " );
	wirelessWriteStringToRadio( ATCmd, strlen( ATCmd ) );
	strcpy( ATCmd, upperESN );
	wirelessWriteStringToRadio( ATCmd, strlen( ATCmd ) );
	delayMSecs( 1 );
	nLen = wirelessReadStringFromRadio( ATReply, sizeof(ATReply) );
	if( !gotOKResponse( ATReply ) )
	{
		printf("Did NOT write dst addr high\n");
	}
	// Save destination ESN
	strcpy( ATCmd, "ATDL " );
	wirelessWriteStringToRadio( ATCmd, strlen( ATCmd ) );
	strcpy( ATCmd, lowerESN );
	wirelessWriteStringToRadio( ATCmd, strlen( ATCmd ) );
	delayMSecs( 1 );
	nLen = wirelessReadStringFromRadio( ATReply, sizeof(ATReply) );
	if( !gotOKResponse( ATReply ) )
	{
		printf("Did NOT write dst addr low\n");
	}
	// Write to radio non-volatile
	strcpy( ATCmd, "ATWR\r" );
	wirelessWriteStringToRadio( ATCmd, strlen( ATCmd ) );
	delayMSecs( 1 );
	nLen = wirelessReadStringFromRadio( ATReply, sizeof(ATReply) );
	if( !gotOKResponse( ATReply ) )
	{
		printf("Did NOT write dst addr\n");
	}
		
	// RSSI
	memset( ATReply, 0, sizeof(ATReply) );
	strcpy( ATCmd, "ATDB\r" );
	wirelessWriteStringToRadio( ATCmd, strlen( ATCmd ) );
	delayMSecs( 1 );
	nLen = wirelessReadStringFromRadio( ATReply, sizeof(ATReply) );
	if( nLen )
	{
		printf("RSSI dBm -%s\n", ATReply);
	}

	wirelessExitATCmdMode( );

	serialEnableInterface(eINTERFACE_WIRELESS);	
	
	//wirelessWriteStringToRadio( ATCmd, strlen( ATCmd ) );

	return( TRUE );
	
}

/*
 * Will take string passed in, pInput, and return the string starting
 * at pInput till it hits a CR('\r').  This string is loaded into
 * pOutput.  This returns the # of characters NOT including CR.
 * NOTE: It's up to the calling routine to keep pushing pInput i.e.
 * pInput += nLen
 */
static uint8_t parseDiscoString( char *pInput, char *pOutput )
{
	uint8_t nLen = 0;
	
	do
	{
		*pOutput = *pInput;
		nLen++;
		if( '\r' == *pOutput )
		{
			break;
		}
		*pOutput++;
		*pInput++;
	} while( 16 > nLen );
	return( nLen );
}


#endif	// Not yet

void debugPinHH( uint8_t value )
{
#ifdef DEBUG_PIN_ENABLED

	FIO1DIR |= (OUT << HANDHELD_RF_LRN);

	if( 0 == value ) { 	FIO1CLR = (1<<HANDHELD_RF_LRN); }
	else if( 1 == value ) { FIO1SET = (1<<HANDHELD_RF_LRN); }
	else if (0 == (FIO1PIN & (1<<HANDHELD_RF_LRN))) { FIO1SET = (1<<HANDHELD_RF_LRN); }
	else { 	FIO1CLR = (1<<HANDHELD_RF_LRN); }
#else
	
#endif
}

