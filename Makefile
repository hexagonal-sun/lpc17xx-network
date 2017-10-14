OBJECTS = main.o arp.o byteswap.o ethernet.o memory.o vectors.o		\
init.o lpc17xx.o emac.o irq.o atomics.o list.o tick.o ipv4.o udp.o	\
tcp.o cbuf.o process.o context.o wait.o

NEWLIB = /usr/arm-none-eabi/lib/armv7-m
LDSCRIPT = linker.ld
OPTIMISATION = 2

TOOLCHAIN = arm-none-eabi
CC = $(TOOLCHAIN)-gcc
AS = $(TOOLCHAIN)-as
LD = $(TOOLCHAIN)-ld

COMMONFLAGS = -mcpu=cortex-m3 -mthumb
COMPILERFLAGS = $(COMMONFLAGS) -nostartfiles
LDLIBS = -lm
LDFLAGS = -L$(NEWLIB) $(COMPILERFLAGS) -T $(LDSCRIPT)
CFLAGS = $(COMPILERFLAGS) -c -g -O$(OPTIMISATION)
ASFLAGS = $(COMMONFLAGS)

lpc-network.elf: $(OBJECTS) $(LDSCRIPT)
	$(CC) $(LDFLAGS) $(OBJECTS) -o $@ $(LDLIBS)

clean:
	rm -f *.o lpc-network.elf

.PHONY: clean
