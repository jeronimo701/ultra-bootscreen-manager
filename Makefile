ifeq ($(strip $(DEVKITPRO)),)
$(error Set DEVKITPRO to your devkitPro installation)
endif
include $(DEVKITPRO)/libnx/switch_rules
APP_TITLE := Ultra Bootscreen Manager
APP_AUTHOR := Jeronimo701
APP_VERSION := 1.1.1
TARGET := UltraBootscreenManager
ARCH := -march=armv8-a+crc+crypto -mtune=cortex-a57 -mtp=soft -fPIE
CFLAGS := -O2 -Wall -Wextra -std=gnu11 $(ARCH) -I$(LIBNX)/include
LDFLAGS := -specs=$(LIBNX)/switch.specs $(ARCH) -Wl,-Map,build/app.map

.PHONY: all
all: $(TARGET).nro
build/main.o: source/app.c source/storage.c
	@mkdir -p build
	$(CC) $(CFLAGS) -c $< -o $@
$(TARGET).elf: build/main.o
	$(CC) $(LDFLAGS) $< -L$(LIBNX)/lib -lnx -o $@
$(TARGET).nacp: Makefile
	nacptool --create "$(APP_TITLE)" "$(APP_AUTHOR)" "$(APP_VERSION)" $@
$(TARGET).nro: $(TARGET).elf $(TARGET).nacp romfs/15a.bmp romfs/17e.bmp romfs/04a.bmp romfs/ultranx-original.bmp romfs/font.bin
	elf2nro $< $@ --nacp=$(TARGET).nacp --icon=$(LIBNX)/default_icon.jpg --romfsdir=romfs
