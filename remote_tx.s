; remote control transmitter using 900Mhz XBee
; just sends the value of PORTA to the serial port

; 18f1320
; SET ALL DIP SWITCHES TO OFF
; ../copter/usb_programmer -p 18f1320 -c 0x300000 1100100100000000
; Internal oscillator
; ../copter/usb_programmer -p 18f1320 -c 0x300002 0001111100001010
; brownout protection
; ../copter/usb_programmer -p 18f1320 -c 0x300006 0000000010000001
; low voltage programming disabled
; make remote
; ../copter/usb_programmer -p 18f1320 remote_tx.hex
; ../copter/usb_programmer -p 18f1320 -r remote_tx.hex


PROCESSOR 18f1320
#include "p18f1320.inc"
#include "util.inc"


#define CLOCKSPEED 8000000
#define XBEE_BAUD 115200
#define BAUD_RATE_CODE (CLOCKSPEED / (XBEE_BAUD * 4) - 1)
#define DELAY -4000


	VARSTART H'00', H'100'
	VARADD CURRENT_OPERATION, 2
	VARADD TEMP, 2


	ORG RESETVECTOR
	goto start

	ORG INTVECTORHI
	goto start

	ORG INTVECTORLO
	goto start




start:
	BANKSEL H'00'
	SET_REGISTER OSCCON, B'01110000'
; pin modes
; pin mode: 1 = digital  0 = analog
	SET_REGISTER ADCON1, B'11111111'

	SET_REGISTER16 CURRENT_OPERATION, send_sync1
	SET_REGISTER RCSTA, B'10000000'
	SET_REGISTER TXSTA, B'00100100'
	SET_REGISTER BAUDCTL, B'00001000'
	SET_REGISTER16 SPBRG, BAUD_RATE_CODE


loop:
	clrwdt

	btfsc PIR1, TXIF
		call handle_operation

	bra loop

handle_operation:
	SET_PC_REG CURRENT_OPERATION

send_sync1:
	SET_REGISTER TXREG, 0xff
	SET_REGISTER16 CURRENT_OPERATION, send_sync2
	return


send_sync2:
	SET_REGISTER TXREG, 0xe9
	SET_REGISTER16 CURRENT_OPERATION, send_byte1
	return

send_byte1:
	COPY_REGISTER TEMP, PORTA
	AND_LITERAL TEMP, b'00000011'
	COPY_REGISTER TXREG, TEMP
	SET_REGISTER16 CURRENT_OPERATION, send_byte2
	return

send_byte2:
	COPY_REGISTER TXREG, TEMP
	SET_REGISTER16 TEMP, DELAY
	SET_REGISTER16 CURRENT_OPERATION, delay1
	return
	
delay1:
	INC16 TEMP
	SKIP_ZERO16 TEMP
	return
		SET_REGISTER16 CURRENT_OPERATION, send_sync1
		return


END
















