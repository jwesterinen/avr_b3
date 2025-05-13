# avr_b3

## Description
The avr_b3 project is an AVR-based single-board computer developed for the Digilent BASYS3 development board.
It contains a soft AVR core, 32K (2-byte) words of program memory (ROM), 60K bytes of data memory (RAM), and 4K
bytes of memory-mapped I/O (peripheral) registers. The project contains all hardware and software development 
files needed to create and run applications.

## Files
The top-level directory contains the HW files and 3 sub-directories, "avr\_b3\_tb," the test
bench, "build," the directory that receives all files created during Verilog compilation, and "devel," the 
application development directory.

## Hardware

The core is based on an open-source AVR softcore created by Andras Pal <apal@szofi.net>. The original package 
is available from https://szofi.net/pub/verilog/softavrcore/. The only HW files used from the original package are
avr\_core.v and avr\_io\_uart.v. The AVR core file was modified to synthesize the AVR 5.10 architecture.
Note: this required a modification to the C runtime file required by any application, as will be described below.

The I/O space of the processor is memory-mapped and resides in the top 4K of data memory, 0xf000 to 0xffff, with each
peripheral allocated up to 256 control and data registers. The included peripherals are as follows:
 - basic IO: 16 LEDs, 16 switches, a quad 7-segment display, and 5 pushbuttons
 - keypad: Digilent PmodKYPD 16-key keypad connected to Pmod connecctor JB
 - sound: Pmod Audio v1.2 connected to Pmod connector JA
 - VGA terminal: VGA connector
 - PS/2 keyboard: USB connector
 - SD memory: PmodMicroSD connected to Pmod connector JXADC
 
## Software Development

Software development is supported under the "devel" directory, which includes peripheral drivers and headers and
a set of existing applications. The drivers include a standard set of I/O functions that give access to the on-board 
peripherals, PS/2 keyboard, VGA display, keypad, speaker, SD card reader, and console device. Also provided is a
header file, avr_b3.h, containing the definition of all peripheral registers that can be used in applications using the
classic AVR programming register read and write paradigm. For example, writing the value of the on-board switches 
to the on-board LEDs is written as follows:
```
    LEDS = SWITCHES;
```

## Applications Development

In order to run an application, it must be built into an object file, then the entire avr_b3 must be compiled into a
file that can be loaded into the Basys3 board.  The Basys3 board must be connected to a USB port on the development 
system.  In general, the procedure is as follows:

```
    cd devel/apps/<app_name>
    make install
    cd ../../../
    make install
```

This will build the application into the image to be executed on the Basys3 board, then load and run the application
on the AVR softcore.


