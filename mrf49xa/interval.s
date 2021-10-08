; intervalometer transmitter

; 18f26k20 setup:
; usb_programmer -p 18f26k20 -e
; chip erase
; usb_programmer -p 18f26k20 -c 0x300000 0000100000000000
; internal oscillator
; usb_programmer -p 18f26k20 -c 0x300002 0001111100011110
; brown out protection
; usb_programmer -p 18f26k20 -c 0x300006 0000000010000001
; PGM pin disabled

; usb_programmer -p 18f26k20 interval.hex
; usb_programmer -p 18f26k20 -r interval.hex



PROCESSOR 18f26k20
#include "p18f26k20.inc"
#include "pic_util.inc"

#define CLOCKSPEED 250000
#define IS_INTERVAL
#define BAUD 9600
; timer1 wraparounds per second
#define HZ 200

#include "interval.inc"
#include "interval_disp.inc"
#include "interval_state.inc"
#include "lcd.inc"
#include "interval_buttons.inc"

; TIMER1 wraparounds between beacons
; each packet is 3 sync codes + 2 data bytes
;#define BEACON_COUNT 12
#define BEACON_COUNT 2

#define SHUTTER_PIN 7
#define METER_PIN 6
#define OUTPUT_LAT LATB
#define OUTPUT_TRIS TRISB



; bits in FLAGS
cblock 0x0
	TIMER1_WRAP : 1
	INTERRUPT_COMPLETE : 1
	SEND_BEACON : 1
	INTERVAL_ON : 1
; intervalometer states
	INTERVAL_MIRROR : 1
	INTERVAL_SHUTTER : 1
	INTERVAL_STORAGE : 1
endc






	VARSTART H'00', H'100'
;  current toggleable operations
	VARADD FLAGS, 1
	RADIO_VARS
	BUTTON_VARS
	LCD_VARS
	STATE_VARS
	DISPLAY_VARS
	VARADD BEACON_COUNTER, 1
; number of bytes in packet left
	VARADD PACKET_BYTES, 1
; count seconds for the current operation
	VARADD SECONDS_COUNTER, 2
; shuts down radio during idle activity
	VARADD IDLE_TIMER, 2
	VARADD TEMP0, 4


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
	SET_TIMER_REGISTER TMR1L, TIMER1_DELAY
; enable interrupt
	bcf PIR1, TMR1IF
	bsf PIE1, TMR1IE

	call init_uart
	call init_buttons
	call init_lcd
	call init_radio
	call init_state
	call init_display
	RADIO_SLEEP

	clrf BEACON_COUNTER

; enable interrupts
	bsf INTCON, PEIE
	bsf INTCON, GIE


; Main loop
loop:
	clrwdt

	call handle_buttons
	call handle_state


	btfsc FLAGS, TIMER1_WRAP
	call handle_timer1_upper

	RADIO_LOOP

;SET_REGISTER TXREG, 0x12
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
	bra handle_timer1_upper1
		INC16 IDLE_TIMER
handle_timer1_upper1:

	call handle_lcd
	call handle_display

	incf BEACON_COUNTER, F
	SKIP_GREATEREQUAL_LITERAL BEACON_COUNTER, BEACON_COUNT
	return

; time to send a beacon
		SET_REGISTER PACKET_BYTES, RADIO_PACKET_SIZE
		clrf BEACON_COUNTER

; if tethered mode, don't send & put radio in sleep mode
		btfss USE_RADIO, 0
		bra handle_radio_send2

; if radio sleeping, don't send
		btfss RADIO_STATE_BITS, RADIO_SLEEP
		bsf FLAGS, SEND_BEACON

; if shutter active, send beacon
		btfss OUTPUT_TRIS, SHUTTER_PIN
		bsf FLAGS, SEND_BEACON

; if meter active send beacon
		btfss OUTPUT_TRIS, METER_PIN
		bsf FLAGS, SEND_BEACON
		return



handle_radio_send:
	btfss FLAGS, SEND_BEACON
	return


		RADIO_WAKE

; copy pin state
		setf TEMP0
		COPY_BIT TEMP0, 0, OUTPUT_TRIS, SHUTTER_PIN
		COPY_BIT TEMP0, 1, OUTPUT_TRIS, METER_PIN
; send byte
		WRITE_RADIO_REG TEMP0
; debug
;			COPY_REGISTER TXREG, TEMP0

; if inactive, the variable is 0xff
		SKIP_EQUAL_LITERAL TEMP0, 0xff
		call reset_idle_timer

		decfsz PACKET_BYTES, F
		return

; packet done
			bcf FLAGS, SEND_BEACON


; if idle timeout reached, shut down radio
			SKIP_LESS_LITERAL16 IDLE_TIMER, RADIO_TIMEOUT
			bra handle_radio_send2
				return

handle_radio_send2:
	RADIO_SLEEP
	return

reset_idle_timer:
	CLEAR_REGISTER16 IDLE_TIMER
	return


handle_radio_receive:
	return



#include "pic_mrf49xa.s"
#include "interval_state.s"
#include "lcd.s"
#include "interval_disp.s"
#include "interval_buttons.s"


END


