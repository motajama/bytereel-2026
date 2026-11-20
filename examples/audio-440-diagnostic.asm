; Diagnostic loop: fixed 440 Hz sine, 2.5 s at each level, no gate retrigger
FREQ 18       ; 200 + 10*0x18 = 440 Hz
WAVE 00       ; sine
ATTACK 00
DECAY 00
GATE 01
AGAIN:
VOLUME 40
WAIT FA       ; 2500 ms at requested VM timing
VOLUME 80
WAIT FA
VOLUME C0
WAIT FA
JMP AGAIN
