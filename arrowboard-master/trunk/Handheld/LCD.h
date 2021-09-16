/*****************************************************************************
 *   LCD.h:  LCD Header File - Functions for controlling the LCD
 *
 *   History
 *   2014.07.24  ver 1.00    Prelimnary version
 *
*****************************************************************************/
#ifndef __LCD_H__
#define __LCD_H__

#define color_brown   0x40c0
#define color_black   0x0000
#define color_white   0xffff
#define color_red     0xf800
#define color_green   0x07e0
#define color_blue    0x001f
#define color_yellow  color_red|color_green
#define color_cyan    color_green|color_blue
#define color_purple  color_red|color_blue
#define color_amber   0xFF66
#define color_background 0xE73C		
#define color_gray 			 0x8410 // 50% gray

// Interrupt Flags
#define MCU_INT     0x01
#define BTE_INT     0x02
#define BTE_MCU_INT 0x03
#define TP_INT			0x04
#define UNKNOWN_INT 0x07


// Display Layer Selection
#define LAYER_ONE 0x00
#define LAYER_TWO 0x01

#define TPBUFSIZE   16       // Depth of the averaging buffers for x and y data

// Display Window
#define CLEAR_ENTIRE_WINDOW 	0x80
#define CLEAR_ACTIVE_WINDOW		0xC0

/////
// common brightness settings
#define BRIGHTNESS_NIGHT_MODE 0x44
#define BRIGHTNESS_DAY_MODE 0xFF

///////////////////////////////
// RA8875 REGISTER DEFINTIIONS
//////////////////////////////
#define PWRR 		0x01 // Power and Display Control Register (Page 14 of RA8875 Users Manual)
#define MRWC 		0x02 // Memory Read/Write Command (Page 15 of RA8875 Users Manual)
#define PCSR 		0x04 // Pixel Clock Setting Register (Page 15 of RA8875 Users Manual)
#define SROC 		0x05 // Serial Flash/ROM Configuration Register (Page 15 of RA8875 Users Manual)
#define SFCLR 	0x06 // Serial Flash/ROM CLK Setting Register()
#define SYSR 		0x10 // System Configuration Register ()
#define GPI 		0x12 // GPI
#define GPO 		0x13 // GPO
#define HDWR 		0x14 // LCD Horizontal Display Width Register ()
#define HNDFTR 	0x15 // Horizontal Non-Display Period Fine Tuning Option Register ()
#define HNDR 		0x16 // LCD Horizontal Non-Display Period Register ()
#define HSTR 		0x17 // HSYNC Start Position Register ()
#define HPWR 		0x18 // HSYNC Pulse Width Register ()
#define VDHR0 	0x19 // LCD Vertical Display Height Register ()
#define VDHR1 	0x1A // LCD Vertical Display Height Register0 ()
#define VNDR0 	0x1B // LCD Vertical Non-Display Period Register ()
#define VNDR1 	0x1C // LCD Vertical Non-Display Period Register ()
#define VSTR0 	0x1D // VSYNC Start Position Register ()
#define VSTR1 	0x1E // VSYNC Start Position Register ()
#define VPWR 		0x1F // VSYNC Pulse Width Register ()
#define DPCR 		0x20 // Display Configuration Register ()
#define FNCR0 	0x21 // Font Control Register0 ()
#define FNCR1 	0x22 // Font Control Register1 ()
#define CGSR 		0x23 // CGRAM Select Register ()
#define HOFS0 	0x24 // Horizontal Scroll Offset Register 0 ()
#define HOFS1 	0x25 // Horizontal Scroll Offset Register 1 ()
#define VOFS0 	0x26 // Vertical Scroll Offset Register 0 ()
#define VOFS1 	0x27 // Vertical Scroll Offset Register 1 ()
#define FLDR 		0x29 // Font Line Distance Setting Register ()
#define F_CURXL 0x2A // Font Write Cursor Horizontal Position Register 0 ()
#define F_CURXH 0x2B // Font Write Cursor Horizontal Position Register 1 ()
#define F_CURYL 0x2C // Font Write Cursor Vertical Position Register 0 ()
#define F_CURYH 0x2D // Font Write Cursor Vertical Position Register 1 ()
#define FWT     0x2E // Font Write Type Setting Register
#define SFR     0x2F // Serial Font ROM Setting
#define HSAW0   0x30 // Horizontal Start Point 0 of Active Window ()
#define HSAW1 	0x31 // Horizontal Start Point 1 of Active Window ()
#define VSAW0 	0x32 // Vertical Start Point 0 of Active Window ()
#define VSAW1 	0x33 // Vertical Start Point 1 of Active Window ()
#define HEAW0 	0x34 // Horizontal End Point 0 of Active Window ()
#define HEAW1 	0x35 // Horizontal End Point 1 of Active Window ()
#define VEAW0 	0x36 // Vertical End Point of Active Window 0 ()
#define VEAW1 	0x37 // Vertical End Point of Active Window 1 ()
#define HSSW0 	0x38 // Horizontal Start Point 0 of Scroll Window ()
#define HSSW1 	0x39 // Horizontal Start Point 1 of Scroll Window ()
#define VSSW0 	0x3A // Vertical Start Point 0 of Scroll Window ()
#define VSSW1 	0x3B // Vertical Start Point 1 of Scroll Window ()
#define HESW0 	0x3C //Horizontal End Point 0 of Scroll Window ()
#define HESW1 	0x3D //Horizontal End Point 1 of Scroll Window ()
#define VESW0 	0x3E // Vertical End Point 0 of Scroll Window ()
#define VESW1 	0x3F // Vertical End Point 1 of Scroll Window ()
#define MWCR0 	0x40 // Memory Write Control Register 0 ()
#define MWCR1   0x41 // Memory Write Control Register1 ()
#define BTCR    0x44 // Blink Time Control Register ()
#define MRCD    0x45 // Memory Read Cursor Direction ()
#define CURH0   0x46 // Memory Write Cursor Horizontal Position Register 0 ()
#define CURH1   0x47 // Memory Write Cursor Horizontal Position Register 1 ()
#define CURV0   0x48 // Memory Write Cursor Vertical Position Register 0 ()
#define CURV1   0x49 // Memory Write Cursor Vertical Position Register 1 ()
#define RCURH0  0x4A // Memory Read Cursor Horizontal Position Register 0 ()
#define RCURH01 0x4B // Memory Read Cursor Horizontal Position Register 1 ()
#define RCURV0  0x4C // Memory Read Cursor Vertical Position Register 0 ()
#define RCURV1  0x4D // Memory Read Cursor Vertical Position Register 1 ()
#define CURHS   0x4E // Font Write Cursor and Memory Write Cursor Horizontal Size Register ()
#define CURVS   0x4F // Font Write Cursor Vertical Size Register ()
#define BECR0   0x50 // BTE Function Control Register 0 ()
#define BECR1   0x51 // BTE Function Control Register1 ()
#define LTPR0   0x52 // Layer Transparency Register0 ()
#define LTPR1   0x53 // Layer Transparency Register1 ()
#define HSBE0   0x54 // Horizontal Source Point 0 of BTE ()
#define HSBE1   0x55 // Horizontal Source Point 1 of BTE ()
#define VSBE0   0x56 // Vertical Source Point 0 of BTE ()
#define VSBE1   0x57 // Vertical Source Point 1 of BTE ()
#define HDBE0   0x58 // Horizontal Destination Point 0 of BTE ()
#define HDBE1   0x59 // Horizontal Destination Point 1 of BTE ()
#define VDBE0   0x5A // Vertical Destination Point 0 of BTE ()
#define VDBE1   0x5B // Vertical Destination Point 1 of BTE ()
#define BEWR0   0x5C // BTE Width Register 0 ()
#define BEWR1   0x5D // BTE Width Register 1 ()
#define BEHR0   0x5E // BTE Height Register 0 ()
#define BEHR1   0x5F // BTE Height Register 1 ()
#define BGCR0   0x60 // Background Color Register 0 ()
#define BGCR1   0x61 // Background Color Register 1 ()
#define BGCR2   0x62 // Background Color Register 2 ()
#define FGCR0   0x63 // Foreground Color Register 0 ()
#define FGCR1   0x64 // Foreground Color Register 1 ()
#define FGCR2   0x65 // Foreground Color Register 2 ()
#define PTNO    0x66 // Pattern Set No for BTE ()
#define BGTR0   0x67 // Background Color Register for Transparent 0 ()
#define BGTR1   0x68 // Background Color Register for Transparent 1 ()
#define BGTR2   0x69 // Background Color Register for Transparent 2 ()
#define TPCR0   0x70 // Touch Panel Control Register 0 ()
#define TPCR1   0x71 // Touch Panel Control Register 1 ()
#define TPXH    0x72 // Touch Panel X High Byte Data Register ()
#define TPYH    0x73 // Touch Panel Y High Byte Data Register ()
#define TPXYL   0x74 // Touch Panel X/Y Low Byte Data Register ()
#define GCHP0   0x80 // Graphic Cursor Horizontal Position Register 0 ()
#define GCHP1   0x81 // Graphic Cursor Horizontal Position Register 1 ()
#define GCVP0   0x82 // Graphic Cursor Vertical Position Register 0 ()
#define GCVP1   0x83 // Graphic Cursor Vertical Position Register 1 ()
#define GCC0    0x84 // Graphic Cursor Color 0 ()
#define GCC1    0x85 // Graphic Cursor Color 1 ()
#define PLLC1   0x88 // PLL Control Register 1 ()
#define PLLC2   0x89 // PLL Control Register 2 ()
#define P1CR    0x8A // PWM1 Control Register ()
#define P1DCR   0x8B // PWM1 Duty Cycle Register ()
#define P2CR    0x8C // PWM2 Control Register ()
#define P2DCR   0x8D // PWM2 Control Register ()
#define MCLR    0x8E // Memory Clear Control Register ()
#define DCR			0x90 // Drawing Control Register (Line/Circle/Square)
#define DLHSR0	0x91 // Draw Horizontal Start Address Register0(Line/Square) 
#define DLHSR1	0x92 // Draw Horizontal Start Address Register1(Line/Square)
#define DLVSR0	0x93 // Draw Vertical Start Address Register0(Line/Square)
#define DLVSR1	0x94 // Draw Vertical Start Address Register1(Line/Square)
#define DLHER0	0x95 // Draw Horizontal End Address Register0(Line/Square) 
#define DLHER1	0x96 // Draw Horizontal End Address Register1(Line/Square)
#define DLVER0	0x97 // Draw Vertical End Address Register0(Line/Square)
#define DLVER1	0x98 // Draw Vertical End Address Register1(Line/Square)
#define INTC1   0xF0 // Interrupt Control Register1 ()
#define INTC2   0xF1 // Interrupt Control Register2 ()

#define MEM_CLEAR_BUSY          0x80
#define MEM_READ_WRITE_BUSY     0x80
#define BTE_BUSY                0x40
#define CIRCLE_DRAW_BUSY				0x40
#define LINE_DRAW_BUSY				  0x80
#define CLEAR_ENTIRE_WINDOW     0x80
#define CLEAR_ACTIVE_WINDOW     0xC0
#define LCD_WAIT_SIGNAL         0x02
#define DISPLAY_MEMORY_WRITE    0x02
#define MCU_INT                 0x01
#define BTE_INT                 0x02
#define TP_INT                  0x04
#define STSR_TP_EVENT_DETECTED  0x20
#define LATCH_TP_X_VALUE        0x46
#define LATCH_TP_Y_VALUE        0x47
#define TP_IDLE_MODE            0x44
#define TP_WAIT_FOR_EVENT_MODE  0x45
#define ENABLE_SLEEP_MODE       0x82
#define DISABLE_SLEEP_MODE      0x80
#define TWO_LAYER_DISPLAY_MODE  0x80
#define AUTO_INCREMENT          0x00
#define LTPR0_LAYER_TWO_VISIBLE 0x01
#define LTPR1_LAYER_TWO_VISIBLE 0x08
#define LCD_WRITE_BTE_WITH_ROP  0xC0
#define LCD_DISABLE_LAYER_ONE_DISPLAY 0x08
#define LCD_DISABLE_LAYER_TWO_DISPLAY 0x80
#define LCD_MOVE_BTE_POSITIVE_DIRECTION_WITH_ROP 0xC2
#define LCD_ENABLE_BTE_BLOCK_MEMORY_ACCESS 0x80
#define LCD_SLEEP_MODE_ENABLED  0x10
#define LCD_SELECT_GRAPHICS_MODE 0x60
#define LCD_SELECT_TEXT_MODE     0x80
#define LCD_NORMAL_FONT_TYPE		 0x00
#define LCD_EXTERNAL_CGROM_FONT  0x20
#define LCD_WRITE_CGRAM_DESTINATION 0x04
#define LCD_EXTERNAL_CGROM_SELECT   0x20
#define LCD_CGRAM_CGROM_TEXT_MODE_FONT_SELECT 0xA0
#define LCD_SELECT_LAYER_ONE                  0x00
#define LCD_SELECT_LAYER_TWO                  0x01
#define LCD_TOUCH_EVENT_DETECTED							0x00
#define LCD_DISPLAY_OFF												0x00

typedef WORD ID;

typedef enum eDestinationLayerSelect
{
LCD_SELECT_DESTINATION_LAYER_ONE = 0x00,
LCD_SELECT_DESTINATION_LAYER_TWO = 0x01	
}eDESTINATIONLAYERSELECT;

typedef enum eSourceLayerSelect
{
LCD_SELECT_SOURCE_LAYER_ONE      = 0x00,
LCD_SELECT_SOURCE_LAYER_TWO      = 0x01,
}eSOURCELAYERSELECT;

// LCD Functions
void LCD_Set_CS(void);
void LCD_Set_Direction(BOOL input);
void LCD_Write_Command_Byte(BYTE command);
void LCD_Write_Command_Word(WORD command);
void LCD_Write_Data_Byte(BYTE data);
void LCD_Write_Data_Word(WORD data);		
void LCD_Write_Data_Direct(BYTE command, BYTE data);
void LCD_Read_Data_Byte(BYTE* data);	
void LCD_Read_Data_Word(WORD* data);
void LCD_Read_Data_Direct(BYTE cmd, BYTE* data);
void LCD_Read_Status(BYTE* status);	
void LCD_PLL_Init(void);
void LCD_Init(void);
void LCD_Text_Background_Color1(WORD b_color);
void LCD_Text_Foreground_Color1(WORD b_color);
void LCD_Text_Foreground_Color(BYTE setRed,BYTE setGreen,BYTE setBlue);
void LCD_Text_Background_Color(BYTE setRed, BYTE setGreen, BYTE setBlue);
void LCD_Clear_Display(BOOL clearEntireWindow, BYTE layer);
void LCD_Chk_Busy(void);
void LCD_Chk_Mem_Clr(void);
void LCD_Chk_Wait(void);
void LCD_Chk_BTE_Busy(void);
BOOL LCD_Calibration_TP_Event(int *nX, int *nY);
void LCD_Set_BTE_Size(WORD width,WORD height);
void LCD_Set_BTE_Source_Destination(WORD SX,WORD DX ,WORD SY ,WORD DY, eSOURCELAYERSELECT sourceLayer, eDESTINATIONLAYERSELECT destinationLayer);
void LCD_Font_Write_Position(WORD X,WORD Y);
void LCD_Read_Font_Position(WORD* X,WORD* Y);
void LCD_Write_DMemory_Position(WORD X,WORD Y);
void LCD_Read_DMemory_Position(WORD* X,WORD* Y);
void LCD_Write_String(BYTE *str, BYTE x, BYTE y, BYTE layer);
void LCD_Set_Active_Window(WORD XL,WORD XR ,WORD YT ,WORD YB);
BYTE LCD_Chk_INT_Flags(int* X, int*Y);
BYTE LCD_ADC_X(void);
BYTE LCD_ADC_Y(void);
BYTE LCD_ADC_XY(void);
void LCD_INT_Handler (void) __irq;
void LCD_DISABLE_TP(void);
void LCD_ENABLE_TP(void);
void LCD_TP_Feedback(void);
void LCD_Display_Bitmap(ID graphicID, int nLeft, int nTop);
void LCD_Get_TP_X_Y_Value(int *X, int *Y);
int LCD_Get_TP_Event_Count(void);
void LCD_Reset_TP_Event(void);
void LCD_Enable_Sleep_Mode(BOOL sleepMode);
BOOL LCD_Sleep_Mode_Enabled(void);
void LCD_Set_Display_Brightness(BYTE brightness);
void LCD_Source_Layer1(void);
void LCD_Source_Layer2(void);
void LCD_Destination_Layer1(void);
void LCD_Destination_Layer2(void);
void LCD_Layer1_Visible(void);
void LCD_Layer2_Visible(void);
void LCD_Write_To_Bank1and2(void);
void LCD_Write_To_Bank1(void);
void LCD_TP_Calibrate(BOOL tpCalibrate);
TP_POINT LCD_Calibration_Screen(WORD X, WORD Y, WORD radius, BOOL tpCalibrate);
BOOL LCD_Calibrate_TP_Point( TP_POINT *displayPtr, TP_POINT *screenPtr );
BOOL LCD_Set_Calibration_Matrix( TP_POINT *displayPtr, TP_POINT *screenPtr);
void LCD_Draw_Circle(WORD X,WORD Y,WORD R);
void LCD_Draw_Circle_Helper(WORD x, WORD y, WORD r, WORD color, BOOL filled);
void LCD_Draw_Rectangle(WORD x, WORD y, WORD w, WORD h, WORD color);
void LCD_Draw_Rectangle_Helper(WORD x, WORD y, WORD w, WORD h, WORD color, BOOL filled);
void LCD_Chk_Circle_Draw_Busy(void);
void LCD_Wait_For_Touch_Event(TP_POINT *point);
BOOL LCD_Calibration_Verification(TP_POINT raw); 
BOOL LCD_Touch_In_Progress(void);
BOOL LCD_Touch_Released(void);
void LCD_Get_Calibrated_X_Y(void);
void LCD_Write_Font_Bitmap_To_CGRAM(WORD *cgramData, BYTE cgramSpace);
void LCD_Write_CGRAM_Font_Bitmap_To_Display(BYTE cgramSpace, WORD xPos, WORD yPos);
BOOL LCD_Test_Bit(BYTE *inputByte, UINT bitPosition );
void LCD_Draw_Bitmap(BYTE *bitmap, WORD xPos, WORD yPos, WORD width, WORD height, WORD onColor, WORD offColor); 
BOOL LCD_Touch_Panel_Read(int *x, int *y);

#endif  //#ifndef __LCD_H__
