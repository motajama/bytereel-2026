#!/usr/bin/env python3
"""Validate specification artifacts, not the future firmware."""
import json,re,csv
from pathlib import Path
R=Path(__file__).resolve().parents[1]
ops=json.loads((R/'spec/instructions.json').read_text())['instructions']
byname={o['mnemonic']:o for o in ops}
def assemble(source,bank):
    mem=bytearray(256); used=set(); pc=0
    for line in source.splitlines():
        line=line.split(';')[0].strip()
        if not line:continue
        parts=line.split(); name=parts[0].upper()
        if name=='ORG':pc=int(parts[1],16);continue
        o=byname[name]; assert o['mode'] in ('ANY',bank)
        p=int(parts[1],16) if len(parts)>1 else 0
        assert len(parts)>1 or o['argument']=='ignored'
        assert 0<=p<=255 and 0<=pc<=254 and pc%2==0
        if o['argument']=='even_address':assert p%2==0
        if o['argument']=='1..255':assert p>=1
        if o['argument'] in ('0..8','0..5','0..1'):assert p<=int(o['argument'][-1])
        assert pc not in used and pc+1 not in used
        mem[pc:pc+2]=bytes([o['opcode'],p]);used.update((pc,pc+1));pc+=2
    return mem
if __name__=='__main__':
    assert len(ops)==59 and len({o['opcode'] for o in ops})==59
    for o in ops:assert int(o['hex'],16)==o['opcode']==int(o['binary'],2) and o['size']==2
    legacy=(R/'reference/legacy-emulator-1.0.html').read_text()
    codes={int(x,16) for x in re.findall(r'case 0x([0-9A-Fa-f]+):',legacy)}
    assert len(codes)==45 and codes.issubset({o['opcode'] for o in ops})
    end=0
    for row in csv.reader((R/'partitions.csv').read_text().splitlines()):
        if not row or row[0].startswith('#'):continue
        offset,size=int(row[3].strip(),16),int(row[4].strip(),16)
        assert offset>=end and offset%4096==0 and size%4096==0
        end=offset+size
    assert end==0x400000
    examples=json.loads((R/'examples/manifest.json').read_text())
    for e in examples:
        expected=(R/'examples'/e['binary']).read_bytes()
        actual=assemble((R/'examples'/e['source']).read_text(),e['bank'])
        assert len(expected)==256 and actual==expected,e['source']
    slot=json.loads((R/'examples/slot-00.json').read_text())
    for key in ['video','audio']:
        assert len(bytes.fromhex(slot[key]['hex']))==256
        assert assemble(slot[key]['source'],key.upper()).hex().upper()==slot[key]['hex']
    print('PASS: 59 unique opcode; all 45 legacy codes retained; HEX/BIN consistent.')
    print('PASS: partition aligned, no overlap, end=4 MiB; '+str(len(examples))+' examples match 256-byte golden images.')
    print('PASS: paired slot JSON source/bytecode consistent. Firmware runtime/hardware NOT TESTED (specification package).')
