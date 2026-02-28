/*
*   avr_b3_fileio.c
*
*   This file contains common SD card file system utilities.
*
*/

#include <stdio.h>
#include <string.h>
#include "../include/avr_b3.h"
#include "../include/avr_b3_stdio.h"
#include "../include/avr_b3_fileio.h"

// global file system structures
FATFS diskA;
DIR topdir;
const char *mountPoint = "/";

// mount the root directory to the mountpoint
FRESULT SdMount()
{
    FRESULT retstat;
    
    // initialize SD card, mount FAT filesystem, and open root directory
    if ((retstat = disk_initialize(0)) == FR_OK) 
    {
        if ((retstat = f_mount(&diskA, mountPoint, 0)) == FR_OK) 
        {
            retstat = f_opendir(&topdir, "");
        }
    }

    return retstat;
}

// unmount the root directory from the mountpoint
FRESULT SdUnmount()
{
    return f_unmount(mountPoint);
}

// load a buffer with the names of files under a directory
FRESULT SdList(SD_FILENAME_BUFFER listBuf, int *plistSize)
{
    FILINFO fno;
    FRESULT retstat;
    *plistSize = 0;
    
    retstat = f_readdir(&topdir, &fno);
    while ((retstat == FR_OK) && (fno.fname[0])) 
    {
        fno.fname[12] = 0;
        sprintf(listBuf[(*plistSize)++], "%s\n", fno.fname);
        retstat = f_readdir(&topdir, &fno);
    }
    if (retstat == FR_OK)
    {
        retstat = f_rewinddir(&topdir);
    }
    
    return retstat;
}

// load the contents of the file into the buffer
FRESULT SdLoad(const char *filename, SD_FILE_BUFFER loadBuf)
{
    FIL fp; 
    TCHAR *bufptr;
    int i;
    FRESULT retstat;
    
    // open the file for reading
    if ((retstat = f_open(&fp, filename, FA_READ)) != FR_OK) 
    {
        return retstat;
    }

    // read the file (until EOF) into the file buffer
    for (i = 0; i < MAX_FILE_BUFFER_LINES; i++) 
    {
        if (f_eof(&fp) != 0)
        {
            // stop reading at EOF
            break;
        }
        if ((bufptr = f_gets(loadBuf[i], MAX_FILE_BUFFER_WIDTH, &fp)) == 0) 
        {
            return f_error(&fp);
        }
    }
    strcpy(loadBuf[i], "");

    // close the file
    retstat = f_close(&fp);

    return retstat;
}

// save the file buffer to a new file
FRESULT SdSave(const char *filename, SD_FILE_BUFFER saveBuf)
{
    FIL fp; 
    FRESULT retstat;
    
    // create a new file
    if ((retstat = f_open(&fp, filename, (FA_CREATE_ALWAYS | FA_READ | FA_WRITE))) != FR_OK) 
    {
        return retstat;
    }

    // write the buffer contents to the currently open file
    for (int line = 0; saveBuf[line][0] != '\0'; line++)
    {
        if (f_puts(saveBuf[line], &fp) < 0) 
        {
            return FR_DISK_ERR;
        }
    }

    // flush data to disk
    if ((retstat = f_sync(&fp)) != FR_OK) 
    {
        return retstat;
    }

    // close the currently open file
    retstat = f_close(&fp);

    return retstat;
}

