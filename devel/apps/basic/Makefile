SHELL=/bin/sh

APP=basic

SYNTH_DIR = ../../..
TB_DIR = ../../../avr_b3_tb
LIB_DIR = ../../lib
INC_DIR = ../../include
MISC_DIR = ../../misc

HEADERS=main.h symtab.h lexer.h parser.h expr.h runtime.h $(INC_DIR)/avr_b3.h $(INC_DIR)/avr_b3_stdio.h $(INC_DIR)/avr_b3_ps2.h $(INC_DIR)/keycodes.h \
$(INC_DIR)/avr_b3_diskio.h $(INC_DIR)/ff.h $(INC_DIR)/ffconf.h
OBJECTS=main.o symtab.o lexer.o parser.o expr.o runtime.o $(LIB_DIR)/avr_b3_stdio.o $(LIB_DIR)/avr_b3_ps2.o \
$(LIB_DIR)/avr_b3_diskio.o $(LIB_DIR)/ff.o $(LIB_DIR)/ffsystem.o $(LIB_DIR)/ffunicode.o

CC=avr-gcc
AS=avr-as
AR=avr-ar
OBJCOPY=avr-objcopy
OBJDUMP=avr-objdump
CHMOD=chmod
STAT=stat
DD=dd

#ARCH = avr25
ARCH = avr51
ARCH_LIB = $(LIB_DIR)/crt0.o

AFLAGS=-mmcu=$(ARCH)
#CFLAGS=-mmcu=$(ARCH) -D__COMPILING_AVR_LIBC__ -Wall -pedantic -O3
CFLAGS=-mmcu=$(ARCH) -D__COMPILING_AVR_LIBC__ -funsigned-char -funsigned-bitfields -fpack-struct -fshort-enums -Wall -Wstrict-prototypes -Wno-unused-function -c -Os
#LDFLAGS=-mmcu=$(ARCH)
#LDFLAGS=-mmcu=$(ARCH) -Wl,--verbose
#LDFLAGS=-mmcu=$(ARCH) -T $(MISC_DIR)/avr_b3_51.xn
LDFLAGS=-mmcu=$(ARCH) -T $(MISC_DIR)/avr_b3_51.xn -Wl,--defsym=__heap_start=0x80b000,--defsym=__heap_end=0x80efff -Wl,-u,vfprintf -lprintf_flt

PMEM_DEPTH=15
PMEM_WORDS=$(shell echo $$((1<<$(PMEM_DEPTH))))
PMEM_SIZE=$(shell echo $$((2<<$(PMEM_DEPTH))))

.PHONY: all clean dummy

TARGET=rom.mem

all: $(TARGET)

%.o: %.c $(HEADERS)
	$(CC) $(DEFINES) $(CFLAGS) -o $@ $<

$(ARCH_LIB): $(LIB_DIR)/crt0.S
	$(AS) $(AFLAGS) -o $(ARCH_LIB) $^

$(APP).elf: $(OBJECTS) $(ARCH_LIB) $(HEADERS)
	$(CC) $(LDFLAGS) -o $@ $(OBJECTS) $(ARCH_LIB)
	$(OBJDUMP) -s -m $(ARCH) -d $(APP).elf > $(APP).disasm

$(APP).bin: $(APP).elf
	$(OBJCOPY) -j .text -j .data -O binary $(APP).elf $(APP).bin

$(TARGET): $(APP).bin
	cat $(APP).bin /dev/zero | head -c $(PMEM_SIZE) | hexdump -v -e '/2 "%.4x\n"' > $(TARGET)

install: $(TARGET)
	cp $< $(SYNTH_DIR)/$<
	cp $< $(TB_DIR)/$<

clean:
	rm -f $(TARGET) *.o *.elf *.mem *.disasm *.hex *.bin $(LIB_DIR)/*.o
	
clean-all:
	rm -f $(TARGET) $(SYNTH_DIR)/$(TARGET) $(TB_DIR)/$(TARGET) *.o *.elf *.mem *.disasm *.hex *.bin $(LIB_DIR)/*.o
	$(MAKE) -C simulator clean

	
