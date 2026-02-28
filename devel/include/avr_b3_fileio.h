#include "ff.h"
#include "avr_b3_diskio.h"

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


