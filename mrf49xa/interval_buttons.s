; mechanical buttons for the intervalometer


init_buttons:
	clrf BUTTON_COUNTER0
	clrf BUTTON_COUNTER1
	clrf BUTTON_COUNTER2
	clrf BUTTON_COUNTER3
	clrf BUTTON_COUNTER4
	clrf BUTTON_CYCLES
	clrf BUTTON_DOWN
	clrf PREV_BUTTON_DOWN
; enable weak pullups for buttons
	SET_REGISTER WPUB, B'00111110'
	bcf INTCON2, RBPU
	return

; button is down if it is low
handle_buttons:
	btfss PORTB, 1
	incf BUTTON_COUNTER0, F
	btfss PORTB, 2
	incf BUTTON_COUNTER1, F
	btfss PORTB, 3
	incf BUTTON_COUNTER2, F
	btfss PORTB, 4
	incf BUTTON_COUNTER3, F
	btfss PORTB, 5
	incf BUTTON_COUNTER4, F

; probing cycles reached?
	incf BUTTON_CYCLES, F
	SKIP_GREATEREQUAL_LITERAL BUTTON_CYCLES, CYCLES_COUNT
	return

; update button states
		clrf BUTTON_CYCLES
		COPY_REGISTER PREV_BUTTON_DOWN, BUTTON_DOWN
		clrf BUTTON_DOWN
		
		SKIP_LESS_LITERAL BUTTON_COUNTER0, DEBOUNCE_COUNT
		bsf BUTTON_DOWN, 0
		SKIP_LESS_LITERAL BUTTON_COUNTER1, DEBOUNCE_COUNT
		bsf BUTTON_DOWN, 1
		SKIP_LESS_LITERAL BUTTON_COUNTER2, DEBOUNCE_COUNT
		bsf BUTTON_DOWN, 2
		SKIP_LESS_LITERAL BUTTON_COUNTER3, DEBOUNCE_COUNT
		bsf BUTTON_DOWN, 3
		SKIP_LESS_LITERAL BUTTON_COUNTER4, DEBOUNCE_COUNT
		bsf BUTTON_DOWN, 4
		
		clrf BUTTON_COUNTER0
		clrf BUTTON_COUNTER1
		clrf BUTTON_COUNTER2
		clrf BUTTON_COUNTER3
		clrf BUTTON_COUNTER4


; user event handlers
; only 1 button can be down
		clrf TEMP0
		btfsc BUTTON_DOWN, 0
		incf TEMP0, F
		btfsc BUTTON_DOWN, 1
		incf TEMP0, F
		btfsc BUTTON_DOWN, 2
		incf TEMP0, F
		btfsc BUTTON_DOWN, 3
		incf TEMP0, F
		btfsc BUTTON_DOWN, 4
		incf TEMP0, F
		SKIP_LESS_LITERAL TEMP0, 2
		return 

		btfss BUTTON_DOWN, 0
		bra button_event1
		btfss PREV_BUTTON_DOWN, 0
		call handle_button0

button_event1:
		btfss BUTTON_DOWN, 1
		bra button_event2
		btfss PREV_BUTTON_DOWN, 1
		call handle_button1

button_event2:
		btfss BUTTON_DOWN, 2
		bra button_event3
		btfss PREV_BUTTON_DOWN, 2
		call handle_button2

button_event3:
		btfss BUTTON_DOWN, 3
		bra button_event4
		btfss PREV_BUTTON_DOWN, 3
		call handle_button3

button_event4:
		btfss BUTTON_DOWN, 4
		bra button_event5
		btfss PREV_BUTTON_DOWN, 4
		call handle_button4


button_event5:
		return

; shutter button
handle_button0:
; skip if shutter currently disabled
	btfss OUTPUT_TRIS, SHUTTER_PIN
	bra disable_shutter

; only enable it if meter is disabled
;	btfss OUTPUT_TRIS, METER_PIN
;	return

		bcf OUTPUT_TRIS, SHUTTER_PIN

; if meter also enabled, display 2 abbreviations
		btfss OUTPUT_TRIS, METER_PIN
		bra handle_meter_shutter_button

; only shutter enabled
		call display_shutter_state1

; debug
;	bcf OUTPUT_TRIS, SHUTTER_PIN
;	btg OUTPUT_LAT, SHUTTER_PIN
		return
	
disable_shutter:
	bsf OUTPUT_TRIS, SHUTTER_PIN

; if meter still enabled, go to meter display
	btfss OUTPUT_TRIS, METER_PIN
	bra disable_shutter2

		call display_return
		return

disable_shutter2:
	call display_meter1
	return




; meter button
handle_button1:
; skip if meter is currently disabled
	btfss OUTPUT_TRIS, METER_PIN
	bra disable_meter

		bcf OUTPUT_TRIS, METER_PIN

; if shutter also enabled, display both abbreviations
		btfss OUTPUT_TRIS, SHUTTER_PIN
		bra handle_meter_shutter_button
		
; shutter disabled
		call display_meter1


; debug
;	bcf OUTPUT_TRIS, METER_PIN
;	btg OUTPUT_LAT, METER_PIN
		return

handle_meter_shutter_button:
	call display_meter_shutter1
	return


; always disable it
disable_meter:
	bsf OUTPUT_TRIS, METER_PIN

; if shutter still enabled, go to shutter display
	btfss OUTPUT_TRIS, SHUTTER_PIN
	bra disable_meter2

	call display_return
	return

disable_meter2:
; since we haven't been measuring time, this is going to start at 0 seconds,
; but you'll rarely go to shutter from shutter + meter
	call display_shutter_state1
	return


; start/stop intervalometer
handle_button2:
; shutter & meter must be disabled to enable intervalometer
	btfsc FLAGS, INTERVAL_ON
	bra disable_interval

		btfss OUTPUT_TRIS, METER_PIN
		return
		btfss OUTPUT_TRIS, SHUTTER_PIN
		return

; start the intervalometer program
			bsf FLAGS, INTERVAL_ON
			bsf FLAGS, INTERVAL_MIRROR
; enable mirror/shutter
			bcf OUTPUT_TRIS, SHUTTER_PIN
			call display_interval1
			return

disable_interval:
	bsf OUTPUT_TRIS, METER_PIN
	bsf OUTPUT_TRIS, SHUTTER_PIN
	bcf FLAGS, INTERVAL_ON
	bcf FLAGS, INTERVAL_MIRROR
	bcf FLAGS, INTERVAL_SHUTTER
	bcf FLAGS, INTERVAL_STORAGE

; reset display
	call display_return
	return

; mode
handle_button3:

	SKIP_NOTEQUAL_LITERAL16 DISPLAY_PREV_STATE, display_mirror_seconds1
	bra display_shutter_seconds1
	SKIP_NOTEQUAL_LITERAL16 DISPLAY_PREV_STATE, display_shutter_seconds1
	bra display_storage_seconds1
	SKIP_NOTEQUAL_LITERAL16 DISPLAY_PREV_STATE, display_storage_seconds1
	bra display_radio_enabled1
	bra display_mirror_seconds1


; select
handle_button4:

	SKIP_NOTEQUAL_LITERAL16 DISPLAY_PREV_STATE, display_mirror_seconds1
	call cycle_mirror_time
	SKIP_NOTEQUAL_LITERAL16 DISPLAY_PREV_STATE, display_shutter_seconds1
	call cycle_shutter_time
; must have all conditions because we always fall through to write the state
	SKIP_NOTEQUAL_LITERAL16 DISPLAY_PREV_STATE, display_storage_seconds1
	call cycle_storage_time
	SKIP_NOTEQUAL_LITERAL16 DISPLAY_PREV_STATE, display_radio_enabled1
	call cycle_radio_enabled

; store it
		call write_state
; update display
		bra display_return



cycle_radio_enabled:
	btg USE_RADIO, 0
	return





cycle_mirror_time:
	SKIP_NOTEQUAL_LITERAL MIRROR_SECONDS, 0
	bra cycle_mirror_time5
	SKIP_NOTEQUAL_LITERAL MIRROR_SECONDS, 3
	bra cycle_mirror_time1
	SKIP_NOTEQUAL_LITERAL MIRROR_SECONDS, 5
	bra cycle_mirror_time2
	SKIP_NOTEQUAL_LITERAL MIRROR_SECONDS, 10
	bra cycle_mirror_time3
	bra cycle_mirror_time4

cycle_mirror_time1:
	SET_REGISTER MIRROR_SECONDS, 5
	return
cycle_mirror_time2:
	SET_REGISTER MIRROR_SECONDS, 10
	return
cycle_mirror_time3:
	SET_REGISTER MIRROR_SECONDS, 30
	return
cycle_mirror_time4:
	SET_REGISTER MIRROR_SECONDS, 0
	return
cycle_mirror_time5:
	SET_REGISTER MIRROR_SECONDS, 3
	return

cycle_shutter_time:
	SKIP_NOTEQUAL_LITERAL16 EXPOSURE_SECONDS, 0
	bra cycle_shutter_time1
	SKIP_NOTEQUAL_LITERAL16 EXPOSURE_SECONDS, 1
	bra cycle_shutter_time2
	SKIP_NOTEQUAL_LITERAL16 EXPOSURE_SECONDS, 2
	bra cycle_shutter_time3
	SKIP_NOTEQUAL_LITERAL16 EXPOSURE_SECONDS, 5
	bra cycle_shutter_time4
	SKIP_NOTEQUAL_LITERAL16 EXPOSURE_SECONDS, 10
	bra cycle_shutter_time5
	SKIP_NOTEQUAL_LITERAL16 EXPOSURE_SECONDS, 15
	bra cycle_shutter_time6
	SKIP_NOTEQUAL_LITERAL16 EXPOSURE_SECONDS, 30
	bra cycle_shutter_time7
	SKIP_NOTEQUAL_LITERAL16 EXPOSURE_SECONDS, 60
	bra cycle_shutter_time8
	SKIP_NOTEQUAL_LITERAL16 EXPOSURE_SECONDS, 60 * 2
	bra cycle_shutter_time9
	bra cycle_shutter_time10

cycle_shutter_time1:
	SET_REGISTER16 EXPOSURE_SECONDS, 1
	return
cycle_shutter_time2:
	SET_REGISTER16 EXPOSURE_SECONDS, 2
	return
cycle_shutter_time3:
	SET_REGISTER16 EXPOSURE_SECONDS, 5
	return
cycle_shutter_time4:
	SET_REGISTER16 EXPOSURE_SECONDS, 10
	return
cycle_shutter_time5:
	SET_REGISTER16 EXPOSURE_SECONDS, 15
	return
cycle_shutter_time6:
	SET_REGISTER16 EXPOSURE_SECONDS, 30
	return
cycle_shutter_time7:
	SET_REGISTER16 EXPOSURE_SECONDS, 60
	return
cycle_shutter_time8:
	SET_REGISTER16 EXPOSURE_SECONDS, 60 * 2
	return
cycle_shutter_time9:
	SET_REGISTER16 EXPOSURE_SECONDS, 60 * 5
	return
cycle_shutter_time10:
	SET_REGISTER16 EXPOSURE_SECONDS, 0
	return

cycle_storage_time:
	SKIP_NOTEQUAL_LITERAL STORAGE_SECONDS, 0
	bra cycle_storage_time1
	SKIP_NOTEQUAL_LITERAL STORAGE_SECONDS, 3
	bra cycle_storage_time5
	SKIP_NOTEQUAL_LITERAL STORAGE_SECONDS, 5
	bra cycle_storage_time2
	SKIP_NOTEQUAL_LITERAL STORAGE_SECONDS, 10
	bra cycle_storage_time3
	bra cycle_storage_time4

cycle_storage_time1:
	SET_REGISTER STORAGE_SECONDS, 3
	return
cycle_storage_time2:
	SET_REGISTER STORAGE_SECONDS, 10
	return
cycle_storage_time3:
	SET_REGISTER STORAGE_SECONDS, 30
	return
cycle_storage_time4:
	SET_REGISTER STORAGE_SECONDS, 0
	return
cycle_storage_time5:
	SET_REGISTER STORAGE_SECONDS, 5
	return


