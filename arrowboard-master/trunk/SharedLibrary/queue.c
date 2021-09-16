#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "shareddefs.h"
#include "timer.h"
#include "queue.h"

#include "hdlc.h"
#include <assert.h>
extern void spewXmitQueueEntry( xmitQueueEntry_t *pXmitEntry ); //debug

static xmitQueueEntry_t xmitQueue[MAX_XMIT_QUEUE_ENTRIES];
static uint8_t presentXmitQueueIndex = 0;
static uint8_t nextXmitQueueIndex = 0;


void queueInitialize(QUEUE* pQueue)
{
	memset( xmitQueue, 0, sizeof(xmitQueue) );
	presentXmitQueueIndex = 0;
	nextXmitQueueIndex = 0;
}
void queueInitializeI2C(QUEUE* pQueue)
{
	pQueue->head = NULL;
}
/*
 * Push all of xmitQueueEntry_onto the queue i.e. meta data, and
 * raw packet.  Will push onto queue if there's room.  If nextXmitQueueIndex
 * plus 1 == presentXmitQueueIndex, then queue is full.  Throw request
 * on the floor.
 */
void pushXmitQueue( xmitQueueEntry_t *pNewXmitEntry )
{
	uint8_t temp;
	
	// Check if queue is full
	temp = nextXmitQueueIndex + 1;
	if( temp >= sizeof(xmitQueue)/sizeof(xmitQueue[0]) )
	{
		temp = 0;
	}
	if( temp == presentXmitQueueIndex )
	{
		printf("Queue FULL, seq %d thrown out\n", getQueuePacketSequenceNum( pNewXmitEntry ));
    setInterface( eINTERFACE_UNKNOWN ); //jgr
    setDevicePairing( FALSE );
		return;
	}
	#ifdef PACKET_PRINT
	printf("Push queue onto[%d] ", nextXmitQueueIndex);
	#endif
	memcpy( &xmitQueue[nextXmitQueueIndex], pNewXmitEntry, sizeof(xmitQueueEntry_t) );
		
	nextXmitQueueIndex++;
	if( nextXmitQueueIndex >= sizeof(xmitQueue)/sizeof(xmitQueue[0]) )
	{
		nextXmitQueueIndex = 0;
	}
	#ifdef PACKET_PRINT
	printf("next[%d]\n", nextXmitQueueIndex);
	#endif
	//assert( nextXmitQueueIndex != presentXmitQueueIndex );	// Check for OV
}
/*
 * Pop off the present active queue entry.  Make sure the sequence #'s 
 * agree.  presentXmitQueueIndex points at the present/active entry, if
 * the sequenceNum == the sequnce # of the present/active entry, then 
 * increment presentXmitQueueIndex.
 */
void popXmitQueue( uint8_t sequenceNum )
{
	xmitQueueEntry_t *pPresentXmitEntry;
	
	pPresentXmitEntry = &xmitQueue[presentXmitQueueIndex];
	
	if( sequenceNum == getQueuePacketSequenceNum( pPresentXmitEntry ) )
	{
		presentXmitQueueIndex++;
		if( presentXmitQueueIndex >= sizeof(xmitQueue)/sizeof(xmitQueue[0]) )
		{
			presentXmitQueueIndex = 0;
		}	
		#ifdef PACKET_PRINT
		printf("Popped queue seq:%d\n",sequenceNum);
		#endif
	}
	else if( !findQueueSequence( sequenceNum ) )	// Look for something already popped
	{																							// occurs when bad rssi
		printf("ERROR: Seq # %d!= %d\n", sequenceNum, getQueuePacketSequenceNum( pPresentXmitEntry ));
    // Remove assert cause if wireless channel out for long time, sequence numbers will wrap
    // and sequence numbers won't be sequnential, cause queue full and will throw new requests away. 
		//assert( FALSE );  // Differnt sequence #
	}
	else
	{
		// Dup
    printf("\nDup and out of order, seq already popped, seq:%d\n", sequenceNum);
	}
}
/*
 * If queue index's == then nothing in queue to xmit...Done here.
 * Else if 1st attempted to xmit, then return TRUE to xmit packet.
 * Else, (Tx is active), look for time out on xmit'ed packet. If not
 * timed out....return FALSE, still waiting for ack. 
 *
 * If timed out and retries left, then xmit again.  Else if retries 
 * are exhausted, bail on that packet go onto the next.
 */
BOOL anythingToXmit( xmitQueueEntry_t *pNextXmitEntry )
{
	// Queue empty?
	if( presentXmitQueueIndex == nextXmitQueueIndex )
	{
		//printf("Queues ==\n");
		return( FALSE );
	}
	// Else load up possible new xmit 
	memcpy( pNextXmitEntry, &xmitQueue[presentXmitQueueIndex], sizeof(xmitQueueEntry_t) );
	
	// 1st time thru?
	if( 0 == pNextXmitEntry->xmitAttempts )
	{
      initTimer( &pNextXmitEntry->pktTimer );
      startTimer( &pNextXmitEntry->pktTimer, /* If pairing command, increase timeout */
							(eCOMMAND_WIRELESS_CONFIG == pNextXmitEntry->packet[3]) ? (5*PACKET_TIMEOUT) : PACKET_TIMEOUT );	

      pNextXmitEntry->xmitAttempts++;
      #ifdef PACKET_PRINT
		printf("1st time thru Seq # %d\n",pNextXmitEntry->packet[2]);
      #endif
		memcpy( &xmitQueue[presentXmitQueueIndex], pNextXmitEntry, sizeof(xmitQueueEntry_t) );		// Save new info into queue
		return( TRUE );
	}
	// Packet out there, have we timed out?
	else if( isTimerExpired(&pNextXmitEntry->pktTimer) )
	{
		pNextXmitEntry->xmitAttempts++;
      printf("Retry %d\n",pNextXmitEntry->xmitAttempts); //jgr
		// Any retries left?
		if( 3 < pNextXmitEntry->xmitAttempts )
		{
			// Exhausted all retries, move on
			presentXmitQueueIndex++;
			if( presentXmitQueueIndex >= sizeof(xmitQueue)/sizeof(xmitQueue[0]) )
			{
				presentXmitQueueIndex = 0;
			}
			printf("XMIT EXHAUSTED RETIES, I/F Unknown, Removing Seq # %d from queue\n",pNextXmitEntry->packet[2]);
			setInterface( eINTERFACE_UNKNOWN ); 
			setDevicePairing( FALSE );
			return( FALSE );
		}
		else	// Timed out and have retries left
		{
      initTimer( &pNextXmitEntry->pktTimer );
			startTimer( &pNextXmitEntry->pktTimer, PACKET_TIMEOUT );	
			printf("\nXmit timed out, Retrying Seq# %d Retry# %d\n",pNextXmitEntry->packet[2],pNextXmitEntry->xmitAttempts);
			memcpy( &xmitQueue[presentXmitQueueIndex], pNextXmitEntry, sizeof(xmitQueueEntry_t) );		// Save new info into queue
      return( TRUE );
    }
	}
	else 	// Have xmit'ed, but not timed out, waiting for ack
	{
		return( FALSE );
	}
}

uint8_t getQueuePacketSequenceNum( xmitQueueEntry_t *pXmitEntry )
{
	return( pXmitEntry->packet[2] );
}

uint8_t getQueuePacketType( xmitQueueEntry_t *pXmitEntry )
{
	return( (pXmitEntry->packet[1] & (0x03 << 6)) >> 6 );
}

void queueAdd(QUEUE* pQueue, QUEUE_ENTRY* pEntry)
{
	int nLimitCount = 10000;
	QUEUE_ENTRY* pNode = pQueue->head;
	pEntry->next = NULL;
	if(NULL == pNode)
	{
		pQueue->head = pEntry;
	}
	else
	{
		if(pNode != pEntry)
		{
			while(NULL != pNode->next)
			{
				if(0 >= nLimitCount--)
				{
					printf("queueAdd-0 Loop Detected\n");
					pNode = NULL;
					break;
				}
				if(pNode->next == pEntry)
				{
					printf("queueAdd-1 Loop Detected\n");
					pNode = NULL;
					break;
				}
				pNode = pNode->next;
			}
			if(NULL != pNode)
			{
				pNode->next = pEntry;
			}
		}
		else
		{
			printf("queueAdd-2 Loop Detected\n");
		}
	}
}

QUEUE_ENTRY* queuePop(QUEUE* pQueue)
{
	QUEUE_ENTRY* pEntry = pQueue->head;
	if(NULL != pEntry)
	{
		pQueue->head = pEntry->next;
		pEntry->next = NULL;
	}
	return pEntry;
}
/*
 * Looks to see if an entry/sequence number has been the 
 * recent history of the queue.
 * If so then reture TRUE  else FALSE
 * In bad rssi, sometimes get dup's
 */
BOOL findQueueSequence( uint8_t seqNumber )
{
	uint8_t queueIndex;
	
	for( queueIndex = 0; queueIndex < sizeof(xmitQueue)/sizeof(xmitQueue[0]); queueIndex++ )
	{
		if( seqNumber == getQueuePacketSequenceNum( &xmitQueue[queueIndex] ) )
		{
			printf("Dup seq at %d",seqNumber);
			return( TRUE );
		}
	}
	return( FALSE );
}
