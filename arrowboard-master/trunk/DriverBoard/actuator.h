// ACTUATOR.H
#ifndef ACTUATOR_H
#define ACTUATOR_H
#include "commands.h"

#define UP	 			3		// 
#define DOWN 			4		//


#define ACT_IDLE_UP			0
#define ACT_IDLE_DN			1
#define ACT_UP					2
#define ACT_DN					3


#define PWMMR2R 7 
#define PWMMR0R	1
#define PWMMR1R	4

#define PWM2_LATCH_EN 2
// CLUTCH STOP CURRENT (picked from graphs of running current)
//#define ACT_STOP_CURRENT (5)

/////
// DEH DEBUG 060614
/////
#define ACT_STOP_CURRENT (1100)


typedef enum eAcuatorInternalStates
{
	eACTUATOR_INTERNAL_STATE_IDLE,
	eACTUATOR_INTERNAL_STATE_UP_BEFORE_CURRENT_CHECK,
	eACTUATOR_INTERNAL_STATE_DOWN_BEFORE_CURRENT_CHECK,
	eACTUATOR_INTERNAL_STATE_UP_WITH_CURRENT_CHECK,
	eACTUATOR_INTERNAL_STATE_DOWN_WITH_CURRENT_CHECK,
	eACTUATOR_INTERNAL_STATE_ERROR
}eACTUATOR_INTERNAL_STATE;

/////////////////////////////////////////////////
// actuator event definitions
/////////////////////////////////////////////////
typedef enum actuatorEvents
{
	eACTUATOR_EVENT_NOOP,
	eACTUATOR_EVENT_COMMAND_STOP,
	eACTUATOR_EVENT_COMMAND_UP,
	eACTUATOR_EVENT_COMMAND_DOWN,
	eACTUATOR_EVENT_COMMAND_TIMEOUT,
	
	eACTUATOR_EVENT_SWITCH_UP,
	eACTUATOR_EVENT_SWITCH_DOWN,
	eACTUATOR_EVENT_SWITCH_OFF,
	
	eACTUATOR_EVENT_INRUSH_TIMER_EXPIRY,
	eACTUATOR_EVENT_OVERCURRENT,
	
	eACTUATOR_EVENT_UP_LIMIT_SWITCH_CLOSURE,
	eACTUATOR_EVENT_DOWN_LIMIT_SWITCH_CLOSURE,

}eACTUATOR_EVENTS;

#define ACTUATOR_COMMAND_TIMEOUT	2500
#define ACTUATOR_INRUSH_TIMEOUT		1000
#define LIMIT_SWITCH_POWER_WAIT_TIMEOUT 50
#define LIMIT_SWITCH_POWER_IDLE_TIMEOUT 500
#define CALIBRATING_LIMIT_MAX_TIMEOUT 10000

void ads7828SetActuatorStopCurrent(int nStopCurrent);
void actuatorInit(unsigned char bUseLineVolts);
void actuatorDoState(eACTUATOR_EVENTS eEvent);

void actuatorDoWork(void);
ePACKET_STATUS actuatorCommand(unsigned short nsActuatorControl);
eACTUATOR_LIMITS actuatorGetLimitState(void);
unsigned short actuatorGetStatus(void);
void actuatorCalibrate(eACTUATOR_LIMIT_CALIBRATE eStage);

// PCBA_REV_K
#define PCA9634_ACT_CONTROL         0xE6

#define ACT_CONTROL_AUTO_INC_ADDR_0	0x80		// 1st i2c byte loads mode and addrs
#define ACT_CONFIG_MODEREG1					0x00		// Awake
#define ACT_CONFIG_MODEREG2					0x25		// 

#define H_BRIDGE_IO_ADDR						0x8C		// Auto-inc bit 

// Bit locations
#define ACT_PWM 										(1 << 0)
#define ACT_A												(1 << 1)
#define ACT_B												(1 << 2)
#define ACT_ENA											(1 << 3)
#define xA_OK												(1 << 4)
#define xB_OK												(1 << 5)
#define ACT_OPENA										(1 << 6)
#define ACT_OPENB										(1 << 7)


#endif		// ACTUATOR_H

