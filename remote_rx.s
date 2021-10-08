; remote control transmitter using 900Mhz XBee
; just sets the value of LATA to the serial port

; 18f1320
; SET ALL DIP SWITCHES TO OFF
; ../copter/usb_programmer -p 18f1320 -c 0x300000 1100100100000000
; Internal oscillator
; ../copter/usb_programmer -p 18f1320 -c 0x300002 0001111100001010
; brownout protection
; ../copter/usb_programmer -p 18f1320 -c 0x300006 0000000010000001
; low voltage programming disabled
; make remote
; ../copter/usb_programmer -p 18f1320 remote_rx.hex
; ../copter/usb_programmer -p 18f1320 -r remote_rx.hex


PROCESSOR 18f1320
#include "p18f1320.inc"
#include "util.inc"


#define CLOCKSPEED 8000000
#define XBEE_BAUD 115200
#define BAUD_RATE_CODE (CLOCKSPEED / (XBEE_BAUD * 4) - 1)



	VARSTART H'00', H'100'
	VARADD CURRENT_OPERATION, 2
	VARADD TEMP, 1


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

	bcf LATA, 0
	bcf LATA, 1
	bcf TRISA, 0
	bcf TRISA, 1

	SET_REGISTER16 CURRENT_OPERATION, read_sync1
	SET_REGISTER RCSTA, B'10010000'
	SET_REGISTER TXSTA, B'00100100'
	SET_REGISTER BAUDCTL, B'00001000'
	SET_REGISTER16 SPBRG, BAUD_RATE_CODE


loop:
	clrwdt

	btfsc PIR1, RCIF
		call handle_operation

	bra loop

handle_operation:
	SET_PC_REG CURRENT_OPERATION

read_sync1:
	COPY_REGISTER TEMP, RCREG
	SKIP_EQUAL_LITERAL TEMP, 0xff
	return
		SET_REGISTER16 CURRENT_OPERATION, read_sync2
		return


read_sync2:
bcf TRISA, 3
btg LATA, 3
	COPY_REGISTER TEMP, RCREG
	SET_REGISTER16 CURRENT_OPERATION, read_sync1
	SKIP_EQUAL_LITERAL TEMP, 0xe9
	return
		SET_REGISTER16 CURRENT_OPERATION, read_byte1
		return

read_byte1:
	COPY_REGISTER TEMP, RCREG
	SET_REGISTER16 CURRENT_OPERATION, read_byte2
	return

read_byte2:
	SET_REGISTER16 CURRENT_OPERATION, read_sync1
	SKIP_EQUAL TEMP, RCREG
	return
	
		COPY_BIT LATA, 0, TEMP, 0
		COPY_BIT LATA, 1, TEMP, 1
		return
	



END















