#include <inttypes.h>
#include <stdbool.h>
#include "ff.h"
#include "avr_b3_diskio.h"

enum VGA_CUR_DIR {CUR_UP, CUR_DOWN, CUR_LEFT, CUR_RIGHT};
typedef char VGA_DISPLAY_BUFFER[VGA_ROW_MAX][VGA_COL_MAX];

void Display(uint16_t value, uint8_t displayQty);
void msleep(uint16_t msec);
void KeyBeep(uint8_t tone, uint16_t durationMs);
uint8_t ReadKeypad(bool beep, uint8_t tone, uint16_t durationMs);
uint8_t ReadButtons(bool beep, uint8_t tone, uint16_t durationMs);
int AppendKeypadValue(int value, bool *pIsNewEntry, bool beep, uint8_t tone, uint16_t durationMs);
uint8_t VgaPrintKeypadCode(uint8_t keypadCode);
void VgaMoveCursor(enum VGA_CUR_DIR dir);
char VgaGetChar(int row, int col);
char VgaPutChar(int row, int col, char c);
void VgaFillFrameBuffer(char c);
void VgaLoadFrameBuffer(VGA_DISPLAY_BUFFER srcBuf);
void VgaFillDisplayBuffer(VGA_DISPLAY_BUFFER buffer, char c);
void VgaLoadDisplayBuffer(VGA_DISPLAY_BUFFER destBuf, VGA_DISPLAY_BUFFER srcBuf);
void VgaReset(void);
void VgaNewline(void);
void VgaPrintStr(const char *str);

#define VGA_BLANK_CHAR ' '
#define VgaClearFrameBuffer() VgaFillFrameBuffer(VGA_BLANK_CHAR)
#define VgaClearDisplayBuffer(b) VgaFillDisplayBuffer((b), VGA_BLANK_CHAR)

#define MAX_FILE_BUFFER_LINES 100
#define MAX_FILE_BUFFER_WIDTH 80

typedef char SD_FILE_BUFFER[MAX_FILE_BUFFER_LINES][MAX_FILE_BUFFER_WIDTH];
typedef char SD_FILENAME_BUFFER[40][12];

FRESULT SdMount(void);
FRESULT SdUnmount(void);
FRESULT SdList(SD_FILENAME_BUFFER listBuf, int *plistSize);
#define SdDelete(filename) f_unlink(filename)
FRESULT SdLoad(const char *filename, SD_FILE_BUFFER loadBuf);
FRESULT SdSave(const char *filename, SD_FILE_BUFFER saveBuf);


