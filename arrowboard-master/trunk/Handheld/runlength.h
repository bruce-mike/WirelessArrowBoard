#ifndef RUNLENGTH_H
#define RUNLENGTH_H

#include "sharedinterface.h"
#include "gen_UI.h"

void initFlash(void);
void writeDataFlash(void);

WORD nReadNextByte(void);
UINT nReadInit(ID graphicID, WORD* width, WORD* height);

#endif
