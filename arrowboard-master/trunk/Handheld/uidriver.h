#ifndef UI_DRIVER_H
#define UI_DRIVER_H
#include "shareddefs.h"
#include "sharedinterface.h"
#include "queue.h"
#include "timer.h"
#include "gen_UI.h"


#define MAX_ELEMENTS_PER_PAGE 30
#define DEFAULT_PERIODIC_TIME_MS 1000
#define PATTERN_DISPLAY_PERIODIC_TIME_MS 466
#define MAX_PAGE_STACK_DEPTH 20

#define BUTTON_ENABLED_INDEX 0
#define BUTTON_DISABLED_INDEX 1
#define BUTTON_PRESSED_INDEX 2
#define BUTTON_SELECTED_INDEX 3
#define ACTUATOR_AUTO_INDEX_OFFSET 4
#define ACTUATOR_AUTO_STOP_ACTIVE_INDEX  8
#define ACTUATOR_AUTO_STOP_SELECTED_INDEX 9

void uiDriverSetModel(unsigned short nModel);
eMODEL uiDriverGetModel(void);
eACTUATOR_TYPES uiDriverGetActuatorType(void);
void uiDriverSetActuatorType(eACTUATOR_TYPES type);
eACTUATOR_TYPES uiDriverGetActuatorType(void);
void uiDriverSendGetConfig(eINTERFACE eInterface);
void uiDisplayCurrentPage(void);
#if 0
void uiDisplaySelectedRadioButton(RADIO_BUTTON_SELECTION_ELEMENT* pRadioButtonElement);
#endif
void uiRefreshCurrentPage(void);
void uiDisplayElement(ELEMENT* pElement, int nImageIndex);
void uiLoadElement(STORED_ELEMENT* pStoredElement, ELEMENT* pElement);
void uiLoadPage(ID pageID);
void uiDriverPushNewPage(ID pageID);
void uiDriverPopPage(BOOL bLoadNewPage);
void uiDriverPopToMain(void);
void uiLoadUI(ID uiID);
BOOL uiDoesIDExist(ID nID);
void uiDriverInit(void);
void uiDriverHandleTouchScreenPress(int nX, int nY);
void uiDriverDoWork(void);
void uiDriverShowDisplayedPattern(eDISPLAY_TYPES eDisplayType);
int uiDriverGetDisplayPatternIDS(eDISPLAY_TYPES eCurrentDisplayType, unsigned short graphicsIDS[]);
BOOL uiDriverTouchInProgress(void);
void uiSetFirstPage(unsigned int page);

#endif		// UI_DRIVER_H
