// Very small ATMega328 intervalometer originally intended 
// to attach to the Gear360.
// It has a serial port for a future VT100 interface.


// compile with make gear360
// program fuses: make gear360_fuse
// program with make gear360_isp




// connect PB4 to PB5 to activate the shutter




#include "gear360.h"
#include "avr_debug.h"
#include <string.h> // memcpy


// pins
#define LED_PIN1 2
#define LED_PIN2 3
#define LED_PORT PORTD
#define LED_TRIS DDRD

#define SHUTTER_PIN 1
#define SHUTTER_PORT PORTC
#define SHUTTER_TRIS DDRC

#define ENABLE_PIN1 5
#define ENABLE_PIN2 4
#define ENABLE_PORT PORTB
#define ENABLE_IN PINB
#define ENABLE_TRIS DDRB


#define HZ 122
#define OFF_TIME (HZ / 8)
#define ON_TIME (HZ / 8)


#define IDLE 0
#define OFF 1
#define ON 2
uint8_t state = IDLE;
uint16_t time = 0;


void led_on()
{
// set TRIS bit to enable output
	bitClear(LED_PORT, LED_PIN1);
	bitSet(LED_TRIS, LED_PIN1);

	bitSet(LED_PORT, LED_PIN2);
	bitSet(LED_TRIS, LED_PIN2);
}

void led_off()
{
	bitClear(LED_PORT, LED_PIN1);
	bitSet(LED_TRIS, LED_PIN1);

	bitClear(LED_PORT, LED_PIN2);
	bitSet(LED_TRIS, LED_PIN2);
}

void shutter_on()
{
	bitSet(SHUTTER_TRIS, SHUTTER_PIN);
}

void shutter_off()
{
	bitClear(SHUTTER_TRIS, SHUTTER_PIN);
}

ISR(TIMER1_OVF_vect)
{
//	bitToggle(DEBUG_PORT, DEBUG_PIN);
	switch(state)
	{
		case IDLE:
// user activated it
			if(!bitRead(ENABLE_IN, ENABLE_PIN1))
			{
				state = OFF;
				time = 0;

			}
			break;
		
		case OFF:
// user deactivated it
			if(bitRead(ENABLE_IN, ENABLE_PIN1))
			{
				state = IDLE;
			}
			else
			{
				time++;
				if(time >= OFF_TIME)
				{
					time = 0;
					state = ON;
					shutter_on();
					led_on();
				}
			}
			break;
		
		case ON:
// user deactivated it
			if(bitRead(ENABLE_IN, ENABLE_PIN1))
			{
				state = IDLE;
				shutter_off();
				led_off();
			}
			else
			{
				time++;
				if(time >= ON_TIME)
				{
					state = OFF;
					time = 0;
					shutter_off();
					led_off();
					
				}
			}
			break;
			
	}


}





int main()
{
// this doesn't work
	asm("wdr");
	WDTCSR |= 0x18;
	WDTCSR = 0;

// verify the prescaler is 0
	CLKPR = 0x80;
	CLKPR = 0x00;

	init_serial();
	print_text("Welcome to the intervalometer\n");

//	bitSet(DDRC, DEBUG_PIN);
//	bitSet(DEBUG_PORT, DEBUG_PIN);

// clear bit to enable input
// shutter pin
	bitClear(SHUTTER_TRIS, SHUTTER_PIN);
	bitClear(SHUTTER_PORT, SHUTTER_PIN);

// set bit to enable output
// LED pin
	led_off();

// pullup enable pin
	bitClear(ENABLE_TRIS, ENABLE_PIN1);
	bitSet(ENABLE_TRIS, ENABLE_PIN2);
	bitSet(ENABLE_PORT, ENABLE_PIN1);
	bitClear(ENABLE_PORT, ENABLE_PIN2);




// enable the timer
	TCCR1B = (1 << CS10);
// enable interrupt
	TIMSK1 = (1 << TOIE1);



// enable interrupts
	sei();
	
	while(1)
	{
// reset the watchdog
		asm("wdr");
		handle_serial();
//		bitToggle(DEBUG_PORT, DEBUG_PIN);
		
		
		
	}
	
}








