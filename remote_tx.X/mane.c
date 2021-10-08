/*
 * REMOTE CONTROL SHUTTER
 * Copyright (C) 2021 Adam Williams <broadcast at earthling dot net>
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 * 
 */





// remote control transmitter using XBee
// sends the value of PORTA to the serial port until it gets an ACK
// with a matching state











// PIC18LF1320 Configuration Bit Settings

// 'C' source line config statements

// CONFIG1H
#pragma config OSC = INTIO2     // Oscillator Selection bits (Internal RC oscillator, port function on RA6 and port function on RA7)
#pragma config FSCM = ON        // Fail-Safe Clock Monitor Enable bit (Fail-Safe Clock Monitor enabled)
#pragma config IESO = ON        // Internal External Switchover bit (Internal External Switchover mode enabled)

// CONFIG2L
#pragma config PWRT = ON        // Power-up Timer Enable bit (PWRT enabled)
#pragma config BOR = ON         // Brown-out Reset Enable bit (Brown-out Reset enabled)
#pragma config BORV = 27        // Brown-out Reset Voltage bits (VBOR set to 2.7V)

// CONFIG2H
#pragma config WDT = ON         // Watchdog Timer Enable bit (WDT enabled)
#pragma config WDTPS = 32768    // Watchdog Timer Postscale Select bits (1:32768)

// CONFIG3H
#pragma config MCLRE = ON       // MCLR Pin Enable bit (MCLR pin enabled, RA5 input pin disabled)

// CONFIG4L
#pragma config STVR = ON        // Stack Full/Underflow Reset Enable bit (Stack full/underflow will cause Reset)
#pragma config LVP = OFF        // Low-Voltage ICSP Enable bit (Low-Voltage ICSP disabled)

// CONFIG5L
#pragma config CP0 = OFF        // Code Protection bit (Block 0 (00200-000FFFh) not code-protected)
#pragma config CP1 = OFF        // Code Protection bit (Block 1 (001000-001FFFh) not code-protected)

// CONFIG5H
#pragma config CPB = OFF        // Boot Block Code Protection bit (Boot Block (000000-0001FFh) not code-protected)
#pragma config CPD = OFF        // Data EEPROM Code Protection bit (Data EEPROM not code-protected)

// CONFIG6L
#pragma config WRT0 = OFF       // Write Protection bit (Block 0 (00200-000FFFh) not write-protected)
#pragma config WRT1 = OFF       // Write Protection bit (Block 1 (001000-001FFFh) not write-protected)

// CONFIG6H
#pragma config WRTC = OFF       // Configuration Register Write Protection bit (Configuration registers (300000-3000FFh) not write-protected)
#pragma config WRTB = OFF       // Boot Block Write Protection bit (Boot Block (000000-0001FFh) not write-protected)
#pragma config WRTD = OFF       // Data EEPROM Write Protection bit (Data EEPROM not write-protected)

// CONFIG7L
#pragma config EBTR0 = OFF      // Table Read Protection bit (Block 0 (00200-000FFFh) not protected from table reads executed in other blocks)
#pragma config EBTR1 = OFF      // Table Read Protection bit (Block 1 (001000-001FFFh) not protected from table reads executed in other blocks)

// CONFIG7H
#pragma config EBTRB = OFF      // Boot Block Table Read Protection bit (Boot Block (000000-0001FFh) not protected from table reads executed in other blocks)

// #pragma config statements should precede project file includes.
// Use project enums instead of #define for ON and OFF.

#include <xc.h>
#include <pic18lf1320.h>
#include <stdint.h>

#define CLOCKSPEED 8000000
#define XBEE_BAUD 115200
#define DELAY -40000
#define LED_LAT LATAbits.LATA6
#define LED_TRIS TRISAbits.TRISA6
// lowest bits must be 00
#define SALT 0b11011000

typedef union 
{
	struct
	{
		unsigned interrupt_complete : 1;
        unsigned got_ack : 1;
	};
	
	unsigned char value;
} flags_t;

flags_t flags;
uint8_t serial_in = 0x00;
uint8_t value = 0xff;
uint8_t prev_value = 0xff;
// last code sent
uint8_t code = 0x00;

void idle();
void read_sync();

void (*tx_state)() = idle;
void (*rx_state)() = read_sync;

void send_data()
{
    code = PORTA;
    code &= 0x3;
    code |= code << 2;
    code |= code << 4;
    code ^= SALT;
    TXREG = code;
    tx_state = idle;
}


void send_sync()
{
    TXREG = 0xff;
    tx_state = send_data;
}

void idle()
{
}

void read_ack()
{
    if(serial_in == code)
    {
// DEBUG. Don't stop transmitting for testing.
        flags.got_ack = 1;
    }
    rx_state = read_sync;
}

void read_sync()
{
    if(serial_in == 0xff)
    {
        rx_state = read_ack;
    }
}

void main()
{
    OSCCON = 0b01110000;
// digital mode
    ADCON1 = 0xff;
// input mode
    TRISAbits.TRISA0 = 1;
    TRISAbits.TRISA1 = 1;

// LED output
    LED_TRIS = 0;
    LED_LAT = 1;
    
    flags.value = 0;
    
    RCSTA = 0b10010000;
    TXSTA = 0b00100100;
    BAUDCTL = 0b00001000;
// fudged baud code
    SPBRG = (CLOCKSPEED / (XBEE_BAUD * 4));
// enable interrupt
    PIR1bits.RCIF = 0;
    PIE1bits.RCIE = 1;

// beacon timer  1:2 prescaler
// always send another packet or turn LED back on after 1 beacon period
    T0CON = 0b10000000;
    TMR0 = DELAY;
// enable interrupt
    INTCONbits.TMR0IF = 0;
    INTCONbits.TMR0IE = 1;
    INTCONbits.PEIE = 1;
    INTCONbits.GIE = 1;

    while(1)
    {
        ClrWdt();
        if(PIR1bits.TXIF)
        {
            tx_state();
        }
    }
}


void __interrupt(low_priority) isr1()
{
}

void __interrupt(high_priority) isr()
{
    flags.interrupt_complete = 0;
// DEBUG
//LED_LAT = !LED_LAT;
	while(!flags.interrupt_complete)
	{
		flags.interrupt_complete = 1;

        if(INTCONbits.TMR0IF)
		{
            INTCONbits.TMR0IF = 0;
            TMR0 = DELAY;
            flags.interrupt_complete = 0;
            prev_value = value;
            value = PORTA & 0x3;
// compare the latest port value to the last one
            if(value != prev_value ||
                !flags.got_ack)
            {
                flags.got_ack = 0;
                prev_value = value;

// send a new beacon
                tx_state = send_sync;
                LED_LAT = !LED_LAT;
            }
            else
            {
// turn LED back on
                if(!LED_LAT)
                {
                    LED_LAT = 1;
                }
            }
        }
        
        
        if(PIR1bits.RCIF)
        {
            flags.interrupt_complete = 0;
            serial_in = RCREG;
            PIR1bits.RCIF = 0;
            rx_state();
        }
        
    }
}















