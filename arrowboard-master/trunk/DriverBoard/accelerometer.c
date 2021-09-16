//////////////////////////////////////////////////////
// Bosch BMA255 3-Axis signed 12 bit accelerometer.
// Device has built in digital filtering.  The band
// width is based on the programmable sample rate of
// the axis info.  In addition to the devices filtering
// a moving average filter in code is added.
//
// 	X-axis is in plane with the PCBA and parallel to short side.  
//  Y-axis is in plane with the PCBA and parallel to long side,
//  Z-axis is perpendicular to the plane of the PCB.
//
// We use only the X and Z axis for measurements.
//
//////////////////////////////////////////////////////
#include <string.h>
#include <stdio.h>
#include "shareddefs.h"
#include "sharedinterface.h"
#include "timer.h"
#include "queue.h"
#include "i2c.h"
#include "math.h"
#include "actuator.h"
#include "mischardware.h"
#include "storedconfig.h"
#include "accelerometer.h"
#include <LPC23xx.H>
#include "ssp.h"

// Uncomment to see readings, state, etc.
//#define ACCEL_OUTPUT_DEBUG

#ifdef ACCEL_BMA255

static TIMERCTL sampleTimer;
static TIMERCTL limitTimer;
static accelerometerRawData_t accelerometerRawData;
static presentInfo_t presentInfo;
static void(*accelStatePtr)(void);
static uint8_t accelCounter = 0;

// Fwd declarations
static BOOL inAccelState( void(*func)(void) );
static void setNextAccelState( void(*func)(void) );
static void accelerometerNopState( void );
static void accelerometerInitState( void );
static void accelerometerDatumState( void );
static void accelerometerActiveState( void );
static void accelerometerTimeSlamState( void );
static BOOL inAutoMode( void );

/*
 * Will read _length_ bytes of data from consecutive addresses
 * in the acceleromter, starting at _address_.  Responsibility 
 * of calling function to make sure there's enough room for 
 * returning data. 
 */
static void spiAccelReadBytes( uint8_t address, uint8_t *readData, uint8_t length )
{
	uint8_t i;
	
	/* Wait if busy(BSY) or TX FIFO full(TNF) */
	while( (SSP1SR & SSPSR_BSY) | !(SSP1SR & SSPSR_TNF)   );

	// Assert CS
	FIO1CLR |= (1 << uP_SPI1_xCS_ACCEL);	
	
	SSP1DR = address | (1 << 7);	// Send address and read bit	
	
	while( SSP1SR & SSPSR_BSY );	// Wait for address to send
	readData[0] = SSP1DR;					// Clear out any crap
	
	for ( i = 0; i < length; i++ )
	{
		/* Wait if TX FIFO full. */
		while ( (SSP1SR & SSPSR_TNF) != SSPSR_TNF );
		SSP1DR = 0xFF;
	}

	// Wait till all data flushed
	while( SSP1SR & SSPSR_BSY );
	// De-assert CS
	FIO1SET |= (1 << uP_SPI1_xCS_ACCEL);
	
	// Read data	
	for ( i = 0; i < length; i++ )
	{
		readData[i] = SSP1DR;
	}
	// Clear out crap, if there
	while( SSP1SR & SSPSR_RNE )
	{
		(void)SSP1DR;
	}
}
/*
 * Will write _length_ bytes of data to consecutive addresses
 * starting at _address_.  Responsibility of calling function to 
 * make sure there's enough data to write.
 */
static void spiAccelWriteBytes( uint8_t address, uint8_t *writeData, uint8_t length )
{
	uint8_t i;
	
	/* Wait if busy(BSY) or TX FIFO full(TNF) */
	while( (SSP1SR & SSPSR_BSY) | !(SSP1SR & SSPSR_TNF)   );

	// Assert CS
	FIO1CLR |= (1 << uP_SPI1_xCS_ACCEL);	
	
	SSP1DR = address & 0x7F;			// Send address and write bit, msb = 0
	
	while( SSP1SR & SSPSR_BSY );	// Wait for address to send
	i = SSP1DR;										// Clear out any crap
	
	for ( i = 0; i < length; i++ )
	{
		/* Wait if TX FIFO full. */
		while ( (SSP1SR & SSPSR_TNF) != SSPSR_TNF );
		SSP1DR = writeData[i];
	}

	// Wait till all data flushed
	while( SSP1SR & SSPSR_BSY );
	// De-assert CS
	FIO1SET |= (1 << uP_SPI1_xCS_ACCEL);
	
	// Clear out crap, if there
	while( SSP1SR & SSPSR_RNE )
	{
		(void)SSP1DR;
	}
}
/*
 * Will read all 3 accelerometer's axis readings.  Will place them
 * in array pointed to by *xAccel, *yAccel, *zAccel.  Will start
 * at X-axis and read consecutive addresses.  Done this way cause 
 * info in device is arranged in consecutive addresses, and we have 
 * shadowing enabled, which means have to read LSB 1st, to ensure that 
 * new data isn't over writing data as it is read.
 */
static void readAccelerometer( int16_t *xAccel, int16_t *yAccel, int16_t *zAccel )
{
	uint8_t readData[ACCEL_READ_BYTE_LENGTH] = {0,};
	int16_t temp16;
	
	// Read all acceleration values
	spiAccelReadBytes( ACCEL_STARTING_ADDRS, readData, sizeof(readData) );
	
	// Make sure data is fresh, then get all 12 bits, then sign extend.
	// X-axis
	*xAccel = 0;
	if( readData[0] & ACCEL_MEAS_DONE )
	{
		temp16 = (readData[1] << 8) & 0xFF00;
		temp16 |= (readData[0] & 0x00F0);
		*xAccel = temp16 >> 4;
	}
	// Y-axis
	*yAccel = 0;
	if( readData[2] & ACCEL_MEAS_DONE )
	{
		temp16 = (readData[3] << 8) & 0xFF00;
		temp16 |= (readData[2] & 0x00F0);
		*yAccel = temp16 >> 4;
	}
	// Z-axis
	*zAccel = 0;
	if( readData[4] & ACCEL_MEAS_DONE )
	{
		temp16 = (readData[5] << 8) & 0xFF00;
		temp16 |= (readData[4] & 0x00F0);
		*zAccel = temp16 >> 4;
	}
}
/*
 * Returns to passed in pointers the moving average of
 * each axis.  Assumes that the raw data has been read
 * and is in the raw data vectors/struct.
 */
static void getAccelAverage( int16_t *xAvg, int16_t *yAvg, int16_t *zAvg )
{
	int32_t sumx = 0;
	int32_t sumy = 0;
	int32_t sumz = 0;
	uint8_t i;
	
	for( i = 0; i < N_AVG_ACCEL; i++ )
	{
		sumx += accelerometerRawData.x[i];
		sumy += accelerometerRawData.y[i];
		sumz += accelerometerRawData.z[i];
	}
	*xAvg = (int16_t)(sumx/(int32_t)N_AVG_ACCEL);
	*yAvg = (int16_t)(sumy/(int32_t)N_AVG_ACCEL);
	*zAvg = (int16_t)(sumz/(int32_t)N_AVG_ACCEL);
}

#if 0	/* Relative procedure 2 */
/*
 * Will try to determine if the arrow board has moved 90 degrees from the
 * starting position OR has reached ending position.  
 * 	X-axis is in plane with the PCBA and parallel to short side.  
 *  Y-axis is in plane with the PCBA and parallel to long side,
 *  Z-axis is perpendicular to the plane of the PCB.
 * 
 */
static void getPresentArrowBoardInfo( void )
{
	// Check if moved 90 degrees
	if( ((ABS16(presentInfo.x - endingPosition.x) < 50
		) &&
	  	((ABS16(presentInfo.z - endingPosition.z)) < 50)) 
	#if 1
			/* Check absolute criteria for up */
			|| ((presentInfo.x < -1000) && 
					(eACTUATOR_COMMAND_MOVE_UP == presentInfo.actuatorCmd ) &&
					(presentInfo.z < 400))
			/* Check absolute criteria for down */
			|| ((presentInfo.z > 1000) && 
					(eACTUATOR_COMMAND_MOVE_DOWN == presentInfo.actuatorCmd ) &&
					(presentInfo.x > -500)) )
	#endif
	{
		if( ABS16(presentInfo.z) > ABS16(presentInfo.x) )
		{
			presentInfo.boardPosition = arrowBoardIsDOWN;
		}
		else
		{
			presentInfo.boardPosition = arrowBoardIsUP;
		}
	}

	#ifdef ACCEL_OUTPUT_DEBUG
	printf("Abs: t:%d x:%d y:%d z:%d\n",getTimerNow( ),presentInfo.x,presentInfo.y,presentInfo.z);
	#endif
}
#endif

#if 1	/* Vibrate procedure */
/*
 * Will try to determine if the arrow board has hit the stops.
 * Does this by 1.) Looking at absolute X & Z axis, then when that 
 * is meet 2.) will look for the 'noise' by looking at the delta 
 * in the X, Y and Z axis.
 * 	X-axis is in plane with the PCBA and parallel to short side.  
 *  Y-axis is in plane with the PCBA and parallel to long side,
 *  Z-axis is perpendicular to the plane of the PCB.
 * 
 */
static void getPresentArrowBoardInfo( void )
{
	static int16_t lastX, lastY, lastZ;
	int16_t deltaX, deltaY, deltaZ;
	
	deltaX = ABS16(presentInfo.x - lastX);
	deltaY = ABS16(presentInfo.y - lastY);
	deltaZ = ABS16(presentInfo.z - lastZ);
	
	// 1.) Determine if absolute position is meet for UP
	if( eACTUATOR_COMMAND_MOVE_UP == presentInfo.actuatorCmd )
	{
		if(	((X_UP_LIMIT > presentInfo.x) && (Z_UP_LIMIT > presentInfo.z)) ||
				( 3 <= accelCounter ) )
		{
			accelCounter++;
			// 2.) Is it vibrating
			if( (50 < deltaX) || (100 < deltaY) || (250 < deltaZ) )
			{
				presentInfo.boardPosition = arrowBoardIsUP;
			}
		}
	}
	// 1.) Determine if absolute position is meet for DOWN
	else if( eACTUATOR_COMMAND_MOVE_DOWN == presentInfo.actuatorCmd )
	{
		if( ((X_DOWN_LIMIT < presentInfo.x) && (Z_DOWN_LIMIT < presentInfo.z)) ||
				( 3 <= accelCounter ) )
		{
			accelCounter++;
			// 2.) Is it vibrating
			if( (50 < deltaX) || (100 < deltaY) || (250 < deltaZ) )
			{
				presentInfo.boardPosition = arrowBoardIsDOWN;
			}
		}
	}
	else		// Didn't meet any of the criteria
	{
		if( 0 != accelCounter )
		{
			accelCounter--;
		}
		if( ABS16(deltaZ) > 1000 )
		{
			printf("DRUM");
		}
	}
	// Delay
	lastX = presentInfo.x;
	lastY = presentInfo.y;
	lastZ = presentInfo.z;

	#ifdef ACCEL_OUTPUT_DEBUG
	printf("Abs: t:%d x:%d y:%d z:%d\n",getTimerNow( ),presentInfo.x,presentInfo.y,presentInfo.z);
	printf("Counter %d  deltaX %d  deltaY %d  deltaZ %d\n",accelCounter,deltaX,deltaY,deltaZ);
	#endif
}
#endif

/*
 * Set up accelerometer to have; 
 * - a range of +/-2g's, 
 * - filter data with bandwidth of ~16Hz
 * - in active power mode
 * - shadow reads enabled.
 * NOTE: Update rate is 2X the filter rate.
 * Assumes FIFO is in default state = ByPass mode
 */
void accelerometerInit( void )
{
	uint8_t initData[] = {0x03, 0x09, 0x00, 0x00,0x00};
	
	spiAccelWriteBytes(ACCEL_RANGE_ADDRS, initData, sizeof(initData) );
	
	memset( &accelerometerRawData, 0, sizeof(accelerometerRawData_t) );
	memset( &presentInfo, 0, sizeof(presentInfo_t) );	// This also sets location to unknown
	
	initTimer(&sampleTimer);
	initTimer(&limitTimer);
	accelCounter = 0;
	setNextAccelState( &accelerometerInitState );
	presentInfo.boardPosition = arrowBoardIsUNKNOWN;
	presentInfo.actuatorCmd = eACTUATOR_COMMAND_STOP;
	startTimer(&sampleTimer, ACCEL_AUTO_MODE_SAMPLE_TIME_MS);
	
	#ifdef ACCEL_OUTPUT_DEBUG
	printf("X-accelerometerInit\n");
	#endif
}
BOOL accelerometerReached90Degrees()
{
	//return b90Degrees;
	return( FALSE );
}
/* 
 * This is continously called while actuator is moving, up or down,
 * from the actuator code.  Will read the present position/acceleraometer 
 * and is used as an datum, if just starting.  
 */
void accelerometerActuatorStarting( eACTUATOR_COMMANDS cmd )
{
	presentInfo.actuatorCmd = cmd;
	
	// 1st time thru?
	if( inAccelState( accelerometerNopState ) && inAutoMode( ) )
	{
		
		switch( presentInfo.actuatorCmd )
		{
			case eACTUATOR_COMMAND_MOVE_UP:
				if( !isArrowBoardUp( ) )
				{
					setNextAccelState( &accelerometerDatumState );
				}
				break;
			case eACTUATOR_COMMAND_MOVE_DOWN:
				if( !isArrowBoardDown( ) )
				{
					setNextAccelState( &accelerometerDatumState );
				}
				break;
			default:
				break;
		}
	}
}
/* 
 * This is called when either 1.) a stop command is issued or
 * 2.) if the actuator is moving and has reached it's limit.
 */
void accelerometerActuatorStopping( eACTUATOR_COMMANDS cmd )
{
		presentInfo.actuatorCmd = cmd;
	
#if 1
	
	setNextAccelState( &accelerometerNopState );

#else
	if( !inAutoMode( ) )
	{
		setNextAccelState( &accelerometerNopState );
		return;
	}
	// Else we in auto mode
	switch( presentInfo.actuatorCmd )
	{
		case eACTUATOR_COMMAND_STOP:
			if( isArrowBoardUp( ) || isArrowBoardDown( ) )
			{
				setNextAccelState( &accelerometerNopState );
			}
			else	// Don't know where we are
			{
				setNextAccelState( &accelerometerDatumState );
			}
			break;
		case eACTUATOR_COMMAND_MOVE_UP:
			if( !isArrowBoardUp( ) )	// Told to stop in auto mode when not done
			{
				setNextAccelState( &accelerometerDatumState );
			}
			break;
		case eACTUATOR_COMMAND_MOVE_DOWN:
			if( !isArrowBoardDown( ) )	// Told to stop in auto mode when not done
			{
				setNextAccelState( &accelerometerDatumState );
			}
			break;
		default:
			break;
	}
#endif
}
/*
 * This checks to see if "sampleTimer" has expired.  If so then reads 
 * the accelerometer's raw data and places data into circular buffer.  
 * If in auto-mode then execute state machine.
 */
void accelerometerDoWork( void )
{
	if( isTimerExpired(&sampleTimer) )
	{
		// Read and save latest raw accel data into FIFO
		readAccelerometer( &accelerometerRawData.x[accelerometerRawData.index],
												&accelerometerRawData.y[accelerometerRawData.index],
												&accelerometerRawData.z[accelerometerRawData.index]	);
		
		#ifdef ACCEL_OUTPUT_DEBUG
		if( !inAccelState( accelerometerNopState ) )
		{
			printf("Raw: t:%d x:%d y:%d z:%d\n",getTimerNow( ),accelerometerRawData.x[accelerometerRawData.index],
																								accelerometerRawData.y[accelerometerRawData.index],
																								accelerometerRawData.z[accelerometerRawData.index]);
		}
		#endif
		accelerometerRawData.index++;

		// Wrap?
		if( N_AVG_ACCEL <= accelerometerRawData.index )
		{
			accelerometerRawData.index = 0;
		}
		// Which mode are we in?
		if( inAutoMode( ) )
		{
			// All states will use filtered position info
			getAccelAverage( &presentInfo.x, &presentInfo.y, &presentInfo.z );
			
			startTimer(&sampleTimer, ACCEL_AUTO_MODE_SAMPLE_TIME_MS);
		}
		else
		{
			startTimer(&sampleTimer, ACCEL_MANUAL_MODE_SAMPLE_TIME_MS);
		}
		// Always execute present state
		accelStatePtr( );
	}
}
/*
 * Helper functions 
 */
void accelerometerActuatorIsDown(BOOL bDown)
{
}
/*
 * These are called by the actuator to determine if we're done
 */
BOOL isArrowBoardUp( void )
{
	return(arrowBoardIsQualifiedUP == presentInfo.boardPosition);
}
BOOL isArrowBoardDown( void )
{
	return(arrowBoardIsQualifiedDOWN == presentInfo.boardPosition);
}
static BOOL inAutoMode( void )
{
		return( 0 != storedConfigGetActuatorButtonMode( ) );	// 0 = manual mode
}	
static BOOL inAccelState( void(*func)(void) )
{
	return( func == accelStatePtr );
}

#ifdef ACCEL_OUTPUT_DEBUG
static void printPresentAccelState( void )
{
	if( inAccelState( accelerometerNopState ) ) {	printf("State: NOP\n"); } 
	else if( inAccelState( accelerometerInitState ) ) {	printf("State: INIT\n"); } 
	else if( inAccelState( accelerometerDatumState ) ) {	printf("State: DATUM\n"); } 
	else if( inAccelState( accelerometerActiveState ) ) {	printf("State: ACTIVE\n"); } 
	else if( inAccelState( accelerometerTimeSlamState ) ) {	printf("State: SLAM\n"); } 
	else { printf("Next accel state: FUBAR\n"); }
}
#endif

static void setNextAccelState( void(*func)(void) )
{
	accelStatePtr = func;
	
	#ifdef ACCEL_OUTPUT_DEBUG
	printf("\nTime:%d  ",getTimerNow( ) );
	printf("New Accel "); printPresentAccelState( );
	#endif
}
/* 
 * Acceleraometers Auto mode States
 */
static void accelerometerNopState( void )
{
}
/*
 * Stay in this state at power up to prime filter and determine 
 * where we are.
 */
static void accelerometerInitState( void )
{
	accelCounter++;
	
	if( N_AVG_ACCEL <= accelCounter )
	{	
		setNextAccelState( &accelerometerNopState );
		// Determine if up, down or don't know
		getAccelAverage( &presentInfo.x, &presentInfo.y, &presentInfo.z );
		if( (X_DOWN_LIMIT < presentInfo.x) && (Z_DOWN_LIMIT < presentInfo.z) )
		{
			presentInfo.boardPosition = arrowBoardIsDOWN;
		}
		else if( (X_UP_LIMIT > presentInfo.x) && (Z_UP_LIMIT > presentInfo.z) )
		{
			presentInfo.boardPosition = arrowBoardIsUP;
		}
		else
		{
			presentInfo.boardPosition = arrowBoardIsUNKNOWN;
		}
		accelCounter = 0;
		#ifdef ACCEL_OUTPUT_DEBUG
			printf("Abs: t:%d x:%d  y:%d  z:%d\n",getTimerNow( ),presentInfo.x,presentInfo.y,presentInfo.z);
			printf("Initial board position: ");
			if( presentInfo.boardPosition == arrowBoardIsUP ) { printf("UP\n"); }
			else if( presentInfo.boardPosition == arrowBoardIsDOWN ) { printf("DOWN\n"); }
			else { printf("UNKNOWN\n"); }
			printf("Actuator in ");
			if( inAutoMode( ) ) { printf("Auto-Mode\n"); }
			else { printf("Manual-Mode\n"); }
		#endif
	}
}
/*
 * The measured averaged position is save and used as the datum or
 * starting position.  Then move onto the next state.
 */
static void accelerometerDatumState( void )
{
  #ifdef ACCEL_OUTPUT_DEBUG
  startingPosition_t startingPosition;
  startingPosition_t endingPosition;
  
	// Save starting position
	startingPosition.x = presentInfo.x;
	startingPosition.y = presentInfo.y;
	startingPosition.z = presentInfo.z;

	// Determine ending position
	endingPosition.x = -presentInfo.z;
	endingPosition.y = presentInfo.y;
	endingPosition.z = -presentInfo.x;
	
	// Sanity check
	if( eACTUATOR_COMMAND_MOVE_UP == presentInfo.actuatorCmd )
	{		
		if( ABS16(endingPosition.x - X_UP_LIMIT) > 80 )
		{
			endingPosition.x = X_UP_LIMIT;
		}
		if( ABS16(endingPosition.z - Z_UP_LIMIT) > 100 )
		{
			endingPosition.z = Z_UP_LIMIT;
		}
	}
	else 
	{
		if( ABS16(endingPosition.x - X_DOWN_LIMIT) > 100 )
		{
			endingPosition.x = X_DOWN_LIMIT;
		}
		if( ABS16(endingPosition.z - Z_DOWN_LIMIT) > 100 )
		{
			endingPosition.z = Z_DOWN_LIMIT;
		}
	}
  	
	printf("STARTING POSITIONS  X:%d Y:%d Z:%d\n",startingPosition.x,startingPosition.y,startingPosition.z);
	printf("ENDING POSITIONS    X:%d Y:%d Z:%d\n",endingPosition.x,endingPosition.y,endingPosition.z);
	#endif
	
	presentInfo.boardPosition = arrowBoardIsUNKNOWN;
	
	startTimer(&limitTimer, MAX_MOVE_TIME_MS);
	
	accelCounter = 0;
		
	setNextAccelState( &accelerometerActiveState );
}
/*
 * Stay in this state until the accelerometer determines that the 
 * board is either 1.) Up, 2.) Down or 3.) we have timed out.
 */
static void accelerometerActiveState( void )
{
	// This is where all the work is done
	getPresentArrowBoardInfo( );
	
	if( (eACTUATOR_COMMAND_MOVE_UP == presentInfo.actuatorCmd) 
				&& (arrowBoardIsUP == presentInfo.boardPosition) )
	{
		setNextAccelState( &accelerometerTimeSlamState );
		startTimer(&limitTimer, ACCEL_SLAM_TIME_MS );
		#ifdef ACCEL_OUTPUT_DEBUG
			printf("\nACTUATOR GOT IT UP, Now SLAM IT\n");
		#endif
	}
	else if( (eACTUATOR_COMMAND_MOVE_DOWN == presentInfo.actuatorCmd) 
						&& (arrowBoardIsDOWN == presentInfo.boardPosition) )
	{
		setNextAccelState( &accelerometerTimeSlamState );
		startTimer(&limitTimer, ACCEL_SLAM_TIME_MS );
		#ifdef ACCEL_OUTPUT_DEBUG
			printf("\nACTUATOR IS DOWN now SLAM IT\n");
		#endif
	}
	else
	{
		#ifdef ACCEL_OUTPUT_DEBUG
		switch( presentInfo.actuatorCmd )
		{
			case eACTUATOR_COMMAND_MOVE_UP:
				printf("Cmd:UP  Pos:");
				break;
			case eACTUATOR_COMMAND_MOVE_DOWN:
				printf("Cmd:DOWN  Pos:");
				break;
		case eACTUATOR_COMMAND_STOP:
			printf("Cmd:STOP  Pos:");
				break;	
		default:
				printf("Cmd:Unknown  Pos:");
				break;	
		}
		switch( presentInfo.boardPosition )
		{
			case arrowBoardIsUP:
				printf("UP  ");
				break;
			case arrowBoardIsDOWN:
				printf("DOWN  ");
				break;
			default:
				printf("Moving  ");
				break;
		}
		
		printPresentAccelState( );
		#endif	// ACCEL_OUTPUT_DEBUG
	}
	if( isTimerExpired(&limitTimer) && inAutoMode( ) )
	{
		presentInfo.boardPosition = ( eACTUATOR_COMMAND_MOVE_UP == presentInfo.actuatorCmd ) ? 
														 arrowBoardIsQualifiedUP : arrowBoardIsQualifiedDOWN;
		setNextAccelState( &accelerometerNopState );
		printf("TIMED OUT MOVING\n");
	}
}
static void accelerometerTimeSlamState( void )
{
	#ifdef ACCEL_OUTPUT_DEBUG
	printf("Sam: t:%d x:%d y:%d z:%d\n",getTimerNow( ),presentInfo.x,presentInfo.y,presentInfo.z);
	#endif
	
	if( isTimerExpired(&limitTimer) )
	{
		presentInfo.boardPosition = ( presentInfo.boardPosition == arrowBoardIsUP ) ? 
														 arrowBoardIsQualifiedUP : arrowBoardIsQualifiedDOWN;
		setNextAccelState( &accelerometerNopState );

		#ifdef ACCEL_OUTPUT_DEBUG
		printf("ACCEL DONE ACTUATOR IS [%s]\n",(arrowBoardIsQualifiedUP == presentInfo.boardPosition) ? "UP" : "DOWN" );
		#endif
	}
}

#else	// ACCEL_BMA255

void accelerometerInit( void ) { }
void accelerometerDoWork( void ) { }
void accelerometerActuatorStarting( eACTUATOR_COMMANDS cmd ) {  }
void accelerometerActuatorStopping( eACTUATOR_COMMANDS cmd ) { }
void accelerometerActuatorIsDown(BOOL bDown) {(void)bDown; }

#endif // ACCEL_BMA255
