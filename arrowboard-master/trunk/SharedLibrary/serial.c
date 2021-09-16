
#include "LPC23xx.H"                   /* LPC23xx definitions               */
#include "shareddefs.h"
#include <stdio.h>
#include <string.h>
#include "timer.h"
#include "queue.h"
#include "sharedinterface.h"

#include "serial.h"
//#include "commboard.h"
#include "irq.h"
#include "hdlc.h"

#include <assert.h>
void debugPinHH( uint8_t value );	//jgr 

/////
// rxAddToIndex is controlled by the interrupt handler
//
// rxRemoveFromIndex is controlled by the foreground process
//
//
// txRemoveFromIndex is controlled by the interrupt handler
//   with the exception of first byte added to tx queue
//   in this case, we know that the interrupt will not interfere
//
// txAddToIndex is controlled by the foreground process
/////
#ifdef DEBUG_USE_INTERRUPTS
static int nRxDebugAddToIndex;
static int nRxDebugRemoveFromIndex;
static int nTxDebugRemoveFromIndex;
static int nTxDebugAddToIndex;
#endif

static int nRxRS485AddToIndex;
static int nRxRS485RemoveFromIndex;
static int nTxRS485RemoveFromIndex;
static int nTxRS485AddToIndex;

static int nRxWirelessAddToIndex;
static int nRxWirelessRemoveFromIndex;
static int nTxWirelessRemoveFromIndex;
static int nTxWirelessAddToIndex;

#define FIFO_LENGTH	1000
//#define FIFO_LENGTH	500

#ifdef DEBUG_USE_INTERRUPTS
static unsigned char rxDebugFIFO[FIFO_LENGTH];
static unsigned char txDebugFIFO[FIFO_LENGTH];
#endif

static unsigned char rxRS485FIFO[FIFO_LENGTH];
static unsigned char txRS485FIFO[FIFO_LENGTH];

/////
// FIFO for wireless
// UART1 on arrow board
// UART3 on handheld
/////
static unsigned char rxWirelessFIFO[FIFO_LENGTH];
static unsigned char txWirelessFIFO[FIFO_LENGTH];

#ifdef DEBUG_USE_INTERRUPTS
static int rxDebugPacketIndex;
#endif
static int rxRS485PacketIndex;
static int rxWirelessPacketIndex;

#ifdef DEBUG_USE_INTERRUPTS
static unsigned char rxDebugPacket[MAX_PACKET_LENGTH];
#endif
static unsigned char rxRS485Packet[MAX_PACKET_LENGTH];
static unsigned char rxWirelessPacket[MAX_PACKET_LENGTH];

static ePLATFORM_TYPE eOurPlatformType;

//////
// one queue and set of free entries
// for each interface type
/////
#if 0
static QUEUE freeQueue[INTERFACE_COUNT];
static QUEUE sendQueue[INTERFACE_COUNT];
static TIMERCTL sendQueueIdleTimer[INTERFACE_COUNT];
#endif
static void serialRS485SetMode(int mode);

#ifdef DEBUG_USE_INTERRUPTS
/////////////////////////////////////////////////////////////////////////
// debug on both arrow board and handheld
/////////////////////////////////////////////////////////////////////////
 __irq void UART0_HANDLER(void)
{
	int nAddIndex;
	unsigned char nData;

	/////
	// read all waiting data
	// don't do this if receive interrupts are turned off
	/////
	if(U0IER & RX_DATA_AVAILABLE_INTERRUPT_ENABLE)
	{
		/////
		// receive interrupts turned on
		// we could have gotten here because of a receive interrupt
		/////
		while(U0LSR & RDR )
		{
			nData = U0RBR;
		
			/////
			// don't let the add to index
			// roll past the remove from index
			// discard the data if it would
			/////
			nAddIndex = nRxDebugAddToIndex+1;
			if(sizeof(rxDebugFIFO) <= nAddIndex)
			{
				nAddIndex = 0;
			}
			if(nAddIndex != nRxDebugRemoveFromIndex)
			{
				rxDebugFIFO[nRxDebugAddToIndex] = nData;
				nRxDebugAddToIndex = nAddIndex;
			}
		}
	}
//	{
//		unsigned char nReg = U0LSR;
//		U0THR='0'+((nReg&0xF0)>>4);
//		U0THR='0'+(nReg&0x0f);
//		U0THR='\n';
//		U0THR='\r';
//	}

	/////
	// only do this if this interrupt is enabled
	// so we don't step on foreground process
	/////
	if(U0IER & THRE_INTERRUPT_ENABLE)
	{
		while(U0LSR & THRE)
		{
			if(nTxDebugRemoveFromIndex != nTxDebugAddToIndex)
			{
				U0THR = txDebugFIFO[nTxDebugRemoveFromIndex++];

				if(sizeof(txDebugFIFO) <= nTxDebugRemoveFromIndex)
				{
					nTxDebugRemoveFromIndex = 0;
				}
			
				/////
				// if this is the last character for awhile
				// so turn off thre interrupt 
				// it will be re-enabled when there is something to send
				/////
				if(nTxDebugRemoveFromIndex == nTxDebugAddToIndex)
				{
					U0IER &= (~THRE_INTERRUPT_ENABLE);
					break;
				}
			}
			else
			{
				U0IER &= (~THRE_INTERRUPT_ENABLE);
				break;
			}
		}
		
	}
 // clear flag
 //ack int

 //clear int flag??

 VICVectAddr = 0x00;
}
#endif

/////////////////////////////////////////////////////////////////////////
// RS485 on both arrow board and handheld
/////////////////////////////////////////////////////////////////////////
  __irq void UART2_HANDLER(void)
{
	int nAddIndex;
	unsigned char nData;
	/////
	// read all waiting data
	// don't do this if receive interrupts are turned off
	/////
	if(U2IER & RX_DATA_AVAILABLE_INTERRUPT_ENABLE)
	{
		/////
		// receive interrupts turned on
		// we could have gotten here because of a receive interrupt
		/////
		while(U2LSR & RDR )
		{
			nData = U2RBR;

			/////
			// don't let the add to index
			// roll past the remove from index
			// discard the data if it would
			/////
			nAddIndex = nRxRS485AddToIndex+1;
			if(sizeof(rxRS485FIFO) <= nAddIndex)
			{
				nAddIndex = 0;
			}
			if(nAddIndex != nRxRS485RemoveFromIndex)
			{
				rxRS485FIFO[nRxRS485AddToIndex] = nData;
				nRxRS485AddToIndex = nAddIndex;
			}
		}
	}

	/////
	// only do this if this interrupt is enabled
	// so we don't step on foreground process
	/////
	if(U2IER & THRE_INTERRUPT_ENABLE)
	{	
		debugPinHH( 1 );
		while(U2LSR & THRE)	// While Tx holding reg empty
		{

			if(nTxRS485RemoveFromIndex != nTxRS485AddToIndex)	// Data to send?
			{
				U2THR = txRS485FIFO[nTxRS485RemoveFromIndex++];	// Load h/w Tx holding fifo
				if(sizeof(txRS485FIFO) <= nTxRS485RemoveFromIndex)
				{

					nTxRS485RemoveFromIndex = 0;
				}
			
				/////
				// if this is the last character for awhile
				// so turn off thre interrupt 
				// it will be re-enabled when there is something to send
				/////
			#if 0	/* Original */
				if( nTxRS485RemoveFromIndex == nTxRS485AddToIndex )
				{
					U2IER &= (~THRE_INTERRUPT_ENABLE);
					break;
				}
			}
			else
			{
				U2IER &= (~THRE_INTERRUPT_ENABLE);
				break;
			}				
			#else  /* De-assert 485 Tx signal/mode here */
				if( (nTxRS485RemoveFromIndex == nTxRS485AddToIndex) && (U2LSR & TEMT) )
				{
					//debugPinHH( 0 );
					serialRS485SetMode(RECV);
					U2IER &= (~THRE_INTERRUPT_ENABLE);
					break;
				}
			}
			else if(U2LSR & TEMT)
			{
				//debugPinHH( 0 );
				U2IER &= (~THRE_INTERRUPT_ENABLE);
				serialRS485SetMode(RECV);
				break;
			}
			#endif
		}
	}
 // clear flag
 //ack int

 //clear int flag??

 VICVectAddr = 0x00;
}

/////////////////////////////////////////////////////////////////////////
// RF Digital wireless on arrow board
/////////////////////////////////////////////////////////////////////////
  __irq void UART1_HANDLER(void)
{
	int nAddIndex;
	unsigned char nData;

	/////
	// read all waiting data
	// don't do this if receive interrupts are turned off
	/////
	if(U1IER & RX_DATA_AVAILABLE_INTERRUPT_ENABLE)
	{
		/////
		// receive interrupts turned on
		// we could have gotten here because of a receive interrupt
		/////
		while(U1LSR & RDR )
		{
			nData = U1RBR;

			/////
			// don't let the add to index
			// roll past the remove from index
			// discard the data if it would
			/////
			nAddIndex = nRxWirelessAddToIndex+1;
			if(sizeof(rxWirelessFIFO) <= nAddIndex)
			{
				nAddIndex = 0;
			}
			if(nAddIndex != nRxWirelessRemoveFromIndex)
			{
				rxWirelessFIFO[nRxWirelessAddToIndex] = nData;
				nRxWirelessAddToIndex = nAddIndex;
			}
		}
	}

	/////
	// only do this if this interrupt is enabled
	// so we don't step on foreground process
	/////
	if(U1IER & THRE_INTERRUPT_ENABLE)
	{		
		while(U1LSR & THRE)
		{
			//driverBoardToggleLED();
			if(nTxWirelessRemoveFromIndex != nTxWirelessAddToIndex)
			{
				U1THR = txWirelessFIFO[nTxWirelessRemoveFromIndex++];
				if(sizeof(txWirelessFIFO) <= nTxWirelessRemoveFromIndex)
				{
					nTxWirelessRemoveFromIndex = 0;
				}
				/////
				// if this is the last character for awhile
				// so turn off thre interrupt 
				// it will be re-enabled when there is something to send
				/////
				if(nTxWirelessRemoveFromIndex == nTxWirelessAddToIndex)
				{
					U1IER &= (~THRE_INTERRUPT_ENABLE);
					break;
				}
			}
			else
			{
				U1IER &= (~THRE_INTERRUPT_ENABLE);
				break;
			}
		}
	}

 VICVectAddr = 0x00;
}

/////////////////////////////////////////////////////////////////////////
// RF Digital wireless on handheld
/////////////////////////////////////////////////////////////////////////
  __irq void UART3_HANDLER(void)
{
	int nAddIndex;
	
	/////
	// read all waiting data
	// don't do this if receive interrupts are turned off
	/////
	if(U3IER & RX_DATA_AVAILABLE_INTERRUPT_ENABLE)
	{
		/////
		// receive interrupts turned on
		// we could have gotten here because of a receive interrupt
		/////
		while(U3LSR & RDR )
		{
			unsigned char nData = U3RBR;

			/////
			// don't let the add to index
			// roll past the remove from index
			// discard the data if it would
			/////
			nAddIndex = nRxWirelessAddToIndex+1;
			if(sizeof(rxWirelessFIFO) <= nAddIndex)
			{
				nAddIndex = 0;
			}
			if(nAddIndex != nRxWirelessRemoveFromIndex)
			{
				rxWirelessFIFO[nRxWirelessAddToIndex] = nData;
				nRxWirelessAddToIndex = nAddIndex;
			}
		}
	}

	/////
	// only do this if this interrupt is enabled
	// so we don't step on foreground process
	/////
	if(U3IER & THRE_INTERRUPT_ENABLE)
	{		
		while(U3LSR & THRE)
		{
			if(nTxWirelessRemoveFromIndex != nTxWirelessAddToIndex)
			{
				unsigned char nData = txWirelessFIFO[nTxWirelessRemoveFromIndex++];
				//U0THR=nData;
				//U0THR = 'B';
				U3THR = nData;

				//U3THR = txWirelessFIFO[nTxWirelessRemoveFromIndex++];
				if(sizeof(txWirelessFIFO) <= nTxWirelessRemoveFromIndex)
				{
					nTxWirelessRemoveFromIndex = 0;
				}
				/////
				// if this is the last character for awhile
				// so turn off thre interrupt 
				// it will be re-enabled when there is something to send
				/////
				if(nTxWirelessRemoveFromIndex == nTxWirelessAddToIndex)
				{
					U3IER &= (~THRE_INTERRUPT_ENABLE);
					break;
				}
			}
			else
			{
				U3IER &= (~THRE_INTERRUPT_ENABLE);
				break;
			}
		}
	}

 VICVectAddr = 0x00;
}

/*----------------------------------------------------------------------------
 *       init_serial:  Initialize Serial Interface
 *---------------------------------------------------------------------------*/
static int serialInitPort(int nUart) {
	int status = SUCCESS;
	/* Initialize the serial interface(S) */

	switch(nUart)
	{
		case 0:
			{
				// SET UP REGS
				// SET UP UART for 115200 baud
				U0LCR |= (1 << DLAB) | (EIGHTBITS << DATA_WIDTH);
				
				switch(eOurPlatformType)
				{
					case ePLATFORM_TYPE_ARROW_BOARD:
					case ePLATFORM_TYPE_REPEATER:
						U0DLM = DLM_19200_36MHZ;
						U0DLL = DLL_19200_36MHZ;
						break;
					
					case ePLATFORM_TYPE_HANDHELD:
						U0DLM = DLM_19200_72MHZ;
						U0DLL = DLL_19200_72MHZ;
						break;
					
					default:
						break;
				}
				
				U0LCR &= ~(1 << DLAB);	
				// config IO pins
				PINSEL0 |= (UART0_SEL << TXD0_SEL) | (UART0_SEL << RXD0_SEL); 
				
				//U0FCR |= FIFO_ENABLE;
			}
			break;
			
		case 1:
			{
				// SET UP REGS
				// SET UP UART for 9600 baud
				U1LCR |= (1 << DLAB) | (EIGHTBITS << DATA_WIDTH);
				
				switch(eOurPlatformType)
				{
					case ePLATFORM_TYPE_ARROW_BOARD:
					case ePLATFORM_TYPE_REPEATER:
						U1DLM = DLM_9600_36MHZ;
						U1DLL = DLL_9600_36MHZ;
						break;
					
					case ePLATFORM_TYPE_HANDHELD:
						U1DLM = DLM_9600_72MHZ;
						U1DLL = DLL_9600_72MHZ;
						break;
					
					default:
						break;
				}

				U1LCR &= ~(1 << DLAB);	
				// set up pins
				PINSEL0 |= (UART1_SEL << TXD1_SEL); 
				PINSEL1 |= (UART1_SEL << RXD1_SEL); 
				
				//U2FCR |= FIFO_ENABLE;
			}
			break;
			
		case 2:
			{
				// SET UP REGS
				// SET UP UART for 9600 baud
				U2LCR |= (1 << DLAB) | (EIGHTBITS << DATA_WIDTH);
				
				switch(eOurPlatformType)
				{
					case ePLATFORM_TYPE_ARROW_BOARD:
					case ePLATFORM_TYPE_REPEATER:
						U2DLM = DLM_9600_36MHZ;
						U2DLL = DLL_9600_36MHZ;
						break;
					
					case ePLATFORM_TYPE_HANDHELD:
						U2DLM = DLM_9600_72MHZ;
						U2DLL = DLL_9600_72MHZ;
						break;
					
					default:
						break;
				}

				U2LCR &= ~(1 << DLAB);	
				// set up pins
				PINSEL0 |= (UART2_SEL << TXD2_SEL) | (UART2_SEL << RXD2_SEL); 
				/////
				// RS485
				// configure these pins as outputs
				/////
				FIO0DIR |= (OUT << uP_W85_xRE) |
							(OUT << uP_W85_DEN) ;				
				//U2FCR |= FIFO_ENABLE;
			}
			break;
			
		case 3:
			{
				// SET UP REGS
				// SET UP UART for 9600 baud, 12MHz clock
				U3LCR |= (1 << DLAB) | (EIGHTBITS << DATA_WIDTH);
				
				switch(eOurPlatformType)
				{
					case ePLATFORM_TYPE_ARROW_BOARD:
					case ePLATFORM_TYPE_REPEATER:
						U3DLM = DLM_9600_36MHZ;
						U3DLL = DLL_9600_36MHZ;
						break;
					
					case ePLATFORM_TYPE_HANDHELD:
						U3DLM = DLM_9600_72MHZ;
						U3DLL = DLL_9600_72MHZ;
						break;
					
					default:
						break;
				}

				U3LCR &= ~(1 << DLAB);	
				// set up pins
				PINSEL9 |= (UART3_SEL << TXD3_SEL) | (UART3_SEL << RXD3_SEL); 
				
				//U3FCR |= FIFO_ENABLE;
			}
			break;
		default:
			{
				status = ERROR;
			}
			break;
	}
return status;
}

//// prepare 485 XCVR
static void serialRS485SetMode(int mode)
{
// SET PIN DIRECTIONS	
	switch (mode) 
	{
		
		// set both
		case XMIT:
			
			FIO0SET |= (1 << uP_W85_xRE) | (1 << uP_W85_DEN);
			break;
	
		// clear both
		case RECV:
			
			FIO0CLR |= (1 << uP_W85_xRE) | (1 << uP_W85_DEN);
			break;

		// set xRE, clear DEN
		case SHUTDOWN:
			
			FIO0SET |= (1 << uP_W85_xRE);
			FIO0CLR |= (1 << uP_W85_DEN);
			break;
		
			
	}	// end switch
	
return;

}
void serialRS485SendData(unsigned char nData)
{
	int nAddIndex;
	BOOL bInterruptsOn = FALSE;
	if((U2IER & THRE_INTERRUPT_ENABLE))
	{
		bInterruptsOn = TRUE;
	}
	/////
	// RS485 mode to transmit
	/////
	serialRS485SetMode(XMIT);
	
	/////
	// turn interrupts off 
	// while we are messing 
	// with the FIFO
	/////
	U2IER &= ~THRE_INTERRUPT_ENABLE;
	
	/////
	// add the character to the fifo
	// don't let the add to index
	// roll past the remove from index
	// discard the data if it would
	/////
	nAddIndex = nTxRS485AddToIndex+1;
	if(sizeof(txRS485FIFO) <= nAddIndex)
	{
			nAddIndex = 0;
	}
	if(nAddIndex != nTxRS485RemoveFromIndex)
	{
			txRS485FIFO[nTxRS485AddToIndex] = nData;
			nTxRS485AddToIndex = nAddIndex;
	}

	/////
	// if fifo was empty
	// then send the first character on the fifo
	// and enable THRE interrupts
	// note: this should be the only place that
	// we enable these interrupts for this UART
	/////
	if(!bInterruptsOn)
	{
		/////
		// note, we can modify the RemoveFrom index
		// here because we know that the interrupt
		// will not interfere
		/////
		int ch2 = txRS485FIFO[nTxRS485RemoveFromIndex++];
		if(nTxRS485RemoveFromIndex >= sizeof(txRS485FIFO))
		{
			nTxRS485RemoveFromIndex = 0;
		}
		U2THR = ch2;
		U2IER |= THRE_INTERRUPT_ENABLE;
	}
	else
	{
		/////
		// restore interrupts
		/////
		U2IER |= THRE_INTERRUPT_ENABLE;
	}
}
void serialWirelessSendData(unsigned char nData)
{
	int nAddIndex;
	BOOL bInterruptsOn = FALSE;
	switch(eOurPlatformType)
	{
		case ePLATFORM_TYPE_ARROW_BOARD:
		case ePLATFORM_TYPE_REPEATER:
			{
				if(U1IER & THRE_INTERRUPT_ENABLE)
				{
					/////
					// turn interrupts off
					// while we are messing
					// with the fifo
					/////
					U1IER &= ~THRE_INTERRUPT_ENABLE;
					bInterruptsOn = TRUE;
				}
			}
			break;
		case ePLATFORM_TYPE_HANDHELD:
			{
				if(U3IER & THRE_INTERRUPT_ENABLE)
				{
					/////
					// turn interrupts off
					// while we are messing
					// with the fifo
					/////
					U3IER &= ~THRE_INTERRUPT_ENABLE;
					bInterruptsOn = TRUE;
				}
			}
		}


	/////
	// add the character to the fifo
	// don't let the add to index
	// roll past the remove from index
	// discard the data if it would
	/////

	nAddIndex = nTxWirelessAddToIndex+1;
	if(sizeof(txWirelessFIFO) <= nAddIndex)
	{
			nAddIndex = 0;
	}
	if(nAddIndex != nTxWirelessRemoveFromIndex)
	{
		txWirelessFIFO[nTxWirelessAddToIndex] = nData;
		nTxWirelessAddToIndex = nAddIndex;
	}

	/////
	// if fifo was empty (we know this because interrupts were off)
	// then send the first character on the fifo
	// and enable THRE interrupts
	// note: this should be the only place that
	// we enable these interrupts for this UART
	/////
	switch(eOurPlatformType)
	{
		case ePLATFORM_TYPE_ARROW_BOARD:
		case ePLATFORM_TYPE_REPEATER:
			{
				if(!bInterruptsOn)
				{
					/////
					// note, we can modify the RemoveFrom index
					// here because we know that the interrupt
					// will not interfere
					/////
					int ch2 = txWirelessFIFO[nTxWirelessRemoveFromIndex++];
					if(nTxWirelessRemoveFromIndex >= sizeof(txWirelessFIFO))
					{
						nTxWirelessRemoveFromIndex = 0;
					}
					//printf("1w[%X]\n\r", ch2);
					U1THR = ch2;
					U1IER |= THRE_INTERRUPT_ENABLE;
				}
			}
			break;
		case ePLATFORM_TYPE_HANDHELD:
			{
				if(!bInterruptsOn)
				{
					/////
					// note, we can modify the RemoveFrom index
					// here because we know that the interrupt
					// will not interfere
					/////
					int ch2 = txWirelessFIFO[nTxWirelessRemoveFromIndex++];
					if(nTxWirelessRemoveFromIndex >= sizeof(txWirelessFIFO))
					{
						nTxWirelessRemoveFromIndex = 0;
					}
					U3THR = ch2;
					//printf("1wx[%X]\n\r", ch2);
					U3IER |= THRE_INTERRUPT_ENABLE;
				}
			}
			break;
	}
	if(bInterruptsOn)
	{
			switch(eOurPlatformType)
	{
		case ePLATFORM_TYPE_ARROW_BOARD:
		case ePLATFORM_TYPE_REPEATER:
			U1IER |= THRE_INTERRUPT_ENABLE;
			break;
		case ePLATFORM_TYPE_HANDHELD:
			U3IER |= THRE_INTERRUPT_ENABLE;
			break;
		}
	}
}
#ifdef DEBUG_USE_INTERRUPTS
static void serialDebugSendData(unsigned char nData)
{
	int nAddIndex;
	BOOL bInterruptsOn = FALSE;
	if((U0IER & THRE_INTERRUPT_ENABLE))
	{
		bInterruptsOn = TRUE;
	}
	/////
	// turn interrupts off 
	// while we are messing 
	// with the FIFO
	/////
	U0IER &= ~THRE_INTERRUPT_ENABLE;
	
	/////
	// add the character to the fifo
	// don't let the add to index
	// roll past the remove from index
	// discard the data if it would
	/////
	nAddIndex = nTxDebugAddToIndex+1;
	if(sizeof(txDebugFIFO) <= nAddIndex)
	{
			nAddIndex = 0;
	}
	if(nAddIndex != nTxDebugRemoveFromIndex)
	{
			txDebugFIFO[nTxDebugAddToIndex] = nData;
			nTxDebugAddToIndex = nAddIndex;
	}


	/////
	// if fifo was empty
	// then send the first character on the fifo
	// and enable THRE interrupts
	// note: this should be the only place that
	// we enable these interrupts for this UART
	/////
	if(!bInterruptsOn)
	{
		/////
		// note, we can modify the RemoveFrom index
		// here because we know that the interrupt
		// will not interfere
		/////
		int ch2 = txDebugFIFO[nTxDebugRemoveFromIndex++];
		if(nTxDebugRemoveFromIndex >= sizeof(txDebugFIFO))
		{
			nTxDebugRemoveFromIndex = 0;
		}

		U0THR = ch2;
		U0IER |= THRE_INTERRUPT_ENABLE;
	}
	else
	{
		U0IER |= THRE_INTERRUPT_ENABLE;
	}
}
#endif

/*----------------------------------------------------------------------------
 *       sendchar:  Write a character to Serial Port attached to UART0
 *                  called by printf through retarget.c
 *---------------------------------------------------------------------------*/
int sendchar (int ch)
{
#ifdef DEBUG_USE_INTERRUPTS
		int nRetVal = 0;

	serialDebugSendData(ch);
	return nRetVal;
#else
   if (ch == '\n') {
      while (!(U0LSR & 0x20));
      U0THR = '\r';
   }
	 
   while (!(U0LSR & 0x20));
	 return U0THR = ch;
#endif
}
/*----------------------------------------------------------------------------
 *       RxDataReady:  if U0RBR contains valid data reurn 1, otherwise 0
 *---------------------------------------------------------------------------*/
int RxDataReady( void ) {
	return ( U0LSR & 0x01 );	//Receive data ready

}
void serialDisableInterface(eINTERFACE eInterface)
{
		switch(eInterface)
	{
		case eINTERFACE_DEBUG:
			U0IER &= ~(RX_DATA_AVAILABLE_INTERRUPT_ENABLE);
			break;
		case eINTERFACE_WIRELESS:
			switch(eOurPlatformType)
			{
				case ePLATFORM_TYPE_ARROW_BOARD:
				case ePLATFORM_TYPE_REPEATER:
					{
						U1IER &= ~(RX_DATA_AVAILABLE_INTERRUPT_ENABLE);
						#ifdef RADIO_V2
						delayMs(1);		// Let any characters pass
						if( U1LSR ^ (THRE | TEMT) )	// Check for abnormal condition
						{
							(void)U1RBR;	// Clear out receive reg
						}
						U1FCR = 0x06;		// Clear both FIFO
						U1FCR = 0xC1;		// Enable both FIFO
						#endif
					}
					break;
				case ePLATFORM_TYPE_HANDHELD:
					{
						U3IER &= ~(RX_DATA_AVAILABLE_INTERRUPT_ENABLE);
						#ifdef RADIO_V2
						delayMs(1);		// Let any characters pass
						if( U3LSR ^ (THRE | TEMT) )	// Check for abnormal condition
						{
							(void)U3RBR;	// Clear out receive reg
						}
						U3FCR = 0x06;		// Clear both FIFO
						U3FCR = 0xC1;		// Enable both FIFO
						#endif
					}
					break;
			}
			break;
		case eINTERFACE_RS485:
			U2IER &= ~(RX_DATA_AVAILABLE_INTERRUPT_ENABLE);
			break;
	}
}
void serialEnableInterface(eINTERFACE eInterface)
{
	/////
	// clear the FIFOs
	// reset the indicies
	// and enable receive interrupts
	// for the specified interface
	/////
	switch(eInterface)
	{
		case eINTERFACE_DEBUG:
#ifdef DEBUG_USE_INTERRUPTS
			nRxDebugAddToIndex = 0;
			nRxDebugRemoveFromIndex = 0;
			nTxDebugRemoveFromIndex = 0;
			nTxDebugAddToIndex = 0;
			memset(rxDebugFIFO, 0, sizeof(rxDebugFIFO));
			memset(txDebugFIFO, 0, sizeof(txDebugFIFO));
			rxDebugPacketIndex = NO_PACKET_DATA;
			memset(rxDebugPacket, 0, sizeof(rxDebugPacket));
			U0IER |= (RX_DATA_AVAILABLE_INTERRUPT_ENABLE);
#endif
			break;
		case eINTERFACE_WIRELESS:
			#ifdef RADIO_V2
			U1FCR = 0x00;		// Disable FIFO
			U3FCR = 0x00;		// Disable FIFO
			#endif
			nRxWirelessAddToIndex = 0;
			nRxWirelessRemoveFromIndex = 0;
			nTxWirelessRemoveFromIndex = 0;
			nTxWirelessAddToIndex = 0;
			memset(rxWirelessFIFO, 0, sizeof(rxWirelessFIFO));
			memset(txWirelessFIFO, 0, sizeof(txWirelessFIFO));
			rxWirelessPacketIndex = NO_PACKET_DATA;
			memset(rxWirelessPacket, 0, sizeof(rxWirelessPacket));
			switch(eOurPlatformType)
			{
				case ePLATFORM_TYPE_ARROW_BOARD:
				case ePLATFORM_TYPE_REPEATER:
					{
						U1IER |= (RX_DATA_AVAILABLE_INTERRUPT_ENABLE);
					}
					break;
				case ePLATFORM_TYPE_HANDHELD:
					{
						U3IER |= (RX_DATA_AVAILABLE_INTERRUPT_ENABLE);
					}
					break;
			}

			break;
		case eINTERFACE_RS485:
			nRxRS485AddToIndex = 0;
			nRxRS485RemoveFromIndex = 0;
			nTxRS485RemoveFromIndex = 0;
			nTxRS485AddToIndex = 0;
			memset(rxRS485FIFO, 0, sizeof(rxRS485FIFO));
			memset(txRS485FIFO, 0, sizeof(txRS485FIFO));
			rxRS485PacketIndex = NO_PACKET_DATA;
			memset(rxRS485Packet, 0, sizeof(rxRS485Packet));
			U2IER |= (RX_DATA_AVAILABLE_INTERRUPT_ENABLE);
			break;
	}
}
void serialInit(ePLATFORM_TYPE ePlatformType)
{
	/////
	// remember platform type
	// so we can handle the differences
	//////
	eOurPlatformType = ePlatformType;
	


	/////
	// printf won't work until debug port is setup
	/////
#if 1
	//////
	// setup RS485 port
	/////
	serialInitPort(2);
	U2IER &= (~THRE_INTERRUPT_ENABLE);
  install_irq( UART2_INT, (void (*) (void) __irq) UART2_HANDLER, HIGHEST_PRIORITY );
	serialEnableInterface(eINTERFACE_RS485);
	serialRS485SetMode(RECV);
#endif
#if 1
	//////
	// setup wireless port
	/////
	switch(eOurPlatformType)
	{
		case ePLATFORM_TYPE_ARROW_BOARD:
		case ePLATFORM_TYPE_REPEATER:
			{
				serialInitPort(1);
				U1IER &= (~THRE_INTERRUPT_ENABLE);				
				install_irq( UART1_INT, (void (*) (void) __irq) UART1_HANDLER, HIGHEST_PRIORITY );
			}
			break;
		case ePLATFORM_TYPE_HANDHELD:
			{
				serialInitPort(3);	
				U3IER &= (~THRE_INTERRUPT_ENABLE);
				install_irq( UART3_INT, (void (*) (void) __irq) UART3_HANDLER, HIGHEST_PRIORITY );
			}
			break;
	}
	serialEnableInterface(eINTERFACE_WIRELESS);
#endif


	/////
	// setup debug port
	/////
	serialInitPort(0);
#ifdef DEBUG_USE_INTERRUPTS
	/////
	// install interrupt handler
	// enable receive interrupts
	// THRE interrupts will be enabled 
	// when there is data to send
	/////
	U0IER &= (~THRE_INTERRUPT_ENABLE);
  install_irq( UART0_INT, (void (*) (void) __irq) UART0_HANDLER, HIGHEST_PRIORITY );
	serialEnableInterface(eINTERFACE_DEBUG);
#endif
	

	/////
	// now, printf will work
	/////
	//printf("Serial0 configured (DEBUG)...\n\r");
//printf("THRE[%X]\n\r", THRE);

	/////
	// initialize the send queue entries
	/////
	//jgrqueueInitialize(&sendQueue[1]);
}
/* 
 * This is called from serialDoWork() when the Rx FIFO has
 * byte(s) in it.  It passes 1 byte at a time to here.
 * This routine will reassemble the raw packet.  It will
 * determine that the packet is complete by looking at the 
 * Rx length byte and the # of bytes recieved.  Once complete 
 * will call packetProcessor()
 */
static void buildPacket(eINTERFACE eInterface, unsigned char nData, unsigned char* pPacket, int *pnPacketIndex)
{
	static uint8_t length = 0;
	
	//printf("1-buildPacket nData[%X] [%d]\n", nData, *pnPacketIndex);
	if( PACKET_DELIM == nData )
	{
		/////
		// found a packet delimeter
		/////
		if(NO_PACKET_DATA == *pnPacketIndex)
		{
			/////
			// start of packet
			/////
			*pnPacketIndex = 0;
			length = 0;
		}
	}
	// To get here means normal packet data, save it
	pPacket[*pnPacketIndex] = nData;
	
	// Look for length byte
	if( 1 == *pnPacketIndex )
	{
		length = nData & 0x3F;
		//printf("Rx len %d\n",length);
		if(13 != length )		// JGR NOTE: Presently hard coded!!
		{
			uint8_t i = length;
			printf("\nPACKET ERROR LENGTH\n\r");
			for( i =0; i <=13; i++ ) { printf("%X ",pPacket[i]); }
			printf("\n");
			*pnPacketIndex = NO_PACKET_DATA;	
			length = 0;
			return;
		}
	}
		

	// Inc index, check OV
	*pnPacketIndex = (*pnPacketIndex)+1;
	
	if(*pnPacketIndex >= MAX_PACKET_LENGTH)
	{
		uint8_t i = length;
		printf("\nPACKET ERROR OV \n\r");
		for( i =0; i <=length; i++ ) { printf("%X ",pPacket[i]); }
		printf("\n");
		*pnPacketIndex = NO_PACKET_DATA;	
		length = 0;
		return;
	}

	// Check if all bytes recieved
	if( (*pnPacketIndex > length) && length )
	{
		#if 0
		{
			uint8_t i; //jgr
			for(i=0; i<=length; i++ ) { printf("%X ",pPacket[i]); }	printf("\n");
		}
		#endif
		// To get here means packet is built, which also means we have communications
		// between the hand held device and driver board
		setInterface( eInterface );
		packetProcessor(eInterface, pPacket, *pnPacketIndex);
		length = 0;
		*pnPacketIndex = NO_PACKET_DATA;	
		
	}
}

/*
 * Will check interrupt flags to determine if any of the FIFO's, that
 * are filled/emptied within the UART interrupt, have data.  If the 
 * receive FIFO has data, read it 1 byte and call buildPacket()
 */
void serialDoWork( void )
{
	unsigned char nData;
	BOOL bReenableInterrupts;
	xmitQueueEntry_t nextXmit;

	if(U2LSR & TEMT)
	{
		//printf("sdw 2empt\n\r");
		/////
		// half duplex RS485
		// finished sending data
		// set up to receive
		/////
		serialRS485SetMode(RECV);
	}
	/////
	// look for a packet
	// from  RS485
	/////
	bReenableInterrupts = FALSE;
	if(U2IER & RX_DATA_AVAILABLE_INTERRUPT_ENABLE)
	{
		bReenableInterrupts = TRUE;
	}
	while(1)
	{
		/////
		// disable receive interrupts
		// while we are messing with the fifo
		/////
		U2IER &= ~(RX_DATA_AVAILABLE_INTERRUPT_ENABLE);
		if(nRxRS485AddToIndex == nRxRS485RemoveFromIndex)
		{
			break;
		}
		
		nData = rxRS485FIFO[nRxRS485RemoveFromIndex++];
		//printf("%X ", nData); // jgr old
		if(sizeof(rxRS485FIFO) <= nRxRS485RemoveFromIndex)
		{
			nRxRS485RemoveFromIndex = 0;
		}
		if(bReenableInterrupts)
		{
			U2IER |= RX_DATA_AVAILABLE_INTERRUPT_ENABLE;
		}
		//startTimer(&sendQueueIdleTimer[2], SEND_QUEUE_IDLE_TIME);
		buildPacket(eINTERFACE_RS485, nData, rxRS485Packet, &rxRS485PacketIndex);
		
		/////
		// set mode to XMIT here?
		// no, not here
		/////
	}

	if(bReenableInterrupts)
	{
		U2IER |= RX_DATA_AVAILABLE_INTERRUPT_ENABLE;
	}
	/////
	// look for a packet
	// from  wireless
	/////
	bReenableInterrupts = FALSE;
	switch(eOurPlatformType)
	{
		case ePLATFORM_TYPE_ARROW_BOARD:
		case ePLATFORM_TYPE_REPEATER:
			{
				if(U1IER & RX_DATA_AVAILABLE_INTERRUPT_ENABLE)
				{
					bReenableInterrupts = TRUE;
				}
			}
			break;
		case ePLATFORM_TYPE_HANDHELD:
			{
				if(U3IER & RX_DATA_AVAILABLE_INTERRUPT_ENABLE)
				{
					bReenableInterrupts = TRUE;
				}
			}
			break;
	}
	while(1)
	{
		/////
		// disable interrupts
		// while we are messing with the FIFO
		/////
		switch(eOurPlatformType)
		{
			case ePLATFORM_TYPE_ARROW_BOARD:
			case ePLATFORM_TYPE_REPEATER:
				U1IER &= ~(RX_DATA_AVAILABLE_INTERRUPT_ENABLE);
				break;
			case ePLATFORM_TYPE_HANDHELD:
				U3IER &= ~(RX_DATA_AVAILABLE_INTERRUPT_ENABLE);
				break;
		}		
		if(nRxWirelessAddToIndex == nRxWirelessRemoveFromIndex)
		{
			break;
		}
		nData = rxWirelessFIFO[nRxWirelessRemoveFromIndex++];
		//printf("2w-nData[%X]\n\r", nData);
		if(sizeof(rxWirelessFIFO) <= nRxWirelessRemoveFromIndex)
		{
			nRxWirelessRemoveFromIndex = 0;
		}
		if(bReenableInterrupts)
		{
			switch(eOurPlatformType)
			{
				case ePLATFORM_TYPE_ARROW_BOARD:
				case ePLATFORM_TYPE_REPEATER:
					U1IER |= RX_DATA_AVAILABLE_INTERRUPT_ENABLE;
					break;
			case ePLATFORM_TYPE_HANDHELD:
					U3IER |= RX_DATA_AVAILABLE_INTERRUPT_ENABLE;
					break;
			}
		}
		buildPacket(eINTERFACE_WIRELESS, nData, rxWirelessPacket, &rxWirelessPacketIndex);
	}
	if(bReenableInterrupts)
	{
		switch(eOurPlatformType)
		{
			case ePLATFORM_TYPE_ARROW_BOARD:
			case ePLATFORM_TYPE_REPEATER:
				U1IER |= RX_DATA_AVAILABLE_INTERRUPT_ENABLE;
				break;
			case ePLATFORM_TYPE_HANDHELD:
				U3IER |= RX_DATA_AVAILABLE_INTERRUPT_ENABLE;
				break;
		}
	}

	// Determine if we can xmit new packet, if so and something is available
	// then xmit it.
	if( anythingToXmit( &nextXmit ) )		// Will load up nextXmit
	{
		serialWrite( nextXmit.eInterface, &nextXmit.packet[0], getQueuePacketLength( &nextXmit ) );
		// If sending back ack, (driver board), then done.  Only one xmit of ack's
		if( PACKET_TYPE_ACK == getQueuePacketType( &nextXmit ) )
		{
			popXmitQueue( getQueuePacketSequenceNum( &nextXmit ) );
		}
	}
}
BOOL serialReadWireless(unsigned char* pData)
{
	BOOL bRetVal = FALSE;
	switch(eOurPlatformType)
	{
		case ePLATFORM_TYPE_ARROW_BOARD:
		case ePLATFORM_TYPE_REPEATER:
			{
				if(U1LSR & RDR )
				{
					*pData = U1RBR;
					bRetVal = TRUE;
				}
			}
			break;
		case ePLATFORM_TYPE_HANDHELD:
		{
			if(U3LSR & RDR )
			{
				*pData = U3RBR;
				bRetVal = TRUE;
			}
			if( U3LSR & ((1 << 1) | (1 << 3) | (1 << 7)) )
			{
				printf("********ERROR 0x%X  *******\n", U3LSR);
			}
		}
		break;
	}
	return bRetVal;
}

BOOL serialWriteWireless(unsigned char nData)
{
	BOOL bRetVal = FALSE;
	switch(eOurPlatformType)
	{
		case ePLATFORM_TYPE_ARROW_BOARD:
		case ePLATFORM_TYPE_REPEATER:
			{
				while(0 == (U1LSR & THRE))
				{
				}
				U1THR = nData;
			}
			break;
		case ePLATFORM_TYPE_HANDHELD:
		{
				while(0 == (U3LSR & THRE))
				{
				}
				U3THR = nData;
		}
		break;
	}
	return bRetVal;
}

void serialWrite(eINTERFACE eInterface, unsigned char* pPacket, int nPacketLength)
{
	int nIndex;
	//int i;
	switch(eInterface)
	{
		case eINTERFACE_DEBUG:
			break;
		case eINTERFACE_WIRELESS:
			#ifdef PACKET_PRINT
			printf("serialWrite(wireless) seq#:%d  time:%d\n",pPacket[2], getTimerNow());
			#endif
		
			for(nIndex=0; nIndex<nPacketLength; nIndex++)
			{
				serialWirelessSendData(pPacket[nIndex]);
				//printf("%X ",pPacket[nIndex]);//jgr new
			}
			break;
		case eINTERFACE_RS485:
			#ifdef PACKET_PRINT
			printf("serialWrite(485) seq#:%d  time:%d\n",pPacket[2], getTimerNow());//jgr new
			#endif
		
			for(nIndex=0; nIndex<nPacketLength; nIndex++)
			{
				serialRS485SendData(pPacket[nIndex]);
			}
			break;
		default:
			printf("serialWrite(Unknown eInterface) defaulting to wireless\n");
				for(nIndex=0; nIndex<nPacketLength; nIndex++)
			{
				serialWirelessSendData(pPacket[nIndex]);
			}
			break;
	}
}
