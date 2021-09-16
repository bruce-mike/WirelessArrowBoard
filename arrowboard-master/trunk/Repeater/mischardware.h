#ifndef MISC_HARDWARE_H
#define MISC_HARDWARE_H

#define OFF 0
#define ON 1
#define TOGGLE 2

#define LAMP_RAIL_8V4					0			// lamp rail select
#define LAMP_RAIL_12V0					1 		// 

#define LAST_4_BITS 0x0000000F		// MASK FOR LAST 4 BITS
#define	MAKE_PRINTABLE 0x30       // 'OR' model select value with this to make printable char '0', etc

void hwGPIOConfig(void);

void readActuatorLimitSwitch(BOOL *pbLimitSwitchA, BOOL *pbLimitSwitchB);
void hwSetLVD(int action);
BOOL hwIsVLPosPolarity(void);
BOOL hwIsVBPosPolarity(void);
eMODEL hwReadModel(void);
int  hwReadBoardRev(void);
void hwSetLT4363_ALT_SLR(int action) ;
BOOL is80DegreeTilt(void);
void hwSetSysLED(void);
void hwClrSysLED(void);
void hwPatWatchDog(void);
WORD hwReadIndicatorErrors(void);
WORD hwReadDriveErrors(void);

void hwSetRedLED( void );
void hwClrRedLED( void );

#endif

