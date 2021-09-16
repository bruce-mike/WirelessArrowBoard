#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "shareddefs.h"
#include "sharedinterface.h"
#include "timer.h"
#include "queue.h"
#include "hdlc.h"
#include "pwm.h"
#include "commands.h"
#include "mischardware.h"
#include "display.h"
#include "actuator.h"
#include "lm73.h"
#include "adc.h"
#include "errorinput.h"
#include "leddriver.h"
#include "flash_nvol.h"
#include "spiFlash.h"
#include "storedconfig.h"
#include "watchdog.h"
#include "ADS7828.h"
#ifdef RADIO_V2
#include "wireless.h"
#endif
void actuatorAndSolarSetup(void);
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
        // process the command
        pCommand->nPacketType_Status &= ~PACKET_STATUS_MASK;
        nCommandEnum = pCommand->nPacketType_Status;
		
		#ifdef RSSI_PRINT
		printf("RSSI %d\n",getRssi( ) );
		#endif
		
		#ifdef PACKET_PRINT
		printf("commandProcessor(%s) cmd:",(1==eInterface)?"wireless":"485"); outputCommand(nCommandEnum);
		printf(" data:0x%X\n",pCommand->nData);//jgr new
		#endif
		
        switch(nCommandEnum)
        {
            case eCOMMAND_GET_MODEL_TYPE:
                pCommand->nData = hwReadModel();
                pCommand->nPacketType_Status |= ePACKET_STATUS_SUCCESS;
                sendResponse(eInterface, pHDLCPacket);
                break;
				
            case eCOMMAND_DISPLAY_CHANGE:
                // return current pattern as part of response
                pCommand->nPacketType_Status |= (unsigned char)displayChange(pCommand->nData);
                storedConfigSetDisplayType((eDISPLAY_TYPES)pCommand->nData);
                sendResponse(eInterface, pHDLCPacket);
                break;

            case eCOMMAND_ACTUATOR_SWITCH:
                // issue the actuator command
                // update the packet status
                actuatorCommand(pCommand->nData);
                pCommand->nPacketType_Status |= ePACKET_STATUS_SUCCESS;
                pCommand->nData = (actuatorGetStatus()<<8)|(unsigned short)actuatorGetLimitState();
                sendResponse(eInterface, pHDLCPacket);
                break;
				
            case eCOMMAND_STATUS_ALARMS:
                pCommand->nData = errorInputGetAlarms();
                pCommand->nPacketType_Status |= ePACKET_STATUS_SUCCESS;
                sendResponse(eInterface, pHDLCPacket);
                break;
				
            case eCOMMAND_STATUS_LINE_VOLTAGE:
            case eCOMMAND_STATUS_BATTERY_VOLTAGE:
                pCommand->nData = ADCGetSystemVoltage();
                pCommand->nPacketType_Status |= ePACKET_STATUS_SUCCESS;
                sendResponse(eInterface, pHDLCPacket);
                break;
								
            case eCOMMAND_STATUS_SIGN_DISPLAY:
                pCommand->nData = displayGetCurrentPattern();
                pCommand->nPacketType_Status |= ePACKET_STATUS_SUCCESS;
                sendResponse(eInterface, pHDLCPacket);
                break;

            case eCOMMAND_STATUS_DISPLAY_ERRORS:
                pCommand->nData = displayGetErrors();
                pCommand->nPacketType_Status |= ePACKET_STATUS_SUCCESS;
                sendResponse(eInterface, pHDLCPacket);
                break;
				
            case eCOMMAND_STATUS_AUX:
                pCommand->nData = ledDriverGetAuxStatus();
                pCommand->nPacketType_Status |= ePACKET_STATUS_SUCCESS;
                sendResponse(eInterface, pHDLCPacket);
                break;
				
            case eCOMMAND_STATUS_AUX_ERRORS:
                pCommand->nData = errorInputGetAuxErrors();
                pCommand->nPacketType_Status |= ePACKET_STATUS_SUCCESS;
                sendResponse(eInterface, pHDLCPacket);
                break;
				
            case eCOMMAND_STATUS_SWITCHES:
                pCommand->nData = errorInputGetSwitchData();
                pCommand->nPacketType_Status |= ePACKET_STATUS_SUCCESS;
                sendResponse(eInterface, pHDLCPacket);
                break;
				
            case eCOMMAND_STATUS_TEMPERATURE:
                pCommand->nData = lm73GetDegreesC(TRUE);
                pCommand->nPacketType_Status |= ePACKET_STATUS_SUCCESS;
                sendResponse(eInterface, pHDLCPacket);
			    break;
            
			case eCOMMAND_STATUS_ACTUATOR:
                pCommand->nData = (actuatorGetStatus()<<8)|(unsigned short)actuatorGetLimitState();
                pCommand->nPacketType_Status |= ePACKET_STATUS_SUCCESS;
                sendResponse(eInterface, pHDLCPacket);
                break;
				
            case eCOMMAND_STATUS_ACTUATOR_LIMIT:
                pCommand->nData = (unsigned short)actuatorGetLimitState();
                pCommand->nPacketType_Status |= ePACKET_STATUS_SUCCESS;
                sendResponse(eInterface, pHDLCPacket);
                break;
				
            case eCOMMAND_WIRELESS_CONFIG:
            {
                uint32_t seed;
                uint32_t senderESN;
                uint32_t ourESN;			
                printf("\neCOMMAND_WIRELESS_CONFIG\n");
                //pCommand->nPacketType_Status |= ePACKET_STATUS_NOT_SUPPORTED;
							
                senderESN = (pCommand->nData<<16) | pCommand->nESNHash;							
                if(eINTERFACE_RS485 == eInterface)
                {												
                    storedConfigSetSendersESN( senderESN );
								
                    //ourESN = wirelessReadOutRadiosESN( );		// This reads out of the radio
                    ourESN = wirelessGetOurESN( );
								
                    seed = senderESN;							// For arrow board use handheld ESN
                    if( wirelessSetWancoRadio( seed, senderESN ) )
                    {
                        pCommand->nPacketType_Status |= ePACKET_STATUS_SUCCESS;
                        printf("PASSED PAIRING\n");//jgr new
                    }
                }
                // Respond with our ESN, high in nData,  low in nESNHash
                pCommand->nData = (ourESN >> 16) & 0x0000FFFFUL;
                pCommand->nESNHash = ourESN & 0x00000FFFFUL;
                sendResponse(eInterface, pHDLCPacket);
            }
            break;
            
            case eCOMMAND_STATUS_INDICATOR_ERRORS:
            #ifdef RADIO_V2		/* Do this because expect an ack that was originally not sent */
                pCommand->nData = 0;
                pCommand->nPacketType_Status |= ePACKET_STATUS_SUCCESS;
                sendResponse(eInterface, pHDLCPacket);
            #endif
                break;
            
            case eCOMMAND_STATUS_PHOTOCELL_ERRORS:
            #if 1//def RADIO_V2
                pCommand->nData = 0;
                pCommand->nPacketType_Status = 0;
                //pHDLCPacket->nHDLCControl = PF_BIT | UF_BIT;
                sendResponse(eInterface, pHDLCPacket);			
            #endif	
                break;
				
            case eCOMMAND_SET_ACTUATOR_TYPE:
                switch(pCommand->nData)
                {
                    case eACTUATOR_TYPE_NONE:
                    case eACTUATOR_TYPE_90_DEGREE_POWER_TILT:
                    case eACTUATOR_TYPE_180_DEGREE_POWER_TILT:
                        pCommand->nPacketType_Status |= ePACKET_STATUS_SUCCESS;
                        storedConfigSetActuatorType((eACTUATOR_TYPES)pCommand->nData);
                        break;
						
                    default:
                        pCommand->nPacketType_Status |= ePACKET_STATUS_NOT_SUPPORTED;
                        break;
                }
                sendResponse(eInterface, pHDLCPacket);
                break;
				
            case eCOMMAND_GET_ACTUATOR_TYPE:
                pCommand->nData = storedConfigGetActuatorType();
                pCommand->nPacketType_Status |= ePACKET_STATUS_SUCCESS;
                sendResponse(eInterface, pHDLCPacket);
                break;
				
            case eCOMMAND_SET_BRIGHTNESS_CONTROL:
                switch(pCommand->nData)
                {
                    case eBRIGHTNESS_CONTROL_NONE:
                    case eBRIGHTNESS_CONTROL_AUTO:
                    case eBRIGHTNESS_CONTROL_MANUAL_BRIGHT:
                    case eBRIGHTNESS_CONTROL_MANUAL_MEDIUM:
                    case eBRIGHTNESS_CONTROL_MANUAL_DIM:
                        pwmSetLampBrightnessControl((eBRIGHTNESS_CONTROL)pCommand->nData);
                        storedConfigSetArrowBoardBrightnessControl((eBRIGHTNESS_CONTROL)pCommand->nData);
                        pCommand->nPacketType_Status |= ePACKET_STATUS_SUCCESS;
                        break;
						
                    default:
                        pCommand->nPacketType_Status |= ePACKET_STATUS_NOT_SUPPORTED;
                        break;
                }
                sendResponse(eInterface, pHDLCPacket);
                break;
				
                case eCOMMAND_GET_BRIGHTNESS_CONTROL:
                    pCommand->nData = storedConfigGetArrowBoardBrightnessControl();
                    pCommand->nPacketType_Status |= ePACKET_STATUS_SUCCESS;
                    sendResponse(eInterface, pHDLCPacket);
                    break;
					
                case eCOMMAND_SET_AUX_BATTERY_TYPE:
                    //storedConfigSetAuxBatteryType((eAUX_BATTERY_TYPES)pCommand->nData);
                    pCommand->nPacketType_Status |= ePACKET_STATUS_SUCCESS;
                    sendResponse(eInterface, pHDLCPacket);
                    break;
                
                case eCOMMAND_GET_AUX_BATTERY_TYPE:
                    pCommand->nData = 0; //storedConfigGetAuxBatteryType();
                    pCommand->nPacketType_Status |= ePACKET_STATUS_SUCCESS;
                    sendResponse(eInterface, pHDLCPacket);
                    break;
					
                case eCOMMAND_SET_DISALLOWED_PATTERNS:
                    storedConfigSetDisallowedPatterns(pCommand->nData);
                    pCommand->nPacketType_Status |= ePACKET_STATUS_SUCCESS;
                    sendResponse(eInterface, pHDLCPacket);
                    break;
					
                case eCOMMAND_GET_DISALLOWED_PATTERNS:
                    pCommand->nData = storedConfigGetDisallowedPatterns();
                    pCommand->nPacketType_Status |= ePACKET_STATUS_SUCCESS;
                    sendResponse(eInterface, pHDLCPacket);
                    break;
					
                case eCOMMAND_SET_ACTUATOR_BUTTON_MODE:
                    storedConfigSetActuatorButtonMode((eACTUATOR_BUTTON_MODE)pCommand->nData);
                    pCommand->nPacketType_Status |= ePACKET_STATUS_SUCCESS;
                    sendResponse(eInterface, pHDLCPacket);
                    break;
					
                case eCOMMAND_GET_ACTUATOR_BUTTON_MODE:
                    pCommand->nData = storedConfigGetActuatorButtonMode();
                    pCommand->nPacketType_Status |= ePACKET_STATUS_SUCCESS;
                    sendResponse(eInterface, pHDLCPacket);
                    break;
					
                case eCOMMAND_DO_ACTUATOR_CALIBRATION:
						// DEH 020315
                        // disable this for the ATSSA show
                    break;
					
                case eCOMMAND_SET_ACTUATOR_CALIBRATION:
                    storedConfigSetActuatorCalibrationHint((eACTUATOR_CALIBRATION_HINT)pCommand->nData);
                    pCommand->nPacketType_Status |= ePACKET_STATUS_SUCCESS;
                    sendResponse(eInterface, pHDLCPacket);
                    break;
					
                case eCOMMAND_GET_ACTUATOR_CALIBRATION:
                    pCommand->nData = storedConfigGetActuatorCalibrationHint();
                    pCommand->nPacketType_Status |= ePACKET_STATUS_SUCCESS;
                    sendResponse(eInterface, pHDLCPacket);
                    break;
					
                case eCOMMAND_DO_RESET:
                    watchdogReboot();
                    break;					
					
                case eCOMMAND_STATUS_LINE_CURRENT:
                    pCommand->nData = ADCGetLineCurrent();
                    pCommand->nPacketType_Status |= ePACKET_STATUS_SUCCESS;
                    sendResponse(eInterface, pHDLCPacket);
                    break;						
					
                case eCOMMAND_STATUS_SYSTEM_CURRENT:
                    pCommand->nData = nADS7828GetIS();
                    pCommand->nData = ADCGetSystemCurrent();
                    pCommand->nPacketType_Status |= ePACKET_STATUS_SUCCESS;
                    sendResponse(eInterface, pHDLCPacket);
                    break;	
					
                case eCOMMAND_STATUS_DRIVER_BOARD_SOFTWARE_VERSION:
                    // DRIVER_BOARD_SOFTWARE_VERSION
                    // Located in commands.h
                    pCommand->nData = (WORD)DRIVER_BOARD_SOFTWARE_VERSION;
                    pCommand->nPacketType_Status |= ePACKET_STATUS_SUCCESS;
                    sendResponse(eInterface, pHDLCPacket);
                    break;							
								
                case eCOMMAND_GET_RSSI_VALUE:
                    pCommand->nData = ADCGetRssiVoltage( );
                    pCommand->nPacketType_Status |= ePACKET_STATUS_SUCCESS;
                    sendResponse(eInterface, pHDLCPacket);
                    break;
								
                default:
                #if 1//def RADIO_V2
                    pCommand->nData = 0;
                    pCommand->nPacketType_Status = 0;
                    //pHDLCPacket->nHDLCControl = PF_BIT | UF_BIT;
                    sendResponse(eInterface, pHDLCPacket);			
                #endif	
                    break;
            }
				
        #ifdef PACKET_PRINT
        printf("commandProcessor Done\n"); 
        #endif
		
    }while(0);
}
