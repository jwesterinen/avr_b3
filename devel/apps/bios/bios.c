
//
//      Reg 0:  LSB is ROM select.  A change forces a reboot
//      Reg 1:  Low 8 bits of read/write address
//      Reg 2:  High 7 bits of read/write address
//      Reg 3:  Low 8 bits of write data to non-selected ROM
//      Reg 4:  High 8 bits of write data to non-selected ROM
//#define MMU_SELECT              __MMIOR(MMIO_BASE_MMU+0x00)
//#define MMU_ADDR_LOW            __MMIOR(MMIO_BASE_MMU+0x01)
//#define MMU_ADDR_HIGH           __MMIOR(MMIO_BASE_MMU+0x02)
//#define MMU_INSTR_LOW           __MMIOR(MMIO_BASE_MMU+0x03)
//#define MMU_INSTR_HIGH          __MMIOR(MMIO_BASE_MMU+0x04)


#include <stdio.h>
#include <stdlib.h>
#include "../../include/avr_b3.h"
#include "../../include/avr_b3_stdio.h"
#include "../../include/avr_b3_console.h"
//#include "../../include/ff.h"
//#include "../../include/avr_b3_diskio.h"



/*************** DEFINES, GLOBALS, FORWARD REFERENCES *****************/
#define BUFSZ  514
char c;




// UART receive ISR
ISR(_VECTOR(3))
{
    int     val = 0;           // value of high or low nible
    static int lownibble = 0;  // ==1 if on low nibble, ==0 on high nibble
    static uint8_t byt[BUFSZ]; // Bytes to send/receive to/from SPI
    static int     bytinx = 0; // index into byt
    int     i;                 // generic loop counter
    uint8_t volatile *addrstatus = (volatile uint8_t *)(0xf501);

    // load the keyboard buffer with the char received by the UART
    c = UDR0;
    sei();

    UDR0 = c;         // echo input
    if (c == ' ') { 
        return;
    }
    else if ((c >= '0') && (c <= '9'))
        val = (int) c - (int) '0';
    else if ((c >= 'a') && (c <= '9')) 
        val = (int) c - (int) 'a';
    else if (c =='\r') {
        *addrstatus = (uint8_t)0x00;  // set cs (active low)
        *addrstatus = (uint8_t)0x02;  // clear cs
        // print return packet and prompt
        printf("<<");
        for (i = 0; i < bytinx; i++)
            printf(" %02x", byt[i]);
        printf("\n\r>> ");
        lownibble = 0;  // high nibble first
        bytinx = 0;   // new line of hex
        return;
    }
    else {
        printf("\n\rIllegal character in input.  Line terminated\n\r");
        lownibble = 0;  // high nibble first
        bytinx = 0;
        return;
    }

    // Got a valid hex character.  Nibble value is in val.
    //printf(" lownibble = %d, val = %d, bytinx = %d\n\r", lownibble, val, bytinx);
    if (lownibble == 1) {
        // low nibble
        byt[bytinx] += val;
        bytinx++;
        if (bytinx == BUFSZ) {
            printf("\n\rPacket exceeds 40 bytes. Line terminated\n\r");
            bytinx = 0;
        }
        lownibble = 0;  // high nibble first
    }
    else {
        // put high nibble in byt[]
        byt[bytinx] = val << 4;
        lownibble = 1;
    }
}

int main(void)
{
    uint16_t  data;

    // set UART baud rate to 115200, enable UART, enable interrupts
    // Baud Rate = (Oscillator Frequency / (16( UBRR Value +1))
    //UBRR0 = 13-1;  // 12.5 MHz system clock
    //UBRR0 = 14-1;  // 12.5 MHz system clock
    UBRR0 = 54-1;  // 50 MHz system clock
    UCSRB0 |= (1<<RXCIE);
    sei();
    stdout = &mystdout;

    while (1) {
        // Get data at ROM location 0x4000 and invert it.
        MMU_ADDR_LOW = 0x00;
        MMU_ADDR_HIGH = 0x40;
        data = MMU_INSTR_LOW;
        data = data + (MMU_INSTR_HIGH << 8);
        data = data ^ 0x01;

        // Write new value back to location 0x4000 and print new value
        // Note that HIGH is printed first and reading HIGH increments
        // the internal pointer so the displayed address will be 4001.
        MMU_ADDR_LOW = 0x00;
        MMU_ADDR_HIGH = 0x40;
        MMU_INSTR_LOW = data & 0xff;
        MMU_INSTR_HIGH = data >> 8;
        printf("Data =%02x%02x : %x\n\r", MMU_ADDR_HIGH, MMU_ADDR_LOW, data);
        msleep(100);
    }
}


