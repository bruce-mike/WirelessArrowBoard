#ifndef COMMANDS_H
#define COMMANDS_H

//==============================
// Driver Board Software Version
//==============================
//#define DRIVER_BOARD_SOFTWARE_VERSION 113
#define DRIVER_BOARD_SOFTWARE_VERSION 65432

void commandDoWork(void);
void commandInit(void);
void commandProcessor(eINTERFACE eInterface, HDLCPACKET *pHDLCPacket);

#endif
