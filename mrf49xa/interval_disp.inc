; display for intervalometer
#define ANIMATION_DELAY   (HZ / 4)



DISPLAY_VARS macro
	VARADD DISPLAY_STATE, 2
; state to return to after shutter/meter
	VARADD DISPLAY_PREV_STATE, 2
	VARADD DISPLAY_COUNTER, 2
	
	VARADD MODE_CHARACTER, 1
; BCD digits.  Least significant is 0
	VARADD DIGITS, 3
	ENDM





