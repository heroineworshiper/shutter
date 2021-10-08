#include "p18f1220.inc"



RESETVECTOR	= H'0000'
INTVECTORHI	= H'0008'
INTVECTORLO	= H'0018'

PROGRAM_START = H'200'
PROGRAM_INTERRUPTHI = H'208'
PROGRAM_INTERRUPTLO = H'210'




; data for the 18f1220 stores a 16 bit value
DATA_BE macro word1, word2
	DATA word1
	DATA word2
	ENDM

SET_REGISTER macro reg, literal
	movlw literal
	movwf reg
	ENDM

SET_REGISTER16 macro reglo, literal
	movlw LOW(literal)
	movwf reglo
	movlw HIGH(literal)
	movwf reglo + D'1'
	ENDM

; timer must be written high byte first or the high byte will be discarded
SET_TIMER_REGISTER macro reglo, literal
	movlw HIGH(literal)
	movwf reglo + D'1'
	movlw LOW(literal)
	movwf reglo
	ENDM

; the assembler doesn't seem to take 32 bit literals so we
; split the literal into shorts for this one
SET_REGISTER32 macro reglo, literal31_16, literal15_0
	movlw LOW(literal15_0)
	movwf reglo
	movlw HIGH(literal15_0)
	movwf reglo + D'1'
	movlw LOW(literal31_16)
	movwf reglo + D'2'
	movlw HIGH(literal31_16)
	movwf reglo + D'3'
	ENDM

; the assembler doesn't seem to take 32 bit literals so we
; split the literal into shorts for this one
SET_REGISTER48 macro reglo, literal47_32, literal31_16, literal15_0
	movlw HIGH(literal47_32)
	movwf reglo + D'5'
	movlw LOW(literal47_32)
	movwf reglo + D'4'
	movlw HIGH(literal31_16)
	movwf reglo + D'3'
	movlw LOW(literal31_16)
	movwf reglo + D'2'
	movlw HIGH(literal15_0)
	movwf reglo + D'1'
	movlw LOW(literal15_0)
	movwf reglo
	ENDM

; the assembler doesn't seem to take 32 bit literals so we
; split the literal into shorts for this one
SET_REGISTER64 macro reglo, literal63_48, literal47_32, literal31_16, literal15_0
	movlw LOW(literal15_0)
	movwf reglo
	movlw HIGH(literal15_0)
	movwf reglo + D'1'
	movlw LOW(literal31_16)
	movwf reglo + D'2'
	movlw HIGH(literal31_16)
	movwf reglo + D'3'
	movlw LOW(literal47_32)
	movwf reglo + D'4'
	movlw HIGH(literal47_32)
	movwf reglo + D'5'
	movlw LOW(literal63_48)
	movwf reglo + D'6'
	movlw HIGH(literal63_48)
	movwf reglo + D'7'
	ENDM

CLEAR_REGISTER48 macro reglo
	clrf reglo
	clrf reglo + D'1'
	clrf reglo + D'2'
	clrf reglo + D'3'
	clrf reglo + D'4'
	clrf reglo + D'5'
	ENDM

CLEAR_REGISTER32 macro reglo
	clrf reglo
	clrf reglo + D'1'
	clrf reglo + D'2'
	clrf reglo + D'3'
	ENDM

CLEAR_REGISTER16 macro reglo
	clrf reglo
	clrf reglo + D'1'
	ENDM

; clear a timer register
CLEAR_TIMER_REGISTER macro reglo
	clrf reglo + D'1'
	clrf reglo
	ENDM

; increment register by 1 without changing status bits
INC_REGISTER macro reg
	movf STATUS, W        ; store carry bit
	incf reg, F
	movwf STATUS          ; retrieve carry bit
	ENDM

; copy value of src register to dst register
COPY_REGISTER macro dst, src
	movff src, dst
	ENDM

; copy value of src register to dst register
COPY_REGISTER64 macro dst, src
	movff src, dst
	movff src + D'1', dst + D'1'
	movff src + D'2', dst + D'2'
	movff src + D'3', dst + D'3'
	movff src + D'4', dst + D'4'
	movff src + D'5', dst + D'5'
	movff src + D'6', dst + D'6'
	movff src + D'7', dst + D'7'
	ENDM

COPY_REGISTER32 macro dst, src
	movff src, dst
	movff src + D'1', dst + D'1'
	movff src + D'2', dst + D'2'
	movff src + D'3', dst + D'3'
	ENDM

; copy value of src register to dst register
COPY_REGISTER16 macro dst, src
	movff src, dst
	movff src + D'1', dst + D'1'
	ENDM;

; copy value of src register to dst register starting with the high byte
; required when setting timers
COPY_TIMER_REGISTER macro dst, src
	movff src + D'1', dst + D'1'
	movff src, dst
	ENDM;


; add literal to register and store in register
ADD_LITERAL macro reg, literal
	movlw literal
	addwf reg, F
	ENDM

; add reg1 to reg2 and store in reg1
ADD_REGISTER macro reg1, reg2
	movf reg2, W
	addwf reg1, F
	ENDM

; add 16 bit literal to 32 bit dstlo and store result in dst
ADD32_LITERAL macro dstlo, literal
	movlw (literal & H'ff')
	addwf dstlo, F
	btfsc STATUS, C
		incf dstlo + D'1', F ; overflow
		btfsc STATUS, C
			incf dstlo + D'2', F ; overflow
			btfsc STATUS, C
				incf dstlo + D'3', F ; overflow

	movlw (literal & H'ff00') >> 8
	addwf dstlo + D'1', F
	btfsc STATUS, C
		incf dstlo + D'2', F ; overflow
		btfsc STATUS, C
			incf dstlo + D'3', F ; overflow
	ENDM



; add 32 bit src to 32 bit dst and store result in dst
ADD_REGISTER32 macro dstlo, srclo
	movf srclo, W
	addwf dstlo, F
	addwfc dstlo + D'1', F
	addwfc dstlo + D'2', F
	addwfc dstlo + D'3', F

	movf srclo + D'1', W
	addwf dstlo + D'1', F
	addwfc dstlo + D'2', F
	addwfc dstlo + D'3', F

	movf srclo + D'2', W
	addwf dstlo + D'2', F
	addwfc dstlo + D'3', F

	movf srclo + D'3', W
	addwf dstlo + D'3', F
	ENDM




; subtract reg2 from reg1 and store result in reg1
SUBTRACT_REGISTER macro reg1, reg2
	movf reg2, W
	subwf reg1, F
	ENDM


; subtract 16 bit register srclo from dstlo and store result in dstlo
SUBTRACT16 macro dstlo, srclo
	movf srclo, W
	subwf dstlo, F
	btfss STATUS, C
		decf dstlo + D'1', F ; negative result
	movf srclo + D'1', W
	subwf dstlo + D'1', F
	ENDM


; subtract 32 bit srclo from dstlo and store result in dstlo
SUBTRACT32 macro dstlo, srclo
	movf srclo, W
	subwf dstlo, F
	btfss STATUS, C
		decf dstlo + D'1', F ; negative result
		btfss STATUS, C
			decf dstlo + D'2', F ; negative result
			btfss STATUS, C
				decf dstlo + D'3', F

	movf srclo + D'1', W
	subwf dstlo + D'1', F
	btfss STATUS, C
		decf dstlo + D'2', F ; negative result
		btfss STATUS, C
			decf dstlo + D'3', F ; negative result

	movf srclo + D'2', W
	subwf dstlo + D'2', F
	btfss STATUS, C
		decf dstlo + D'3', F ; negative result

	movf srclo + D'3', W
	subwf dstlo + D'3', F
	ENDM





; skip if the contents of the registers are equal
SKIP_EQUAL macro reg1, reg2
	movf reg1, W
	cpfseq reg2
	ENDM

; skip if the contents of the registers are not equal
SKIP_NOTEQUAL macro reg1, reg2
	movf reg1, W
	subwf reg2, W
	btfsc STATUS, Z
	ENDM

; skip if the register equals the literal
SKIP_EQUAL_LITERAL macro reg1, literal
	movlw literal
	cpfseq reg1, W
	ENDM

; skip if the register is not equal to the literal
SKIP_NOTEQUAL_LITERAL macro reg1, literal
	movlw literal
	subwf reg1, W
	btfsc STATUS, Z
	ENDM



; skip if the register contents are > literal
SKIP_GREATER_LITERAL macro reg, literal
; load register into W
	movf reg, W
; subtract register from literal and put result in W
	sublw literal
; if register was > literal, the result is negative
	btfss STATUS, N
	ENDM




; skip if the register contents are > literal
SKIP_GREATEREQUAL_LITERAL macro reg, literal
; subtract literal from register and put result in W
	subwf reg, W
; if result is negative, the N status is 1
	btfsc STATUS, N
	ENDM



; skip if reg1 < reg2
; temp must point to a 32 bit register
SKIP_LESS32 macro temp, reg1, reg2
	COPY_REGISTER32 temp, reg1
	SUBTRACT32 temp, reg2
	btfss temp + D'3', D'7' ; bit 31 is 1 if literal was > reg
	ENDM


; skip if register < literal
SKIP_LESS_LITERAL macro reg, literal
	movlw literal
	cpfslt reg
	ENDM



; skip if the register contents are <= than literal
SKIP_LESSEQUAL macro reg, literal
; load register into W
	movf reg, W
; subtract register from literal and put result in W
	sublw literal
; if register was <= literal, the result is not negative
	btfsc STATUS, N
	ENDM























; To be programmed into the microprocessor first while it's in the
; programming socket.
;
; Then the microprocessor is transferred to the circuit.
; PGC, PGD, and MCLR are connected to the programmer.
; The programmer is run with -b to program the firmware using the bootloader.
;
; The program must contain 3 jump points to which the bootloader passes
; through the 3 hardware vectors:
; 
; PROGRAM_START, PROGRAM_INTERRUPTHI, PROGRAM_INTERRUPTLO
;
; In the circuit we have:
; PGC - pulled down in the application and connected to programmer
; PGD - connected to programmer.  This is pulled down in the programmer.
; 
; The bootloader is programmed when the chip is in the programmer.  Then it runs
; when the chip is in the circuit.
; 
; On reset or interrupt, 
; the bootloader tests PGC and PGD for high.  If they're high it goes into 
; programming mode.  If either is low it jumps to the program's reset vectors.
; This allows the application to use interrupts and PGC to clock the debugger 
; without going into the bootloader.
;
; Mind you, the application can't use interrupt priorities since these would 
; trigger interrupts in the middle of debugger output, when the PGC and PGD pins
; are undetermined.
;
; The bootloader uses RAM starting from 0x7f and working backwards for its
; variables. 
;
; The programmer recieves commands by debouncing and reading PGC and
; PGD.
; 
; Commands:
; 
; READ
; WRITE
; 
; The command take a starting address.  For writes, the starting address is
; always the start of a 64 byte block to program.  The block is erased
; and 64 bytes are read in and programmed.  Then it waits for the next
; command.
; 
; The read command just dumps all the memory starting from the address.
; PGD goes into write mode after the command byte and address are recieved.
; The chip must be reset to perform other commands after this.
; 
; PGC is always driven by the programmer.







;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; constants
BOOTLOADER_READ = B'10101010'
BOOTLOADER_WRITE = B'01010101'

BOOTLOADER_DEBOUNCE_THESHOLD = H'80'


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; REGISTERS
; the highest byte the bootloader can access
BOOTLOADER_BASE = H'7f'

BOOTLOADER_DEBOUNCE_COUNTER = BOOTLOADER_BASE
BOOTLOADER_DEBOUNCE_ACCUM = BOOTLOADER_BASE - H'01'
BOOTLOADER_DEBOUNCE_MASK = BOOTLOADER_BASE - H'02'

; result of debounce is 1 or 0
BOOTLOADER_DEBOUNCE_RESULT = BOOTLOADER_BASE - H'03'

; result of read byte
BOOTLOADER_COUNTER = BOOTLOADER_BASE - H'04'
BOOTLOADER_READBYTE_RESULT = BOOTLOADER_BASE - H'05'

; value for write byte
BOOTLOADER_WRITEBYTE_VALUE = BOOTLOADER_BASE - H'06'

; address for read and write commands
BOOTLOADER_ADDRESS16 = BOOTLOADER_BASE - H'08'

; counter for writing blocks of 64 bytes
BOOTLOADER_WRITECOUNTER = BOOTLOADER_BASE - H'09'

; whether the bootloader recieved a reset, interrupthi, or interruptlo
BOOTLOADER_STATE = BOOTLOADER_BASE - H'0a'
; bits for the state
BOOTLOADER_RESET = D'0'
BOOTLOADER_HI = D'1'
BOOTLOADER_LO = D'2'






; macros
; these overwrite FSR0

; skip next command if debounced mask in register is on
BOOTLOADER_SKIP_DEBOUNCE_ON MACRO register, mask
	SET_REGISTER FSR0L, LOW(register)
	SET_REGISTER BOOTLOADER_DEBOUNCE_MASK, mask
	call bootloader_debounce
	SKIP_EQUAL_LITERAL BOOTLOADER_DEBOUNCE_ACCUM, BOOTLOADER_DEBOUNCE_THESHOLD
	ENDM

; skip next command if debounced mask in register is off
BOOTLOADER_SKIP_DEBOUNCE_OFF MACRO register, mask
	SET_REGISTER FSR0L, LOW(register)
	SET_REGISTER BOOTLOADER_DEBOUNCE_MASK, mask
	call bootloader_debounce
; skip if accum under threshold
	SKIP_EQUAL_LITERAL BOOTLOADER_DEBOUNCE_ACCUM, H'00'
	ENDM

; read 16 bits from pins
BOOTLOADER_READ_INT16 MACRO dst
	call bootloader_readbyte
	COPY_REGISTER dst, BOOTLOADER_READBYTE_RESULT
	call bootloader_readbyte
	COPY_REGISTER dst + D'1', BOOTLOADER_READBYTE_RESULT
	ENDM

; write an 8 bit literal to PGD using PGC
; these overwrite FSR0
BOOTLOADER_WRITE_LITERAL MACRO literal
	SET_REGISTER FSR0H, H'0f'
	SET_REGISTER BOOTLOADER_WRITEBYTE_VALUE, B'00101111'
	call bootloader_writebyte
	SET_REGISTER BOOTLOADER_WRITEBYTE_VALUE, literal
	call bootloader_writebyte
	ENDM

; write an 8 bit register to PGD using PGC
; these overwrite FSR0
BOOTLOADER_WRITE_REG MACRO reg
	SET_REGISTER FSR0H, H'0f'
	SET_REGISTER BOOTLOADER_WRITEBYTE_VALUE, B'00101111'
	call bootloader_writebyte
	COPY_REGISTER BOOTLOADER_WRITEBYTE_VALUE, reg
	call bootloader_writebyte
	ENDM

; write a 16 bit register to PGD using PGC
; these overwrite FSR0
BOOTLOADER_WRITE_REG16 MACRO reg
	SET_REGISTER FSR0H, H'0f'
	SET_REGISTER BOOTLOADER_WRITEBYTE_VALUE, B'01001111'
	call bootloader_writebyte
	COPY_REGISTER BOOTLOADER_WRITEBYTE_VALUE, reg
	call bootloader_writebyte
	COPY_REGISTER BOOTLOADER_WRITEBYTE_VALUE, reg + D'1'
	call bootloader_writebyte
	ENDM

; write a 32 bit register to PGD using PGC
; these overwrite FSR0
BOOTLOADER_WRITE_REG32 MACRO reg
	SET_REGISTER FSR0H, H'0f'
	SET_REGISTER BOOTLOADER_WRITEBYTE_VALUE, B'10001111'
	call bootloader_writebyte
	COPY_REGISTER BOOTLOADER_WRITEBYTE_VALUE, reg
	call bootloader_writebyte
	COPY_REGISTER BOOTLOADER_WRITEBYTE_VALUE, reg + D'1'
	call bootloader_writebyte
	COPY_REGISTER BOOTLOADER_WRITEBYTE_VALUE, reg + D'2'
	call bootloader_writebyte
	COPY_REGISTER BOOTLOADER_WRITEBYTE_VALUE, reg + D'3'
	call bootloader_writebyte
	ENDM

; write a 48 bit register to PGD using PGC
; these overwrite FSR0
BOOTLOADER_WRITE_REG48 MACRO reg
	SET_REGISTER FSR0H, H'0f'
	SET_REGISTER BOOTLOADER_WRITEBYTE_VALUE, B'11001111'
	call bootloader_writebyte
	COPY_REGISTER BOOTLOADER_WRITEBYTE_VALUE, reg
	call bootloader_writebyte
	COPY_REGISTER BOOTLOADER_WRITEBYTE_VALUE, reg + D'1'
	call bootloader_writebyte
	COPY_REGISTER BOOTLOADER_WRITEBYTE_VALUE, reg + D'2'
	call bootloader_writebyte
	COPY_REGISTER BOOTLOADER_WRITEBYTE_VALUE, reg + D'3'
	call bootloader_writebyte
	COPY_REGISTER BOOTLOADER_WRITEBYTE_VALUE, reg + D'4'
	call bootloader_writebyte
	COPY_REGISTER BOOTLOADER_WRITEBYTE_VALUE, reg + D'5'
	call bootloader_writebyte
	ENDM

; write a 64 bit register to PGD using PGC
; these overwrite FSR0
BOOTLOADER_WRITE_REG64 MACRO reg
	SET_REGISTER FSR0H, H'0f'
	SET_REGISTER BOOTLOADER_WRITEBYTE_VALUE, B'11101111'
	call bootloader_writebyte
	COPY_REGISTER BOOTLOADER_WRITEBYTE_VALUE, reg
	call bootloader_writebyte
	COPY_REGISTER BOOTLOADER_WRITEBYTE_VALUE, reg + D'1'
	call bootloader_writebyte
	COPY_REGISTER BOOTLOADER_WRITEBYTE_VALUE, reg + D'2'
	call bootloader_writebyte
	COPY_REGISTER BOOTLOADER_WRITEBYTE_VALUE, reg + D'3'
	call bootloader_writebyte
	COPY_REGISTER BOOTLOADER_WRITEBYTE_VALUE, reg + D'4'
	call bootloader_writebyte
	COPY_REGISTER BOOTLOADER_WRITEBYTE_VALUE, reg + D'5'
	call bootloader_writebyte
	COPY_REGISTER BOOTLOADER_WRITEBYTE_VALUE, reg + D'6'
	call bootloader_writebyte
	COPY_REGISTER BOOTLOADER_WRITEBYTE_VALUE, reg + D'7'
	call bootloader_writebyte
	ENDM




;;;;;;;;;;;;;;;;;;;;;;;;;;; START OF BOOTLOADER EXECUTABLE
; This always appears at the start of every program.
; This part of the program is only written in the programming harness.
; It is skipped when writing using the bootloader.




; jump to start on reset
	ORG RESETVECTOR
	SET_REGISTER BOOTLOADER_STATE, B'00000001'
	goto bootloader_start

; jump to interrupt handler on interrupt
	ORG INTVECTORHI
	SET_REGISTER BOOTLOADER_STATE, B'00000010'
	goto bootloader_start

	ORG INTVECTORLO
	SET_REGISTER BOOTLOADER_STATE, B'00000100'
	goto bootloader_start






bootloader_debounce:
; reset variables
	SET_REGISTER BOOTLOADER_DEBOUNCE_COUNTER, BOOTLOADER_DEBOUNCE_THESHOLD
	clrf BOOTLOADER_DEBOUNCE_ACCUM
	clrwdt

bootloader_debounce1:
; load mask for debouncer
	movf BOOTLOADER_DEBOUNCE_MASK, W
; and mask with user register
	andwf INDF0, W
; if all zero, skip
	btfss STATUS, Z

; mask had a 1
		incf BOOTLOADER_DEBOUNCE_ACCUM, F
	
; decrease counter
	decfsz BOOTLOADER_DEBOUNCE_COUNTER, F
; counter not zero
	goto bootloader_debounce1;

; counter zero
	return







; wait for down edge and read the value off PGD
bootloader_readbit:
; wait for PGC to go high
	BOOTLOADER_SKIP_DEBOUNCE_ON PORTB, B'01000000'
; try again
	goto bootloader_readbit




; wait for PGC to go low
; skip if PGC low
bootloader_readbit2:
	BOOTLOADER_SKIP_DEBOUNCE_OFF PORTB, B'01000000'
	goto bootloader_readbit2



; get debounced value of PGD
	SET_REGISTER FSR0L, LOW(PORTB)
	SET_REGISTER BOOTLOADER_DEBOUNCE_MASK, B'10000000'
	rcall bootloader_debounce

	bcf BOOTLOADER_DEBOUNCE_RESULT, D'0'
	SKIP_EQUAL_LITERAL BOOTLOADER_DEBOUNCE_ACCUM, H'00'
		bsf BOOTLOADER_DEBOUNCE_RESULT, D'0'

	return







; read a byte on the down cycles
; least significant bits are first
bootloader_readbyte:
	SET_REGISTER BOOTLOADER_COUNTER, H'08'
	clrf BOOTLOADER_READBYTE_RESULT

bootloader_readbyte1:
; right shift result
	rrncf BOOTLOADER_READBYTE_RESULT, F
; get next bit
	rcall bootloader_readbit


; or bit from PGD if it's on
	btfsc BOOTLOADER_DEBOUNCE_RESULT, D'0'
	bsf BOOTLOADER_READBYTE_RESULT, D'7'

; decrease counter and return if 0
	dcfsnz BOOTLOADER_COUNTER, F
	return

; repeat
	goto bootloader_readbyte1









; write a byte on the up cycles
; least significant bits are first
bootloader_writebyte:
	SET_REGISTER BOOTLOADER_COUNTER, H'08'

bootloader_writebyte1:
; wait for PGC to go high
	BOOTLOADER_SKIP_DEBOUNCE_ON PORTB, B'01000000'
	goto bootloader_writebyte1


; Now that the user has disabled PGD drivers, set PGD to output mode
	bcf TRISB, D'7'

; set PGD to bit
	bcf PORTB, D'7'
	btfsc BOOTLOADER_WRITEBYTE_VALUE, D'0'
	bsf PORTB, D'7'


bootloader_writebyte2:
; wait for PGC to go low, indicating reception of the bit in progress
	BOOTLOADER_SKIP_DEBOUNCE_OFF PORTB, B'01000000'
	goto bootloader_writebyte2

; decrease counter and return if 0
	dcfsnz BOOTLOADER_COUNTER, F
	goto bootloader_writebyte3


; shift value and repeat
	rrcf BOOTLOADER_WRITEBYTE_VALUE, F
	goto bootloader_writebyte1

bootloader_writebyte3:
; wait for PGC to go high indicating bit recieved
	BOOTLOADER_SKIP_DEBOUNCE_ON PORTB, B'01000000'
	goto bootloader_writebyte3

; Set PGD back to input mode.
	bsf TRISB, D'7'
	return









; The 18F1220 does not reset the registers in a reset.
; Must handle interrupts from the last invocation of the firmware and reset
; the registers.
bootloader_start:


; set internal oscillator to full speed
	SET_REGISTER OSCCON, B'11110011'

; skip if PGC is on
	btfss PORTB, D'6'
; go to user program if PGC is off
	goto bootloader_skip
; skip if PGD is on
	btfss PORTB, D'7'
; go to user program if PGD is off
	goto bootloader_skip

; disable interrupts
	clrf INTCON
	clrf T0CON
	clrf T1CON

; reset pins to inputs
	setf TRISA
	setf TRISB
	setf ADCON1
;test:
; pin mode: 1 = digital  0 = analog
;	bsf ADCON1, D'0'
; 1 = input  0 = output
;	bcf TRISA, D'0'
;	bsf PORTA, D'0'
;	clrwdt
;	goto test

;	bsf ADCON1, D'0'
;	bcf TRISA, D'0'
;	bsf PORTA, D'0'

; set high byte of all pins to debounce
	SET_REGISTER FSR0H, H'0f'


	goto bootloader_start1


bootloader_skip:
	btfsc BOOTLOADER_STATE, BOOTLOADER_RESET
	goto PROGRAM_START
	btfsc BOOTLOADER_STATE, BOOTLOADER_HI
	goto PROGRAM_INTERRUPTHI
	btfsc BOOTLOADER_STATE, BOOTLOADER_LO
	goto PROGRAM_INTERRUPTLO



; wait for 0 bit followed by 1 bit to align the signal
bootloader_start1:
;	rcall bootloader_readbit
;	btfsc BOOTLOADER_DEBOUNCE_RESULT, D'0'
;	goto bootloader_start1



;bootloader_start2:
;	rcall bootloader_readbit
;	btfss BOOTLOADER_DEBOUNCE_RESULT, D'0'
;	goto bootloader_start2










; main loop
bootloader:

; read command on the falling edge of pgc
	rcall bootloader_readbyte

	SKIP_NOTEQUAL_LITERAL BOOTLOADER_READBYTE_RESULT, BOOTLOADER_READ
	goto bootloader_read


	SKIP_NOTEQUAL_LITERAL BOOTLOADER_READBYTE_RESULT, BOOTLOADER_WRITE
	goto bootloader_write




	clrwdt
	goto bootloader






bootloader_read:
	BOOTLOADER_READ_INT16 TBLPTRL

	clrf TBLPTRU

	


bootloader_read1:
; read byte with post increment
	tblrd *+
; copy to user register
	COPY_REGISTER BOOTLOADER_WRITEBYTE_VALUE, TABLAT
; send through pins
	rcall bootloader_writebyte
	goto bootloader_read1







bootloader_write:


; get address of next 64 byte block
	BOOTLOADER_READ_INT16 TBLPTRL
	clrf TBLPTRU

; direct access to code memory
	bsf EECON1, EEPGD
	bcf EECON1, CFGS

; enable memory write and setup an erase
	bsf EECON1, WREN
	bsf EECON1, FREE

; unlock flash
	SET_REGISTER EECON2, H'55'
	SET_REGISTER EECON2, H'aa'

; start erase
	bsf EECON1, WR
	nop


; read and write 64 bytes
; set counter for 8 blocks of 8 bytes
	SET_REGISTER BOOTLOADER_WRITECOUNTER, D'8'

bootloader_write1:
	rcall bootloader_readbyte
	COPY_REGISTER TABLAT, BOOTLOADER_READBYTE_RESULT
	tblwt *+
	rcall bootloader_readbyte
	COPY_REGISTER TABLAT, BOOTLOADER_READBYTE_RESULT
	tblwt *+
	rcall bootloader_readbyte
	COPY_REGISTER TABLAT, BOOTLOADER_READBYTE_RESULT
	tblwt *+
	rcall bootloader_readbyte
	COPY_REGISTER TABLAT, BOOTLOADER_READBYTE_RESULT
	tblwt *+
	rcall bootloader_readbyte
	COPY_REGISTER TABLAT, BOOTLOADER_READBYTE_RESULT
	tblwt *+
	rcall bootloader_readbyte
	COPY_REGISTER TABLAT, BOOTLOADER_READBYTE_RESULT
	tblwt *+
	rcall bootloader_readbyte
	COPY_REGISTER TABLAT, BOOTLOADER_READBYTE_RESULT
	tblwt *+
	rcall bootloader_readbyte
	COPY_REGISTER TABLAT, BOOTLOADER_READBYTE_RESULT
	tblwt *

	SET_REGISTER EECON2, H'55'
	SET_REGISTER EECON2, H'aa'
	bsf EECON1, WR
	nop

; increase table pointer
; write operations always line up on 0x40 byte boundaries so don't worry
; about the high byte
	incf TBLPTRL, F

; decrease write counter and skip if zero
	decfsz BOOTLOADER_WRITECOUNTER, F
	goto bootloader_write1

	goto bootloader










