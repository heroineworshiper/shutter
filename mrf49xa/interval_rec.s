; intervalometer receiver

; 18f14k50 setup:
; usb_programmer -p 18f14k50 -e
; chip erase
; usb_programmer -p 18f14k50 -c 0x300000 0010100000000000
; internal oscillator
; usb_programmer -p 18f14k50 -c 0x300002 0001111100011110
; brown out protection
; usb_programmer -p 18f14k50 -c 0x300006 0000000000000001
; PGM pin disabled

; usb_programmer -p 18f14k50 interval_rec.hex
; usb_programmer -p 18f14k50 -r interval_rec.hex



PROCESSOR 18f14k50
include "p18f14k50.inc"
#include "pic_util.inc"

#define CLOCKSPEED 250000
#define BAUD 9600
;#define CLOCKSPEED 31000
;#define BAUD 300
; timer1 wraparounds per second
#define HZ 64

#include "interval.inc"



; bits in FLAGS
cblock 0x0
	TIMER1_WRAP : 1
	INTERRUPT_COMPLETE : 1
endc


#define SHUTTER_PIN 4
#define METER_PIN 5
#define OUTPUT_LAT LATC
#define OUTPUT_TRIS TRISC




	VARSTART H'00', H'100'
;  current toggleable operations
	VARADD FLAGS, 1
	RADIO_VARS
; incoming radio packet
	VARADD PACKET, 2
; shuts down output pins during dead radio
	VARADD IDLE_TIMER, 2
	VARADD TEMP0, 1


	ORG RESETVECTOR
	goto start



	ORG INTVECTORHI
	goto interrupt_handler

	ORG INTVECTORLO
	goto interrupt_handler



	ORG 0x200


start:
	BANKSEL H'00'
; oscillator frequency
	SET_REGISTER OSCCON, B'00010000'
	SET_REGISTER OSCTUNE, B'00000000'
	clrf ANSEL
	clrf ANSELH
	clrf FLAGS
	bsf OUTPUT_LAT, SHUTTER_PIN
	bsf OUTPUT_LAT, METER_PIN
	bsf OUTPUT_TRIS, SHUTTER_PIN
	bsf OUTPUT_TRIS, METER_PIN
	CLEAR_REGISTER16 IDLE_TIMER

; Timer 1 is for LCD refresh & buttons
	SET_REGISTER T1CON, B'10000001'
	CLEAR_TIMER_REGISTER TMR1L
; enable interrupt
	bcf PIR1, TMR1IF
	bsf PIE1, TMR1IE

	call init_uart
	
	call init_radio

; enable interrupts
	bsf INTCON, PEIE
	bsf INTCON, GIE

; Main loop
loop:
	clrwdt



	btfsc FLAGS, TIMER1_WRAP
	call handle_timer1_upper

	RADIO_LOOP

	bra loop



interrupt_handler:
	clrwdt
	bsf FLAGS, INTERRUPT_COMPLETE

	btfsc PIR1, TMR1IF
	call handle_timer1

	btfsc PIR1, RCIF
	call handle_uart_in

; loop again if interrupt was handled
	btfss FLAGS, INTERRUPT_COMPLETE
	bra interrupt_handler
; return after popping context from fast register stack
		retfie FAST


handle_timer1:
	bsf FLAGS, TIMER1_WRAP
	bcf PIR1, TMR1IF
; compensate for interrupt handler latency
	SET_TIMER_REGISTER TMR1L, TIMER1_DELAY + 16
	bcf FLAGS, INTERRUPT_COMPLETE

	return
	
	


handle_uart_in:
; clear interrupt
	movf RCREG, W

	bcf FLAGS, INTERRUPT_COMPLETE
	return





init_uart:
	SET_REGISTER TXSTA, B'00100100'
	SET_REGISTER RCSTA, B'10010000'
	SET_REGISTER BAUDCON, B'00001000'
	SET_REGISTER16 SPBRG, BAUD_RATE_CODE
; enable interrupt
	bcf PIR1, RCIF
	bsf PIE1, RCIE
	return



handle_timer1_upper:
	bcf FLAGS, TIMER1_WRAP


; increase idle timer
	SKIP_LESS_LITERAL16 IDLE_TIMER, RADIO_TIMEOUT
	bra handle_idle_timeout
		INC16 IDLE_TIMER
handle_timer1_upper1:



	return



; radio inactive too long.
; disable shutter pins
handle_idle_timeout:
	bsf OUTPUT_TRIS, SHUTTER_PIN
	bsf OUTPUT_TRIS, METER_PIN
	bra handle_timer1_upper1



handle_radio_send:
	return


handle_radio_receive:
; reset the idle timeout
	CLEAR_REGISTER16 IDLE_TIMER

; debug
;	COPY_REGISTER TXREG, RADIO_DATA + 1

	btfss RADIO_STATE_BITS, RADIO_PACKET_START
	bra handle_radio_receive2
; read 1st byte of packet
		READ_RADIO PACKET + 0
		return

; read 2nd byte of packet
handle_radio_receive2:
	READ_RADIO PACKET + 1

; 2 packet bytes must be identical
	SKIP_EQUAL PACKET + 0, PACKET + 1
	return

; must be 0xf0 | bits
		btfss PACKET + 1, 7
		return

			COPY_REGISTER TXREG, PACKET + 1
; copy state to shutter pins
			COPY_BIT OUTPUT_TRIS, SHUTTER_PIN, PACKET + 1, 0
			COPY_BIT OUTPUT_TRIS, METER_PIN, PACKET + 1, 1
	

			return


#include "pic_mrf49xa.s"


END


