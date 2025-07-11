/*
*   avr_b3_stdio.c
*
*   This file contains utilities to access avr_b3 peripherals.
*
*/

#define F_CPU 50000000UL
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <util/delay.h>
#include "../include/avr_b3.h"
#include "../include/avr_b3_stdio.h"

#define TABSIZE 4

void msleep(uint16_t msec)
{
    while (msec)
    {	
        _delay_loop_2((uint32_t)F_CPU/4000UL);
        msec--;
    }
}

void Display(uint16_t value, uint8_t displayQty)
{
    if (displayQty > 2)
    {
        DISPLAY0 = (value & 0xf000) >> 12;
        DISPLAY1 = (value & 0x0f00) >> 8;
    }
    DISPLAY2 = (value & 0x00f0) >> 4;
    DISPLAY3 = (value & 0x000f);
}

void KeyBeep(uint8_t tone, uint16_t durationMs)
{
    MIXER = VCO1;
    VCO1_FREQ = tone;
    if (durationMs != 0)
    {
        msleep(durationMs);
        VCO1_FREQ = 0;
    }
}

uint8_t ReadKeypad(bool beep, uint8_t tone, uint16_t durationMs)
{
    uint8_t keyCode = KEYPAD;
    
    if (keyCode)
    {
        if (beep)
            KeyBeep(tone, durationMs);
        while (KEYPAD)
        ;
    }
    
    return keyCode;
}

uint8_t ReadButtons(bool beep, uint8_t tone, uint16_t durationMs)
{
    uint8_t ButtonCode = BUTTONS;
    
    if (ButtonCode)
    {
        if (beep)
            KeyBeep(tone, durationMs);
        while (BUTTONS)
        ;
    }
    
    return ButtonCode;
}

int AppendKeypadValue(int value, bool *pIsNewEntry, bool beep, uint8_t tone, uint16_t durationMs)
{
    uint8_t keycode = ReadKeypad(beep, tone, durationMs);
    
    if (keycode)
    {
        if (*pIsNewEntry)
        {
            value = 0;
            *pIsNewEntry = false;
        }
        value = (value << 4) | (keycode & 0x000f);
    }
    
    return value;
}

uint8_t VgaPrintKeypadCode(uint8_t keypadCode)
{
    uint8_t asciiVal;
    
    // decode the keypad code, convert it to ASCII, then print it
    keypadCode &= 0x0f;
    asciiVal = keypadCode + ((keypadCode <= 9) ? 0x30 : 0x37);
    VGA_CHAR = asciiVal;
    
    return asciiVal;
}

void VgaMoveCursor(enum VGA_CUR_DIR dir)
{
    switch (dir)
    {
        // cursor up
        case CUR_UP:
            // move the cursor up
            VGA_CUR_ROW -= (VGA_CUR_ROW > VGA_ROW_MIN) ? 1 : 0;
            break;
            
        // cursor down
        case CUR_DOWN:
            // move the cursor down
            VGA_CUR_ROW += (VGA_CUR_ROW < VGA_ROW_MAX) ? 1 : 0;
            break;
            
        // cursor left
        case CUR_LEFT:
            // move the cursor left
            VGA_CUR_COL -= (VGA_CUR_COL > VGA_COL_MIN) ? 1 : 0;
            break;
            
        // cursor right
        case CUR_RIGHT:
            // move the cursor right
            VGA_CUR_COL += (VGA_CUR_COL < VGA_COL_MAX) ? 1 : 0;
            break;
    }
}

char VgaGetChar(int row, int col)
{
    if ((VGA_ROW_MIN <= row && row <= VGA_ROW_MAX) && (VGA_COL_MIN <= col && col <= VGA_COL_MAX))
    {
        VGA_CUR_ROW = row;
        VGA_CUR_COL = col;
        return VGA_CHAR;    
    }
    return 0;
}

char VgaPutChar(int row, int col, char c)
{
    if ((VGA_ROW_MIN <= row && row <= VGA_ROW_MAX) && (VGA_COL_MIN <= col && col <= VGA_COL_MAX))
    {
        VGA_CUR_ROW = row;
        VGA_CUR_COL = col;
        VGA_CHAR = c;   
        return c;    
    } 
    return 0;
}

void VgaFillFrameBuffer(char c)
{
    VGA_CUR_ROW = VGA_ROW_MIN;
    VGA_CUR_COL = VGA_COL_MIN;
    for (int i = 0; i < (VGA_ROW_MAX+1)*(VGA_COL_MAX+1); i++)
        VGA_CHAR = c;
}

void VgaLoadFrameBuffer(VGA_DISPLAY_BUFFER srcBuf)
{
    for (int row = 0; row <= VGA_ROW_MAX; row++)
        for (int col = 0; col <= VGA_COL_MAX; col++)
            VgaPutChar(VGA_ROW_MIN + row, VGA_COL_MIN + col, srcBuf[row][col]);
}

void VgaFillDisplayBuffer(VGA_DISPLAY_BUFFER buffer, char c)
{
    for (int row = 0; row <= VGA_ROW_MAX; row++)
        for (int col = 0; col <= VGA_COL_MAX; col++)
            buffer[row][col] = c;
}

void VgaLoadDisplayBuffer(VGA_DISPLAY_BUFFER destBuf, VGA_DISPLAY_BUFFER srcBuf)
{
    for (int row = 0; row <= VGA_ROW_MAX; row++)
        for (int col = 0; col <= VGA_COL_MAX; col++)
            destBuf[row][col] = srcBuf[row][col];
}

void VgaReset(void)
{
    VGA_CUR_ROW = 0;
    VGA_CUR_COL = 0;
    VGA_ROW_OFFSET = 0;
    VgaClearFrameBuffer();
}

void VgaNewline(void)
{
    // line feed
    if (VGA_CUR_ROW < VGA_ROW_MAX)
    {
        // move the cursor to the next row down
        VGA_CUR_ROW++;
    }
    else
    {
        // scroll the display up one row if the cursor is on the last row
        VGA_ROW_OFFSET = (VGA_ROW_OFFSET == 39) ? 0 : VGA_ROW_OFFSET+1;
        
        // clear the line
        for (int i = VGA_COL_MIN; i < VGA_COL_MAX; i++)
        {
            VGA_CUR_COL = i;
            VGA_CHAR = 0x20;
        }
    }
    
    // carriage return
    VGA_CUR_COL = VGA_COL_MIN;
}

void VgaPrintStr(const char *str)
{
    int i = 0;
    
    while (str[i])
    {
        switch (str[i])
        {
            case '\n':
                VgaNewline();
                i++;
                break;
            
            case '\t':
                for (int j = 0; j < TABSIZE; j++)
                    VGA_CHAR = ' ';
                i++;
                break;
            
            default:
                VGA_CHAR = str[i++];
                break;
        }
    }
}

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

// end of avr_b3_stdio.c

