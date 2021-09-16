#define READ_DATA_1			((BYTE*)0x7FE00000) // Use 16 kB Eternet RAM
#define READ_DATA_1_LENGTH			16384
#define READ_DATA_2			((BYTE*)0x7FD00000) // Use 8 kB USB RAM
#define READ_DATA_2_LENGTH			8192

void spiFlashInit(void);
void spiFlashDoWork(void);
STORED_UI* spiFlashOpenUI(ID uiID);
void spiFlashFillColorMap(ID mapID);
void* spiFlashGetData(ID uID);
void spiFlashDisplayGraphic(ID graphicID, int nLeft, int nTop);
BOOL spiFlashDoesIDExist(ID nID);

