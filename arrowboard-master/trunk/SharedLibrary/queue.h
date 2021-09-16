#ifndef QUEUE_H
#define QUEUE_H
typedef struct queue_entry
{
	struct queue_entry* next;
	//unsigned char data[1];
}QUEUE_ENTRY;
typedef struct queue
{
	QUEUE_ENTRY *head;
}QUEUE;
void queueInitialize(QUEUE* pQueue);
void queueInitializeI2C(QUEUE* pQueue);
void queueAdd(QUEUE* pQueue, QUEUE_ENTRY* pEntry);
QUEUE_ENTRY* queuePop(QUEUE* pQueue);
//QUEUE_ENTRY* queuePeek(QUEUE* pQueue, QUEUE_ENTRY* pPrevEntry);

#ifdef RADIO_V2
//jgr#define PACKET_TIMEOUT (2000)
#define PACKET_TIMEOUT (3000)
// Raw Packet  |  Marker |Frame Ctl|  Seq #  |  Data   | ........| CRC lsb | CRC msb |

#define PACKET_TYPE_DATA 0
#define PACKET_TYPE_ACK 1

typedef struct
{
	uint8_t type;
	uint8_t length;
	uint8_t sequenceNum;
	uint16_t data;
	uint8_t command;
	uint16_t crc;
} xmitQueueEntryInfo_t;

#define MAX_PACKET_BYTES 16

// Basic object/blob that'a placed on the xmit queue
typedef struct
{
	TIMERCTL pktTimer;				// Used for time out
	uint8_t xmitAttempts;			// # of times transmitted packet
	eINTERFACE eInterface;
	uint8_t packet[MAX_PACKET_BYTES];
	
} xmitQueueEntry_t;

#define MAX_XMIT_QUEUE_ENTRIES 16

void pushXmitQueue( xmitQueueEntry_t *pNewXmitEntry );
void popXmitQueue( uint8_t sequenceNum );
BOOL anythingToXmit( xmitQueueEntry_t *pNextXmitEntry );
uint8_t getQueuePacketType( xmitQueueEntry_t *pXmitEntry );
uint8_t getQueuePacketSequenceNum( xmitQueueEntry_t *pXmitEntry );
BOOL findQueueSequence( uint8_t seqNumber );
#endif // RADIO_V2

#endif		// QUEUE_H
