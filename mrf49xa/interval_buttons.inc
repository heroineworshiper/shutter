
; number of cycles before updating button states
#define CYCLES_COUNT 64
; number of cycles the button must be down to be really down
#define DEBOUNCE_COUNT 32


BUTTON_VARS macro
; number of cycles tested
	VARADD BUTTON_CYCLES, 1
; number of cycles the button was on
	VARADD BUTTON_COUNTER0, 1
	VARADD BUTTON_COUNTER1, 1
	VARADD BUTTON_COUNTER2, 1
	VARADD BUTTON_COUNTER3, 1
	VARADD BUTTON_COUNTER4, 1
; button states.  1 bit for each button
	VARADD BUTTON_DOWN, 1
	VARADD PREV_BUTTON_DOWN, 1
	ENDM
