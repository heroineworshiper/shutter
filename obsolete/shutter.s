; shutter driver program for 18f458

; Sends the following signals in a loop.
;
; Send mirror lockup pulse for 1 second if mirror lockup enabled.
; Delay (1) to settle down.
;
; Pull down PNP transistor to open the shutter.
; Delay (2) to expose sensor.
; Raise PNP transistor to close the shutter
; Delay (3) to store exposure.


; If bulb mode is used, the driver determines the shutter speed.
; Automatic shutter speeds override the program but delays 2 & 3 must be
; long enough to let the automatic shutter finish or the driver will
; lose sync.
; If using automatic shutter speeds, delay 2 can be used as a delay between
; exposures.
;
; 
; Delay values are selected using dip switches:
;
; Mirror lockup delays ( bits 7, 6 ):
; 00 - 0 seconds/disabled
; 01 - 3 seconds
; 10 - 5 seconds
; 11 - 10 seconds
;
;
; Exposure delay values ( bits 5, 4, 3, 2 ):
; 0000 - continuous
; 0001 - 1 seconds
; 0010 - 2 seconds
; 0011 - 5 seconds
; 0100 - 10 seconds
; 0101 - 15 seconds
; 0110 - 30 seconds
; 0111 - 45 seconds
; 1000 - 60 seconds
; 1001 - 2 minutes
; 1010 - 5 minutes
; Flash storage/between frame delays ( bits 1, 0 ):
; 00 - 1 second Canon requires shutter release delay.
; 01 - 5 seconds
; 10 - 10 seconds
; 11 - 30 seconds
;
;
; Sample dip switch settings:
; 3 second mirror lock
; 30 second exposure
; 1 second storage
;
; 10010100


; Parts:
; The remote controls from Canon are $50 - $133.
; 2 way switch - mode switch.                                       $1
; 2 way switch - light meter switch.                                $1
; momentary pushbutton to bypass microprocessor                     $1
; 8 bit dip switch to set delays                                    $1.10
; N type connector                                               $$$$50

; Pins for 18F1220:
; RB0 - mirror lockup enable
; RB1 - shutter output
; RA6 - mirror delay bit
; RA7 - mirror delay bit
; RA0 - exposure delay bit
; RA1 - exposure delay bit
; RA2 - exposure delay bit
; RA3 - exposure delay bit
; RB2 - storage delay bit
; RB3 - storage delay bit


; All programming can be done in the programming harness.
; First program the configuration registers to use the internal oscillator.
; 	programmer -c 0x300000 1100100000000000
; Second install the bootloader.
; 	programmer bootloader.hex
; Third install the shutter program.
; 	programmer -b shutter.hex

#include "util.inc"


; registers
CURRENT_OPERATION = H'00'
; values for current operation
RAISE_MIRROR = H'01'
MIRROR_DELAY = H'02'
EXPOSURE_DELAY = H'03'
STORAGE_DELAY = H'04'

; 16 upper bits of current timer value in units of Fosc/4
CURRENT_TIME16 = H'01'

; bitmask value to be converted to a time
BITMASK_VALUE = H'05'

INTERRUPT_DONE = H'06'


	ORG PROGRAM_START
	goto start

	ORG PROGRAM_INTERRUPTHI
	goto interrupt
	
	ORG PROGRAM_INTERRUPTLO
	goto interrupt





; tables of times for each bitmask
; big endian for readability
; based on Fosc / 4 of 2Mhz
; Since the hardware timers count up, 
; the table is calculated by subtracting the time from 0x100000000
mirror_delays:
; 00 - 0 seconds/disabled
	DATA_BE H'ffff', H'0000'
; 01 - 3 seconds
    DATA_BE H'ffa4', H'7280'
; 10 - 5 seconds
	DATA_BE H'ff67', H'6980'
; 11 - 10 seconds
	DATA_BE H'fece', H'd300'







exposure_delays:
; 0000 - continuous
	DATA_BE H'0000', H'0000'
; 0001 - 1 second
	DATA_BE H'ffe1', H'7b80'
; 0010 - 2 seconds
	DATA_BE H'ffc2', H'f700'
; 0011 - 5 seconds
	DATA_BE H'ff67', H'6980'
; 0100 - 10 seconds
	DATA_BE H'fece', H'd300'
; 0101 - 15 seconds
	DATA_BE H'fe36', H'3c80'
; 0110 - 30 seconds
	DATA_BE H'fc6c', H'7900'
; 0111 - 45 seconds
	DATA_BE H'faa2', H'b580'
; 1000 - 1 minute
	DATA_BE H'f8d8', H'f200'
; 1001 - 2 minutes
	DATA_BE H'f1b1', H'e400'
; 1010 - 5 minutes
	DATA_BE H'dc3c', H'ba00'







storage_delays:
; 00 - 1 second
	DATA_BE H'ffe1', H'7b80'
; 01 - 5 seconds
	DATA_BE H'ff67', H'6980'
; 10 - 10 seconds
	DATA_BE H'fece', H'd300'
; 11 - 30 seconds
	DATA_BE H'fc6c', H'7900'










; set the starting operation based on mirror lockup delay
; no lockup delay disables the mirror lockup pulse
reset_operation:
	SET_REGISTER CURRENT_OPERATION, RAISE_MIRROR
	btfsc PORTA, D'6'
	return
	btfsc PORTA, D'7'
	return
	SET_REGISTER CURRENT_OPERATION, EXPOSURE_DELAY
	return












; convert the bitmask value to a timer value
; and store it in the timer
bitmask_to_timer:
; add current bitmask
	movf BITMASK_VALUE, W
	addwf TBLPTRL, F
	movlw H'00'
	addwfc TBLPTRH, F

; read bytes with post increment
	tblrd *+
	movff TABLAT, CURRENT_TIME16 + D'0'
	tblrd *+
	movf TABLAT, W
	movff TABLAT, CURRENT_TIME16 + D'1'
	tblrd *+
	movf TABLAT, W
	movff TABLAT, CURRENT_TIME16 + D'2'
	tblrd *+
	movf TABLAT, W
	movff TABLAT, CURRENT_TIME16 + D'3'


; must write timer high byte first
	movff CURRENT_TIME16 + D'3', TMR0H
	movff CURRENT_TIME16 + D'2', TMR0L


; debug
;	BOOTLOADER_WRITE_REG BITMASK_VALUE
;	BOOTLOADER_WRITE_REG16 CURRENT_TIME16
	return









; do the current operation, update the timer, and advance operation
do_operation:
; shutter on, set timer to minimum
	SKIP_EQUAL_LITERAL CURRENT_OPERATION, RAISE_MIRROR
	goto do_operation2

; don't enable if powering down
		bcf PORTB, D'1'
		btfsc PORTB, D'0'
			bsf PORTB, D'1'

; send required 1 second pulse for mirror lockup
		SET_REGISTER16 CURRENT_TIME16, H'ffe1'
		SET_TIMER_REGISTER TMR0L, H'7b80'

		incf CURRENT_OPERATION, F
		return







do_operation2:
; shutter of, set timer to mirror delay
	SKIP_EQUAL_LITERAL CURRENT_OPERATION, MIRROR_DELAY
	goto do_operation4

		bcf PORTB, D'1'
		clrf BITMASK_VALUE
; get bitmask from pins
; shift left 2 bits to correspond to table offset in memory
		btfsc PORTA, D'6'
			bsf BITMASK_VALUE, D'3'
		btfsc PORTA, D'7'
			bsf BITMASK_VALUE, D'2'
; write start address of bitmask table
		clrf TBLPTRU
		SET_REGISTER16 TBLPTRL, mirror_delays

; translate bitmask to timer value
		call bitmask_to_timer

		incf CURRENT_OPERATION, F
		return



do_operation4:
; shutter on, set timer to exposure delay
	SKIP_EQUAL_LITERAL CURRENT_OPERATION, EXPOSURE_DELAY
	goto do_operation5

; don't enable if powering down
		bcf PORTB, D'1'
		btfsc PORTB, D'0'
			bsf PORTB, D'1'


		clrf BITMASK_VALUE
; get bitmask from pins
; shift left 2 bits to correspond to table offset in memory
		btfsc PORTA, D'0'
			bsf BITMASK_VALUE, D'5'
		btfsc PORTA, D'1'
			bsf BITMASK_VALUE, D'4'
		btfsc PORTA, D'2'
			bsf BITMASK_VALUE, D'3'
		btfsc PORTA, D'3'
			bsf BITMASK_VALUE, D'2'
; write start address of bitmask table
		clrf TBLPTRU
		SET_REGISTER16 TBLPTRL, exposure_delays

; translate bitmask to timer value
		call bitmask_to_timer

		incf CURRENT_OPERATION, F
		return


do_operation5:
; if continuous mode, do nothing.  User must power down
		movlw H'0f'
		andwf PORTA, W
		bnz shutter_off
			return

shutter_off:
; shutter off, set timer to storage delay
	bcf PORTB, D'1'



	clrf BITMASK_VALUE
; get bitmask from pins
; shift left 2 bits to correspond to table offset in memory
	btfsc PORTB, D'2'
		bsf BITMASK_VALUE, D'3'
	btfsc PORTB, D'3'
		bsf BITMASK_VALUE, D'2'
; write start address of bitmask table
	clrf TBLPTRU
	SET_REGISTER16 TBLPTRL, storage_delays

; translate bitmask to timer value
	call bitmask_to_timer


	
; go to first operation on next iteration
	call reset_operation
	return






handle_timer:
; reset timer overflow
	bcf INTCON, TMR0IF         
	clrf INTERRUPT_DONE

; increase most significant bits and skip if timeout
	infsnz CURRENT_TIME16, F
	incfsz CURRENT_TIME16 + D'1', F
	return


; do next operation
		call do_operation
		return








start:
; configure pin modes
; pin mode: 1 = digital  0 = analog
	SET_REGISTER ADCON1, B'11111111'
; set shutter high
	SET_REGISTER PORTB, B'00000000'
; 1 = input  0 = output
	SET_REGISTER TRISA, B'11111111'
	SET_REGISTER TRISB, B'11111101'




; set sleep mode to idle to keep the timers running
	bsf OSCCON, IDLEN

; set first operation based on mode
	call reset_operation

; perform the operation
	call do_operation




; configure interrupts and timers
; set first pulse rise and activate timer (timer 0)
	SET_REGISTER T0CON, B'10001000'
	SET_TIMER_REGISTER TMR0L, H'0000'
; enable interrupts
; enable timer 0 interrupt
	SET_REGISTER INTCON, B'11100000'




loop:
	sleep
	clrwdt
	goto loop



interrupt:
	clrwdt
	setf INTERRUPT_DONE

	btfsc INTCON, TMR0IF
		call handle_timer

	btfss INTERRUPT_DONE, D'0'
	goto interrupt

		retfie





END




