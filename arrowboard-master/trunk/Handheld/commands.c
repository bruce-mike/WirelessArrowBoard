#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "shareddefs.h"
#include "sharedinterface.h"
#include "timer.h"
#include "queue.h"
#include "hdlc.h"
#include "commands.h"
#include "commboard.h"
#include "flash_nvol.h"
#include "spiFlash.h"
#include "status.h"
#include "uidriver.h"
#include "menufunctions.h"
#include "storedconfig.h"
#ifdef RADIO_V2
#include "wireless.h"
#endif
static unsigned short nReturnData = 0;
	
#ifdef RADIO_V2
/*
 * Spoof over the air packet.  Data in new protocol is pertinent info
 * of the hdlc protocol
 */
void commandSendCommand(eINTERFACE eInterface, eCOMMANDS eCommand, unsigned short nData)
{
	xmitQueueEntry_t XmitEntry;
	HDLCPACKET hdlcPacket;
	uint8_t data[sizeof(HDLCPACKET)] = {0};
	
	memset( &XmitEntry, 0, sizeof(xmitQueueEntry_t) );
	
	memset((unsigned char*)&hdlcPacket, 0, sizeof(hdlcPacket));
	hdlcPacket.command.nPacketType_Status = eCommand;
	hdlcPacket.command.nData = nData;
		
	memcpy( data, &hdlcPacket, sizeof(data) );
	
	buildQueueXmitPacket( &XmitEntry, eCommand, data, sizeof(data), FALSE );
	XmitEntry.eInterface = eInterface;
	XmitEntry.xmitAttempts = 0;
	// Time out is initialized when sent
	#ifdef PACKET_PRINT
	spewXmitQueueEntry( &XmitEntry );
	#endif
	pushXmitQueue( &XmitEntry );
	

}

#else

void commandSendCommand(eINTERFACE eInterface, eCOMMANDS eCommand, unsigned short nData)
{
	HDLCPACKET hdlcPacket;
	memset((unsigned char*)&hdlcPacket, 0, sizeof(hdlcPacket));
	hdlcPacket.command.nPacketType_Status = eCommand;
	hdlcPacket.command.nData = nData;
	hdlcSendCommand(eInterface, &hdlcPacket);	
}
#endif	// RADIO_V2

void commandSendResponse(eINTERFACE eInterface, HDLCPACKET* pHDLCPacket, unsigned short nData)
{
	#ifdef PACKET_PRINT
	printf("commandSendResponse\n");
	#endif
	pHDLCPacket->command.nData = nData;
	sendResponse(eInterface, pHDLCPacket);
}

void commandDoWork()
{
	HDLCPACKET hdlcPacket;
	eINTERFACE eInterface;
	while(gotNewRxPacket(&hdlcPacket, &eInterface))
	{
		commandProcessor(eInterface, &hdlcPacket, FALSE );
	}
}

void commandInit()
{
}

void commandProcessor(eINTERFACE eInterface, HDLCPACKET *pHDLCPacket, BOOL bResponseRequested)
{
	int nCommandEnum;
	
	COMMAND* pCommand = &pHDLCPacket->command;
	
	// If RS485 cable, handheld does no commands
	// If wireless, handheld does no RS485
	if(isRS485Power())
	{
		if(eINTERFACE_WIRELESS == eInterface)
		{
			return;
		}
	}
	else
	{
		if(eINTERFACE_RS485 == eInterface)
		{
			return;
		}
	}

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
					printf(" data:0x%X...\n",pCommand->nData);
				#endif

				switch(nCommandEnum)
				{
					case eCOMMAND_GET_MODEL_TYPE:
						{
							uiDriverSetModel(pCommand->nData);
							/////
							// model changed
							// make sure pattern display is up to date
							/////
							resetPatternImages();
						}
						break;
					case eCOMMAND_DISPLAY_CHANGE:
						{
							setCurrentDisplayType((eDISPLAY_TYPES)pCommand->nData);
						}
						break;
					case eCOMMAND_ACTUATOR_SWITCH:
						{
							/////
							// this response contains actuator status
							// so grab it here
							/////
							storeStatus(eCOMMAND_STATUS_ACTUATOR,((pCommand->nData>>8)&0x00ff));
							storeStatus(eCOMMAND_STATUS_ACTUATOR_LIMIT,(pCommand->nData&0x00ff));
						}
						break;
					case eCOMMAND_STATUS_ALARMS:
            {
							storeStatus(eCOMMAND_STATUS_ALARMS,pCommand->nData);
						}
						break;
					case eCOMMAND_STATUS_LINE_VOLTAGE:
						{
							storeStatus(eCOMMAND_STATUS_LINE_VOLTAGE,pCommand->nData);
						}
						break;
					case eCOMMAND_STATUS_BATTERY_VOLTAGE:
						{
							storeStatus(eCOMMAND_STATUS_BATTERY_VOLTAGE,pCommand->nData);
						}
						break;
					case eCOMMAND_STATUS_SIGN_DISPLAY:
						{
							setCurrentDisplayType((eDISPLAY_TYPES)pCommand->nData);
						}
						break;
					case eCOMMAND_STATUS_DISPLAY_ERRORS:
						{
							storeStatus(eCOMMAND_STATUS_DISPLAY_ERRORS,pCommand->nData);
						}
						break;
					case eCOMMAND_STATUS_AUX:
						{
							storeStatus(eCOMMAND_STATUS_AUX,pCommand->nData);
						}
						break;
					case eCOMMAND_STATUS_AUX_ERRORS:
						{
							storeStatus(eCOMMAND_STATUS_AUX_ERRORS,pCommand->nData);
						}
						break;
					case eCOMMAND_STATUS_SWITCHES:
						{
							storeStatus(eCOMMAND_STATUS_SWITCHES,pCommand->nData);
						}
						break;
					case eCOMMAND_STATUS_TEMPERATURE:
						{
							storeStatus(eCOMMAND_STATUS_TEMPERATURE,pCommand->nData);
						}
						break;
					case eCOMMAND_STATUS_ACTUATOR:
						{
							storeStatus(eCOMMAND_STATUS_ACTUATOR,((pCommand->nData>>8)&0x00ff));
							storeStatus(eCOMMAND_STATUS_ACTUATOR_LIMIT,(pCommand->nData&0x00ff));
              uiDriverSetActuatorType((eACTUATOR_TYPES)(pCommand->nData&0x00ff));
						}
						break;
					case eCOMMAND_WIRELESS_CONFIG:
						{
							uint32_t seed;
							uint32_t senderESN;
							
							printf("\neCOMMAND_WIRELESS_CONFIG\n");
							
							senderESN = (pCommand->nData << 16) | pCommand->nESNHash;
							
							if(eINTERFACE_RS485 == eInterface)
							{
								storedConfigSetSendersESN( senderESN );
								//seed = wirelessReadOutRadiosESN( );		// For handheld board use our own ESN
								seed = wirelessGetOurESN( );
								
								if( wirelessSetWancoRadio( seed, senderESN ) )
								{
									pCommand->nPacketType_Status |= ePACKET_STATUS_SUCCESS;
									printf("PASSED PAIRING\n");//jgr new
								}
							}
						}
						break;
					case eCOMMAND_STATUS_ACTUATOR_LIMIT:
						{
							storeStatus(eCOMMAND_STATUS_ACTUATOR_LIMIT,pCommand->nData);
						}
						break;
					case eCOMMAND_STATUS_PHOTOCELL_ERRORS:
						storeStatus(eCOMMAND_STATUS_PHOTOCELL_ERRORS,pCommand->nData);
						break;
#if 0
					case eCOMMAND_ACTUATOR_STOP:
						break;
					case eCOMMAND_ACTUATOR_UP:	
						break;
					case eCOMMAND_ACTUATOR_DOWN:
						break;
#endif
					case eCOMMAND_SET_ACTUATOR_TYPE:
						break;
					case eCOMMAND_GET_ACTUATOR_TYPE:
						UpdateActuatorType((eACTUATOR_TYPES)pCommand->nData);
						break;
					case eCOMMAND_SET_DISALLOWED_PATTERNS:
						break;
					case eCOMMAND_GET_DISALLOWED_PATTERNS:
						UpdateDisallowedPatterns(pCommand->nData);
						break;
					case eCOMMAND_SET_BRIGHTNESS_CONTROL:
						break;
					case eCOMMAND_GET_BRIGHTNESS_CONTROL:
						UpdateArrowBoardBrightnessControl((eBRIGHTNESS_CONTROL)pCommand->nData);
						break;
					case eCOMMAND_SET_AUX_BATTERY_TYPE:
						break;
					case eCOMMAND_GET_AUX_BATTERY_TYPE:
						//UpdateAuxBatteryType((eAUX_BATTERY_TYPES)pCommand->nData);
						break;
					case eCOMMAND_SET_ACTUATOR_BUTTON_MODE:
						break;
					case eCOMMAND_GET_ACTUATOR_BUTTON_MODE:
						UpdateActuatorButtonMode((eACTUATOR_BUTTON_MODE)pCommand->nData);
						break;
					case eCOMMAND_SET_ACTUATOR_CALIBRATION:
						break;
					case eCOMMAND_GET_ACTUATOR_CALIBRATION:
						UpdateActuatorCalibrationHint((eACTUATOR_CALIBRATION_HINT)pCommand->nData);
						break;
					case eCOMMAND_STATUS_LINE_CURRENT:
						storeStatus(eCOMMAND_STATUS_LINE_CURRENT,pCommand->nData);
						break;
					case eCOMMAND_STATUS_SYSTEM_CURRENT:
						storeStatus(eCOMMAND_STATUS_SYSTEM_CURRENT,pCommand->nData);
						break;					
					case eCOMMAND_STATUS_DRIVER_BOARD_SOFTWARE_VERSION:
						storeStatus(eCOMMAND_STATUS_DRIVER_BOARD_SOFTWARE_VERSION,pCommand->nData);
						break;
					default:
						break;
				}
		
		#ifdef PACKET_PRINT
		printf("commandProcessor Done\n"); 
		#endif
				
		if(bResponseRequested)
		{
			commandSendResponse(eInterface, pHDLCPacket, nReturnData--);
		}

	}while(0);
}
