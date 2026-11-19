#include <cassert>
#include <fstream>
#include <iterator>
#include <cstdio>
#include "../firmware/bytereel_2026/assembler.h"
static std::string read(const char*p){std::ifstream f(p);return std::string((std::istreambuf_iterator<char>(f)),{});}
int main(){const char*names[]={"video-glitch","piezo-pulse","audio-pulses","uart-echo","uart-transmit","uart-audio"};const char*banks[]={"VIDEO","VIDEO","AUDIO","VIDEO","VIDEO","AUDIO"};for(int i=0;i<6;i++){std::string base=std::string("examples/")+names[i];BrAsmResult a=br_assemble(read((base+".asm").c_str()),banks[i]);std::ifstream f(base+".bin",std::ios::binary);uint8_t golden[256];f.read((char*)golden,256);assert(f.gcount()==256);assert(!memcmp(a.bytes,golden,256));}
 auto a=br_assemble("JMP finish\nORG 04\nDB 0b10101010, %11110000\nfinish: HALT\n","VIDEO");assert(a.bytes[0]==0x30&&a.bytes[1]==6&&a.bytes[4]==0xaa&&a.bytes[5]==0xf0&&a.bytes[6]==1);
 const char*bad[]={"FREQ 20","JMP 03","COUNT 00","WAVE 06","GATE 02","PATTERN 09","ORG 01\nHALT","00: DB 11 22\n01: DB 33","SET"};const char*mods[]={"VIDEO","VIDEO","VIDEO","AUDIO","AUDIO","VIDEO","VIDEO","VIDEO","VIDEO"};for(int i=0;i<9;i++){bool failed=false;try{br_assemble(bad[i],mods[i]);}catch(const std::exception&){failed=true;}assert(failed);}
 std::puts("PASS C++ assembler: six golden examples, labels/ORG/DB, mode/range/alignment/overlap failures");}
