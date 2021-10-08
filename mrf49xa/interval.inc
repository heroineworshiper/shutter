; common intervalometer include



#define BAUD_RATE_CODE (CLOCKSPEED / (BAUD * 4) - 1)
#define TIMER1_DELAY -(CLOCKSPEED / 4 / HZ)

#define RADIO_CHANNEL 3903
; 8620 baud
;#define RADIO_BAUD_CODE 4
; this is the limit for 250000 clockspeed
#define RADIO_BAUD_CODE 20
;#define RADIO_BAUD_CODE 142
; timer1 wraparounds before shutting down radio due to inactivity
#define RADIO_TIMEOUT (HZ * 5)

#include "pic_mrf49xa.inc"









