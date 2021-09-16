#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "shareddefs.h"
#include "sharedinterface.h"
#include "timer.h"
#include "queue.h"
#include "serial.h"
#include "hdlc.h"
#include "storedconfig.h"
#include "wireless.h"


#ifdef LINUX
BOOL bDiscardNext = FALSE;
extern BOOL bIsController;
extern BOOL bTossSendPacket;
extern int nNextNS;
extern char* pHandheldOffset;
#else
char* pHandheldOffset = "";
#endif

static BOOL incomingCmdReady = FALSE;
static HDLCPACKET storedHdlcPacket;
static eINTERFACE eStoredCmdInterface;

static BOOL bPaired = FALSE;
static eINTERFACE eConnectToInterface = eINTERFACE_UNKNOWN;
static int nNewConnectionsCount[INTERFACE_COUNT];

static void changeNewCommandStatus( BOOL bStatus )
{
	incomingCmdReady = bStatus;
}
static BOOL gotNewCommand( void )
{
	return( incomingCmdReady );
}


int getNewConnectionsCount(eINTERFACE eInterface)
{
	return nNewConnectionsCount[(int)eInterface];
}

/*
 * This accepts *pPacket, which is a complete recieved packet. This 
 * function is called from buildPacket(), which is continously called 
 * from serialDoWork().  This function will make sure packet is valid
 * via checkRxPacket(), then determine the type of packet; is it an 
 * ack or a command?  If ack, remove from queue, else command.
 * In both cases call a new command flag is set.
 *
 */
void packetProcessor(eINTERFACE eInterface, unsigned char* pPacket, int nPacketLength)
{
	HDLCPACKET hdlcPacket; 	// Used to spoof commandProcessor
	
	if( !checkRxPacket( pPacket ) )
	{
		printf("\nFAIL: packetProcessor() bad pkt\n");//jgr new
		return;
	}

	// To get here means packet is valid
	// For now copy only hdlc portion
	memset( &hdlcPacket, 0, sizeof(HDLCPACKET) );
	memcpy( &hdlcPacket, &pPacket[4], sizeof(HDLCPACKET) );
	
	// Still spoofing, save in stored packet and set new cmd flag to let commandProcessor() take over
	memcpy((unsigned char*)&storedHdlcPacket, (unsigned char*)&hdlcPacket, sizeof(HDLCPACKET));
	changeNewCommandStatus( TRUE );
	
	// Show we're talking to an i/f
	eStoredCmdInterface = eInterface;
	setDevicePairing( TRUE );
	
	#ifdef PACKET_PRINT
	printf("\nIncoming packetProcessor type:%s  seq:%d  cmd:0x%X  time:%d\n",((PACKET_TYPE_DATA == getRawPacketType( pPacket )) ? "data" : "ack"), 
										 																						getRawPacketSequenceNum( pPacket ), pPacket[3], getTimerNow());
	#endif
	
	// If recieving an ack, command done, remove command from queue.
	if( PACKET_TYPE_ACK == getRawPacketType( pPacket ) )
	{
		popXmitQueue( getRawPacketSequenceNum( pPacket ) );
	}
	// Else must be new command, spoof cmd processing by taking new protocol/format
	// packet and filling in hdlc packet format. nData & nPacketType_Status are filled
	// in exactly, nChecksum = sequenceNum, nESNHash[1] = cmd.
	#ifdef PACKET_PRINT
	printf("packetProcessor Done\n"); 
	#endif
}

//=======================================================================================
// Public interface
//=======================================================================================
eINTERFACE whichInterfaceIsActive( void )
{
	return eConnectToInterface;
}


void setInterface( eINTERFACE eInterface )
{
	if( eInterface == eINTERFACE_UNKNOWN )
	{
		setDevicePairing( FALSE );
	}
	if( eConnectToInterface == eInterface )
	{
		return;
	}
	
	eConnectToInterface = eInterface;

	printf("New I/F is "); printfInterface( eConnectToInterface, TRUE );
}

BOOL areWeConnectedTo(eINTERFACE eInterface)
{
	return( eInterface == eConnectToInterface );
}
eINTERFACE whickInterfaceWeConntedTo( void )
{
	return( eConnectToInterface );
}

BOOL devicesAreConnected( void )
{
	return( bPaired );
}

/*
 * Continously called from commandDoWork() within main loop.
 * Returns TRUE if got new command.  If we got a new packet
 * will copy it into *pReceivedPacket.
 *
 */
BOOL gotNewRxPacket(HDLCPACKET* pReceivedPacket, eINTERFACE* peInterface )
{
	BOOL bRetVal = FALSE;
	
	if( gotNewCommand( ) )
	{
		memcpy((unsigned char*)pReceivedPacket, (unsigned char*)&storedHdlcPacket, sizeof(HDLCPACKET));
		
		*peInterface = eStoredCmdInterface;
		changeNewCommandStatus( FALSE );
		bRetVal = TRUE;
	}
	return bRetVal;
}
/*
 * Called from within commandProcessor() after command executed
 * Spoof from received hdlcPacket
 */
void sendResponse(eINTERFACE eInterface, HDLCPACKET* pHDLCPacket)
{
	sendAcknowledgment( eInterface, pHDLCPacket );
}

/*
 * Generates the CRC of a byte stream.  Uses the standard CRC-16/CCITT
 * algorithm.  Polynomial is 0x1021, same as X.25, XModem, etc.
 * Inputs are pointer to array and length.
 */
uint16_t crc16( const uint8_t *pByte, uint8_t length )
{
	uint8_t x;
  uint16_t crc = 0xFFFF;

  while (length--)
	{
		x = (crc >> 8) ^ *pByte++;
		x ^= x >> 4;
		crc = (crc << 8) ^ ((uint16_t)(x << 12)) ^ ((uint16_t)(x <<5)) ^ ((uint16_t)x);
	}
  return crc;
}
/*
 * Will determine if passed in, *pByte, is a valid packet.  Does this
 * by first checking marker byte is correct, next get the length,
 * next generate the CRC across the packet, and finally check
 * that the 2 CRC's are ==
 */
BOOL checkRxPacket( const uint8_t *pByte )
{
	uint16_t calcCRC;
	uint16_t pktCRC;
	uint8_t length;
	
	if( 0x7E != pByte[0] )
	{
		printf("ERROR Bad marker byte\n");
		return( FALSE );
	}
	length = pByte[1] & 0x3F;
	pktCRC = LOW_HIGH_TO_16( pByte[length-1], pByte[length] );
	//pktCRC = ((uint16_t)pByte[length] << 8) | ((uint16_t)pByte[length-1]);
	calcCRC = crc16( pByte, length - 1 );
	if( pktCRC != calcCRC )
	{
		printf("ERROR CRC 0x%X != 0x%X\n",calcCRC, pktCRC);
		return( FALSE );
	}
	return( TRUE );
}
/*
 * Builds up raw packet that is transmitted over the air. The packet's Frame Control 
 * is set based on the length & if it's an ack packet.  Ack packets have eCommand msb 
 * set and will use the passed in AckSequenceNum for the sequence #.
 */
void buildQueueXmitPacket( xmitQueueEntry_t *pXmitEntry, eCOMMANDS eCommand, uint8_t *pData, uint8_t dataLength, uint8_t AckSequenceNum )
{
	static uint8_t sequenceNumber = 0;
	uint16_t crc;
	uint8_t i = 0;
	
	pXmitEntry->packet[i++] = 0x7E;
	pXmitEntry->packet[i++] = 0;				// Holder for data type and length
	pXmitEntry->packet[i++] = sequenceNumber;
	pXmitEntry->packet[i++] = eCommand & 0x7F;
	while( dataLength-- )
	{
		pXmitEntry->packet[i++] = *pData++;
	}
	// Add in length
	pXmitEntry->packet[1] |= i + 2 - 1;		// +2 for CRC and -1 for 0x7E
	
	// Ack?
	if ( eCommand & (1 << 7) )
	{
		pXmitEntry->packet[1] |= (PACKET_TYPE_ACK << 6);
		pXmitEntry->packet[2] = AckSequenceNum;
	}
	else	// Sending cmd
	{
		pXmitEntry->packet[11] = sequenceNumber;	// Add in seq # into checksum
		sequenceNumber++;
		if( 0x3F < sequenceNumber )
		{
			sequenceNumber = 0;
		}
	}
	// NOTE: This is fubared, got to be better way
	// Pairing command, upper 16 ESN in nData, lower 16 ESN in nESNHash
	// Upper 16 stored prior to this function, add in lower 16 here.
	if( eCOMMAND_WIRELESS_CONFIG == (eCommand & 0x7F) )
	{
		uint32_t ourESN = wirelessGetOurESN( );		// Valid from wirelessInit()
		printf("Sending ourESN = 0x%X\n", ourESN);
		pXmitEntry->packet[6] = (ourESN >> 16) & 0x0000000FF;
		pXmitEntry->packet[7] = (ourESN >> 24) & 0x0000000FF;
		pXmitEntry->packet[8] = (ourESN >> 0) & 0x0000000FF;
		pXmitEntry->packet[9] = (ourESN >> 8) & 0x0000000FF;
	}

	// Add in CRC
	crc = crc16( &pXmitEntry->packet[0], i );
	pXmitEntry->packet[i++] = (uint8_t)(crc & 0x00FF);
	pXmitEntry->packet[i++] = (uint8_t)((crc >> 8) & 0x00FF);
}

void getXmitQueueInfo( xmitQueueEntry_t *pXmitEntry, xmitQueueEntryInfo_t *pInfo )
{
	pInfo->type = pXmitEntry->packet[1] >> 6;
	pInfo->length = pXmitEntry->packet[1] & 0x3F;
	pInfo->sequenceNum = pXmitEntry->packet[2];
	pInfo->command = pXmitEntry->packet[3];
	pInfo->data = LOW_HIGH_TO_16( pXmitEntry->packet[6], pXmitEntry->packet[7] );
	pInfo->crc = LOW_HIGH_TO_16( pXmitEntry->packet[pInfo->length-1], pXmitEntry->packet[pInfo->length] );
}

/*
 * Got here because completed a command.  Return any data and 
 * set up packet for response/ack.  Set msb in command to 
 * signal that's it's a response.  Push onot Queue.
 */
void sendAcknowledgment( eINTERFACE eInterface, HDLCPACKET *hdlcPacket )
{
	xmitQueueEntry_t XmitEntry;
	uint8_t data[sizeof(HDLCPACKET)];

	memset( &XmitEntry, 0, sizeof(xmitQueueEntry_t) );
	
	memcpy( data, hdlcPacket, sizeof(HDLCPACKET) );
	buildQueueXmitPacket( &XmitEntry, (eCOMMANDS)(hdlcPacket->command.nPacketType_Status | 0x80), 
												data, sizeof(data), (uint8_t)(hdlcPacket->nChecksum) );
	XmitEntry.eInterface = eInterface;
	XmitEntry.xmitAttempts = 0;
	
	// Time out is initialized when sent
	#ifdef PACKET_PRINT
	printf("Send Reply "); spewXmitQueueEntry( &XmitEntry ); // Debug
	#endif
	
	pushXmitQueue( &XmitEntry );	
	
	return;
}
/* Sets bPaired based on passed in state.  If the
 * present state != passed in state, change to new
 * passed in state and increment connection count.
 */
void setDevicePairing( BOOL state )
{
	if( bPaired == state )
	{
		return;
	}
	else
	{
		bPaired = state;
		if( bPaired )
		{
			nNewConnectionsCount[whickInterfaceWeConntedTo( )]++;
		}
	}
}
/******************* Debug  **********************/
uint8_t getQueuePacketLength( xmitQueueEntry_t *pXmitEntry )
{
	return( 1 + pXmitEntry->packet[1] & 0x3F );
}
uint8_t getRawPacketType( uint8_t *pPacket )
{
	return( (pPacket[1] & (0x03 << 6)) >> 6 );
}
uint8_t getRawPacketSequenceNum( uint8_t *pPacket )
{
	return( pPacket[2] );
}
void spewXmitQueueEntry( xmitQueueEntry_t *pXmitEntry )
{
	xmitQueueEntryInfo_t info;
	HDLCPACKET hdlcPacket;
	
	getXmitQueueInfo( pXmitEntry, &info );
	
		printf("Seq:%d i/f:",info.sequenceNum ); printfInterface( pXmitEntry->eInterface, FALSE );
		printf(" type:%s len:%d cmd:0x%X data:0x%X attempts:%d\n",((0 == info.type) ? "data" : "ack"), info.length, info.command,
																								               info.data, pXmitEntry->xmitAttempts );
	
	memcpy( (uint8_t*)&hdlcPacket, (uint8_t*)&pXmitEntry->packet[4], sizeof(HDLCPACKET) );

	spewHdlcPacket( &hdlcPacket );


	#if DEBUGOUT >= 7
	{
		uint8_t i;
		printf("[");
		for( i = 0; i <= info.length; i++ )
		{
			printf("0x%X ",pXmitEntry->packet[i] );
		}
		printf("]\n");
	}
	#endif
}
void spewHdlcPacket( HDLCPACKET *phdlcPacket )
{
	printf("ctl:0x%X  nData:0x%X  hash:0x%X  cmd/status:0x%X  chksum:0x%X\n", phdlcPacket->nHDLCControl, 
																																						phdlcPacket->command.nData,
																																						phdlcPacket->command.nESNHash,
																																						phdlcPacket->command.nPacketType_Status,
																																						phdlcPacket->nChecksum);
}
void printfInterface( eINTERFACE eInterface, BOOL newLine )
{
	switch( eInterface ) 
	{
		case eINTERFACE_DEBUG: printf("Debug"); break;
		case eINTERFACE_WIRELESS: printf("Wireless"); break;
		case eINTERFACE_RS485: printf("RS485"); break;
		default: printf("Unknown"); break; 
	} 
	if( newLine )
	{
		printf("\n");
	}
}
void printPlatform( ePLATFORM_TYPE ePlatform, BOOL newLine )
{
	switch( ePlatform )
	{
		case ePLATFORM_TYPE_ARROW_BOARD:
			printf("Arrow board");
			break;
		case ePLATFORM_TYPE_HANDHELD:
			printf("Handheld board");
			break;
		default:
			printf("Unknown board");
			break;
	}
	if( newLine )
	{
		printf("\n");
	}
}
/*
 * Will output uint8_t hex # followed by string of command.
 * No CRLF added
 */
void outputCommand( uint8_t eCommand )
{
	printf("0x%X=",eCommand );
	
	switch( eCommand )
	{
		case eCOMMAND_GET_MODEL_TYPE:
			printf("[Get Model Type]");
			break;
		case eCOMMAND_DISPLAY_CHANGE:
			printf("[Display Chhange]");
			break;
		case eCOMMAND_ACTUATOR_SWITCH:
			printf("[Actuator Switch]");
			break;
		case eCOMMAND_STATUS_SIGN_DISPLAY:
			printf("[Status Sign Display]");
			break;
		case eCOMMAND_STATUS_AUX:
			printf("[Status Aux]");
			break;
		case eCOMMAND_STATUS_SWITCHES:
			printf("[Status Switches]");
			break;
		case eCOMMAND_STATUS_ACTUATOR:
			printf("[Status Actuator]");
			break;
		case eCOMMAND_STATUS_ACTUATOR_LIMIT:
			printf("[Status Actuator Limit]");
			break;
		case eCOMMAND_WIRELESS_CONFIG:
			printf("[Wireless Config]");
			break;
		case eCOMMAND_REPEATER_CONFIG:
			printf("[Repeater Config]");
			break;
//		case eCOMMAND_DO_PING:
//			printf("[Ping]");
//			break;
		case eCOMMAND_STATUS_ALARMS:
			printf("[Status Alarms]");
			break;
		case eCOMMAND_STATUS_INDICATOR_ERRORS:
			printf("[Status Indicator Error]");
			break;
		case eCOMMAND_STATUS_LINE_VOLTAGE:
			printf("[Status Line Voltage]");
			break;
		case eCOMMAND_STATUS_BATTERY_VOLTAGE:
			printf("[Status Battery Voltage]");
			break;
		case eCOMMAND_STATUS_DISPLAY_ERRORS:
			printf("[Status Display Error]");
			break;
		case eCOMMAND_STATUS_AUX_ERRORS:
			printf("[Status Aux Errors]");
			break;
		case eCOMMAND_STATUS_TEMPERATURE:
			printf("[Status Temp]");
			break;
		case eCOMMAND_STATUS_PHOTOCELL_ERRORS:
			printf("Status Photocell Errors]");
			break;
		case eCOMMAND_SET_ACTUATOR_TYPE:
			printf("[Set Actuator Type]");
			break;
		case eCOMMAND_GET_ACTUATOR_TYPE:
			printf("[Get Actuator Type]");
			break;
		case eCOMMAND_SET_DISALLOWED_PATTERNS:
			printf("[Set Disallowed Patterns]");
			break;
		case eCOMMAND_GET_DISALLOWED_PATTERNS:
			printf("[Get Disallowed Patterns]");
			break;
		case eCOMMAND_SET_BRIGHTNESS_CONTROL:
			printf("[Set Brightness]");
			break;
		case eCOMMAND_GET_BRIGHTNESS_CONTROL:
			printf("[Get Brightness]");
			break;
		case eCOMMAND_SET_AUX_BATTERY_TYPE:
			printf("[Set Aux Battery Type]");
			break;
		case eCOMMAND_GET_AUX_BATTERY_TYPE:
			printf("[Get Aux Battery Type]");
			break;
		case eCOMMAND_SET_ACTUATOR_BUTTON_MODE:
			printf("[Set Actuator Button Mode]");
			break;
		case eCOMMAND_GET_ACTUATOR_BUTTON_MODE:
			printf("[Get Actuator Button Mode]");
			break;
		case eCOMMAND_DO_ACTUATOR_CALIBRATION:
			printf("[Actuator Calibrate]");
			break;
		case eCOMMAND_DO_RESET:
			printf("[Reset]");
			break;
		case eCOMMAND_SET_ACTUATOR_CALIBRATION:
			printf("[Set Actuator Calibration]");
			break;
		case eCOMMAND_GET_ACTUATOR_CALIBRATION:
			printf("[Get Actuator Calibration]");
			break;			
		case eCOMMAND_STATUS_LINE_CURRENT:
			printf("[Status Line Current]");
			break;
		case eCOMMAND_STATUS_SYSTEM_CURRENT:
			printf("[Status System Current]");
			break;			
		case eCOMMAND_STATUS_HANDHELD_SOFTWARE_VERSION:
			printf("[Status Handheld Software Ver]");
			break;
		case eCOMMAND_STATUS_DRIVER_BOARD_SOFTWARE_VERSION:
			printf("[Status Driver Software Ver]");
			break;
		default:
			printf("[Unknown]");
			break;
	}
}
