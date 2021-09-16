/*****************************************************************************
 *   LCD.c:  LCD C Functions for controlling the LCD
 *
 *   History
 *   2014.07.24  ver 1.00    Prelimnary version
 *
*****************************************************************************/
#include "LPC23xx.h"			/* LPC23XX/24xx Peripheral Registers */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "shareddefs.h"
#include "sharedinterface.h"
#include "irq.h"
#include "commboard.h"
#include "timer.h"
#include "queue.h"
#include "runlength.h"
#include "uidriver.h"
#include "pwm.h"
#include "storedconfig.h"
#include "LCD.h"
#include "watchdog.h"

#define DEBUG_SPEW 1

static WORD X1,Y1;
static int X2,Y2;
static BOOL touchDetected = FALSE;
static eSLEEP_MODE_STATES eLastSleepModeState = eSLEEP_MODE_DISABLED;
static BOOL						tpCalibrationSucceeded  = FALSE;
static BOOL 					tpVerificationSucceeded = FALSE;
static TP_POINT       tpLCDPoints[3]; 
static TP_POINT       tpTPPoints[3]; 
static TP_MATRIX      tpMatrix;
static TP_POINT       raw,calibrated;
static int lcdDisplayHeight = 272;
static int lcdDisplayWidth  = 480;

static unsigned int x_samples[50]; 
static unsigned int y_samples[50]; 
static int oldX, oldY;

void LCD_Set_CS(void)
{
	FIO1CLR =  (1 << LCD_CS);
}

void LCD_Set_Direction(BOOL input)
{
	if(input)
	{
		FIO2DIR0 = 0x00;
	}
	else
	{
		FIO2DIR0 = 0xFF;
	}
}

void LCD_Write_Command_Byte(BYTE command)
{
		LCD_Set_Direction(FALSE); // Set Pins D0-D7 as Outputs
	
		FIO2PIN0  = command;
								
		// Write Command Byte: RD=1, WR=0, DC=1	
		FIO1CLR = (1 << LCD_WR);
		FIO1SET = (1 << LCD_RD)|
							(1 << LCD_DC);	
	
		FIO1SET = (1 << LCD_WR);
		FIO1CLR = (1 << LCD_RD)|
							(1 << LCD_DC);	
	
		LCD_Chk_Wait();
}

void LCD_Write_Command_Word(WORD command)
{
		BYTE temp;
		
		temp = command;
		LCD_Write_Command_Byte(temp);
		temp = command >> 8;
		LCD_Write_Command_Byte(temp);		
}

void LCD_Write_Data_Byte(BYTE data)
{
		LCD_Set_Direction(FALSE);
	
		FIO2PIN0 = data;
		
    // Write Data Byte: RD=1, WR=0, DC=0   			
		FIO1CLR = (1 << LCD_WR);
		FIO1CLR = (1 << LCD_DC);	
		FIO1SET = (1 << LCD_RD);

		FIO1SET = (1 << LCD_WR);
		FIO1SET = (1 << LCD_DC);
		FIO1CLR = (1 << LCD_RD);
	
		LCD_Chk_Wait();
}

void LCD_Write_Data_Word(WORD data)
{
		BYTE temp;
		
		LCD_Set_Direction(FALSE); // Set Pins D0-D7 as Outputs
					
		temp = data>>8;
		LCD_Write_Data_Byte(temp);
	
		temp = data;
		LCD_Write_Data_Byte(temp);		
}

void LCD_Write_Data_Direct(BYTE command, BYTE data)
{
		LCD_Write_Command_Byte(command);
	
		LCD_Write_Data_Byte(data);
}

void LCD_Read_Data_Byte(BYTE* data)
{
		LCD_Set_Direction(TRUE); // Set Pins D0-D7 as Inputs
	
		// Read Data Byte: RD=0, WR=1, DC=0 
		FIO1CLR = (1 << LCD_RD);
		FIO1CLR = (1 << LCD_DC);
	
		*data = FIO2PIN0;
			
		FIO1SET = (1 << LCD_RD);
		FIO1SET = (1 << LCD_DC);
	
		LCD_Chk_Wait();	
}

void LCD_Read_Data_Word(WORD* data)
{
    union
    {
       BYTE bytes[2];
       WORD word;
    } retVal;
		
		LCD_Read_Data_Byte((BYTE *)&retVal.bytes[1]);
		LCD_Read_Data_Byte((BYTE *)&retVal.bytes[0]);
		
	*data = retVal.word;
}

void LCD_Read_Data_Direct(BYTE cmd, BYTE* data)
{
		LCD_Write_Command_Byte(cmd);
		LCD_Read_Data_Byte(data);
}

void LCD_Read_Status(BYTE* status)
{
		LCD_Set_Direction(TRUE); // Set Pins D0-D7 as Inputs
	
		// Read Status Byte: RD=0, WR=1, DC=1 
		FIO1SET = (1 << LCD_WR);
		FIO1CLR = (1 << LCD_RD);
		FIO1SET = (1 << LCD_DC);
	
		*status = FIO2PIN0;
					
		FIO1SET = (1 << LCD_RD);
	
		LCD_Chk_Wait();	
}

void LCD_PLL_Init(void)
{
    LCD_Write_Data_Direct(PLLC1,0x0a);
        
    LCD_Write_Data_Direct(PLLC2,0x02);
}	

void LCD_Init(void)
{			
    FIO1DIR1 = 0xFD;	
		
    LCD_PLL_Init();
	
		//====================
		// PWRR Reset Display
		//====================
    LCD_Write_Data_Direct(PWRR,0x81);  
	
		//==================
		// PWRR Normal Mode 
		//==================
    LCD_Write_Data_Direct(PWRR,0x80);    
		
		//=================================================================
    // SYSR bit[3:2] color,  bit[1:0]= MPU interface, 8BIT, 65K Colors
		//==================================================================
    LCD_Write_Data_Direct(SYSR,0x08); 
		
		//=====
		//PCLK
		//=====
    LCD_Write_Data_Direct(PCSR,0x82); 
		
		//====================
    //Horizontal settings
		//====================
    LCD_Write_Data_Direct(HDWR,0x3b); //HDWR//Horizontal Display Width Setting Bit[6:0]  
                                      //Horizontal display width(pixels) = (HDWR + 1)*8  0x27
		

    LCD_Write_Data_Direct(HNDFTR,0x02);  //HNDFCR//Horizontal Non-Display Period fine tune Bit[3:0] 
                                         //(HNDR + 1)*8 +HNDFCR
		

    LCD_Write_Data_Direct(HNDR,0x03);  //HNDR//Horizontal Non-Display Period Bit[4:0] 
                                       //Horizontal Non-Display Period (pixels) = (HNDR + 1)*8 
		

    LCD_Write_Data_Direct(HSTR,0x01);  //HSTR//HSYNC Start Position[4:0] 
                                       //HSYNC Start Position(PCLK) = (HSTR + 1)*8 
		

    LCD_Write_Data_Direct(HPWR,0x03); //HPWR//HSYNC Polarity ,The period width of HSYNC. 
                                      //HSYNC Width [4:0]   HSYNC Pulse width(PCLK) = (HPWR + 1)*8 
		//==================
    //Vertical settings
		//==================
    LCD_Write_Data_Direct(VDHR0,0x0F); //VDHR0 //Vertical Display Height Bit [7:0] 
                                        //Vertical pixels = VDHR + 1	0xef

    LCD_Write_Data_Direct(VDHR1,0x01); //VDHR1 //Vertical Display Height Bit [8] 
                                       //Vertical pixels = VDHR + 1 	0x01

    LCD_Write_Data_Direct(VNDR0,0x0F); //VNDR0 //Vertical Non-Display Period Bit [7:0]
                                       //Vertical Non-Display area = (VNDR + 1) 

    LCD_Write_Data_Direct(VNDR1,0x00); //VNDR1 //Vertical Non-Display Period Bit [8] 
                                       //Vertical Non-Display area = (VNDR + 1) 
		
    LCD_Write_Data_Direct(VSTR0,0x00); //VSTR0 //VSYNC Start Position[7:0]
                                       //VSYNC Start Position(PCLK) = (VSTR + 1) 

    LCD_Write_Data_Direct(VSTR1,0x00); //VSTR1 //VSYNC Start Position[8] 
                                       //VSYNC Start Position(PCLK) = (VSTR + 1) 

    LCD_Write_Data_Direct(VPWR,0x01); //VPWR //VSYNC Polarity ,VSYNC Pulse Width[6:0]
																      //VSYNC Pulse Width(PCLK) = (VPWR + 1) 
																 
		//============
		//PWM setting
		//============
    LCD_Write_Data_Direct(P1CR,0x80); 
		
		//======================================================
		//Default Backlight brightness setting: Full Brightness
		//======================================================
    LCD_Write_Data_Direct(P1DCR,0xFF);
		
		//===================
		// Touch Panel Init
		//===================
    LCD_Write_Data_Direct(INTC1,0x04);   // Enable TP_INT
		
    LCD_Write_Data_Direct(TPCR1,0x05);   // Set to 4-wire touch screen/Manual Mode/Wait for TP Event
		
    LCD_Write_Data_Direct(TPCR0,0x88);   // TP Enable/Sample Timing/Wake Up Enable/ADC Clk
		
		//==============================
		// Set the working window size
		//==============================
    LCD_Set_Active_Window(0,479,0,271); 

		//======================
		// Clear Display Memory
		//======================
		LCD_Write_Data_Direct(MCLR,0x80);    
		
		//=================================
		// Wait for Memory Clear to Finish
		//=================================
		LCD_Chk_Mem_Clr();
		
		//==============================================================================================================
		// Initialize the Block Transfer Engine (BTE). Section 7-6 of the RAIO RA8875 TFT LCD Controller Specification
		//==============================================================================================================		
	  LCD_Write_Data_Direct(DPCR,TWO_LAYER_DISPLAY_MODE); // Display Configuration - 2 Layer/Normal Horizontal Scan/Normal Vertical Scan
 	
    LCD_Write_Data_Direct(MWCR0,AUTO_INCREMENT); // Set Graphics Mode, Auto Increment	
		
    LCD_Write_Data_Direct(LTPR1,LCD_DISABLE_LAYER_TWO_DISPLAY);	// Disable DIsplay of Layer 2
		
		LCD_Source_Layer2(); //Select Layer 2 as the Bitmap Data Source
	
		LCD_Destination_Layer1();	// Select Layer 1 as the Bitmap Data Destination
	
	  LCD_Layer1_Visible();	// Select Layer 1 as the Visible Layer
		
		LCD_Clear_Display(TRUE, LAYER_ONE); //Clear Both Layers
		LCD_Clear_Display(TRUE, LAYER_TWO);
		
		LCD_Text_Background_Color1(color_background);	
#if(0)			
		// Configure the EINT1 pin/handler
  	EXTMODE |= 0x00000002;		// EINT1 edge trigger
  	EXTPOLAR = 1;				      // EINT1 Rising Edge 
		
		if (!(install_irq( EINT1_INT, (void (*)(void) __irq)LCD_INT_Handler, HIGHEST_PRIORITY )))
		{
			printf("Could Not install LCD_TP_Handler!\n");
		}
		else
		{
			printf("LCD_TP_Handler Installed Successfully!\n");
		}	
#endif		
		LCD_Chk_Wait();
		
		storedConfigGetTPCalCoefficients(&tpMatrix);
}

///////////////Text Background color settings
void LCD_Text_Background_Color1(WORD b_color)
{	
    LCD_Write_Data_Direct(BGCR0,(BYTE)(b_color>>11));//BGCR0: Red
	
    LCD_Write_Data_Direct(BGCR1,(BYTE)(b_color>>5));//BGCR1: Green

    LCD_Write_Data_Direct(BGCR2,(BYTE)(b_color));//BGCR2: Blue
} 

////////////////Text Foreground color settings
void LCD_Text_Foreground_Color1(WORD f_color)
{	
    LCD_Write_Data_Direct(FGCR0,(BYTE)(f_color>>11));//FGCR0
	
    LCD_Write_Data_Direct(FGCR1,(BYTE)(f_color>>5)); //FGCR1
	
    LCD_Write_Data_Direct(FGCR2,(BYTE)(f_color));    //FGCR2
}

////////////////Individual (Red,Green, and Blue) Foreground color settings
void LCD_Text_Foreground_Color(BYTE setRed,BYTE setGreen,BYTE setBlue)
{	    
    LCD_Write_Data_Direct(FGCR0,setRed);   //FGCR0
   
    LCD_Write_Data_Direct(FGCR1,setGreen); //FGCR1

    LCD_Write_Data_Direct(FGCR2,setBlue);  //FGCR2
}

///////////////Individual (Red,Green, and Blue) Background color settings
void LCD_Text_Background_Color(BYTE setRed, BYTE setGreen, BYTE setBlue)
{
    LCD_Write_Data_Direct(BGCR0,setRed);  //BGCR0: Red
   
    LCD_Write_Data_Direct(BGCR1,setGreen);//BGCR1: Green

    LCD_Write_Data_Direct(BGCR2,setBlue); //BGCR2: Blue
} 

void LCD_Clear_Display(BOOL clearEntireWindow, BYTE layer)
{
		if(layer == LAYER_ONE || layer == LAYER_TWO)
		{
				// Select Display Layer 1 or 2
				LCD_Write_Data_Direct(MWCR1,layer);
			
				if( clearEntireWindow )
				{
					//Clear Display Memory (Entire Window) 
					LCD_Write_Data_Direct(MCLR,CLEAR_ENTIRE_WINDOW);
				}
				else
				{
					//Clear Display Memory (Active Window) 
					LCD_Write_Data_Direct(MCLR,CLEAR_ACTIVE_WINDOW);
					
				}
					//Check for Memory Clear Complete
					LCD_Chk_Mem_Clr(); 
		}
}

///////////////check busy
void LCD_Chk_Busy(void)
{
    BYTE temp; 
    	
    do
    {
      LCD_Read_Status(&temp);
#if(DEBUG_SPEW)			
			printf("BUSY[%X]\n",temp);
#endif			
			
    } while((temp & MEM_READ_WRITE_BUSY) == MEM_READ_WRITE_BUSY);
}

void LCD_Chk_Mem_Clr(void)
{
    BYTE temp; 
    	
    do
    {
       LCD_Read_Data_Direct(MCLR,&temp);
			
    }while((temp & MEM_CLEAR_BUSY) == MEM_CLEAR_BUSY);
}

///////////////check wait
void LCD_Chk_Wait(void)
{
 	while(!(FIO1PIN1 & LCD_WAIT_SIGNAL))
	{
//printf("LCD_Chk_Wait\n");  
	}
}

///////////////check bte busy
void LCD_Chk_BTE_Busy(void)
{
    BYTE temp; 	
	
    do
    {
      LCD_Read_Status(&temp);
#if(DEBUG_SPEW)			
			printf("BTE_BUSY[%X]\n",temp);	
#endif
			
    }while((temp & BTE_BUSY) == BTE_BUSY);		   
}

void LCD_Chk_Circle_Draw_Busy(void)
{
	BYTE temp;
	
	LCD_Write_Command_Byte(0x90);
	
	do
	{
		LCD_Read_Data_Byte(&temp);
		
	}while((temp & CIRCLE_DRAW_BUSY) == CIRCLE_DRAW_BUSY);
}

void LCD_Chk_Line_Draw_Busy(void)
{
	BYTE temp;
	
	LCD_Write_Command_Byte(0x90);
	
	do
	{
		LCD_Read_Data_Byte(&temp);
		
	}while((temp & LINE_DRAW_BUSY) == LINE_DRAW_BUSY);
}

///////////drawing circle
void  LCD_Draw_Circle(WORD X,WORD Y,WORD R)
{
	BYTE temp;
    
	temp=X;   
  LCD_Write_Command_Byte(0x99);
	LCD_Write_Data_Byte(temp);
	temp=X>>8;   
  LCD_Write_Command_Byte(0x9a);	   
	LCD_Write_Data_Byte(temp);  
	  
	temp=Y;   
  LCD_Write_Command_Byte(0x9b);
	LCD_Write_Data_Byte(temp);
	temp=Y>>8;   
  LCD_Write_Command_Byte(0x9c);	   
	LCD_Write_Data_Byte(temp);

	temp=R;   
  LCD_Write_Command_Byte(0x9d);
	LCD_Write_Data_Byte(temp);
} 

////////////////////////////////////////////////////////////////
//   Helper function for higher level circle drawing code
////////////////////////////////////////////////////////////////
void LCD_Draw_Circle_Helper(WORD xCoord, WORD yCoord, WORD radius, WORD color, BOOL filled)
{
  /* Set X */
  LCD_Write_Command_Byte(0x99);
  LCD_Write_Data_Byte(xCoord);
  LCD_Write_Command_Byte(0x9a);
  LCD_Write_Data_Byte(xCoord >> 8);
  
  /* Set Y */
  LCD_Write_Command_Byte(0x9b);
  LCD_Write_Data_Byte(yCoord); 
  LCD_Write_Command_Byte(0x9c);	   
  LCD_Write_Data_Byte(yCoord >> 8);
  
  /* Set Radius */
  LCD_Write_Command_Byte(0x9d);
  LCD_Write_Data_Byte(radius);  
  
  /* Set Color */
  LCD_Write_Command_Byte(0x63);
  LCD_Write_Data_Byte((color & 0xf800) >> 11);
  LCD_Write_Command_Byte(0x64);
  LCD_Write_Data_Byte((color & 0x07e0) >> 5);
  LCD_Write_Command_Byte(0x65);
  LCD_Write_Data_Byte((color & 0x001f));
  
  /* Draw! */
  LCD_Write_Command_Byte(0x90);
	
  if (filled)
  {
    LCD_Write_Data_Byte(0x40 | 0x20);
  }
  else
  {
    LCD_Write_Data_Byte(0x40);
  }
  
  /* Wait for the circle drawing command to finish */
	LCD_Chk_Circle_Draw_Busy();
}

void LCD_Draw_Rectangle(WORD xCoord, WORD yCoord, WORD width, WORD height, WORD color)
{
  LCD_Draw_Rectangle_Helper(xCoord, yCoord, xCoord+width, yCoord+height, color, FALSE);
}

void LCD_Draw_Rectangle_Helper(WORD xCoord, WORD yCoord, WORD width, WORD height, WORD color, BOOL filled)
{
  /* Set X */
  LCD_Write_Command_Byte(0x91);
  LCD_Write_Data_Byte(xCoord);
  LCD_Write_Command_Byte(0x92);
  LCD_Write_Data_Byte(xCoord >> 8);
  
  /* Set Y */
  LCD_Write_Command_Byte(0x93);
  LCD_Write_Data_Byte(yCoord); 
  LCD_Write_Command_Byte(0x94);	   
  LCD_Write_Data_Byte(yCoord >> 8);
  
  /* Set X1 */
  LCD_Write_Command_Byte(0x95);
  LCD_Write_Data_Byte(width);
  LCD_Write_Command_Byte(0x96);
  LCD_Write_Data_Byte((width) >> 8);
  
  /* Set Y1 */
  LCD_Write_Command_Byte(0x97);
  LCD_Write_Data_Byte(height); 
  LCD_Write_Command_Byte(0x98);
  LCD_Write_Data_Byte((height) >> 8);

  /* Set Color */
  LCD_Write_Command_Byte(0x63);
  LCD_Write_Data_Byte((color & 0xf800) >> 11);
  LCD_Write_Command_Byte(0x64);
  LCD_Write_Data_Byte((color & 0x07e0) >> 5);
  LCD_Write_Command_Byte(0x65);
  LCD_Write_Data_Byte((color & 0x001f));

  /* Draw Line */
  LCD_Write_Command_Byte(0x90);
	
  if (filled)
  {
    LCD_Write_Data_Byte(0xB0);
  }
  else
  {
    LCD_Write_Data_Byte(0x90);
  }
  
  /* Wait for the line drawing command to finish */
	LCD_Chk_Line_Draw_Busy();
}

void LCD_Source_Layer1(void)
{   
	BYTE temp;	
	LCD_Write_Command_Byte(VSBE1);
	LCD_Read_Data_Byte(&temp);
	temp &= 0x7F;
	LCD_Write_Data_Byte(temp);
}

void LCD_Source_Layer2(void)
{	
	BYTE temp;	
	LCD_Write_Command_Byte(VSBE1);
	LCD_Read_Data_Byte(&temp);
	temp |= 0x80;
	LCD_Write_Data_Byte(temp);
}

void LCD_Destination_Layer1(void)
{	
	BYTE temp;	
	LCD_Write_Command_Byte(VDBE1);
	LCD_Read_Data_Byte(&temp);
	temp &= 0x7F ;
	LCD_Write_Data_Byte(temp);
}

void LCD_Destination_Layer2(void)
{	
	BYTE temp;	
	LCD_Write_Command_Byte(VDBE1);
	LCD_Read_Data_Byte(&temp);
	temp |= 0x80 ;
	LCD_Write_Data_Byte(temp);
}

void LCD_Layer1_Visible(void)
{	
	BYTE temp;
	LCD_Write_Command_Byte(LTPR0);
	LCD_Read_Data_Byte(&temp);
	temp&=0xf8;
	LCD_Write_Data_Byte(temp);  
}

void LCD_Layer2_Visible(void)
{   
	BYTE temp;
	LCD_Write_Command_Byte(LTPR0);
	LCD_Read_Data_Byte(&temp);
	temp&=0xf8;
	temp|=0x01;
	LCD_Write_Data_Byte(temp);  
}	

//////////////////Block Transfer Engine (BTE) area size settings
void LCD_Set_BTE_Size(WORD width,WORD height)
{
    BYTE temp;

    temp=width;   
    LCD_Write_Data_Direct(BEWR0,temp);//BET Width 0

    temp=width>>8;   
    LCD_Write_Data_Direct(BEWR1,temp);//BET Width	1   

    temp=height;   
    LCD_Write_Data_Direct(BEHR0,temp);//BET Height 0

    temp=height>>8;   
    LCD_Write_Data_Direct(BEHR1,temp);//BET Height 1   
}

////////////////////Block Transfer Engine (BTE) starting position
void LCD_Set_BTE_Source_Destination( WORD sourceX, WORD destinationX ,WORD sourceY ,WORD destinationY, eSOURCELAYERSELECT sourceLayer, eDESTINATIONLAYERSELECT destinationLayer)
{
    WORD temp;
		BYTE temp1;
    
    temp=sourceX;   
    LCD_Write_Data_Direct(HSBE0,temp); // BTE Horizontal Source Point 0 (8 bits)

    temp=sourceX>>8;   
    LCD_Write_Data_Direct(HSBE1,temp); // BTE Horizontal Source Point 1 (2 bits) 

    temp=destinationX;
    LCD_Write_Data_Direct(HDBE0,temp); // BET Horizontal Destination Point 0  (8 bits)

    temp=destinationX>>8;   
    LCD_Write_Data_Direct(HDBE1,temp); // BET Horizontal Destination Point 1  (2 bits)   
    
    temp=sourceY;   
    LCD_Write_Data_Direct(VSBE0,temp); // BTE Vertical Source Point 0 (8 bits)

    temp=sourceY>>8;   
    LCD_Write_Data_Direct(VSBE1,temp); // BTE Vertical Source Point 1 (2 bits)
    LCD_Read_Data_Direct(VSBE1,&temp1);
		
		switch(sourceLayer)
		{
			case LCD_SELECT_SOURCE_LAYER_ONE:
				temp1 &= 0x80; // Select Source Layer 1
				break;
			
			case LCD_SELECT_SOURCE_LAYER_TWO:
				temp1 |= 0x80; // Select Source Layer 2
				break;
		}
		
    temp=temp|temp1;
		
    LCD_Write_Data_Direct(VSBE1,temp);//BTE Vertical Source Layer 1  

    temp=destinationY;   
    LCD_Write_Data_Direct(VDBE0,temp);//BTE Vertical Destination Point 0

    temp=destinationY >> 8;   
    LCD_Write_Data_Direct(VDBE1,temp);//BTE Vertical Destination Point 1
    LCD_Read_Data_Direct(VDBE1,&temp1);
		
		switch(destinationLayer)
		{
			case LCD_SELECT_DESTINATION_LAYER_ONE:
				temp1 &= 0x80; // Select Destination Layer 1
				break;
			
			case LCD_SELECT_DESTINATION_LAYER_TWO:
				temp1 |= 0x80; // Select Destination Layer 2
				break;
		}
		
    temp=temp|temp1;
		
    LCD_Write_Data_Direct(VDBE1,temp);//BTE Vertical Destination Layer 2
}				

void LCD_Font_Write_Position(WORD X,WORD Y)
{
    BYTE temp;

    temp=X;   
    LCD_Write_Data_Direct(F_CURXL,temp);

    temp=X>>8;   
    LCD_Write_Data_Direct(F_CURXH,temp);

    temp=Y;   
    LCD_Write_Data_Direct(F_CURYL,temp);

    temp=Y>>8;   
    LCD_Write_Data_Direct(F_CURYH,temp);
}

void LCD_Read_Font_Position(WORD* X,WORD* Y)
{
    WORD temp;
      
    LCD_Read_Data_Direct(F_CURXL,(BYTE *)&temp);
    *X=temp;

    LCD_Read_Data_Direct(F_CURXH,(BYTE *)&temp);
    *X|=temp<<8;   
    *X -= 1;

    LCD_Read_Data_Direct(F_CURYL,(BYTE *)&temp);
    *Y=temp;   

    LCD_Read_Data_Direct(F_CURYH,(BYTE *)&temp);
    *Y|=temp<<8;
    *Y -= 1;   
}

///////////////Write Display Memory X&Y Position
void LCD_Write_DMemory_Position(WORD X,WORD Y)
{
    BYTE temp;

    temp=X;   
    LCD_Write_Data_Direct(CURH0,temp);

    temp=X>>8;   
    LCD_Write_Data_Direct(CURH1,temp);

    temp=Y;   
    LCD_Write_Data_Direct(CURV0,temp);

    temp=Y>>8;   
    LCD_Write_Data_Direct(CURV1,temp);
}

///////////////Read Display Memory X&Y Position
void LCD_Read_DMemory_Position(WORD* X,WORD* Y)
{
    WORD temp;
       
    LCD_Read_Data_Direct(RCURH0,(BYTE *)&temp);
    *X=temp;

    LCD_Read_Data_Direct(0x4B,(BYTE *)&temp);
    *X|=temp<<8;
		*X -= 1;

    LCD_Read_Data_Direct(RCURV0,(BYTE *)&temp);
    *Y=temp;   

    LCD_Read_Data_Direct(RCURV1,(BYTE *)&temp);
    *Y|=temp<<8;
		*Y -= 1;
}

void LCD_Write_String(BYTE *str, BYTE x, BYTE y, BYTE layer)
{ 		
    LCD_Write_Data_Direct(FNCR0,0x00); //Select the internal character set
                                     
    LCD_Write_Data_Direct(FNCR1,0x00); //Select the internal character set

    LCD_Write_Data_Direct(FWT,0xC0);  

    LCD_Write_Data_Direct(MWCR0,0x80); //Set the text mode
	
    LCD_Write_Data_Direct(MWCR1,layer); //Set the layer to write to

    LCD_Font_Write_Position(x, y); //Text written to the position

    LCD_Write_Command_Byte(DISPLAY_MEMORY_WRITE);

    while(*str != '\0')
    {
        LCD_Write_Data_Byte(*str); 

        ++str;
    } 
}

void LCD_Set_Active_Window(WORD XL,WORD XR ,WORD YT ,WORD YB)
{
    BYTE temp;

    //setting active window X
    temp=XL;   
    LCD_Write_Data_Direct(HSAW0,temp);//HSAW0

    temp=XL>>8;   
    LCD_Write_Data_Direct(HSAW1,temp);//HSAW1	   

    temp=XR;   
    LCD_Write_Data_Direct(HEAW0,temp);//HEAW0

    temp=XR>>8;   
    LCD_Write_Data_Direct(HEAW1,temp);//HEAW1	   

    //setting active window Y
    temp=YT;   
    LCD_Write_Data_Direct(VSAW0,temp);//VSAW0

    temp=YT>>8;   
    LCD_Write_Data_Direct(VSAW1,temp);//VSAW1	   

    temp=YB;   
    LCD_Write_Data_Direct(VEAW0,temp);//VEAW0

    temp=YB>>8;   
    LCD_Write_Data_Direct(VEAW1,temp);//VEAW1	   
}


/////////Read the X coordinate from the TP
BYTE LCD_ADC_X(void)
{
    BYTE Xcoord;

		LCD_Write_Command_Byte(TPXH);//TPXH	 X_coordinate high byte
    
    LCD_Read_Data_Byte(&Xcoord);
	
    return Xcoord;
}

/////////Read the Y coordinate from the TP
BYTE LCD_ADC_Y(void)
{
    BYTE Ycoord;
			
    LCD_Write_Command_Byte(TPYH);//TPYH	  Y_coordinate high byte
    
    LCD_Read_Data_Byte(&Ycoord);
		
    return Ycoord;
}

////////////Read the TP X and Y coordinates low bits
BYTE LCD_ADC_XY(void)
{	
    BYTE XYcoord;
	
    LCD_Write_Command_Byte(TPXYL);//TPXYL	  bit[3:2] Y_coordinate low bits  bit[1:0] X_coordinate low bits 
    
    LCD_Read_Data_Byte(&XYcoord);
		
    return XYcoord;
} 

// LCD Interrupt Handler
void LCD_INT_Handler (void) __irq 
{
	BYTE temp;
	
	EXTINT = EXTINT1_CLEAR;  //clear the EINT1 flag
	
	LCD_Read_Data_Direct(INTC2,&temp);
	
	if(temp & MCU_INT)
	{
#if(DEBUG_SPEW)
		printf("MCU_INT[%X]\n",(temp&0x01));
#endif
		LCD_Write_Data_Direct(0xF1,(temp&0x01));//Clear MCU_INT	
	}
		
	if(temp & BTE_INT)
	{
#if(DEBUG_SPEW)
		printf("BTE_INT[%X]\n",(temp&0x02));
#endif
		LCD_Write_Data_Direct(0xF1,(temp&0x02));//Clear BTE_INT	
	}
		
	if(temp & TP_INT)
	{
#if(DEBUG_SPEW)
		printf("TP_INT[%X]\n",(temp&0x04));
#endif
		//===================================================
		// Read Y Axis High and Low Bytes
		// D[9:2] from reg TPYH, D[1:0] from reg TPXYL[3:2]	
		//===================================================
		Y1 = LCD_ADC_Y() << 2 | ( (LCD_ADC_XY() & 0x0C) >> 2 ); 
		
		//==================================================
		// Read X Axis High and Low Bytes
		// D[9:2] from reg TPXH, D[1:0] from reg TPXYL[1:0]
		//==================================================
		X1 = LCD_ADC_X() << 2 | ( (LCD_ADC_XY() & 0x03) ); 
		
		//==================================================
		// Convert Y to work with the Height of the display
		//==================================================
		Y2 =(lcdDisplayHeight-(((Y1-63))*10/33));
		
		//==============================
		// If Y is Negative set to zero
		//==============================
		if(Y2 < 0)
		{
			Y2=0;
		}
		
		//==================================================
		// Convert X to work with the Width of the display
		//==================================================		
		X2=(lcdDisplayWidth-(((X1-50))*10/18));
		
		//==============================
		// If X is Negative set to zero
		//==============================
		if(X2 < 0)
		{
			X2=0;
		}		
		LCD_Write_Data_Direct(INTC2,(temp & TP_INT));//Clear TP_INT
	}
	
  VICVectAddr = 0;		/* Acknowledge Interrupt */	
}

BOOL LCD_Touch_In_Progress(void)
{
	BYTE tpInterrupt,tpTouchStatus;
	BOOL tpTouchInProgress = FALSE;

	// If sleep mode was enabled and we got a TP interrupt,
	// ignore the TP interrupt so we don't process the TP
	// press and refresh the current screen.
	if(eLastSleepModeState == eSLEEP_MODE_ENABLED)
	{				 
		LCD_Read_Data_Direct(INTC2,&tpInterrupt); // Check for TP_INT
		
		if((tpInterrupt & TP_INT) == TP_INT)
		{
			LCD_Write_Data_Direct(INTC2,TP_INT); //Clear INTC2 TP Interrupt	
		}
		
		eLastSleepModeState = eSLEEP_MODE_DISABLED;
		
		delayMs(2);
		
		uiDisplayCurrentPage(); // Redraw the current page.
	}	
	else
	{	
		LCD_Read_Status(&tpTouchStatus);	
			
		LCD_Read_Data_Direct(INTC2,&tpInterrupt); // Check for TP_INT
		
		if( (tpTouchStatus & STSR_TP_EVENT_DETECTED) == STSR_TP_EVENT_DETECTED &&
			  (tpInterrupt & TP_INT) == TP_INT )
		{				
			LCD_Write_Data_Direct(INTC2,TP_INT); //Clear INTC2 TP Interrupt	

			tpTouchInProgress = TRUE;
						
			LCD_Get_Calibrated_X_Y();
		}
	}	
	
	return tpTouchInProgress;
}

void LCD_Get_Calibrated_X_Y(void)
{
	int X = 0;
	int Y = 0;
	int i = 0;
	int numberGoodSamples = 0;
	int numberBadSamples = 0;
		
	///////////////////
	// Take 16 Samples
	///////////////////	
  while(numberGoodSamples < 16)
  {
    LCD_Get_TP_X_Y_Value(&X,&Y);
		
    ////////////////////////
    // Toss out bad samples
    ////////////////////////
    if((0 != X) && (0 != Y))
    {
      /////
      // average the good ones
      /////
      x_samples[numberGoodSamples] = X;
			
      y_samples[numberGoodSamples] = Y;
			
      numberGoodSamples++;
    }
		else
		{
			break;
		}
  }
	
	X = 0;
	Y = 0;
	
	if( numberGoodSamples > 0 )
	{
		oldX = x_samples[i];
		
		oldY = y_samples[i];
		
		while(i < numberGoodSamples)
		{
			if( (abs(oldX - x_samples[i]) <= 40) && (abs(oldY - y_samples[i]) <= 40) )
			{
				X += x_samples[i];
		
				Y += y_samples[i];
			}
			else
			{
				numberBadSamples++;
			}
			
			i++;
		}
		
		raw.nX = X/(numberGoodSamples-numberBadSamples);
		
		raw.nY = Y/(numberGoodSamples-numberBadSamples);

		////////////////////////////////////
		// Send to UI Driver For Processing 
		////////////////////////////////////
		if(LCD_Calibrate_TP_Point(&calibrated, &raw ))
		{	
	#if(0)
			printf("LCD_Get_Calibrated_X_Y: USING CALIBRATION COEFFICIENTS FROM FLASH\n");
			printf("LCD_Get_Calibrated_X_Y: raw.nX[%d] raw.nY[%d]\n",raw.nX,raw.nY);					
			printf("LCD_Get_Calibrated_X_Y: calibrated.nX[%d] calibrated.nY[%d]\n",calibrated.nX,calibrated.nY);
	#endif
			
			uiDriverHandleTouchScreenPress(calibrated.nX,calibrated.nY);
		}
		else
		{
	#if(0)
			printf("LCD_Get_Calibrated_X_Y: NO CALIBRATION COEFFICIENTS AVAILABLE IN FLASH! USING RAW X & Y\n");
			printf("LCD_Get_Calibrated_X_Y: USE THE CALIBRATION OPTION TO OBTAIN COEFFICIENTS\n");
			printf("LCD_Get_Calibrated_X_Y: raw.nX[%d] raw.nY[%d]\n",raw.nX,raw.nY);					
			printf("LCD_Get_Calibrated_X_Y: calibrated.nX[%d] calibrated.nY[%d]\n",calibrated.nX,calibrated.nY);
#endif			
			uiDriverHandleTouchScreenPress(calibrated.nX,calibrated.nY);
		}
	}
	else
	{
#if(DEBUG_SPEW)
		printf("LCD_Get_Calibrated_X_Y: Zero Samples Taken\n");
#endif
	}
}

BOOL LCD_Calibration_TP_Event(int *nX, int *nY)
{
    BYTE tpIntStatus,tpTouchStatus;
		int X,Y,i;
		int numTries = 0;

		LCD_Read_Data_Direct(INTC2,&tpIntStatus); // Check for TP_INT
		
		if((tpIntStatus & TP_INT) == TP_INT) // Check for TP_INT
		{	
			LCD_Write_Data_Direct(INTC2,TP_INT); //Clear INTC2 TP Interrupt
				
			///////////////////
			// Take 36 Samples
			///////////////////
			i=0;
			
			while(i<36)
			{
				LCD_Get_TP_X_Y_Value(&X,&Y);
				
				////////////////////////
				// Toss out bad samples
				////////////////////////
				if((0 != X) && (0 != Y))
				{
					/////
					// average the good ones
					/////
					x_samples[i] = X;
					y_samples[i] = Y;
					i++;
				}
				
				if(numTries++ >= 40)
				{
					LCD_DISABLE_TP();
					return FALSE;
				}
			}
			
			LCD_DISABLE_TP();
			
			i = 10;
			
			while( i < 26 )
			{
				// Take 16 samples from the middle of the X&Y data
				X += x_samples[i];
				
				Y += y_samples[i];
				
				i++;
			}
			
			// Take the average	
			X = X/16;
			Y = Y/16;
			
			///////////////////////////////
			// Return for Calibration Mode
			///////////////////////////////
			*nX = X;
			*nY = Y;
			
			////////////////////////////////////////	
			// Wait until TP Released for Selection
			////////////////////////////////////////
			do
			{
				LCD_Read_Status(&tpTouchStatus);	
						
				LCD_Read_Data_Direct(INTC2,&tpIntStatus); // Check for TP_INT
				
				if((tpIntStatus & TP_INT) == TP_INT)
				{
					LCD_Write_Data_Direct(INTC2,TP_INT); //Clear INTC2 (TP) Interrupt	
				}	
				
			}while((tpTouchStatus & STSR_TP_EVENT_DETECTED) == STSR_TP_EVENT_DETECTED); // Wait while TP Event Detected
			
			LCD_DISABLE_TP();
			
			touchDetected = TRUE;	
		}
		else
		{
			touchDetected = FALSE;			
		}
		
		return touchDetected;
}

//=================================================
// Get the Latched X&Y Values from the Touch Panel. 
//=================================================
void LCD_Get_TP_X_Y_Value(int *X, int *Y)
{
	int i;
	
	Y1=0;
	X1=0;
	
	//================================
	//=================
	//Latch TP Y Value
	//=================
	LCD_Write_Data_Direct(TPCR1,LATCH_TP_Y_VALUE);
	
	//=======================
	// Let Y Value Stabilize
	//=======================
	for(i=0; i<5000; i++); 
	
	//==================
	// Latch TP X Value
	//==================
	LCD_Write_Data_Direct(TPCR1,LATCH_TP_X_VALUE);
	
	//=======================
	// Let X Value Stabilize
	//=======================
	for(i=0; i<5000; i++); 
	
	//====================
	//Put TP in Idle Mode
	//====================
	LCD_DISABLE_TP(); 
	
	//===================================================
	// Read Y Axis High and Low Bytes
	// D[9:2] from reg TPYH, D[1:0] from reg TPXYL[3:2]	
  //===================================================
	Y1 = LCD_ADC_Y() << 2 | ( (LCD_ADC_XY() & 0x0C) >> 2 ); 
  
	//==================================================
	// Read X Axis High and Low Bytes
	// D[9:2] from reg TPXH, D[1:0] from reg TPXYL[1:0]
	//==================================================
	X1 = LCD_ADC_X() << 2 | ( (LCD_ADC_XY() & 0x03) ); 
	
	//==================================================
	// Convert Y to work with the Height of the display
	//==================================================
	*Y =(lcdDisplayHeight-(((Y1-63))*10/33));
	
	//==============================
	// If Y is Negative set to zero
	//==============================
	if(*Y < 0)
	{
		*Y=0;
	}
	
	//==================================================
	// Convert X to work with the Width of the display
	//==================================================		
	*X=(lcdDisplayWidth-(((X1-50))*10/18));
	
	//==============================
	// If X is Negative set to zero
	//==============================
	if(*X < 0)
	{
		*X=0;
	}	
		
	LCD_ENABLE_TP();	
}

void LCD_Reset_TP_Event(void)
{
	int i;
	
	touchDetected = FALSE;
	
	LCD_ENABLE_TP();
	
	LCD_Write_Data_Direct(INTC2,TP_INT); //Clear INTC2 TP Interrupt	
	
	for(i=0; i<10000; i++);
}

void LCD_DISABLE_TP(void)
{
	LCD_Write_Data_Direct(TPCR1,TP_IDLE_MODE); // Put TP in Idle Mode
}

void LCD_ENABLE_TP(void)
{
	LCD_Write_Data_Direct(TPCR1,TP_WAIT_FOR_EVENT_MODE); // Put TP in Wait For TP Event Mode
}

void LCD_TP_Feedback(void)
{
	int i;
	
	pwmControl(ON);
	
	for(i=0; i<1000000; i++);
		
	pwmControl(OFF);
}

// Set LCD Backlight Brightness Level (0x00 to 0xFF)
void LCD_Set_Display_Brightness(BYTE brightness)
{
	LCD_Write_Data_Direct(P1DCR,brightness);
}

// Turns LCD Backlight Off/On. A press of the Touch Panel wakes the display up
void LCD_Enable_Sleep_Mode(BOOL sleepMode)
{
	if(sleepMode)
	{
		LCD_Write_Data_Direct(PWRR,ENABLE_SLEEP_MODE);  //Enable Sleep Mode/Display is Off
		delayMs(1);
	}
	else
	{
		LCD_Write_Data_Direct(PWRR,DISABLE_SLEEP_MODE); //Disable Sleep Mode/Display is On
	}				
}

BOOL LCD_Sleep_Mode_Enabled(void)
{
		BYTE temp;
		BOOL sleepModeEnabled = FALSE;
		LCD_Read_Status(&temp);
	
		if(temp & LCD_SLEEP_MODE_ENABLED)
		{
			sleepModeEnabled = TRUE;
			eLastSleepModeState = eSLEEP_MODE_ENABLED;
		}
		
		return sleepModeEnabled;
}

//////////////////////////////////////////////////////////////////////////////////
//// SUMMARY:  Calculates the difference between the touch screen and the
////	         actual screen co-ordinates, taking into account misalignment
////	         and any physical offset of the touch screen.
////	
////	NOTE:  	 This is based on the public domain touch screen calibration code
////					 written by Carlos E. Vidales (copyright (c) 2001).
////	
////	         For more information, see the following app notes:
////	
////	         - AN2173 - Touch Screen Control and Calibration
////	           Svyatoslav Paliy, Cypress Microsystems
////	         - Calibration in touch-screen systems
////	           Wendy Fang and Tony Chang,
////	           Analog Applications Journal, 3Q 2007 (Texas Instruments)
///////////////////////////////////////////////////////////////////////////////////
BOOL LCD_Set_Calibration_Matrix( TP_POINT *displayPtr, TP_POINT *screenPtr)
{
  BOOL  retValue = FALSE;

  tpMatrix.nDivider = ((screenPtr[0].nX - screenPtr[2].nX) * (screenPtr[1].nY - screenPtr[2].nY)) - 
											((screenPtr[1].nX - screenPtr[2].nX) * (screenPtr[0].nY - screenPtr[2].nY)) ;
	
  if( tpMatrix.nDivider != 0 )
  {
    tpMatrix.nA = ((displayPtr[0].nX - displayPtr[2].nX) * (screenPtr[1].nY - screenPtr[2].nY)) - 
                  ((displayPtr[1].nX - displayPtr[2].nX) * (screenPtr[0].nY - screenPtr[2].nY)) ;
  
    tpMatrix.nB = ((screenPtr[0].nX - screenPtr[2].nX) * (displayPtr[1].nX - displayPtr[2].nX)) - 
                  ((displayPtr[0].nX - displayPtr[2].nX) * (screenPtr[1].nX - screenPtr[2].nX)) ;
  
    tpMatrix.nC = (screenPtr[2].nX * displayPtr[1].nX - screenPtr[1].nX * displayPtr[2].nX) * screenPtr[0].nY +
                  (screenPtr[0].nX * displayPtr[2].nX - screenPtr[2].nX * displayPtr[0].nX) * screenPtr[1].nY +
                  (screenPtr[1].nX * displayPtr[0].nX - screenPtr[0].nX * displayPtr[1].nX) * screenPtr[2].nY ;
  
    tpMatrix.nD = ((displayPtr[0].nY - displayPtr[2].nY) * (screenPtr[1].nY - screenPtr[2].nY)) - 
                  ((displayPtr[1].nY - displayPtr[2].nY) * (screenPtr[0].nY - screenPtr[2].nY)) ;
  
    tpMatrix.nE = ((screenPtr[0].nX - screenPtr[2].nX) * (displayPtr[1].nY - displayPtr[2].nY)) - 
                  ((displayPtr[0].nY - displayPtr[2].nY) * (screenPtr[1].nX - screenPtr[2].nX)) ;
  
    tpMatrix.nF = (screenPtr[2].nX * displayPtr[1].nY - screenPtr[1].nX * displayPtr[2].nY) * screenPtr[0].nY +
                  (screenPtr[0].nX * displayPtr[2].nY - screenPtr[2].nX * displayPtr[0].nY) * screenPtr[1].nY +
                  (screenPtr[1].nX * displayPtr[0].nY - screenPtr[0].nX * displayPtr[1].nY) * screenPtr[2].nY ;
	
		retValue = TRUE;
	}
	else
	{
		displayPtr->nX = screenPtr->nX;
		displayPtr->nY = screenPtr->nY;
	}

  return( retValue ) ;
} 

///////////////////////////////////////////////////////////////////////////////////
////
//// SUMMARY:  Converts raw touch screen locations (screenPtr) into actual
////         	 pixel locations on the display (displayPtr) using the
////         	 supplied matrix.
////         
//// @param[out] displayPtr  Pointer to the TP_POINT object that will hold
////                         the compensated pixel location on the display
//// @param[in]  screenPtr   Pointer to the TP_POINT object that contains the
////                         raw touch screen co-ordinates (before the
////                         calibration calculations are made)
//// @param[in]  matrixPtr   Pointer to the calibration matrix coefficients
////                         used during the calibration process (calculated
////                         via the tsCalibrate() helper function)
////
//// @note  This is based on the public domain touch screen calibration code
////        written by Carlos E. Vidales (copyright (c) 2001).
////
//////////////////////////////////////////////////////////////////////////////////
BOOL LCD_Calibrate_TP_Point( TP_POINT *displayPtr, TP_POINT *screenPtr)
{
  BOOL  retValue = FALSE;
	
  //printf("LCD_Calibrate_TP_Point\n");
	
  if( tpMatrix.nDivider != 0 )
  {
    displayPtr->nX = ( ( tpMatrix.nA * screenPtr->nX ) + 
                       ( tpMatrix.nB * screenPtr->nY ) + 
                         tpMatrix.nC ) / tpMatrix.nDivider ;

    displayPtr->nY = ( ( tpMatrix.nD * screenPtr->nX ) + 
                       ( tpMatrix.nE * screenPtr->nY ) + 
                         tpMatrix.nF ) / tpMatrix.nDivider ;
		
		retValue = TRUE;
  }
	else
	{
		displayPtr->nX = screenPtr->nX;
		displayPtr->nY = screenPtr->nY;
	}
	
  return( retValue );
}

/////////////////////////////////////////////////////////////////////
//// SUMMARY:  Displays the calibration screen with an appropriately
////           placed test point and waits for a touch event
/////////////////////////////////////////////////////////////////////
TP_POINT LCD_Calibration_Screen(WORD X, WORD Y, WORD radius, BOOL tpCalibrate)
{
  BOOL valid = FALSE;
  TP_POINT point = { 0, 0 };

	if(tpCalibrate)
	{
		//////////////////////////////
		// Clear both display layers
		//////////////////////////////
		LCD_Clear_Display(TRUE,LAYER_ONE);
		LCD_Clear_Display(TRUE,LAYER_TWO);
		
		//////////////////////////////
		// Put Title Text on Display
		//////////////////////////////
		LCD_Text_Foreground_Color1(color_red);
		LCD_Write_String("LCD Calibration Screen",150,0,LAYER_ONE);
	}
	else
	{
		LCD_Text_Foreground_Color1(color_red);
		LCD_Write_String("LCD Calibration Verification Screen",120,0,LAYER_ONE);
	}	
	
	/////////////////////////////////////////////////
	// Use concentric circles (Red and Gray) for 
	// calibration touch points
	/////////////////////////////////////////////////
  LCD_Draw_Circle_Helper(X, Y, radius,color_red,FALSE);
  LCD_Draw_Circle_Helper(X, Y, radius + 2,color_gray,FALSE); 

	///////////////////////////////////
  // Wait for a valid touch event
  // Keep polling until the TP event 
	// flag is valid
	////////////////////////////////////
  if (!valid)
  {
    LCD_Wait_For_Touch_Event(&point);

		LCD_DISABLE_TP();

		delayMs(250);
		
		////////////////////////////////////////////////////
		// Generate a pleasant tone when a point is pressed 
		////////////////////////////////////////////////////
		LCD_TP_Feedback();
#if(DEBUG_SPEW)		
		printf("X[%d] point.nX[%d]  Y[%d] point.nY[%d]\n",X,point.nX,Y,point.nY); 
#endif
		
    if (point.nX && point.nY) 
    {
      valid = TRUE;
    }
  }
  
  return point;
}

//=========================================================================
//  SUMMARY:  Starts the screen calibration process.  Each corner will be
//            tested, meaning that each boundary (top, left, right and 
//            bottom) will be tested twice and the readings averaged.
//=========================================================================
void LCD_TP_Calibrate(BOOL tpCalibrate)
{
  TP_POINT data;
	
	watchdogFeed();

  //================== Welcome Screen ====================
	if(tpCalibrate)
	{
		printf("STARTING THE CALIBRATION PROCESS\n");
	}
	else
	{
		printf("STARTING THE VERIFICATION PROCESS\n");
	}
	
  data = LCD_Calibration_Screen(480 / 2, 272 / 2, 5, tpCalibrate);
	
	if(!tpCalibrate)
	{
		watchdogFeed();
		LCD_Calibration_Verification(data);		
	}
	
	watchdogFeed();

	//============== First Dot ================
	// 10% over and 10% down	
	//=========================================
	data = LCD_Calibration_Screen(480 / 10, 272 / 10, 5, tpCalibrate);
	
	watchdogFeed();
	
	if(tpCalibrate)
	{
		tpLCDPoints[0].nX = 480 / 10;
		tpLCDPoints[0].nY = 272 / 10;
		tpTPPoints[0].nX = data.nX;
		tpTPPoints[0].nY = data.nY;
#if(DEBUG_SPEW)
		printf("POINT 1 - LCD");
		printf(" LCD: X[%d], Y[%d], TP: X[%d], Y[%d]\n",tpLCDPoints[0].nX,tpLCDPoints[0].nY,tpTPPoints[0].nX,tpTPPoints[0].nY);
#endif
	}
	else
	{
		LCD_Calibration_Verification(data);				
	}
	
	//============== Second Dot ===============
	// 50% over and 90% down
	//=========================================
	data = LCD_Calibration_Screen(480 / 2, 272 - (272 / 10), 5, tpCalibrate);
	
	watchdogFeed();
	
	if(tpCalibrate)
	{
		tpLCDPoints[1].nX = 480 / 2;
		tpLCDPoints[1].nY = 272 - (272 / 10);
		tpTPPoints[1].nX = data.nX;
		tpTPPoints[1].nY = data.nY;
#if(DEBUG_SPEW)
		printf("POINT 2 - LCD");
		printf(" LCD: X[%d], Y[%d], TP: X[%d], Y[%d]\n",tpLCDPoints[1].nX,tpLCDPoints[1].nY,tpTPPoints[1].nX,tpTPPoints[1].nY);
#endif
	}
	else
	{
		LCD_Calibration_Verification(data);				
	}
	
	//=============== Third Dot =================
	// 90% over and 50% down
	//===========================================
	data = LCD_Calibration_Screen(480 - (480 / 10), 272 / 2, 5, tpCalibrate);
	
	watchdogFeed();
	
	if(tpCalibrate)
	{
		tpLCDPoints[2].nX = 480 - (480 / 10);
		tpLCDPoints[2].nY = 272 / 2;
		tpTPPoints[2].nX = data.nX;
		tpTPPoints[2].nY = data.nY;
#if(DEBUG_SPEW)
		printf("Point 3 - LCD");
		printf(" LCD: X[%d], Y[%d], TP: X[%d], Y[%d]\n",tpLCDPoints[2].nX,tpLCDPoints[2].nY,tpTPPoints[2].nX,tpTPPoints[2].nY);
#endif
		
		//////////////////////////////////////////////////////////////////////////
		// After the last point, perform matrix calculations for the 
		// calibration coefficients and write them to Stored Config in SPI Flash 
		//////////////////////////////////////////////////////////////////////////
		if(LCD_Set_Calibration_Matrix(&tpLCDPoints[0], &tpTPPoints[0]))
		{
			tpCalibrationSucceeded = TRUE;
		}
  }
	else
	{
		if(LCD_Calibration_Verification(data) && tpCalibrationSucceeded)
		{
			tpVerificationSucceeded = TRUE;
			
			////////////////////////////////////////////////////////////
			// Write Calibration Coefficients to Stored Configuration
			// in SPI Flash.
			////////////////////////////////////////////////////////////
			
			watchdogFeed();
			
			storedConfigSetTPCalCoefficients(&tpMatrix);
		}
		else
		{
			tpVerificationSucceeded = FALSE;
		}
	}
	watchdogFeed();
	
  /* Clear the screen */
  LCD_Clear_Display(TRUE,LAYER_ONE);
  LCD_Clear_Display(TRUE,LAYER_TWO);
	
	/////////////////////////////////////
	// Take us back to the setup screen
	// after verification
	/////////////////////////////////////
	
	if(!tpCalibrate) 
	{
		if(tpVerificationSucceeded)
		{
			watchdogFeed();
			LCD_Text_Foreground_Color1(color_red);
			LCD_Write_String("CALIBRATION SUCCEEDED! TOUCH THE SCREEN TO EXIT",75,0,LAYER_ONE);
			LCD_Wait_For_Touch_Event(&data);
			LCD_TP_Feedback();
			watchdogFeed();
			uiDisplayCurrentPage();
		}
		else
		{
			watchdogFeed();
			LCD_Text_Foreground_Color1(color_red);
			LCD_Write_String("CALIBRATION FAILED! TOUCH THE SCREEN TO EXIT",75,0,LAYER_ONE);
			LCD_Wait_For_Touch_Event(&data);
			LCD_TP_Feedback();
			watchdogFeed();
			uiDisplayCurrentPage();
		}
	}
}

//==============================
// Verify the LCD calibration
//==============================
BOOL LCD_Calibration_Verification(TP_POINT raw) 
{
  TP_POINT calibrated;
	BOOL tpXVerificationSucceeded = FALSE;
	BOOL tpYVerificationSucceeded = FALSE;
	BOOL tpVerificationSucceeded  = FALSE;
	
	//////////////////////////////////////////////////////////////////
  // Calculate the Real X/Y position using the Stored Calibration
	// Coefficients
	//////////////////////////////////////////////////////////////////
  LCD_Calibrate_TP_Point(&calibrated, &raw );
	
	if(raw.nX > calibrated.nX)
	{
		if( (raw.nX - calibrated.nX) < 40 )
		{
			tpXVerificationSucceeded = TRUE;
		}
	}
	else
	{
		if( (calibrated.nX - raw.nX) < 40 )
		{
			tpXVerificationSucceeded = TRUE;
		}		
	}
	
	if(raw.nY > calibrated.nY)
	{
		if( (raw.nY - calibrated.nY) < 40 )
		{
			tpYVerificationSucceeded = TRUE;
		}
	}
	else
	{
		if( (calibrated.nY - raw.nY) < 40 )
		{
			tpYVerificationSucceeded = TRUE;
		}		
	}
	
	if( tpXVerificationSucceeded && tpYVerificationSucceeded )
	{
		printf("Verification Succeeded!\n");
	
		////////////////////////////////////////////////
		// Draw a single pixel at the calibrated point
		// for comparison
		////////////////////////////////////////////////
		LCD_Draw_Circle_Helper(calibrated.nX, calibrated.nY, 3, color_black,TRUE);
		
		tpVerificationSucceeded = TRUE;
	}
	else
	{
		printf("Verification Failed!\n");
	
		//=============================================
		// Draw a single pixel at the calibrated point
		// for comparison
		//=============================================
		LCD_Draw_Circle_Helper(calibrated.nX, calibrated.nY, 3, color_black,TRUE);
	}
#if(DEBUG_SPEW)	
	printf("raw.nX[%d] calibrated.nX[%d]   raw.nY[%d] calibrated.nY[%d]\n", raw.nX, calibrated.nX, raw.nY, calibrated.nY);
#endif
	
	return tpVerificationSucceeded;
}

//========================
// Wait for a touch event
//========================
void LCD_Wait_For_Touch_Event(TP_POINT *point)
{
  int x, y;
	
	LCD_Reset_TP_Event();

  /* Wait around for a new touch event (INT pin goes low) */
  while (!LCD_Calibration_TP_Event(&x,&y))
	{
			watchdogFeed();		
	}
	
  point->nX = x;
  point->nY = y;
}

//=================================
// Write Font Bitmap Data to CGRAM
//=================================
void LCD_Write_Font_Bitmap_To_CGRAM(WORD *cgramData, BYTE cgramSpace)
{
	int i;
		
	// Select graphics mode
	LCD_Write_Data_Direct(MWCR0,LCD_SELECT_GRAPHICS_MODE);
	
	LCD_Write_Data_Direct(FWT,LCD_NORMAL_FONT_TYPE);// Font Write Type Normal
	
	LCD_Write_Data_Direct(FNCR0,LCD_EXTERNAL_CGROM_FONT);// Select External CGROM Font
	
	LCD_Write_Data_Direct(MWCR1,LCD_WRITE_CGRAM_DESTINATION);// Select CGRAM as the Write Destination
	
	LCD_Text_Foreground_Color1(color_red);//Set the foreground color
	
	//LCD_Text_Background_Color1(color_white);//Set the background color
	
	// CGRAM Space Select
	LCD_Write_Data_Direct(CGSR,cgramSpace);
	
	LCD_Write_Command_Byte(DISPLAY_MEMORY_WRITE);// Select Memory
	
	for (i=0; i<16; i++) 
	{
		LCD_Write_Data_Word(cgramData[i]);// Write data to CGRAM
	}
}

//=============================================
// Write CGRAM Font Bitmap Data to the Display
//=============================================
void LCD_Write_CGRAM_Font_Bitmap_To_Display(BYTE cgramSpace, WORD xPos, WORD yPos)
{
	LCD_Write_Data_Direct(MWCR0,LCD_SELECT_TEXT_MODE); // Select text mode
	
	LCD_Write_Data_Direct(FNCR0,LCD_CGRAM_CGROM_TEXT_MODE_FONT_SELECT); // CGRAM/CGROM Font Selection Bit in Text Mode
	
	LCD_Write_Data_Direct(FWT,LCD_NORMAL_FONT_TYPE);// Font Write Type Normal
	
	LCD_Write_Data_Direct(MWCR1,LCD_SELECT_LAYER_TWO); // Write to LCD Layer One
	
  LCD_Font_Write_Position(xPos, yPos); //Text written to the position

  LCD_Write_Command_Byte(DISPLAY_MEMORY_WRITE);//Select Display Memory
	
  LCD_Write_Data_Byte(cgramSpace); // Write CGRAM Data to the Display	
}

void LCD_Write_Pixel_To_Display(WORD xPos, WORD yPos, WORD color) 
{

   LCD_Write_DMemory_Position(xPos,yPos);//Memory write position
   LCD_Write_Command_Byte(0x02);
   LCD_Write_Data_Word(color);  //start data write 
}

//======================================================================
// Draw a color bitmap at the specified x, y position from the
// provided bitmap buffer using onColor for bits that are on and offColor 
// for bits that are off.
//======================================================================
void LCD_Draw_Bitmap(BYTE *bitmap, WORD xPos, WORD yPos, WORD width, WORD height, WORD onColor, WORD offColor) 
{

  WORD i, j;
	UINT bitPosition;
	UINT xIndx,yIndx;
	WORD rowStride = (width + 7) / 8;
	
	for (yIndx = 0; yIndx < height*rowStride; yIndx+=rowStride) 
	{
		for (j =0; j<rowStride; j++) 
		{	
			//Memory write starting x position	
			LCD_Write_DMemory_Position(xPos,yIndx +j + yPos); 
			
			// Memory write command
			LCD_Write_Command_Byte(MRWC);
	
			for (xIndx = 0; xIndx < rowStride; xIndx++)                             
			{	
				// Set starting bit position
				bitPosition = 8;
				
				while(bitPosition--) 
				{
					// Test to see if bits in the byte are off or on 
					if( LCD_Test_Bit(&bitmap[xIndx+((yIndx/rowStride)*rowStride)],bitPosition) )
					{								
						for (i=0; i<rowStride; i++) 
						{
							// Color for bits that are on
							LCD_Write_Data_Word(color_red); 
						}
					}
					else
					{
						for (i=0; i<rowStride; i++) 
						{
							// Color for bits that are off
							LCD_Write_Data_Word(color_white);
						}
					}
				}
			}
		}
	}
}

BOOL LCD_Test_Bit(BYTE *inputByte, UINT bitPosition)
{
	BOOL bitSet = FALSE;
	
	if(*inputByte & (1 << bitPosition))
	{
		bitSet = TRUE;
	}
	
  return bitSet;
}  
//=====================================================================================================
// LCD_Display_Bitmap uses the Block Transfer Engine (BTE) and the Two Layer Mode of the LCD to
// create a Double Buffer method. This method uses the Display RAM of Layer 2 to buffer the Bitmap 
// Data. The LCD BTE is then used to move the data from Layer 2 Display RAM to Layer 1 Display giving 
// the Illusion of Faster Display Updates.
//
// PLEASE REFER TO SECTION 7-6 OF THE RAIO RA8875 TFT LCD CONTROLLER SPECIFICATION FOR AN IN DEPTH 
// DISCUSSION LCD BLOCK TRANSFER ENGINE.
//=====================================================================================================
void LCD_Display_Bitmap(ID graphicID, int nLeft, int nTop)
{
		WORD width;
	  WORD height;
		int i;
		int nSize = nReadInit(graphicID,&width,&height);
	
		//=======================================================================================
		// Write the Bitmap Data to Layer 2 (Source Layer) Using the Block Transfer Engine (BTE)
		//=======================================================================================
		LCD_Write_Data_Direct(MWCR1,LCD_SELECT_LAYER_TWO); // Write Display Data to Layer Two (This is the Source Layer)
		
		//=========================================================================================
		// Set the BTE Source and Destination. In this instance Source and Destination are Layer 2
		//=========================================================================================
		LCD_Set_BTE_Source_Destination(0,nLeft,0,nTop,LCD_SELECT_SOURCE_LAYER_TWO,LCD_SELECT_DESTINATION_LAYER_TWO);
	
		//=============================================
		// Set the BTE Source Layer 2 Width and Height
		//=============================================
		LCD_Set_BTE_Size(width,height); 
		
		//===========================================================================================
		// This will cause the Bitmap Data to be written from the CPU to the Display RAM of Layer 2 
		// of the LCD using Write BTE with Raster Operation (ROP) command
		//===========================================================================================
    LCD_Write_Data_Direct(BECR1,LCD_WRITE_BTE_WITH_ROP); 
		
		//=================================================================
	  // Enable BTE Block Memory Access Mode for Source and Destination
		//=================================================================
    LCD_Write_Data_Direct(BECR0,LCD_ENABLE_BTE_BLOCK_MEMORY_ACCESS);
		
		//==============================================================
		// This will Write the Bitmap Data to the Display RAM using BTE
		//==============================================================
		LCD_Write_Command_Byte(DISPLAY_MEMORY_WRITE);
		
		//=======================================================
		// Write the Bitmap Pixels to the Display RAM of Layer 2
		//=======================================================
		for(i=0; i < nSize; i++)
		{
			LCD_Write_Data_Word(nReadNextByte());
		}
		
		//======================================================================================
		// Now Move the Bitmap Data From Layer 2 (Source Layer) to Layer 1 (Destination Layer)
		//======================================================================================	
		LCD_Set_BTE_Source_Destination(nLeft,nLeft,nTop,nTop,LCD_SELECT_SOURCE_LAYER_TWO,LCD_SELECT_DESTINATION_LAYER_ONE);
		
		//====================================================================
		// Set the BTE Source Layer 2 and Destinatin Layer 1 Width and Height
		//====================================================================		
    LCD_Set_BTE_Size(width,height);
		
		//============================================================================
		// Move Data from Source Layer 2 to Destination Layer 1 in Positive Direction
		//============================================================================
    LCD_Write_Data_Direct(BECR1,LCD_MOVE_BTE_POSITIVE_DIRECTION_WITH_ROP);
	  
		//================================================================================
	  // Enable BTE Block Memory Access Mode for Source Layer 2 and Destination Layer 1
		//================================================================================
    LCD_Write_Data_Direct(BECR0,LCD_ENABLE_BTE_BLOCK_MEMORY_ACCESS);
}
