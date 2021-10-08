; the size for this chip
#define EEPROM_SIZE 1024
; the size of each state block
; 1 byte less is available for storage
#define STATE_BLOCK 8



STATE_VARS macro
	VARADD STATE_STATE, 2
; position being written to EEPROM
	VARADD STATE_PTR, 2
; offset in EEPROM of next state
	VARADD EEPROM_OFFSET, 2

	VARADD MIRROR_SECONDS, 1
	VARADD EXPOSURE_SECONDS, 2
	VARADD STORAGE_SECONDS, 1
	VARADD USE_RADIO, 1
	ENDM
	
	



