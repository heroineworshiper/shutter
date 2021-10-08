// PIC 18F programmer for the Trapezoidian Equatorial Mount
//
// This programmer supports a bootloader.  The advantage is very few 
// connections.  Only PGC, PGD, and MCLR need to be connected for all
// programming and debugging operations.
// Usage:
//
// First the CPU must be programmed with the bootloader.
// The chip is put in the programming harness and the programmer run.
//     programmer bootloader.hex
//
// The CPU can be verified in the programming harness.
//     programmer -r bootloader.hex
//
// The CPU can be incrementally programmed in the harness.
//     programmer -i bootloader.hex
//
// Second the CPU must be transferred to the circuit since the bootloader
// requires an oscillator.  PGD, PGC, and MCLR must be patched to the 
// programming hardness.  GND must also be connected if the power supplies
// are different.
// It is programmed in the circuit by invoking the bootloader.
//     programmer -b program.hex
//
// The bootloader can also write programs incrementally.
//     programmer -b -i program.hex
//
// The bootloader can verify the program in memory.
//     programmer -b -r program.hex

#include "parapin.h"
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

pthread_mutex_t parport_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_t charge_pump;



// LP_PIN01     programming voltage on MCLR
// LP_PIN02     PGD output
// LP_PIN03     PGD output enable when low
// LP_PIN04     ground MCLR
// LP_PIN05     Vdd enable
// LP_PIN14     charge pump oscillator
// LP_PIN15     PGD input
// LP_PIN16     enable charge pump ground due to current leakage on startup
// LP_PIN17     PGC

// Registers
#define PORTA 0x80
#define TRISA 0x92
#define EECON1 0xa6
#define EECON2 0xa7
#define ADCON1 0xc1
#define TBLPTRL 0xf6
#define TBLPTRH 0xf7
#define TBLPTRU 0xf8

// Special instructions
#define NOP 0x0
#define TBLWT 0xd
#define TBLWT_NO_INC 0xc
#define TBLRD 0x9

// Bootloader instructions
#define BOOTLOADER_READ 0xaa
#define BOOTLOADER_WRITE 0x55
// Program start when bootloader is installed
#define PROGRAM_START 0x200




// Debugger state
// Wait for 0xf
#define DEBUG_WAIT 0
#define DEBUG_READ_SIZE1 1
// Read value size
#define DEBUG_READ_SIZE2 2
// Read value
#define DEBUG_READ_VALUE 3





// delay for programming harness
#define DELAY1 300
// delay for bootloader
#define DELAY2 1000


#define COMMAND_DELAY 0
#define ERASE_DELAY 10000
#define CHARGE_PUMP_DELAY 2

#define MAX(x, y) ((x) > (y) ? (x) : (y))
#define MIN(x, y) ((x) < (y) ? (x) : (y))

#define SET_REGISTER(reg, literal) \
/* movlw literal */ \
send_command(NOP, (0x0e << 8) | literal, 0, 0, 0); \
/* movwf reg */ \
send_command(NOP, (0x6e << 8) | reg, 0, 0, 0);

#define SET_POINTER \
	if((address & 0xff0000) != (prev_address & 0xff0000)) \
	{ \
		send_command(0x0, (0x0e << 8) | ((address & 0xff0000) >> 16), 0, 0, 0); \
		send_command(0x0, 0x6ef8, 0, 0, 0); \
	} \
 \
	if((address & 0xff00) != (prev_address & 0xff00)) \
	{ \
		send_command(0x0, (0x0e << 8) | ((address & 0xff00) >> 8), 0, 0, 0); \
		send_command(0x0, 0x6ef7, 0, 0, 0); \
	} \
 \
	if((address & 0xff) != (prev_address & 0xff)) \
	{ \
		send_command(0x0, (0x0e << 8) | (address & 0xff), 0, 0, 0); \
		send_command(0x0, 0x6ef6, 0, 0, 0); \
	} \




// Print bit activity
int debug = 0;
int use_bootloader = 0;
int do_program = 1;
int max_byte;
int min_address;
unsigned char *data_diff64;
unsigned char *data_bytes;
uint16_t *data;
int delay;
FILE *debug_file = 0;



void lock_port()
{
	pthread_mutex_lock(&parport_lock);
}

void unlock_port()
{
	pthread_mutex_unlock(&parport_lock);
}

void* charge_pump_thread(void *ptr)
{
	while(1)
	{
		lock_port();
		set_pin(LP_PIN16);
		set_pin(LP_PIN14);
		unlock_port();

		usleep(100000);

		lock_port();
		clear_pin(LP_PIN14);
		unlock_port();

		usleep(100000);
	}
}

void reset_pins()
{
// Reset the chip
	printf("Resetting chip.\n");
	usleep(100000);
	clear_pin(LP_PIN01 | 
		LP_PIN02 | 
		LP_PIN04 | 
		LP_PIN05 |
		LP_PIN14 | 
		LP_PIN16 | 
		LP_PIN17);
	set_pin(LP_PIN03);

	set_pin(LP_PIN04);
	sleep(1);
	clear_pin(LP_PIN04);

	if(debug_file) fclose(debug_file);
}

void signal_entry()
{
	printf("\nGot interrupted.\n");
	reset_pins();
	exit(0);
}

void read_bits(uint16_t *bits, int total)
{
	int i;
	unsigned int bit;

	*bits = 0;

	clear_pin(LP_PIN02);
	usleep(delay);

	for(i = 0; i < total; i++)
	{
		*bits >>= 1;

		if(i == 8 || (i == 0 && use_bootloader))
		{
// Enable read of PGD
			*bits = 0;
			set_pin(LP_PIN03);
			usleep(delay);
		}



		lock_port();
		set_pin(LP_PIN17);
		unlock_port();


		usleep(delay);

		lock_port();
		clear_pin(LP_PIN17);
		bit = pin_is_set(LP_PIN15) ? 1 : 0;
		unlock_port();

		*bits |= (bit << 7);


		if(debug)
		{
			printf("%d", bit ? 1 : 0);
			fflush(stdout);
		}
		usleep(delay);

//sleep(1);
	}

// Bootloader never writes once it starts reading
	if(!use_bootloader)
	{
// Enable write of PGD
		clear_pin(LP_PIN03);
		usleep(delay);
	}
}

// Send number of bits, least significant first.
void send_bits(uint16_t bits, int total, int programming_time)
{
	int i;
	int debug = 1;
	int bit;
	for(i = 0; i < total; i++)
	{
		lock_port();
// Clock up
		set_pin(LP_PIN17);
		unlock_port();

		usleep(delay);

		lock_port();

// Set data
		bit = bits & 0x1;
		if(bit)
		{
			set_pin(LP_PIN02);
			if(debug) printf("1");
		}
		else
		{
			clear_pin(LP_PIN02);
			if(debug) printf("0");
		}
		unlock_port();


		if(debug) fflush(stdout);

// Hold PGC high for programming time
		if(programming_time && i == total - 1)
		{
			printf("delay");
			usleep(2000);
		}
		else
			usleep(delay);


// Clock down to latch bit
		lock_port();
		clear_pin(LP_PIN17);
// Verify
		int verify_bit = pin_is_set(LP_PIN15) ? 1 : 0;
		unlock_port();



//		printf("%d ", verify_bit);
//		fflush(stdout);

		while(verify_bit != bit)
		{
			printf("\n*** error\n");
			sleep(1);
			lock_port();
			verify_bit = pin_is_set(LP_PIN15) ? 1 : 0;
			unlock_port();
			printf("%d", verify_bit);
		}


		usleep(delay);
		bits >>= 1;
	}
}

void send_command(unsigned char command, 
	uint16_t output, 
	uint16_t *input,
	int erase_delay,
	int program_delay)
{
	int i;
	int debug = 1;
	static int last_command = 0;

// Clock in command
	if(debug)
	{
		printf("send_command %02x -> ", command);
		fflush(stdout);
	}

// Send in command
	send_bits(command, 
		4, 
		(program_delay || last_command == TBLWT_NO_INC) ? 1 : 0);

	if(erase_delay)
	{
		printf("bulk erase delay");
		usleep(10000);
	}

// Send in data
	if(command == TBLRD)
	{
		if(debug)
			printf(" ");
		read_bits(input, 16);
		if(debug)
			printf(" -> %04x ", *input);
	}
	else
	{
		if(debug)
			printf(" %04x -> ", output);
		send_bits(output, 16, 0);
	}


	if(debug) printf("\n");
	usleep(COMMAND_DELAY);
	last_command = command;
}


void start_programming_mode()
{
	if(use_bootloader)
	{
		lock_port();
// Disable charge pump
		clear_pin(LP_PIN14);
// Enable Vdd op-amp
		set_pin(LP_PIN16);
// Ground MCLR
		set_pin(LP_PIN04);
// Raise PGC
		set_pin(LP_PIN17);
// Raise PGD
		set_pin(LP_PIN02);
// Enable PGD output
		clear_pin(LP_PIN03);
// Raise Vdd
		clear_pin(LP_PIN05);
		unlock_port();
		sleep(1);

		lock_port();
// Raise MCLR to 5V
		clear_pin(LP_PIN04);
		unlock_port();

// Taking a long time for the internal oscillator to start.
		sleep(5);




#if 0
// Send 0
		lock_port();
		clear_pin(LP_PIN02);
		unlock_port();

		usleep(delay);

// Clock down
		lock_port();
		clear_pin(LP_PIN17);
		unlock_port();

		usleep(delay);

// Clock up
// PGD up
		lock_port();
		set_pin(LP_PIN17);
		unlock_port();

		usleep(delay);

// Send 1
		lock_port();
		set_pin(LP_PIN02);
		unlock_port();

		usleep(delay);

// Clock down
	  	lock_port();
		clear_pin(LP_PIN17);
		unlock_port();

		usleep(delay);
#endif









	}
	else
	{

// Ground MCLR
// Lower PGC and PGD
// Enable PGD output
// Power down
		lock_port();
		clear_pin(LP_PIN02);
		clear_pin(LP_PIN03);
		set_pin(LP_PIN04);
		set_pin(LP_PIN05);
		clear_pin(LP_PIN17);
		unlock_port();


		sleep(2);

// Raise Vdd
		lock_port();
		clear_pin(LP_PIN05);
		unlock_port();

		sleep(1);

// Raise MCLR
		lock_port();
		clear_pin(LP_PIN04);
		set_pin(LP_PIN01);
		unlock_port();

		sleep(1);
	}
}



void verify(int max_byte)
{
	int total_errors = 0;
	int j;


	if(use_bootloader)
	{
		printf("Reading %d bytes\n", max_byte - PROGRAM_START);
		start_programming_mode();

		send_bits(BOOTLOADER_READ, 8, 0);
		send_bits(PROGRAM_START, 16, 0);

		printf("\n");

// Output disable on PGD
		lock_port();
		set_pin(LP_PIN03);
		unlock_port();

// Read out data
		for(j = PROGRAM_START; j < max_byte; j++)
		{
			uint16_t temp;
			printf("0x%04x: ", j);
			read_bits(&temp, 8);
			if(debug) printf(" ");
			printf("0x%02x\n", temp);
			if(temp != data_bytes[j])
			{
				printf("*** Error.  Orig = %02x\n", data_bytes[j]);
				total_errors++;
			}
		}

	}
	else
	{
		printf("Reading %d bytes\n", max_byte);
		start_programming_mode();

// Set table pointer
		SET_REGISTER(TBLPTRL, 0x00);
		SET_REGISTER(TBLPTRH, 0x00);
		SET_REGISTER(TBLPTRU, 0x00);

		for(j = 0; j < max_byte; j++)
		{
			uint16_t temp;
			printf("0x%04x: ", j);
			send_command(TBLRD, 0, &temp, 0, 0);
			if((temp & 0xff) != data_bytes[j])
			{
				printf("*** Error.  Orig = %02x\n", data_bytes[j]);
				total_errors++;
			}
		}
	}

	printf("Got %d errors\n", total_errors);
}



void write_program()
{
	int i, j, k;
	uint16_t *data = (uint16_t*)data_bytes;

	if(use_bootloader)
	{
		printf("Uploading %d bytes\n", max_byte - PROGRAM_START);

		start_programming_mode();


// Write data in blocks of 64 bytes
		for(i = PROGRAM_START; i < max_byte; i += 64)
		{
			send_bits(BOOTLOADER_WRITE, 8, 0);

			send_bits(i, 16, 0);

			printf("\n");

// Write 64 bytes before next erase must be called
			for(j = i; j < i + 64 && j < max_byte; j += 8)
			{
// Must write 8 bytes to cause a write operation
				for(k = j; k < j + 8; k++)
				{
					printf("0x%04x: ", k);
					send_bits(data_bytes[k], 8, 0);
					printf("\n");
				}
			}
		}
	}
	else
	{
		printf("Uploading %d bytes\n", max_byte);
		start_programming_mode();


// Bulk erase from the documentation
		send_command(0x0, 0x0e3c, 0, 0, 0);
		send_command(0x0, 0x6ef8, 0, 0, 0);
		send_command(0x0, 0x0e00, 0, 0, 0);
		send_command(0x0, 0x6ef7, 0, 0, 0);
		send_command(0x0, 0x0e04, 0, 0, 0);
		send_command(0x0, 0x6ef6, 0, 0, 0);
		send_command(0xc, 0x0080, 0, 0, 0);
		send_command(0x0, 0x0000, 0, 0, 0);
		send_command(0x0, 0x0000, 0, 1, 0);


// Enable writing program memory from documentation
		send_command(0x0, 0x8ea6, 0, 0, 0);
		send_command(0x0, 0x9ca6, 0, 0, 0);

		int address = 0;
		int prev_address = 0x7fffffff;
		for(address = 0; address < max_byte; )
		{
			SET_POINTER

// Write 8 bytes
			send_command(0xd, data[address / 2], 0, 0, 0);
			address += 2;
			send_command(0xd, data[address / 2], 0, 0, 0);
			address += 2;
			send_command(0xd, data[address / 2], 0, 0, 0);
			address += 2;
			prev_address = address;
			send_command(0xf, data[address / 2], 0, 0, 0);
			address += 2;
			send_command(0x0, 0, 0, 0, 0);
		}
	}
}










void write_incremental(char *orig_path)
{
	int i, j, k;
	printf("Reading previous program from %s\n", orig_path);

	FILE *orig = fopen(orig_path, "r");
	int total_sectors = 0;
	int program_start = 0;
	if(use_bootloader) program_start = PROGRAM_START;


	if(!orig)
	{
		printf("No previous program found.\n");
		do_program = 1;
	}
	else
	{
		fseek(orig, program_start, SEEK_SET);
		for(i = program_start; i < max_byte; i++)
		{
			int end_of_file = feof(orig);
			unsigned char prev_byte = fgetc(orig);


// Bytes not equivalent or current byte is beyond end of previous file
			if(prev_byte != data_bytes[i] || end_of_file)
			{
				data_diff64[i / 64] = 1;
			}
		}
		fclose(orig);


		for(i = program_start; i < max_byte; i += 64)
		{
			if(data_diff64[i / 64])
			{
				total_sectors++;
			}
		}

		printf("Writing %d bytes\n", MIN(total_sectors * 64, max_byte));
		start_programming_mode();


		for(i = program_start; i < max_byte; i += 64)
		{
			if(data_diff64[i / 64])
			{
				int address = i;




				if(use_bootloader)
				{
					send_bits(BOOTLOADER_WRITE, 8, 0);
					send_bits(address, 16, 0);

					printf("\n");
// Write 64 bytes per write command
					for(j = i; j < i + 64 && j < max_byte; j += 8)
					{
// Must write 8 bytes to cause a write operation
						for(k = j; k < j + 8; k++)
						{
							printf("0x%04x: ", k);
							send_bits(data_bytes[k], 8, 0);
							printf("\n");
						}
					}
				}
				else
				{




// Enable writing program memory from documentation
					send_command(0x0, 0x8ea6, 0, 0, 0);
					send_command(0x0, 0x9ca6, 0, 0, 0);

// Set write pointer
					send_command(0x0, (0x0e << 8) | ((address & 0xff0000) >> 16), 0, 0, 0);
					send_command(0x0, 0x6ef8, 0, 0, 0);
					send_command(0x0, (0x0e << 8) | ((address & 0xff00) >> 8), 0, 0, 0);
					send_command(0x0, 0x6ef7, 0, 0, 0);
					send_command(0x0, (0x0e << 8) | (address & 0xff), 0, 0, 0);
					send_command(0x0, 0x6ef6, 0, 0, 0);

// Setup erase
					send_command(0x0, 0x84a6, 0, 0, 0);
					send_command(0x0, 0x88a6, 0, 0, 0);

// Unlock flash
					send_command(0x0, 0x0e55, 0, 0, 0);
					send_command(0x0, 0x6ea7, 0, 0, 0);
					send_command(0x0, 0x0eaa, 0, 0, 0);
					send_command(0x0, 0x6ea7, 0, 0, 0);

// Start erase
					send_command(0x0, 0x82a6, 0, 0, 0);
					send_command(0x0, 0x0000, 0, 0, 0);
					usleep(10000);

// Disable writes
					send_command(0x0, 0x94a6, 0, 0, 0);

// Write 64 bytes
					int prev_address = address;
					for(j = 0; j < 64 && address < max_byte; j += 8)
					{
// Set pointer
						if(j > 0)
						{
							SET_POINTER
						}

						printf("0x%04x: ", address);
// Write 8 bytes
						send_command(0xd, data[address / 2], 0, 0, 0);
						address += 2;
						send_command(0xd, data[address / 2], 0, 0, 0);
						address += 2;
						send_command(0xd, data[address / 2], 0, 0, 0);
						address += 2;
						prev_address = address;
						send_command(0xf, data[address / 2], 0, 0, 0);
						address += 2;
						send_command(0x0, 0, 0, 0, 0);
					}
				}
			}
		}



// Write new program
		printf("Writing original to %s\n", orig_path);
		FILE *orig = fopen(orig_path, "w");
		fwrite(data, 1, max_byte, orig);
		fclose(orig);

	}
}














// The debug routine drives PGC and waits for 0xff to come from PGD.
// This aligns the serial signal.
// Every 0xff is expected to proceed with a single byte of information.
void dump_pgd()
{
	printf("Dumping PGD\n");

// Enter executable
	reset_pins();

	lock_port();
// Disable charge pump
	clear_pin(LP_PIN14);
// Enable Vdd op-amp
	set_pin(LP_PIN16);
// Raise Vdd
	clear_pin(LP_PIN05);
	unlock_port();


	debug_file = fopen("/tmp/debug", "w");

	int state = DEBUG_WAIT;
	int counter = 0;
	uint64_t value = 0;
	int size = 0;
	int i;
	while(1)
	{
		uint16_t bit;
		uint16_t type;
		read_bits(&bit, 1);
		if(bit) bit = 1;


		switch(state)
		{
			case DEBUG_WAIT:
				if(bit == 0)
					counter = 0;
				else
					counter++;

				if(counter == 4)
				{
					state = DEBUG_READ_SIZE1;
				}
				break;

			case DEBUG_READ_SIZE1:
				if(!bit)
				{
					state = DEBUG_READ_SIZE2;
					counter = 0;
					size = 0;
				}
				else
// keep reading until first 0 bit
				{
					counter = 3;
					state = DEBUG_WAIT;
				}
				break;

			case DEBUG_READ_SIZE2:
				size >>= 1;
				size |= (bit ? 0x04 : 0x00);
				counter++;
				if(counter == 3)
				{
					if(size > 0)
					{
// special case for 64 bits
						if(size == 7) size = 8;
						state = DEBUG_READ_VALUE;
					}
					else
					{
						state = DEBUG_WAIT;
					}
					counter = 0;
					value = 0;
					if(debug) printf("\n");
				}
				break;

			case DEBUG_READ_VALUE:
				value >>= 1;
				value |= ((uint64_t)bit) << (size * 8 - 1);
				counter++;
				if(counter == size * 8)
				{
					state = DEBUG_WAIT;
					counter = 0;
					if(debug) printf("\n");
					printf("Got ");

// print binary result
					if(1)					
						for(i = size * 8 - 1; i >= 0; i--)
						{
							uint64_t mask = ((uint64_t)1) << i;
							if(value & mask) 
								printf("1");
							else
								printf("0");
						}
					printf(" 0x%llx\n", value);


					if(debug_file)
					{
						for(i = 0; i < size; i++)
						{
							fputc(value & 0xff, debug_file);
							value >>= 8;
							fflush(debug_file);
						}
					}
				}
				else
				if(!(counter % 8) && debug) 
					printf(" ");
				break;
		}
	}
}






// Program 2 bytes of configuration memory starting on an even address
void program_configuration(int address, uint16_t data)
{
	printf("programming 0x%x with %d%d%d%d%d%d%d%d\n", 
		address,
		(data & 0x0001) ? 1 : 0,
		(data & 0x0002) ? 1 : 0,
		(data & 0x0004) ? 1 : 0,
		(data & 0x0008) ? 1 : 0,
		(data & 0x0010) ? 1 : 0,
		(data & 0x0020) ? 1 : 0,
		(data & 0x0040) ? 1 : 0,
		(data & 0x0080) ? 1 : 0);
	printf("programming 0x%x with %d%d%d%d%d%d%d%d\n", 
		address + 1,
		(data & 0x0100) ? 1 : 0,
		(data & 0x0200) ? 1 : 0,
		(data & 0x0400) ? 1 : 0,
		(data & 0x0800) ? 1 : 0,
		(data & 0x1000) ? 1 : 0,
		(data & 0x2000) ? 1 : 0,
		(data & 0x4000) ? 1 : 0,
		(data & 0x8000) ? 1 : 0);

	start_programming_mode();
// Direct access to configuration memory
	send_command(0x0, 0x8ea6, 0, 0, 0);
	send_command(0x0, 0x8ca6, 0, 0, 0);
// Position program counter with a goto command
	send_command(0x0, 0xef00, 0, 0, 0);
	send_command(0x0, 0xf800, 0, 0, 0);
// Set table pointer to configuration register
	int prev_address = 0;
	SET_POINTER
	send_command(0xf, (data & 0xff), 0, 0, 0);
	send_command(0x0, 0x0000, 0, 0, 1);
	send_command(0x0, 0x2af6, 0, 0, 0);
	send_command(0xf, (data & 0xff00), 0, 0, 0);
	send_command(0x0, 0x0000, 0, 0, 1);
}








int main(int argc, char *argv[])
{
	int i, j, k;
	int do_configuration = 0;
	uint16_t configuration_bits = 0;
	int configuration_address = 0;
	data = (uint16_t*)calloc(sizeof(uint16_t), 131072);
	data_diff64 = (unsigned char*)calloc(1, 131072);
	data_bytes = (unsigned char*)data;
	char string[1024];
	char program_path[1024];
	char orig_path[1024];
	int page;
// Top address in bytes
	int incremental = 0;
	int do_read = 0;
	int do_debugger = 0;

	if(argc < 2)
	{
		printf(" -r <file> - verify against original\n");
		printf(" -i <file> - program only changes\n");
		printf(" -d - enter debugging mode (bootloader only)\n");
		printf(" -b <-rid> [file] - use the bootloader to perform the operations instead of the programmer\n");
		printf(" -c <address> <bits> -  program 2 bytes of configuration data\n");
		printf("Examples:\n");
		printf("Program the configuration register at 0x300000 with 00000000 and\n");
		printf("0x300001 with 11001111 using the programming harness.\n");
		printf("    programmer -c 0x300000 1100111100000000\n");
		printf("Upload the program using the programming harness.\n");
		printf("    programmer trapezoidian.hex\n");
		printf("Upload the program using the bootloader.\n");
		printf("    programmer -b trapezoidian.hex\n");
		printf("Upload the program incrementally on the programming harness.\n");
		printf("    programmer -i trapezoidian.hex\n");
		printf("Upload the program incrementally on the bootloader.\n");
		printf("    programmer -b -i trapezoidian.hex\n");
		printf("Verify the program on the programming harness.\n");
		printf("    programmer -r trapezoidian.hex\n");
		printf("Verify the program on the bootloader.\n");
		printf("    programmer -b -r trapezoidian.hex\n");
		printf("Run the program and dump the PGD output.\n");
		printf("    programmer -d\n");
		exit(1);
	}

	delay = DELAY1;


	for(i = 1; i < argc; i++)
	{
		if(!strcmp(argv[i], "-c"))
		{
			do_configuration = 1;
			do_program = 0;
			if(i + 2 >= argc)
			{
				printf("Need address and bits for configuration programming.\n");
				exit(1);
			}
			if(strlen(argv[i + 2]) != 16)
			{
				printf("Configuration bits must be 16 bits\n");
				exit(1);
			}
			sscanf(argv[i + 1], "%x", &configuration_address);
			for(j = 0, k = 0x8000; j < 16; j++, k >>= 1)
			{
				if(argv[i + 2][j] == '1') configuration_bits |= k;
			}
			i += 2;
		}
		else
		if(!strcmp(argv[i], "-r"))
		{
			do_read = 1;
			do_program = 0;
		}
		else
		if(!strcmp(argv[i], "-b"))
		{
			use_bootloader = 1;
			delay = DELAY2;
		}
		else
		if(!strcmp(argv[i], "-i"))
		{
			incremental = 1;
			do_program = 0;
		}
		else
		if(!strcmp(argv[i], "-d"))
		{
			use_bootloader = 1;
			do_debugger = 1;
			do_program = 0;
		}
		else
		{
			strcpy(program_path, argv[i]);
			FILE *in = fopen(program_path, "r");
			if(!in)
			{
				perror("fopen");
				exit(1);
			}

			page = 0;
			
// Top address in bytes
			max_byte = 0;
			min_address = 0xffff;

			while(fgets(string, 1024, in))
			{
// Convert string to binary
				unsigned char string2[1024];
				unsigned char *in_ptr = string + 1;
				unsigned char *out_ptr = string2;
				for(j = 0; j < strlen(string); j += 2)
				{
					int character = *in_ptr++;
					if(character >= '0' && character <= '9')
						*out_ptr = (character - '0') << 4;
					else
						*out_ptr = (10 + character - 'A') << 4;

					character = *in_ptr++;
					if(character >= '0' && character <= '9')
						*out_ptr |= character - '0';
					else
						*out_ptr |= 10 + character - 'A';
					out_ptr++;
				}


				unsigned char *ptr = string2;

// Number of bytes of data in the line
				int data_bytes = *ptr++;
				if(!data_bytes) break;

// Starting address of data
				int address = (*ptr++) << 8;
				address |= *ptr++;

				int type = *ptr++;

// Data is the number of a page
				if(type == 4)
				{
					page = (*ptr++) << 8;
					page |= *ptr++;
					if(page != 0)
					{
						printf("page = %d  Don't know what to do.\n", page);
					}
				}
				else
// Data is program data
				{
					if(min_address > address)
						min_address = address;
					for(j = 0; j < data_bytes; j += 2)
					{
						uint16_t word = *ptr++;
						word |= (*ptr++) << 8;
						data[address >> 1] = word;
						address += 2;
					}
					if(max_byte < address)
						max_byte = address;
				}

// Checksum
				ptr++;
			}
		}
	}

	int total_bytes = max_byte - min_address;
	int current_bytes = 0;

	sprintf(orig_path, "%s.orig", program_path);

	char *address_changed = calloc(1, total_bytes / 2);
	struct sched_param params;
	params.sched_priority = 1;
	sched_setscheduler(0, SCHED_RR, &params);

	signal(SIGINT, signal_entry);


	pin_init_user(LPT1);
	pin_output_mode(LP_DATA_PINS | LP_SWITCHABLE_PINS);


	if(!use_bootloader)
	{
// Start charge pump
		pthread_create(&charge_pump, 0, charge_pump_thread, 0);
// Wait for charge buildup
		printf("Starting charge pump\n");
		sleep(CHARGE_PUMP_DELAY);
	}


	if(do_configuration)
	{
		program_configuration(configuration_address, configuration_bits);
	}


	if(do_read)
	{
		verify(max_byte);
	}
	else
	if(incremental)
	{
		write_incremental(orig_path);
	}

	if(do_program)
	{
// Write original copy
		printf("Writing original to %s\n", orig_path);
		FILE *orig = fopen(orig_path, "w");
		fwrite(data, 1, max_byte, orig);
		fclose(orig);


		write_program();
	}

	if(!use_bootloader)
	{
	 	pthread_cancel(charge_pump);
		pthread_join(charge_pump, 0);
	}

	if(do_debugger)
	{
		dump_pgd();
	}


	reset_pins();
}

