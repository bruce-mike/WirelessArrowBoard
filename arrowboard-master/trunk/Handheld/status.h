#include "shareddefs.h"
#include "sharedinterface.h"

void storeStatus(eCOMMANDS eCommand, WORD sData);
WORD getAlarmBitmap(void);
WORD getAlarms(void);
WORD getBatteryVoltage(void);
WORD getSignDisplay(void);
WORD getDisplayError(void);
WORD getAux(void);
WORD getAuxErrors(void);
WORD getSwitches(void);
WORD getTemperature(void);
WORD getActuatorStatus(void);
WORD getActuatorLimit(void);
WORD getLineCurrent(void);
WORD getSystemCurrent(void);
WORD getHandHeldSoftwareVersion(void);
WORD getDriverBoardSoftwareVersion(void);
