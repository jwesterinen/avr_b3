# make file for avr_b3

PROJ = avr_b3

PROJ_SOURCES = top.v avr_core.v flash.v ram.v prescaler.v basic_io_b3.v display_b3.v keypad_b3.v PmodKYPD.v sound_b3.v mmio.v rom.mem sd.v

PROJ_XDC_FILE = $(PROJ).xdc

BUILD_DIR = build

STATUS_FILE = status
TARGET = $(BUILD_DIR)/$(PROJ).bit
DEVICE = arty_a7_35t

SOURCES = $(PROJ_SOURCES) $(PROJ_XDC_FILE)

VIVADO_DIR ?= /tools/Xilinx/Vivado/2019.1

.PHONY: all
all: $(TARGET)

$(TARGET): $(SOURCES)
	mkdir -p build
	$(VIVADO_DIR)/bin/vivado -mode tcl < $(PROJ).tcl > $(BUILD_DIR)/$(STATUS_FILE)
	#/tools/Xilinx/Vivado/2019.1/bin/vivado -mode tcl < $(PROJ).tcl > $(BUILD_DIR)/$(STATUS_FILE)
	#/media/jeff/c6a09b86-d374-458d-91bb-685ca2c65f6a/tools/Xilinx/Vivado/2022.2/bin/vivado -mode tcl < $(PROJ).tcl > $(BUILD_DIR)/$(STATUS_FILE)

install: $(TARGET)
	openFPGALoader -b $(DEVICE) $<

flash: $(TARGET)
	openFPGALoader -b $(DEVICE) --write-flash $<

.PHONY: clean clean-all
clean:
	rm -f *.html *.xml *.jou *.log build/*
	
clean-all:
	rm -f *.html *.xml *.jou *.log build/*
	rm -fr .Xil
	$(MAKE) -C devel clean-all
	$(MAKE) -C avr_b3_tb clean
	

