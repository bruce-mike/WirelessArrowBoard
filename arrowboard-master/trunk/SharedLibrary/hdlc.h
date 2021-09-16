#ifndef HDLC_H
#define HDLC_H
#include "queue.h"
#include "serial.h"

#ifndef RADIO_V2
#define NO_TRAFFIC_TIMEOUT 1000
#else
#define NO_TRAFFIC_TIMEOUT 5000
#endif
#define NO_TRAFFIC_RETRIES 5

int getNewConnectionsCount(eINTERFACE eInterface);
void packetProcessor(eINTERFACE eInterface, unsigned char* pPacket, int nPacketLength);
BOOL areWeConnectedTo(eINTERFACE eInterface);
BOOL devicesAreConnected(void);
void sendResponse(eINTERFACE eInterface, HDLCPACKET* pHDLCPacket);
BOOL gotNewRxPacket(HDLCPACKET* pReceivedPacket, eINTERFACE* peInterface);

#ifdef RADIO_V2
eINTERFACE whichInterfaceIsActive(void);
void setInterface( eINTERFACE eInterface );
void setDevicePairing( BOOL state );
uint16_t crc16( const uint8_t *pByte, uint8_t length );
void buildQueueXmitPacket( xmitQueueEntry_t *pXmitEntry, eCOMMANDS eCommand, uint8_t *pData, uint8_t dataLength, uint8_t sequenceNum );
void getXmitQueueInfo( xmitQueueEntry_t *pXmitEntry, xmitQueueEntryInfo_t *pInfo );
void spewXmitQueueEntry( xmitQueueEntry_t *pXmitEntry );
BOOL checkRxPacket( const uint8_t *pByte );
uint8_t getQueuePacketLength( xmitQueueEntry_t *pXmitEntry );
uint8_t getRawPacketType( uint8_t *pPacket );
uint8_t getRawPacketCommand( uint8_t *pPacket );
void sendAcknowledgment( eINTERFACE eInterface, HDLCPACKET *hdlcPacket );
void spewHdlcPacket( HDLCPACKET *phdlcPacket );

uint8_t getRawPacketSequenceNum( uint8_t *pPacket );
void printfInterface( eINTERFACE eInterface, BOOL newLine );
void printPlatform( ePLATFORM_TYPE ePlatform, BOOL newLine );
void outputCommand( uint8_t eCommand );
#endif		// RADIO_V2

#endif		// HDLC_H
