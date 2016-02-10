OBJECTS = main.o arp.o byteswap.o ethernet.o memory.o vectors.o	\
init.o lpc17xx.o emac.o irq.o atomics.o list.o tick.o

NEWLIB = /usr/arm-none-eabi/lib/armv7-m
LDSCRIPT = linker.ld
OPTIMISATION = 2

TOOLCHAIN = arm-none-eabi
CC = $(TOOLCHAIN)-gcc
AS = $(TOOLCHAIN)-as
LD = $(TOOLCHAIN)-ld

COMMONFLAGS = -mcpu=cortex-m3 -mthumb -nostartfiles
LIBS = -lm
LDFLAGS = -L$(NEWLIB) $(COMMONFLAGS) -T $(LDSCRIPT)
CFLAGS = $(COMMONFLAGS) -nostartfiles -c -g -O$(OPTIMISATION)

lpc-network.elf: $(OBJECTS) $(LDSCRIPT)
	$(CC) $(LDFLAGS) $(OBJECTS) -o $@ $(LIBS)

clean:
	rm *.o

%.o : %.c
	$(CC) $^ -o $@ $(CFLAGS)

%.o : %.s
	$(AS) $^ -o $@

.PHONY: clean
