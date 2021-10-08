init_display:
	call display_mirror_seconds1
	CLEAR_REGISTER16 DISPLAY_COUNTER
	return

handle_display:
	SET_PC_REG DISPLAY_STATE





; convert TEMP0 to minutes in DIGITS
reg16_to_minutes:
	clrf DIGITS + 0
	clrf DIGITS + 1
division_60:
	SKIP_GREATER_LITERAL16 TEMP0, 59
	return
		SUBTRACT_LITERAL16 TEMP0, 60
		incf DIGITS + 0, F
		SKIP_GREATER_LITERAL DIGITS + 0, 9
		bra division_60
			incf DIGITS + 1, F
			clrf DIGITS + 0
			bra division_60


; convert TEMP0 to DIGITS
reg16_to_bcd3:
	clrf DIGITS + 0
	clrf DIGITS + 1
	clrf DIGITS + 2
	SKIP_GREATER_LITERAL16 TEMP0, 999
	bra division_100
		SET_REGISTER DIGITS + 0, 9
		SET_REGISTER DIGITS + 1, 9
		SET_REGISTER DIGITS + 2, 9
		return

; divide by 100
division_100:
	SKIP_GREATER_LITERAL16 TEMP0, 99
	bra division_10
		SUBTRACT_LITERAL16 TEMP0, 100
		incf DIGITS + 2, F
		bra division_100
	
; divide by 10
division_10:
	SKIP_GREATER_LITERAL16 TEMP0, 9
	bra division_1
		SUBTRACT_LITERAL16 TEMP0, 10
		incf DIGITS + 1, F
		bra division_10

division_1:
; copy remainder
	COPY_REGISTER DIGITS + 0, TEMP0
	return




display_idle:
	return

; return to previous state
display_return:
	SET_PC_REG DISPLAY_PREV_STATE


; display shutter delay
display_shutter1:
	SET_REGISTER16 DISPLAY_PREV_STATE, display_shutter1
	return

; display storage delay
display_storage1:
	SET_REGISTER16 DISPLAY_PREV_STATE, display_storage1
	return

; enter the shutter + meter display
display_meter_shutter1:
	CLEAR_REGISTER16 DISPLAY_COUNTER
	UPDATE_LCD_CODE 2, MASK5
	UPDATE_LCD_CODE 1, MASK_M1
	UPDATE_LCD_CODE 0, MASK_M2
	SET_REGISTER16 DISPLAY_STATE, display_idle
	return

; enter the shutter status display
display_meter1:
	CLEAR_REGISTER16 DISPLAY_COUNTER
	UPDATE_LCD_CODE 2, MASK_SPACE
	UPDATE_LCD_CODE 1, MASK_M1
	UPDATE_LCD_CODE 0, MASK_M2
	SET_REGISTER16 DISPLAY_STATE, display_idle
	return


#if 0
; update the animated meter status
display_meter2:
	infsnz DISPLAY_COUNTER, F
	incf DISPLAY_COUNTER + 1, F
	SKIP_GREATEREQUAL_LITERAL16 DISPLAY_COUNTER, ANIMATION_DELAY
	return
		CLEAR_REGISTER16 DISPLAY_COUNTER

		SKIP_NOTEQUAL_LITERAL DIGIT_TABLE + 0, MASK_TOP
			bra display_meter3
		SKIP_NOTEQUAL_LITERAL DIGIT_TABLE + 0, MASK_RIGHT
			bra display_meter4
		SKIP_NOTEQUAL_LITERAL DIGIT_TABLE + 0, MASK_BOTTOM
			bra display_meter5
			UPDATE_LCD_CODE 0, MASK_TOP
			return

display_meter3:
	UPDATE_LCD_CODE 0, MASK_RIGHT
	return
	
display_meter4:
	UPDATE_LCD_CODE 0, MASK_BOTTOM
	return

display_meter5:
	UPDATE_LCD_CODE 0, MASK_LEFT
	return
#endif


; enter the shutter status
display_shutter_state1:
	CLEAR_REGISTER16 DISPLAY_COUNTER
	CLEAR_REGISTER16 SECONDS_COUNTER
	UPDATE_LCD_CODE 2, MASK0
	UPDATE_LCD_CODE 1, MASK0
	UPDATE_LCD_CODE 0, MASK0
	SET_REGISTER16 DISPLAY_STATE, display_shutter_state2
	return

; update the shutter status
display_shutter_state2:
	INC16 DISPLAY_COUNTER
	SKIP_GREATEREQUAL_LITERAL16 DISPLAY_COUNTER, HZ
	return

		CLEAR_REGISTER16 DISPLAY_COUNTER
		INC16 SECONDS_COUNTER
		COPY_REGISTER16 TEMP0, SECONDS_COUNTER
		call reg16_to_bcd3
		UPDATE_LCD_DIGIT 0, DIGITS + 0
		UPDATE_LCD_DIGIT 1, DIGITS + 1
		UPDATE_LCD_DIGIT 2, DIGITS + 2
		return

; start the intervalometer program
; enter the intervalometer time
display_interval1:
	CLEAR_REGISTER16 DISPLAY_COUNTER
	CLEAR_REGISTER16 SECONDS_COUNTER
	clrf DIGITS + 0
	clrf DIGITS + 1
	clrf DIGITS + 2
	UPDATE_LCD_CODE 2, MASK0
	UPDATE_LCD_CODE 1, MASK0
	UPDATE_LCD_CODE 0, MASK0
	SET_REGISTER16 DISPLAY_STATE, display_interval2
	return

; update the intervalometer status
display_interval2:
	btfsc FLAGS, INTERVAL_MIRROR
	call display_interval3
	btfsc FLAGS, INTERVAL_SHUTTER
	call display_interval4
	btfsc FLAGS, INTERVAL_STORAGE
	call display_interval5

	INC16 DISPLAY_COUNTER
	SKIP_GREATEREQUAL_LITERAL16 DISPLAY_COUNTER, HZ
	return

; update seconds
		CLEAR_REGISTER16 DISPLAY_COUNTER
		INC16 SECONDS_COUNTER
		COPY_REGISTER16 TEMP0, SECONDS_COUNTER
		call reg16_to_bcd3
		UPDATE_LCD_DIGIT 0, DIGITS + 0
		UPDATE_LCD_DIGIT 1, DIGITS + 1
		UPDATE_LCD_DIGIT 2, DIGITS + 2
		return

; handle mirror
display_interval3:
; if no mirror time, go to shutter
	SKIP_NONZERO MIRROR_SECONDS
	bra mirror_done

; if 1 second reached, release pin
		SKIP_EQUAL_LITERAL SECONDS_COUNTER, 1
		bra test_mirror_done
			bsf OUTPUT_TRIS, SHUTTER_PIN
			return

test_mirror_done:
; if seconds + 1 reached, go to shutter
		SKIP_GREATER SECONDS_COUNTER, MIRROR_SECONDS
		return

mirror_done:
; go to shutter
	bcf FLAGS, INTERVAL_MIRROR
	bsf FLAGS, INTERVAL_SHUTTER
	CLEAR_REGISTER16 SECONDS_COUNTER
	CLEAR_REGISTER16 DISPLAY_COUNTER
	bcf OUTPUT_TRIS, SHUTTER_PIN
	return


; handle shutter
display_interval4:
; if no shutter time, don't test time
	SKIP_NONZERO16 EXPOSURE_SECONDS
	return

; if seconds reached, release pin
		SKIP_EQUAL SECONDS_COUNTER, EXPOSURE_SECONDS
		bra test_shutter_done
			bsf OUTPUT_TRIS, SHUTTER_PIN
			return

test_shutter_done:
; if seconds + 1 reached, go to storage
		SKIP_GREATER SECONDS_COUNTER, EXPOSURE_SECONDS
		return

; go to storage
	bcf FLAGS, INTERVAL_SHUTTER
	bsf FLAGS, INTERVAL_STORAGE
	CLEAR_REGISTER16 SECONDS_COUNTER
	CLEAR_REGISTER16 DISPLAY_COUNTER
	return

;  handle storage
display_interval5:
	SKIP_GREATEREQUAL SECONDS_COUNTER, STORAGE_SECONDS
	return

; go to mirror/shutter
	bcf FLAGS, INTERVAL_STORAGE
	bsf FLAGS, INTERVAL_MIRROR
	bcf OUTPUT_TRIS, SHUTTER_PIN
	
	CLEAR_REGISTER16 SECONDS_COUNTER
	CLEAR_REGISTER16 DISPLAY_COUNTER
	return





; display mirror lockup delay
display_mirror_seconds1:
	SET_REGISTER16 DISPLAY_PREV_STATE, display_mirror_seconds1
	UPDATE_LCD_CODE 2, MASK_TOP
	COPY_REGISTER TEMP0, MIRROR_SECONDS
	clrf TEMP0 + 1
	call reg16_to_bcd3
	UPDATE_LCD_DIGIT 0, DIGITS + 0
	UPDATE_LCD_DIGIT 1, DIGITS + 1
	SET_REGISTER16 DISPLAY_STATE, display_idle
	return

display_shutter_seconds1:
	SET_REGISTER16 DISPLAY_PREV_STATE, display_shutter_seconds1
	UPDATE_LCD_CODE 2, MASK_MIDDLE

display_shutter_seconds3:
	COPY_REGISTER16 TEMP0, EXPOSURE_SECONDS
	SKIP_GREATEREQUAL_LITERAL16 TEMP0, 60
	bra display_shutter_seconds4
		call reg16_to_minutes
		bra display_shutter_seconds5

display_shutter_seconds4:
	call reg16_to_bcd3
display_shutter_seconds5:
	UPDATE_LCD_DIGIT 0, DIGITS + 0
	UPDATE_LCD_DIGIT 1, DIGITS + 1
	CLEAR_REGISTER16 DISPLAY_COUNTER
	SET_REGISTER16 DISPLAY_STATE, display_shutter_seconds2
	return


display_shutter_seconds2:
; flash minutes
	SKIP_GREATEREQUAL_LITERAL16 EXPOSURE_SECONDS, 60
	return

		INC16 DISPLAY_COUNTER
		SKIP_GREATEREQUAL_LITERAL16 DISPLAY_COUNTER, ANIMATION_DELAY
		return

			CLEAR_REGISTER16 DISPLAY_COUNTER
; if blank, go to number
			SKIP_NOTEQUAL_LITERAL DIGIT_TABLE + 0, MASK_SPACE
			bra display_shutter_seconds3

; if showing the number, go blank
				UPDATE_LCD_CODE 0, MASK_SPACE
				UPDATE_LCD_CODE 1, MASK_SPACE
				return


display_storage_seconds1:
	SET_REGISTER16 DISPLAY_PREV_STATE, display_storage_seconds1
	UPDATE_LCD_CODE 2, MASK_BOTTOM
	COPY_REGISTER TEMP0, STORAGE_SECONDS
	clrf TEMP0 + 1
	call reg16_to_bcd3
	UPDATE_LCD_DIGIT 0, DIGITS + 0
	UPDATE_LCD_DIGIT 1, DIGITS + 1
	SET_REGISTER16 DISPLAY_STATE, display_idle
	return



display_radio_enabled1:
	SET_REGISTER16 DISPLAY_PREV_STATE, display_radio_enabled1
	SET_REGISTER16 DISPLAY_STATE, display_idle
	
	btfss USE_RADIO, 0
	bra display_radio_enabled2

; wireless mode
		UPDATE_LCD_CODE 2, MASK_SPACE
		UPDATE_LCD_CODE 1, MASK_R
		UPDATE_LCD_CODE 0, MASK_F
		return

; tethered mode
display_radio_enabled2:
	UPDATE_LCD_CODE 2, MASK_P
	UPDATE_LCD_CODE 1, MASK_L
	UPDATE_LCD_CODE 0, MASK_G
	return




