#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "shareddefs.h"
#include "sharedinterface.h"
#include "timer.h"
#include "queue.h"
#include "hdlc.h"
#include "commands.h"
#include "mischardware.h"
#include "adc.h"
#include "flash_nvol.h"
#include "storedconfig.h"
#include "watchdog.h"
#ifdef RADIO_V2
#include "wireless.h"
#endif

int nBoardRev;

void commandDoWork()
{
	HDLCPACKET hdlcPacket;
	eINTERFACE eInterface;
	while( gotNewRxPacket(&hdlcPacket, &eInterface))
	{
		commandProcessor(eInterface, &hdlcPacket);
	}
}
void commandInit()
{
	nBoardRev = hwReadBoardRev();
}
void commandProcessor(eINTERFACE eInterface, HDLCPACKET *pHDLCPacket)
{
	int nCommandEnum;
	
	COMMAND* pCommand = &pHDLCPacket->command;

	do
	{			
				/////
				// so process the command
				/////
				pCommand->nPacketType_Status &= ~PACKET_STATUS_MASK;
				nCommandEnum = pCommand->nPacketType_Status;
		
				#ifdef RSSI_PRINT
				printf("RSSI %d\n",getRssi(ADCGetRssiVoltage( )) );
				#endif
		
				#ifdef PACKET_PRINT
				printf("commandProcessor(%s) cmd:",(1==eInterface)?"wireless":"485"); outputCommand(nCommandEnum);
				printf(" data:0x%X\n",pCommand->nData);//jgr new
				#endif
		
				switch(nCommandEnum)
				{
					case eCOMMAND_GET_MODEL_TYPE:
						{
							pCommand->nData = hwReadModel();
							pCommand->nPacketType_Status |= ePACKET_STATUS_SUCCESS;
							sendResponse(eInterface, pHDLCPacket);
						}
						break;
					case eCOMMAND_REPEATER_CONFIG:
						{
							uint32_t handHeldESN;
							uint32_t seed;

							printf("\neCOMMAND_REPEAETR_CONFIG\n");
							// If 485 & we're assuming that we're already paired with hand held					
							if(eINTERFACE_RS485 == eInterface)
							{												
								handHeldESN = (pCommand->nData<<16) | pCommand->nESNHash;
								printf("Handheld ESN [%X]\n",handHeldESN);
								
								seed = handHeldESN;					// For repeater board use handheld ESN
								if( wirelessSetWancoRepeaterRadio( seed, 0x0000FFFF ) )
								{
									printf("PASSED REPEATER PAIRING\n");
								}
							}
						}
						break;
					case eCOMMAND_DO_PING:
						/* This is presently here to determine if the RS485 channel
						 * is active for the repeater.  Use it for what ever in the future.
						 * The repeater starts the cmd, then the driver board responds, if 
						 * 485 connected, to here.
						 */
						break;
					case eCOMMAND_DO_RESET:
						watchdogReboot();
						break;					
				}	
		#ifdef PACKET_PRINT
		printf("commandProcessor Done\n"); 
		#endif
				
	}while(0);
}
/* 
 * The one and only command initiated by the repeater.  Used to set up the radio 
 * on the repeater for the correct nwk and other parameters.  
 */
void commandSendRepeaterCommand( eCOMMANDS eCommand )
{
	xmitQueueEntry_t XmitEntry;
	HDLCPACKET hdlcPacket;
	uint8_t data[sizeof(HDLCPACKET)] = {0};
		
	memset( &XmitEntry, 0, sizeof(xmitQueueEntry_t) );
	
	memset((unsigned char*)&hdlcPacket, 0, sizeof(hdlcPacket));
	hdlcPacket.nHDLCControl = 0xDEAD;
	hdlcPacket.command.nData = 0xBEEF;
	hdlcPacket.command.nPacketType_Status = eCommand;
	hdlcPacket.command.nData = 0xCAFE;
		
	memcpy( data, &hdlcPacket, sizeof(data) );
	
	buildQueueXmitPacket( &XmitEntry, eCommand, data, sizeof(data), FALSE );
	XmitEntry.eInterface = 	eINTERFACE_RS485;
	XmitEntry.xmitAttempts = 0;
	// Time out is initialized when sent
	#ifdef PACKET_PRINT
	spewXmitQueueEntry( &XmitEntry );
	#endif
	pushXmitQueue( &XmitEntry );
	
}
