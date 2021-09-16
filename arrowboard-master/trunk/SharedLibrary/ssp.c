/*****************************************************************************
 *   ssp.c:  SSP C file for NXP LPC23xx/24xx Family Microprocessors
 *
 *   Copyright(C) 2006, NXP Semiconductor
 *   All rights reserved.
 *
 *   History
 *   2006.07.20  ver 1.00    Prelimnary version, first Release
 *
*****************************************************************************/
#include "LPC23xx.h"			/* LPC23XX/24xx Peripheral Registers */
#include <stdio.h>
#include "ssp.h"
#include "irq.h"

/* statistics of all the interrupts */
volatile DWORD interruptRxStat = 0;
volatile DWORD interruptOverRunStat = 0;
volatile DWORD interruptRxTimeoutStat = 0;

/*****************************************************************************
** Function name:		SSP0Handler
**
** Descriptions:		SSP port is used for SPI communication.
**									SSP0(SSP) interrupt handler
**									The algorithm is, if RXFIFO is at least half full, 
**									start receive until it's empty; if TXFIFO is at least
**									half empty, start transmit until it's full.
**									This will maximize the use of both FIFOs and performance.
**
** parameters:			None
** Returned value:	None
** 
*****************************************************************************/
void SSP0Handler (void) __irq 
{
  DWORD regValue;
  
  IENABLE;				/* handles nested interrupt */

  regValue = SSP0MIS;
	
	//printf("SSP0Handler\n");
	
  if ( regValue & SSPMIS_RORMIS )	/* Receive overrun interrupt */
  {
		interruptOverRunStat++;
		SSP0ICR = SSPICR_RORIC;		/* clear interrupt */
  }
	
  if ( regValue & SSPMIS_RTMIS )	/* Receive timeout interrupt */
  {
		interruptRxTimeoutStat++;
		SSP0ICR = SSPICR_RTIC;		/* clear interrupt */
  }

  /* please be aware that, in main and ISR, CurrentRxIndex and CurrentTxIndex
  are shared as global variables. It may create some race condition that main
  and ISR manipulate these variables at the same time. SSPSR_BSY checking (polling)
  in both main and ISR could prevent this kind of race condition */
  if ( regValue & SSPMIS_RXMIS )	/* Rx at least half full */
  {
		interruptRxStat++;		/* receive until it's empty */		
  }
	
  IDISABLE;
  VICVectAddr = 0;		/* Acknowledge Interrupt */
}

/*****************************************************************************
** Function name:		SSP1Handler
**
** Descriptions:		SSP port is used for SPI communication.
**									SSP1(SSP) interrupt handler
**									The algorithm is, if RXFIFO is at least half full, 
**									start receive until it's empty; if TXFIFO is at least
**									half empty, start transmit until it's full.
**									This will maximize the use of both FIFOs and performance.
**
** parameters:			None
** Returned value:	None
** 
*****************************************************************************/
void SSP1Handler (void) __irq 
{
  DWORD regValue;
  
  IENABLE;				/* handles nested interrupt */

  regValue = SSP1MIS;
	
	//printf("SSP1Handler\n");
	
  if ( regValue & SSPMIS_RORMIS )	/* Receive overrun interrupt */
  {
		interruptOverRunStat++;
		SSP1ICR = SSPICR_RORIC;		/* clear interrupt */
  }
	
  if ( regValue & SSPMIS_RTMIS )	/* Receive timeout interrupt */
  {
		interruptRxTimeoutStat++;
		SSP1ICR = SSPICR_RTIC;		/* clear interrupt */
  }

  /* please be aware that, in main and ISR, CurrentRxIndex and CurrentTxIndex
  are shared as global variables. It may create some race condition that main
  and ISR manipulate these variables at the same time. SSPSR_BSY checking (polling)
  in both main and ISR could prevent this kind of race condition */
  if ( regValue & SSPMIS_RXMIS )	/* Rx at least half full */
  {
		interruptRxStat++;		/* receive until it's empty */		
  }
  IDISABLE;
  VICVectAddr = 0;		/* Acknowledge Interrupt */
}

/*****************************************************************************
** Function name:		SSPInit
**
** Descriptions:		SSP port initialization routine
**				
** parameters:			None
** Returned value:		true or false, if the interrupt handler
**										can't be installed correctly, return false.
** 
*****************************************************************************/
DWORD SSPInit(ePLATFORM_TYPE ePlatformType)
{
  BYTE i, Dummy=Dummy;
	
	switch(ePlatformType)
	{
		case ePLATFORM_TYPE_ARROW_BOARD:
			printf("SSPInit: ePLATFORM_TYPE_ARROW_BOARD\n");
			/* Configure PIN connect block SSP1 port SCK1, MISO1, MOSI1, and SSEL1 */		
			/* NOTE: ALWAYS SET SSEL1 OF PINSEL0 AS A GPIO PIN. THIS ALLOWS SOFTWARE CONTROL OF 
				 THE SPI CHIP-SELECT PIN */	
			PINSEL0 |= 0x000A8000;
			FIO0CLR |= (1 << uP_SPI1_xCS);	
				
			/* BMA255 Accelerometer P1.17 = GPIO */
			PINSEL3 &= ~(3  << 2);		// Make GPIO
			FIO1DIR |= (OUT << uP_SPI1_xCS_ACCEL);	// Make output
			FIO1SET |= (1 << uP_SPI1_xCS_ACCEL);		// Set high

			/* Set DSS data to 8-bit, Frame format SPI, CPOL = 0, CPHA = 0, and SCR is 15 */
			SSP1CR0 = 0x0707;
		
			/* SSP1CPSR clock prescale register, master mode, minimum divisor is 0x02 */
			SSP1CPSR = 0x2;
		
			for ( i = 0; i < FIFOSIZE; i++ )
			{
				Dummy = SSP1DR;		/* clear the RxFIFO */
			}
		
			if ( install_irq( SSP1_INT,(void (*)(void) __irq)SSP1Handler, HIGHEST_PRIORITY ) == FALSE )
			{
				printf("Unable to Install SSP1_INT\n");
				return (FALSE);
			}
			else
			{
				printf("SSP1_INT Installed Successfully\n");		
			}
			
			/* Device select as master, SSP Enabled */
			SSP1CR1 = SSPCR1_SSE;
			
			/* Set SSPINMS registers to enable interrupts */
			/* enable all error related interrupts */
			SSP1IMSC = SSPIMSC_RORIM | SSPIMSC_RTIM;
			break;
			
		case ePLATFORM_TYPE_HANDHELD:
			printf("SSPInit: ePLATFORM_TYPE_HANDHELD\n");
			/* Configure PIN connect block SSP0 port SCK0, MISO0, MOSI0, and SSEL0 */		
			/* NOTE: ALWAYS SET SSEL0 OF PINSEL1 AS A GPIO PIN. THIS ALLOWS SOFTWARE CONTROL OF 
				 THE SPI CHIP-SELECT PIN */
			PINSEL0 |= 0x80000000;
			PINSEL1 |= 0x00000028;
			FIO0CLR |= (1 << uP_SPI0_xCS);	
				
			/* Set DSS data to 8-bit, Frame format SPI, CPOL = 0, CPHA = 0, and SCR is 15 */
			SSP0CR0 = 0x0707;
		
			/* SSPCPSR clock prescale register, master mode, minimum divisor is 0x02 */
			SSP0CPSR = 0x2;
		
			for ( i = 0; i < FIFOSIZE; i++ )
			{
				Dummy = SSP0DR;		/* clear the RxFIFO */
			}
		
			if ( install_irq( SSP0_INT,(void (*)(void) __irq)SSP0Handler, HIGHEST_PRIORITY ) == FALSE )
			{
				printf("Unable to Install SSP0_INT\n");
				return (FALSE);
			}
			else
			{
				printf("SSP0_INT Installed Successfully\n");		
			}
			
			/* Device select as master, SSP Enabled */
			SSP0CR1 = SSPCR1_SSE;
			
			/* Set SSPINMS registers to enable interrupts */
			/* enable all error related interrupts */
			SSP0IMSC = SSPIMSC_RORIM | SSPIMSC_RTIM;
			break;
			
		case ePLATFORM_TYPE_REPEATER:
			printf("SSPInit: ePLATFORM_TYPE_REPEATER\n");
	
			break;
	}
	
  return( TRUE );
}

/*****************************************************************************
** Function name:		SSPSend
**
** Descriptions:		Send a block of data to the SSP port, the 
**									first parameter is the buffer pointer, the 2nd 
**									parameter is the block length, and the 3rd parameter is 
**									platform type.
**
** parameters:			buffer pointer, block length, and ePlatformType
** Returned value:	None
** 
*****************************************************************************/
void SSPSend(BYTE *buf, DWORD Length, ePLATFORM_TYPE ePlatformType)
{
  DWORD i;
  BYTE Dummy = Dummy;
	
	switch(ePlatformType)
	{
		case ePLATFORM_TYPE_ARROW_BOARD:
			
			//Assert CS
			FIO0CLR |= (1 << uP_SPI1_xCS);	
			
			for ( i = 0; i < Length; i++ )
			{
				/* Move on only if NOT busy and TX FIFO not full. */
				while ( (SSP1SR & (SSPSR_TNF|SSPSR_BSY)) != SSPSR_TNF );
		
				SSP1DR = buf[i];

				while ( (SSP1SR & (SSPSR_BSY|SSPSR_RNE)) != SSPSR_RNE );
				
				/* Whenever a byte is written, MISO FIFO counter increments, Clear FIFO 
					on MISO. Otherwise, when SSP1Receive() is called, previous data byte
					is left in the FIFO. */
				Dummy = SSP1DR;
			}
			break;
			
		case ePLATFORM_TYPE_HANDHELD:
			//Assert CS
			FIO0CLR |= (1 << uP_SPI0_xCS);	
			
			for ( i = 0; i < Length; i++ )
			{
				/* Move on only if NOT busy and TX FIFO not full. */
				while ( (SSP0SR & (SSPSR_TNF|SSPSR_BSY)) != SSPSR_TNF );
		
				SSP0DR = buf[i];
				//printf("SSP0Send: buf[%d]\n",buf[i]);  
				while ( (SSP0SR & (SSPSR_BSY|SSPSR_RNE)) != SSPSR_RNE );
				
				/* Whenever a byte is written, MISO FIFO counter increments, Clear FIFO 
					on MISO. Otherwise, when SSP0Receive() is called, previous data byte
					is left in the FIFO. */
				Dummy = SSP0DR;
			}			
			break;
	}
	
  return; 
}

/*****************************************************************************
** Function name:		SSPReceive
** Descriptions:		the module will receive a block of data from 
**									the SSP, the 2nd parameter is the block 
**									length, and the 3rd parameter is platform type.
** parameters:			buffer pointer, block length, ePlatformType
** Returned value:	None
** 
*****************************************************************************/
void SSPReceive( BYTE *buf, WORD Length, ePLATFORM_TYPE ePlatformType )
{
  WORD i;
	
	switch(ePlatformType)
	{
		case ePLATFORM_TYPE_ARROW_BOARD:
			for ( i = 0; i < Length; i++ )
			{
				/* As long as Receive FIFO is not empty, I can always receive. */
				/* if it's a peer-to-peer communication, SSPDR needs to be written
					before a read can take place. */
				SSP1DR = 0xFF;
				
				/* Wait until the Busy bit is cleared */
				while ( (SSP1SR & (SSPSR_BSY|SSPSR_RNE)) != SSPSR_RNE );
				
				buf[i] = SSP1DR;
			}
			break;
			
		case ePLATFORM_TYPE_HANDHELD:
			for ( i = 0; i < Length; i++ )
			{
				/* As long as Receive FIFO is not empty, I can always receive. */
				/* if it's a peer-to-peer communication, SSPDR needs to be written
					before a read can take place. */
				SSP0DR = 0xFF;
				
				/* Wait until the Busy bit is cleared */
				while ( (SSP0SR & (SSPSR_BSY|SSPSR_RNE)) != SSPSR_RNE );
				
				buf[i] = SSP0DR;
			}
			break;
	}
  return; 
}

/******************************************************************************
**                            End Of File
******************************************************************************/

