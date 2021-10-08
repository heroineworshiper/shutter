; read & write persistent state in EEPROM

; increment address & read byte
READ_EEPROM_BYTE macro dst
	infsnz EEADR, F
	incf EEADRH, F
	bsf EECON1, RD
	COPY_REGISTER dst, EEDATA
	ENDM

; write byte
WRITE_EEPROM_LITERAL macro src
; disable interrupts
	bcf   INTCON, GIE
	SET_REGISTER EEDATA, src
	call write_sequence
	ENDM

; increment address & write byte
WRITE_EEPROM_BYTE macro src
; disable interrupts
	bcf   INTCON, GIE
	infsnz EEADR, F
	incf EEADRH, F
	COPY_REGISTER EEDATA, src
	call write_sequence
	ENDM


init_state:
	SET_REGISTER MIRROR_SECONDS, 3
	SET_REGISTER STORAGE_SECONDS, 5
	SET_REGISTER USE_RADIO, 1
	SET_REGISTER16 EXPOSURE_SECONDS, 10
	SET_REGISTER16 STATE_STATE, state_idle
	

; scan EEPROM for last state & read it
	CLEAR_REGISTER16 EEPROM_OFFSET

search_eeprom:
	clrwdt
	COPY_REGISTER16 EEADR, EEPROM_OFFSET
	bcf EECON1, EEPGD
	bcf EECON1, CFGS
	bsf EECON1, RD
; 0 marks the valid state
	SKIP_NOTEQUAL_LITERAL EEDATA, 0
	bra read_state

; next block
		ADD_LITERAL16 EEPROM_OFFSET, STATE_BLOCK
		SKIP_EQUAL_LITERAL16 EEPROM_OFFSET, EEPROM_SIZE
		bra search_eeprom

; no data found by end of EEPROM
			CLEAR_REGISTER16 EEPROM_OFFSET
			return

; read valid state
read_state:
;COPY_REGISTER16 TEMP0, EEPROM_OFFSET
;call debug_print

	READ_EEPROM_BYTE MIRROR_SECONDS
	READ_EEPROM_BYTE EXPOSURE_SECONDS
	READ_EEPROM_BYTE EXPOSURE_SECONDS + 1
	READ_EEPROM_BYTE STORAGE_SECONDS
	READ_EEPROM_BYTE USE_RADIO
	return

handle_state:
	SET_PC_REG STATE_STATE



; store state in EEPROM if it is different
write_state:
; invalidate current block by writing 0xff to 1st byte
	COPY_REGISTER16 EEADR, EEPROM_OFFSET
	bcf   EECON1, EEPGD
	bcf   EECON1, CFGS
; enable writes
	bsf   EECON1, WREN
	WRITE_EEPROM_LITERAL 0xff
	SET_REGISTER16 STATE_STATE, handle_write_state2
	return



handle_write_state2:
; abort if not ready for write
	btfsc EECON1, WR
	return

; go to next block
		ADD_LITERAL16 EEPROM_OFFSET, STATE_BLOCK
; wrap around
		SKIP_GREATEREQUAL_LITERAL16 EEPROM_OFFSET, EEPROM_SIZE
		bra write_state2

			CLEAR_REGISTER16 EEPROM_OFFSET

write_state2:
		COPY_REGISTER16 EEADR, EEPROM_OFFSET
; validate it
		WRITE_EEPROM_LITERAL 0x00
; start writing vars
		SET_REGISTER16 STATE_PTR, MIRROR_SECONDS
		SET_REGISTER16 STATE_STATE, handle_write_state3
		return


handle_write_state3:
; abort if not ready for write
	btfsc EECON1, WR
	return

; write next byte
		SET_POINTER0_REG STATE_PTR
		WRITE_EEPROM_BYTE POSTINC0
		COPY_REGISTER16 STATE_PTR, POINTER0

; 1 byte of the 32 byte block is used for invalidating it
		SKIP_GREATEREQUAL_LITERAL16 STATE_PTR, MIRROR_SECONDS + STATE_BLOCK - 1
		return
		
			SET_REGISTER16 STATE_STATE, handle_write_state4
			return

handle_write_state4:
; abort if not ready for write
	btfsc EECON1, WR
	return

; disable writes
		bcf   EECON1, WREN
		SET_REGISTER16 STATE_STATE, state_idle
;COPY_REGISTER16 TEMP0, EEPROM_OFFSET
;call debug_print
		return



write_sequence:
	SET_REGISTER EECON2, H'55'
	SET_REGISTER EECON2, H'aa'
	bsf   EECON1, WR
; enable interrupts
	bsf   INTCON, GIE
	return

state_idle:
	return











