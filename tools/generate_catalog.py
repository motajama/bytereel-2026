#!/usr/bin/env python3
"""Generate canonical opcode catalog and checked example bytecode. Standard Python only."""
import json,csv,re
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
rows=[]
def op(code,name,mode,arg,desc,z='beze změny'):
 rows.append(dict(opcode=int(code,16),hex=code,binary=f'{int(code,16):08b}',mnemonic=name,mode=mode,argument=arg,effect=desc,z=z,size=2))
op('00','NOP','ANY','ignored','Bez operace.')
op('01','HALT','ANY','ignored','Zastaví VM, ztiší audio a piezo; VIDEO drží poslední snímek. Pro nový RUN je nutné STOP→RUN.')
op('02','RESET','ANY','ignored','Výchozí parametry aktivního AV generátoru. Zachová A,C,Z,PC a paměť; neovlivní UART ani piezo.')
op('03','WAIT','ANY','u8','Dodatečné čekání p×10 ms; 00 bez čekání. SPEED nemění délku WAIT.')
for code,name,desc in [('30','JMP','Vždy skočí na sudou absolutní adresu p.'),('31','JZ','Skočí na p při Z=1.'),('32','JNZ','Skočí na p při Z=0.')]:op(code,name,'ANY','even_address',desc)
op('33','COUNT','ANY','1..255','C=p. COUNT 00 je chyba.')
op('34','LOOP','ANY','even_address','Při C=0 chyba; jinak C-- a při C!=0 skok na p. C se nesmí podtéct.')
for code,name,desc in [('40','SET','A=p.'),('41','ADD','A=(A+p) mod 256.'),('42','SUB','A=(A-p) mod 256.'),('43','XOR','A=A XOR p.'),('44','RAND','A=floor(u32_rng×(p+1)/2^32), rovnoměrná mapa 0..p včetně.'),('46','AND','A=A AND p.'),('47','OR','A=A OR p.')]:op(code,name,'ANY','u8',desc,'Z=(A==0)')
op('45','CMP','ANY','u8','Porovnání A s p; A se nemění.','Z=(A==p)')
vid=[('50','PATTERN','0..8','Výběr vzoru 00 černá, 01 šum, 02 vodorovné, 03 svislé pruhy, 04 šachovnice, 05 bloky, 06 XOR, 07 generovaná bitmapa, 08 diagonály.'),('51','MOTION','signed_u8','int8(p)/16 pixelu na 40ms animační krok. SPEED nemění rychlost pohybu.'),('52','DENSITY','u8','Hustota p; pro pruhy den=max(1,4p).'),('53','LEVEL','u8','Jas světlých bodů p/255.'),('54','SCALE','u8','Měřítko 1+floor(p/32).'),('55','GLITCH','u8','Horní nibble síla, dolní pravděpodobnost zásahu za animační krok /16.'),('56','INVERT','u8','00 normální obraz, nenulové p inverze.'),('57','SHIFT','signed_u8','Horizontální posun int8(p) pixelů.'),('58','FEEDBACK','u8','Podíl předchozího snímku p/255. FF drží předchozí snímek.'),('59','SEED','u8','Nastaví video a VM PRNG podle specifikace.'),('5E','DENS_A','ignored','DENSITY převezme A.'),('5F','GLITCH_A','ignored','GLITCH převezme A.'),('60','VWAIT','u8','Dodatečné čekání p×40 ms, nezávisle na SPEED.'),('70','VJMP','even_address','Videové synonymum JMP.')]
for c,n,a,d in vid:op(c,n,'VIDEO',a,d)
aud=[('10','FREQ','u8','Frekvence 200+10p Hz.'),('11','WAVE','0..5','00 sinus, 01 pila, 02 obdélník, 03 trojúhelník, 04 bílý šum, 05 pulz.'),('12','VOLUME','u8','Úroveň p/255 násobená pevnou výstupní rezervou.'),('13','ATTACK','u8','Náběh p×4 ms.'),('14','DECAY','u8','Odeznění po GATE 00: p×4 ms.'),('15','GATE','0..1','00 zavře, 01 otevře obálku; jiné p chyba.'),('16','PULSE','u8','Střída clamp(p/255,0.01,0.99).'),('17','CRUSH','u8','01..07 hloubka 1..7 bitů; 00 nebo 08..FF bez redukce (8 bitů).'),('18','NOISE','u8','Příměs bílého šumu p/255.'),('19','LFO_RATE','u8','00 LFO vypnuto; jinak 0.1+0.05(p−1) Hz.'),('1A','LFO_DEPTH','u8','Relativní hloubka frekvenční modulace p/255.'),('1B','FILTER','u8','Dolní propust 100+5900p/255 Hz.'),('1E','FREQ_A','ignored','FREQ podle A: 200+10A Hz.'),('1F','VOL_A','ignored','VOLUME podle A/255.'),('20','AWAIT','u8','Dodatečné čekání p×20 ms, nezávisle na SPEED.')]
for c,n,a,d in aud:op(c,n,'AUDIO',a,d)
for c,n,a,d in [('80','PFREQ','u8','Nastaví frekvenci piezo 200+10p Hz. Změna platí i pro právě znějící tón.'),('81','PDUR','u8','Doba příštího PPLAY: p×10 ms; 00 znamená nepřetržitý tón. Výchozí 0A (100 ms).'),('82','PPLAY','ignored','Spustí/restartuje piezo tón s PFREQ a PDUR. Neblokuje VM.'),('83','PSTOP','ignored','Okamžitě vypne piezo; parametry zachová.'),('84','PFREQ_A','ignored','PFREQ podle A; Z a A zachová.')]:op(c,n,'ANY',a,d)
for c,n,a,d,z in [('90','TX','u8','Zařadí bajt p do TX FIFO. Při plné frontě čeká neblokujícím stavem do UTIME.', '0 úspěch, 1 timeout; A zachován'),('91','TX_A','ignored','Jako TX, odesílá A. Hodnotu A zachytí při fetchi.', '0 úspěch, 1 timeout; A zachován'),('92','RX','ignored','Převezme jeden bajt RX FIFO do A. Při prázdné frontě čeká do UTIME.', '0 přijat bajt včetně 00; 1 timeout, A zachován'),('93','TRYRX','ignored','Okamžitý RX bez čekání. Prázdná fronta nemění A ani sticky timeout.', '0 přijat bajt včetně 00; 1 fronta prázdná'),('94','RXREADY','ignored','A=min(počet bajtů RX FIFO,255); data nespotřebuje.', 'Z=(A==0)'),('95','UTIME','u8','Timeout RX/TX=p×10 ms, 00 bez časového limitu. Výchozí 64 hex = 1000 ms.', 'beze změny'),('96','USTAT','ignored','A=status UART: bit0 RX dostupná, bit1 TX má místo, bit2 RX overflow, bit3 RX timeout, bit4 TX timeout, bit5 line error; bity6/7=0.', 'Z=(A==0)'),('97','UCLR','u8','Vymaže sticky bity 2..5 označené p; p=00 maže všechny sticky bity. FIFO nemění, ostatní bity p ignoruje.', 'beze změny')]:op(c,n,'ANY',a,d,z)
rows.sort(key=lambda x:x['opcode'])
assert len(rows)==59 and len({r['opcode'] for r in rows})==len(rows)
(ROOT/'spec/instructions.json').write_text(json.dumps(dict(project='byteREEL 2026',isa_version='2.0',instruction_bytes=2,default_numeric_base=16,instructions=rows),ensure_ascii=False,indent=2)+'\n')
with (ROOT/'spec/instructions.csv').open('w') as f:
 w=csv.DictWriter(f,fieldnames=list(rows[0]));w.writeheader();w.writerows(rows)
s='# byteREEL 2026 — instrukční sada 2.0\n\nKaždá instrukce má dva bajty `[opcode][argument]`. Čísla bez prefixu jsou HEX. U ignorovaného argumentu zapisuj `00`. ANY = obě banky. Starých 45 kódů zachováno; nové kódy jsou 03, 80–84 a 90–97. Časování změněno podle `FIRMWARE-SPEC.md`.\n\n| HEX | BIN | Mnemonika | Banka | Argument | Účinek | Z |\n|---|---|---|---|---|---|---|\n'
for r in rows:s+='| '+ ' | '.join(str(r[k]).replace('|','/') for k in ['hex','binary','mnemonic','mode','argument','effect','z'])+' |\n'
(ROOT/'spec/INSTRUCTIONS.md').write_text(s)
catalog=json.dumps(dict(project='byteREEL 2026',isa_version='2.0',instruction_bytes=2,default_numeric_base=16,instructions=rows),ensure_ascii=False,separators=(',',':'))
(ROOT/'firmware/bytereel_2026/instruction_catalog.h').write_text(
    '#pragma once\n#include <Arduino.h>\nstatic const char BR_INSTRUCTION_CATALOG[] PROGMEM = R"BR26CAT('+catalog+')BR26CAT";\n')
defs=['#pragma once\n#include <stdint.h>\n#ifndef PROGMEM\n#define PROGMEM\n#endif\nstruct BrInstructionDef { uint8_t opcode; const char* mnemonic; const char* mode; const char* argument; };\nstatic const BrInstructionDef BR_ISA[59] PROGMEM = {']
for r in rows:
    defs.append('  {0x%02X,"%s","%s","%s"},'%(r['opcode'],r['mnemonic'],r['mode'],r['argument']))
defs.append('};\nstatic inline const BrInstructionDef* br_instruction(uint8_t code){for(unsigned i=0;i<59;i++)if(BR_ISA[i].opcode==code)return &BR_ISA[i];return nullptr;}\n')
(ROOT/'firmware/bytereel_2026/isa_table.h').write_text('\n'.join(defs))
