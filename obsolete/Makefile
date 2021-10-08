GCC := gcc
PROGRAMMER := programmer
ASM := shutter
BOOTLOADER := bootloader
TEST := test

PROGRAMMER_OBJS := \
	programmer.o \
	parapin.o


all: $(PROGRAMMER) $(BOOTLOADER).hex $(ASM).hex $(TEST).hex

$(PROGRAMMER): $(PROGRAMMER_OBJS)
	$(GCC) -o $(PROGRAMMER) $(PROGRAMMER_OBJS) -lpthread

$(ASM).hex: $(ASM).s
	gpasm -p 18f1220 -o $(ASM).hex $(ASM).s

$(BOOTLOADER).hex: $(BOOTLOADER).s
	gpasm -p 18f1220 -o $(BOOTLOADER).hex $(BOOTLOADER).s

$(TEST).hex: $(TEST).s
	gpasm -p 18f1220 -o $(TEST).hex $(TEST).s


$(PROGRAMMER_OBJS):
	$(GCC) -c -O2 $*.c -o $*.o

clean:
	rm -f $(PROGRAMMER) $(PROGRAMMER_OBJS) $(ASM) $(BOOTLOADER) *.cod *.lst *.hex

parapin.o: parapin.c
programmer.o: programmer.c
$(ASM).hex: $(ASM).s util.inc
$(BOOTLOADER).hex: $(BOOTLOADER).s util.inc
