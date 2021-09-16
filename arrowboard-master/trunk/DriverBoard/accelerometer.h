#ifndef ACCELEROMETER_H
#define ACCELEROMETER_H

#define ACCEL_AUTO_MODE_SAMPLE_TIME_MS 100UL
#define ACCEL_MANUAL_MODE_SAMPLE_TIME_MS 333UL
#define ACCEL_SLAM_TIME_MS 500UL
#define MAX_MOVE_TIME_MS 15000UL

#define X_DOWN_LIMIT ((int16_t)-175)
#define X_UP_LIMIT ((int16_t)-950)
#define Z_DOWN_LIMIT ((int16_t)800)
#define Z_UP_LIMIT ((int16_t)250)

#define NOT_MOVING_THRESHOLD 25
#define NOT_MOVING_COUNT 10	/* 1 second */

#define ACCEL_STARTING_ADDRS 0x02
#define ACCEL_READ_BYTE_LENGTH 6
#define ACCEL_MEAS_DONE 0x01

#define ACCEL_RANGE_ADDRS 0x0F
#define ACCEL_BW_ADDRS 0x10

// NOTE: Make power of 2^N....integer math
#define N_AVG_ACCEL 4

typedef struct
{
	int16_t x[N_AVG_ACCEL];
	int16_t y[N_AVG_ACCEL];
	int16_t z[N_AVG_ACCEL];
	uint8_t index;
} accelerometerRawData_t;

typedef struct
{
	int16_t x;
	int16_t y;
	int16_t z;
} startingPosition_t;

typedef enum
{
	arrowBoardIsUNKNOWN,
	arrowBoardIsUP,
	arrowBoardIsDOWN,
	arrowBoardIsQualifiedUP,	// Up for some time
	arrowBoardIsQualifiedDOWN,	// Down for some time
} presentBoardPosition_t;

typedef struct
{
	int16_t x;
	int16_t y;
	int16_t z;
	uint8_t movingCounterX;
	uint8_t movingCounterZ;
	presentBoardPosition_t boardPosition;
	eACTUATOR_COMMANDS actuatorCmd;
} presentInfo_t;


///////////////////////////
// function prototypes
///////////////////////////
void accelerometerInit(void);
BOOL accelerometerReached90Degrees(void);
void accelerometerDoWork(void);
void accelerometerActuatorIsDown(BOOL bDown);
void accelerometerActuatorStarting( eACTUATOR_COMMANDS cmd );
void accelerometerActuatorStopping( eACTUATOR_COMMANDS cmd );
BOOL isArrowBoardUp( void );
BOOL isArrowBoardDown( void );

#endif				// ACCELEOMETER_H
