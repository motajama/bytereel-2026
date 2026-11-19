#!/usr/bin/env python3
"""Host-side reference assembler using the canonical spec/instructions.json."""
import json,re
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]
TABLE={x['mnemonic']:x for x in json.loads((ROOT/'spec/instructions.json').read_text())['instructions']}
def number(s, labels, line):
    u=s.upper()
    if u in labels:return labels[u]
    if u.startswith('0D') and u[2:].isdigit():v=int(u[2:],10)
    elif u.startswith('0B') and re.fullmatch(r'0B[01]+',u):v=int(u[2:],2)
    elif u.startswith('%') and re.fullmatch(r'%[01]+',u):v=int(u[1:],2)
    elif u.startswith('$'):v=int(u[1:],16)
    elif u.startswith('0X'):v=int(u[2:],16)
    elif re.fullmatch(r'[0-9A-F]{1,2}',u):v=int(u,16)
    else:raise ValueError(f'line {line}: unknown value {s}')
    if not 0<=v<=255:raise ValueError(f'line {line}: byte out of range')
    return v
def assemble(source,bank='ANY'):
    pc=0;labels={};records=[]
    for ln,raw in enumerate(source.splitlines(),1):
        line=re.split(r'[;#]',raw,1)[0].strip()
        if not line:continue
        addr=re.match(r'^([0-9A-Fa-f]{2}):\s*',line)
        if addr:pc=int(addr.group(1),16);line=line[addr.end():].strip()
        while True:
            m=re.match(r'^([A-Za-z_][A-Za-z_0-9]*):\s*',line)
            if not m:break
            name=m.group(1).upper()
            if name in labels or name in TABLE:raise ValueError(f'line {ln}: duplicate/reserved label {name}')
            labels[name]=pc;line=line[m.end():].strip()
        if not line:continue
        t=line.replace(',',' ').split();op=t[0].upper()
        if op=='ORG':
            if len(t)!=2:raise ValueError(f'line {ln}: ORG requires one byte address')
            pc=number(t[1],labels,ln);continue
        if op=='DB': vals=t[1:];size=len(vals)
        elif op in TABLE:
            row=TABLE[op]
            if len(t)>2:raise ValueError(f'line {ln}: too many arguments')
            arg=t[1] if len(t)>1 else '00'
            if row['mode'] not in ('ANY',bank):raise ValueError(f'line {ln}: {op} not valid in {bank}')
            size=2;vals=[row['hex'],arg]
        else:
            if len(t)!=2:raise ValueError(f'line {ln}: raw data requires two hex bytes')
            vals=t;size=2
        if op in TABLE and pc&1:raise ValueError(f'line {ln}: instruction at odd address')
        if not vals or pc+size>256:raise ValueError(f'line {ln}: memory overflow or empty DB')
        records.append((ln,pc,vals,op));pc+=size
    out=bytearray(256);written=set()
    for ln,at,vals,op in records:
        data=[number(x,labels,ln) for x in vals]
        if op in TABLE:
            row=TABLE[op]
            if row['argument']=='1..255' and data[1]==0:raise ValueError(f'line {ln}: COUNT requires 01..FF')
            if op=='WAVE' and data[1]>5:raise ValueError(f'line {ln}: WAVE range is 00..05')
            if op=='GATE' and data[1]>1:raise ValueError(f'line {ln}: GATE range is 00..01')
            if op=='PATTERN' and data[1]>8:raise ValueError(f'line {ln}: PATTERN range is 00..08')
        if op in TABLE and TABLE[op]['argument']=='even_address' and data[1]&1:raise ValueError(f'line {ln}: odd jump target')
        for i,b in enumerate(data):
            p=at+i
            if p in written:raise ValueError(f'line {ln}: overlapping write at {p:02X}')
            written.add(p);out[p]=b
    return bytes(out)

if __name__=='__main__':
    import argparse
    ap=argparse.ArgumentParser();ap.add_argument('source',type=Path);ap.add_argument('--bank',default='ANY');ap.add_argument('--output',type=Path);args=ap.parse_args()
    result=assemble(args.source.read_text(),args.bank)
    if args.output:args.output.write_bytes(result)
    else:print(result.hex().upper())
