/*
*   This is the main module for a Basic interpreter retro computer.
*/

#include <time.h>
#include <stdlib.h>
#include <string.h>
#include "../../include/avr_b3.h"
#include "../../include/avr_b3_stdio.h"
#include "../../include/avr_b3_fileio.h"
#include "../../include/avr_b3_ps2.h"
#include "../../include/avr_b3_console.h"
#include "../../include/avr_b3_diskio.h"
#include "symtab.h"
#include "expr.h"
#include "parser.h"
#include "runtime.h"

// display buffer used for animation
VGA_DISPLAY_BUFFER dispBuf;

// default is PS2 keyboard
#define USE_CONSOLE_KB

#ifdef USE_CONSOLE_KB
    #define CR      '\r'    // newlines are returned as carriage return (CR) by terminal emulators
    #define BS      0x7f    // backspaces are returned as delete (DEL) by terminal emulators

    char keycode = 0;
    #define GetKey() keycode    
#else
    #define CR      '\n'
    #define BS      '\b'

    uint16_t keycode;
    char GetKey(void)
    {
        return (keycode = getps2());
    }    
#endif


// UART receive ISR
ISR(_VECTOR(3))
{
    // load the keyboard buffer with the char received by the UART
    keycode = UDR0;
    sei();
}

#ifndef USE_CONSOLE_KB
// PS2 receive ISR
ISR(_VECTOR(2))
{
    // PS2 HW API to get a ps2_keycode when there is a keypress
    getkey();
    sei();
}
#endif

char message[80];
char *versionStr = "v4.0";
char *promptStr = "> ";
    
extern bool ready;

// print a message to the console device
void Console(const char *string)
{
    stdout = &mystdout;
    printf(string);
}

// print out messages during runtime
void Message(const char *message)
{
    VgaPrintStr(message);
}

// print out system error messages unconditionally during runtime
void Panic(const char *message)
{
    VgaPrintStr(message);
}

void PutString(char *string)
{
    if (textMode)
    {
        VgaPrintStr(string);
    }
}

// returns the next CR-terminated string from the input device
char *GetString(char *buffer)
{
    unsigned i = 0;
        
    while (1)
    {
        if (GetKey())
        {
            //sprintf(message, "keycode = %d\r\n", keycode);
            //Console(message);
            
            // Enter - terminate the command string and return
            if (keycode == CR)
            {
                buffer[i++] = keycode;
                buffer[i] = (uint8_t)'\0';
                VgaNewline();
                keycode = 0x00;
                return buffer;
            }

            // Backspace - move the cursor back on place stopping at the prompt
            else if (keycode == BS)
            {
                if (VGA_CUR_COL > VGA_COL_MIN + strlen(promptStr))
                {          
                    VGA_CUR_COL--;
                    VGA_CHAR = ' ';
                    VGA_CUR_COL--;
                    i--;
                }
            }

            // display only printable characters
            else if (0x20 <= keycode && keycode <= 0x7f)
            {
                buffer[i++] = keycode;
                VGA_CHAR = keycode;
            }
            
            // clear the old keycode -- really only necessary for the console
            keycode = 0x00;
        }
        msleep(50);
    }
}

uint8_t MemRead(uint16_t addr)
{
    return (*(volatile uint8_t *)(addr));
}

void MemWrite(uint16_t addr, uint8_t data)
{
    (*(volatile uint8_t *)(addr)) = data;
}

void Tone(uint16_t freq, uint16_t duration)
{
    KeyBeep(freq, duration);
}

void InitDisplay(void)
{
    VgaReset();
    PutString("AVR_B3 Basic Interpreter ");
    PutString(versionStr);
    PutString("\n\n");
}

uint16_t Switches(void)
{
    return SW;
}

uint8_t Buttons(void)
{
    return BUTTONS;
}

void Leds(uint16_t value)
{
    LED = value;
}

void Display7(uint16_t value, uint8_t displayQty)
{
    Display(value, displayQty);
}

void Delay(uint16_t duration)
{
    msleep(duration);
}

uint8_t GfxPutChar(uint8_t row, uint8_t col, uint8_t c)
{
    return VgaPutChar(row, col, c);
}

uint8_t GfxGetChar(uint8_t row, uint8_t col)
{
    return VgaGetChar(row, col);
}

uint8_t GfxPutDB(uint8_t row, uint8_t col, uint8_t c)
{
    dispBuf[row][col] = c;
    return dispBuf[row][col];
}

uint8_t GfxGetDB(uint8_t row, uint8_t col)
{
    return dispBuf[row][col];
}

void GfxLoadFB(void)
{
    VgaLoadFrameBuffer(dispBuf);
}

void GfxClearScreen(void)
{
    VgaClearFrameBuffer();
}

void GfxClearDB(void)
{
    VgaClearDisplayBuffer(dispBuf);
}

void GfxTextMode(uint8_t mode)
{
    VGA_CUR_STYLE = (mode) ? VGA_CUR_VISIBLE : VGA_CUR_INVISIBLE;
}

// buffer used for load and save
SD_FILE_BUFFER fileBuffer;
SD_FILENAME_BUFFER filenameBuf;

// mount the root directory
bool FsMount(void)
{
    FRESULT retstat;
    
    // mount the SD card
    //if ((retstat = SdMount(&topdir, "/")) != FR_OK) 
    if ((retstat = SdMount()) != FR_OK) 
    {
        sprintf(errorStr, "mount failed (error %d)\n", retstat);
        return false;
    }

    return true;
}

// unmount the SD card
bool FsUnmount(void)
{
    FRESULT retstat;
    
    //if ((retstat = SdUnmount("/")) != FR_OK)
    if ((retstat = SdUnmount()) != FR_OK)
    {
        sprintf(errorStr, "unmount failed (error %d)\n", retstat);
        return false;
    }
    PutString("...it is safe to remove the disk\n");

    return true;
}

// print a listing of the files on the SD card
bool FsList(void)
{
    int fileQty;
    FRESULT retstat;
    
    if ((retstat = SdList(filenameBuf, &fileQty)) == FR_OK)
    {
        for (int i = 0; i < fileQty; i++)
        {
            strcpy(message, filenameBuf[i]);
            PutString(message);
        }
    }
    else
    {
        sprintf(errorStr, "the file system may not be mounted (error %d)\n", retstat);
        return false;
    }

    return true;
}

// remove the file from the SD card
bool FsDelete(const char *filename)
{
    FRESULT retstat;
    
    if (filename != NULL)
    {
        if ((retstat = SdDelete(filename)) != FR_OK)
        {
            sprintf(errorStr, "file delete failed (error %d)\n", retstat);
            return false;
        }
    }
    else
    {
        strcpy(errorStr, "missing filename");
        return false;
    }

    return true;
}

// load the file as the current program
bool FsLoad(const char *filename)
{
    char *nextChar;
    FRESULT retstat;
    
    if (filename != NULL)
    {
        
        if ((retstat = SdLoad(filename, fileBuffer)) != FR_OK) 
        {
            sprintf(errorStr, "load failed (error %d)\n", retstat);
            return false;
        }
        
        // process each line of the file buffer
        for (int i = 0; fileBuffer[i][0] != '\0'; i++)
        {
            // remove any line endings
            for (nextChar = fileBuffer[i]; *nextChar != '\0'; nextChar++)
            {
                if (*nextChar == '\n' || *nextChar == '\r')
                {
                    *nextChar = '\0';
                    break;
                }
            }
            
            if (!ProcessCommand(fileBuffer[i]))
            {
                PutString(errorStr);
                PutString("\n");
            }
        }
    }
    else
    {
        strcpy(errorStr, "missing filename");
        return false;
    }

    return true;
}

// save the current program to the file
bool FsSave(const char *filename)
{
    int i;
    FRESULT retstat;
    
    if (filename != NULL)
    {
        // fill the file buffer with the program commands
        for (i = 0; i < programSize; i++)
        {
            strcpy(fileBuffer[i], Program[i].commandStr);
            strcat(fileBuffer[i], "\n");
        }
        strcpy(fileBuffer[i], "");
        
        // write the file buffer to the file
        if ((retstat = SdSave(filename, fileBuffer)) != FR_OK) 
        {
            sprintf(errorStr, "save failed (error %d)\n", retstat);
            return false;
        }
    }
    else
    {
        strcpy(errorStr, "missing filename");
        return false;
    }

    return true;
}


int main(void)
{
    // set UART baud rate to 115200
    //UBRR0 = 13-1;
    UBRR0 = 54-1;

    // enable UART receiver interrupts
    UCSRB0 |= (1<<RXCIE);

    // enable global interrupts
    sei();

    Console("starting basic interpreter...\r\n");
    
    char command[80];
    
    InstallBuiltinFcts();
    InitDisplay();
    FsMount();
    while (1)
    {
        if (ready)
        {
            PutString("ready\n");
        }
        PutString(promptStr);        
        GetString(command);
        command[strlen(command)-1] = '\0';
        if (!ProcessCommand(command))
        {
            PutString(errorStr);
            PutString("\n");
        }
    }
}

// end of main.c

