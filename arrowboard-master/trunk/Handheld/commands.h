#ifndef COMMANDS_H
#define COMMANDS_H
void commandSendCommand(eINTERFACE eInterface, eCOMMANDS eCommand, unsigned short nData);
void commandDoWork(void);
void commandInit(void);
void commandProcessor(eINTERFACE eInterface, HDLCPACKET *pHDLCPacket, BOOL bResponseRequested);

#endif
