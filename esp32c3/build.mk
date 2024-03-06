PROG        ?= firmware
ARCH        ?= esp32c3
MDK         ?= $(realpath $(dir $(lastword $(MAKEFILE_LIST)))/..)
ESPUTIL     ?= esputil
CFLAGS      ?= -W -Wall -Wextra -Werror -Wundef -Wshadow -pedantic \
               -Wdouble-promotion -fno-common -Wconversion \
               -march=rv32imc -mabi=ilp32 \
               -Os -ffunction-sections -fdata-sections \
               -I. -I$(MDK)/$(ARCH) $(EXTRA_CFLAGS)
LINKFLAGS   ?= -T$(MDK)/$(ARCH)/link.ld -nostdlib -nostartfiles -Wl,--gc-sections $(EXTRA_LINKFLAGS)
CWD         ?= $(realpath $(CURDIR))
FLASH_ADDR  ?= 0  # 2nd stage bootloader flash offset
DOCKER      ?= docker run -it --rm -v $(CWD):$(CWD) -v $(MDK):$(MDK) -w $(CWD) mdashnet/riscv
TOOLCHAIN   ?= riscv32-esp-elf
SRCS        ?= $(MDK)/$(ARCH)/boot.c $(SOURCES)

build: $(PROG).bin

$(PROG).elf: $(SRCS)
	$(TOOLCHAIN)-gcc  $(CFLAGS) $(SRCS) $(LINKFLAGS) -o $@
#	$(TOOLCHAIN)-size $@

$(PROG).bin: $(PROG).elf
	$(ESPUTIL) mkbin $(PROG).elf $@

flash: $(PROG).bin
	$(ESPUTIL) flash $(FLASH_ADDR) $(PROG).bin

monitor:
	$(ESPUTIL) monitor

clean:
	@rm -rf *.{bin,elf,map,lst,tgz,zip,hex} $(PROG)*
