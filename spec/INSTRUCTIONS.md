# byteREEL 2026 — instrukční sada 2.0

Každá instrukce má dva bajty `[opcode][argument]`. Čísla bez prefixu jsou HEX. U ignorovaného argumentu zapisuj `00`. ANY = obě banky. Starých 45 kódů zachováno; nové kódy jsou 03, 80–84 a 90–97. Časování změněno podle `FIRMWARE-SPEC.md`.

| HEX | BIN | Mnemonika | Banka | Argument | Účinek | Z |
|---|---|---|---|---|---|---|
| 00 | 00000000 | NOP | ANY | ignored | Bez operace. | beze změny |
| 01 | 00000001 | HALT | ANY | ignored | Zastaví VM, ztiší audio a piezo; VIDEO drží poslední snímek. Pro nový RUN je nutné STOP→RUN. | beze změny |
| 02 | 00000010 | RESET | ANY | ignored | Výchozí parametry aktivního AV generátoru. Zachová A,C,Z,PC a paměť; neovlivní UART ani piezo. | beze změny |
| 03 | 00000011 | WAIT | ANY | u8 | Dodatečné čekání p×10 ms; 00 bez čekání. SPEED nemění délku WAIT. | beze změny |
| 10 | 00010000 | FREQ | AUDIO | u8 | Frekvence 200+10p Hz. | beze změny |
| 11 | 00010001 | WAVE | AUDIO | 0..5 | 00 sinus, 01 pila, 02 obdélník, 03 trojúhelník, 04 bílý šum, 05 pulz. | beze změny |
| 12 | 00010010 | VOLUME | AUDIO | u8 | Úroveň p/255 násobená pevnou výstupní rezervou. | beze změny |
| 13 | 00010011 | ATTACK | AUDIO | u8 | Náběh p×4 ms. | beze změny |
| 14 | 00010100 | DECAY | AUDIO | u8 | Odeznění po GATE 00: p×4 ms. | beze změny |
| 15 | 00010101 | GATE | AUDIO | 0..1 | 00 zavře, 01 otevře obálku; jiné p chyba. | beze změny |
| 16 | 00010110 | PULSE | AUDIO | u8 | Střída clamp(p/255,0.01,0.99). | beze změny |
| 17 | 00010111 | CRUSH | AUDIO | u8 | 01..07 hloubka 1..7 bitů; 00 nebo 08..FF bez redukce (8 bitů). | beze změny |
| 18 | 00011000 | NOISE | AUDIO | u8 | Příměs bílého šumu p/255. | beze změny |
| 19 | 00011001 | LFO_RATE | AUDIO | u8 | 00 LFO vypnuto; jinak 0.1+0.05(p−1) Hz. | beze změny |
| 1A | 00011010 | LFO_DEPTH | AUDIO | u8 | Relativní hloubka frekvenční modulace p/255. | beze změny |
| 1B | 00011011 | FILTER | AUDIO | u8 | Dolní propust 100+5900p/255 Hz. | beze změny |
| 1E | 00011110 | FREQ_A | AUDIO | ignored | FREQ podle A: 200+10A Hz. | beze změny |
| 1F | 00011111 | VOL_A | AUDIO | ignored | VOLUME podle A/255. | beze změny |
| 20 | 00100000 | AWAIT | AUDIO | u8 | Dodatečné čekání p×20 ms, nezávisle na SPEED. | beze změny |
| 30 | 00110000 | JMP | ANY | even_address | Vždy skočí na sudou absolutní adresu p. | beze změny |
| 31 | 00110001 | JZ | ANY | even_address | Skočí na p při Z=1. | beze změny |
| 32 | 00110010 | JNZ | ANY | even_address | Skočí na p při Z=0. | beze změny |
| 33 | 00110011 | COUNT | ANY | 1..255 | C=p. COUNT 00 je chyba. | beze změny |
| 34 | 00110100 | LOOP | ANY | even_address | Při C=0 chyba; jinak C-- a při C!=0 skok na p. C se nesmí podtéct. | beze změny |
| 40 | 01000000 | SET | ANY | u8 | A=p. | Z=(A==0) |
| 41 | 01000001 | ADD | ANY | u8 | A=(A+p) mod 256. | Z=(A==0) |
| 42 | 01000010 | SUB | ANY | u8 | A=(A-p) mod 256. | Z=(A==0) |
| 43 | 01000011 | XOR | ANY | u8 | A=A XOR p. | Z=(A==0) |
| 44 | 01000100 | RAND | ANY | u8 | A=floor(u32_rng×(p+1)/2^32), rovnoměrná mapa 0..p včetně. | Z=(A==0) |
| 45 | 01000101 | CMP | ANY | u8 | Porovnání A s p; A se nemění. | Z=(A==p) |
| 46 | 01000110 | AND | ANY | u8 | A=A AND p. | Z=(A==0) |
| 47 | 01000111 | OR | ANY | u8 | A=A OR p. | Z=(A==0) |
| 50 | 01010000 | PATTERN | VIDEO | 0..8 | Výběr vzoru 00 černá, 01 šum, 02 vodorovné, 03 svislé pruhy, 04 šachovnice, 05 bloky, 06 XOR, 07 generovaná bitmapa, 08 diagonály. | beze změny |
| 51 | 01010001 | MOTION | VIDEO | signed_u8 | int8(p)/16 pixelu na 40ms animační krok. SPEED nemění rychlost pohybu. | beze změny |
| 52 | 01010010 | DENSITY | VIDEO | u8 | Hustota p; pro pruhy den=max(1,4p). | beze změny |
| 53 | 01010011 | LEVEL | VIDEO | u8 | Jas světlých bodů p/255. | beze změny |
| 54 | 01010100 | SCALE | VIDEO | u8 | Měřítko 1+floor(p/32). | beze změny |
| 55 | 01010101 | GLITCH | VIDEO | u8 | Horní nibble síla, dolní pravděpodobnost zásahu za animační krok /16. | beze změny |
| 56 | 01010110 | INVERT | VIDEO | u8 | 00 normální obraz, nenulové p inverze. | beze změny |
| 57 | 01010111 | SHIFT | VIDEO | signed_u8 | Horizontální posun int8(p) pixelů. | beze změny |
| 58 | 01011000 | FEEDBACK | VIDEO | u8 | Podíl předchozího snímku p/255. FF drží předchozí snímek. | beze změny |
| 59 | 01011001 | SEED | VIDEO | u8 | Nastaví video a VM PRNG podle specifikace. | beze změny |
| 5E | 01011110 | DENS_A | VIDEO | ignored | DENSITY převezme A. | beze změny |
| 5F | 01011111 | GLITCH_A | VIDEO | ignored | GLITCH převezme A. | beze změny |
| 60 | 01100000 | VWAIT | VIDEO | u8 | Dodatečné čekání p×40 ms, nezávisle na SPEED. | beze změny |
| 70 | 01110000 | VJMP | VIDEO | even_address | Videové synonymum JMP. | beze změny |
| 80 | 10000000 | PFREQ | ANY | u8 | Nastaví frekvenci piezo 200+10p Hz. Změna platí i pro právě znějící tón. | beze změny |
| 81 | 10000001 | PDUR | ANY | u8 | Doba příštího PPLAY: p×10 ms; 00 znamená nepřetržitý tón. Výchozí 0A (100 ms). | beze změny |
| 82 | 10000010 | PPLAY | ANY | ignored | Spustí/restartuje piezo tón s PFREQ a PDUR. Neblokuje VM. | beze změny |
| 83 | 10000011 | PSTOP | ANY | ignored | Okamžitě vypne piezo; parametry zachová. | beze změny |
| 84 | 10000100 | PFREQ_A | ANY | ignored | PFREQ podle A; Z a A zachová. | beze změny |
| 90 | 10010000 | TX | ANY | u8 | Zařadí bajt p do TX FIFO. Při plné frontě čeká neblokujícím stavem do UTIME. | 0 úspěch, 1 timeout; A zachován |
| 91 | 10010001 | TX_A | ANY | ignored | Jako TX, odesílá A. Hodnotu A zachytí při fetchi. | 0 úspěch, 1 timeout; A zachován |
| 92 | 10010010 | RX | ANY | ignored | Převezme jeden bajt RX FIFO do A. Při prázdné frontě čeká do UTIME. | 0 přijat bajt včetně 00; 1 timeout, A zachován |
| 93 | 10010011 | TRYRX | ANY | ignored | Okamžitý RX bez čekání. Prázdná fronta nemění A ani sticky timeout. | 0 přijat bajt včetně 00; 1 fronta prázdná |
| 94 | 10010100 | RXREADY | ANY | ignored | A=min(počet bajtů RX FIFO,255); data nespotřebuje. | Z=(A==0) |
| 95 | 10010101 | UTIME | ANY | u8 | Timeout RX/TX=p×10 ms, 00 bez časového limitu. Výchozí 64 hex = 1000 ms. | beze změny |
| 96 | 10010110 | USTAT | ANY | ignored | A=status UART: bit0 RX dostupná, bit1 TX má místo, bit2 RX overflow, bit3 RX timeout, bit4 TX timeout, bit5 line error; bity6/7=0. | Z=(A==0) |
| 97 | 10010111 | UCLR | ANY | u8 | Vymaže sticky bity 2..5 označené p; p=00 maže všechny sticky bity. FIFO nemění, ostatní bity p ignoruje. | beze změny |
