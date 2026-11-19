# byteREEL 2026 — canonical hardware wiring

Source: the physically exercised panel and service firmware 1.0. The pin map
must not be changed to match generic library defaults. I2C addresses are
7-bit. The input expander at `0x22` is read-only; it uses external pull-ups.

| Function | Pin / connection | Behavior |
|---|---|---|
| SDA / SCL | ESP GPIO21 / GPIO22 | I2C at 10,000 Hz |
| LED expander | `0x20` | 16-bit writes, low byte first, high byte second |
| Input expander | `0x22` | Read two bytes only; external pull-ups |
| Lower D0–D7 LEDs | `0x20` P00–P07 | Active LOW |
| Upper A0–A7 LEDs | `0x20` P10–P17 | Active LOW |
| Data switches D0–D7 | `0x22` P00–P07 | LOW = logical 1 |
| HOME | `0x22` P10 (bit 8) | LOW = pressed |
| RESET / CLR | `0x22` P11 (bit 9) | LOW = pressed |
| WRITE | `0x22` P12 (bit 10) | LOW = pressed |
| WRITE+ | `0x22` P13 (bit 11) | LOW = pressed |
| EXP/EXAM (MEMORY:SAVE) | `0x22` P14 (bit 12) | LOW = pressed |
| NEXT (MEMORY:LOAD) | `0x22` P15 (bit 13) | LOW = pressed |
| Unused | `0x22` P16/P17 | Masked; do not decode as controls |
| RUN / STOP | GPIO13, INPUT_PULLUP | LOW = RUN, HIGH = STOP |
| VIDEO / SOUND | GPIO14, INPUT_PULLUP | LOW = VIDEO, HIGH = AUDIO |
| LIVE / SCAN | GPIO16, INPUT_PULLUP | LOW = LIVE, HIGH = SCAN |
| AUTO / STEP | GPIO17, INPUT_PULLUP | LOW = AUTO, HIGH = STEP |
| EDIT / MEMORY | GPIO18, INPUT_PULLUP | LOW = EDIT, HIGH = MEMORY |
| SPEED | GPIO33 ADC1 | Increases from left to right |
| PIEZO | GPIO23 → 1k → passive piezo → GND | LEDC channel 0, 50% duty |
| VIDEO RCA | GPIO25 to center; GND to shield | DAC1 PAL composite |
| AUDIO RCA | GPIO26 through RC/coupling network | DAC2 line output |
| GX12 pin 1 | GPIO27 TX | 3.3 V UART output |
| GX12 pin 2 | GPIO32 RX | 3.3 V UART input |
| GX12 pin 3 | GND | No power output |

LED anodes connect through individual 1k resistors to 3V3; cathodes connect to
the LED expander. POWER LED has its own 1k resistor to 3V3 and GND and is not
software-controlled. Viewed from the front, LED order is A7…A0 and D7…D0 from
left to right. Switch COM terminals share GND, and each of the 14 used input
expander signals has an individual 10k pull-up to 3V3. Expander VCC and VDD
are both 3.3 V and are jumpered together; INT is unused.

## Audio output network

GPIO26 → 1k → node A; node A → 10 nF (`103`) → GND; node A → positive side of
10 µF capacitor, negative side → node B; node B → 100k → GND; node B → AUDIO
RCA center. RCA shield connects to GND. This is a line-level input for an
amplifier or powered speaker, not a power output. A historical question about
a capacitor marked `10K` was not resolved by identification; working during a
service test is not a frequency-response measurement.

## Power and safety

The LM2596 is adjusted to approximately 5.08 V at ESP 5V; panel logic uses
ESP 3V3. During USB tests, do not connect USB 5 V and an active converter
output without the established isolation procedure. The previous procedure
disconnects the converter's positive output from ESP 5V. Software PASS cannot
verify converter supply, GND continuity, or POWER LED operation.

## Superseded connector pinout

An older manual showed GX12 pin 1 = GND, pin 2 = TX, pin 3 = RX. That pinout
is obsolete. The current assignment is pin 1 = TX/GPIO27, pin 2 = RX/GPIO32,
pin 3 = GND; the loopback connects pins 1–2. Do not apply RS-232 or 5 V to
GPIO32. External peripherals need their own power source.
