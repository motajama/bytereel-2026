#pragma once
#include <stdint.h>
#include <ctype.h>
#include <cstring>
#include <map>
#include <string>
#include <vector>
#include <stdexcept>
#include "isa_table.h"

struct BrAsmResult { uint8_t bytes[256]; unsigned written; std::map<std::string,uint8_t> labels; };
struct BrAsmRecord { unsigned line,pc;std::string op;std::vector<std::string> values;bool instruction; };
static inline std::string br_trim(const std::string&s){size_t a=0,b=s.size();while(a<b&&isspace((unsigned char)s[a]))++a;while(b>a&&isspace((unsigned char)s[b-1]))--b;return s.substr(a,b-a);}
static inline std::string br_upper(std::string s){for(char &c:s)c=(char)toupper((unsigned char)c);return s;}
static inline bool br_symbol(const std::string&s){if(s.empty()||!(isalpha((unsigned char)s[0])||s[0]=='_'))return false;for(char c:s)if(!(isalnum((unsigned char)c)||c=='_'))return false;return true;}
static inline const BrInstructionDef* br_mnemonic(const std::string&s){for(unsigned i=0;i<59;i++)if(s==BR_ISA[i].mnemonic)return &BR_ISA[i];return nullptr;}
static inline int br_hex_digit(char c){if(c>='0'&&c<='9')return c-'0';c=(char)toupper((unsigned char)c);if(c>='A'&&c<='F')return c-'A'+10;return -1;}
static inline uint8_t br_value(const std::string&token,const std::map<std::string,uint8_t>&labels,unsigned line){std::string t=br_upper(token);long v=-1;try{if(t.size()>2&&t.substr(0,2)=="0D")v=std::stol(t.substr(2),nullptr,10);else if(t.size()>2&&t.substr(0,2)=="0B"){v=0;for(size_t i=2;i<t.size();i++){if(t[i]!='0'&&t[i]!='1')throw std::runtime_error("invalid");v=v*2+(t[i]-'0');}}else if(t.size()>1&&t[0]=='%'){v=0;for(size_t i=1;i<t.size();i++){if(t[i]!='0'&&t[i]!='1')throw std::runtime_error("invalid");v=v*2+(t[i]-'0');}}else if(t.size()>1&&t[0]=='$')v=std::stol(t.substr(1),nullptr,16);else if(t.size()>2&&t.substr(0,2)=="0X")v=std::stol(t.substr(2),nullptr,16);else if(t.size()<=2&&!t.empty()){v=0;for(char c:t){int d=br_hex_digit(c);if(d<0)throw std::runtime_error("invalid");v=v*16+d;}}}catch(...){v=-1;}
  if(v<0){auto it=labels.find(t);if(it==labels.end())throw std::runtime_error("line "+std::to_string(line)+": unknown value "+token);v=it->second;}if(v>255)throw std::runtime_error("line "+std::to_string(line)+": byte out of range");return (uint8_t)v;}
static inline std::vector<std::string> br_tokens(std::string s){for(char &c:s)if(c==',')c=' ';std::vector<std::string> out;std::string word;for(char c:s){if(isspace((unsigned char)c)){if(!word.empty()){out.push_back(word);word.clear();}}else word+=c;}if(!word.empty())out.push_back(word);return out;}
static inline uint8_t br_org(const std::string&t,unsigned line){std::map<std::string,uint8_t> empty;return br_value(t,empty,line);}
static inline BrAsmResult br_assemble(const std::string&source,const char*bank){
  BrAsmResult result{};std::vector<BrAsmRecord> records;unsigned pc=0,lineNo=0;std::map<std::string,uint8_t> labels;size_t pos=0;
  while(pos<=source.size()){size_t end=source.find('\n',pos);if(end==std::string::npos)end=source.size();std::string line=source.substr(pos,end-pos);++lineNo;pos=end+1;size_t comment=line.find_first_of(";#");if(comment!=std::string::npos)line.resize(comment);line=br_trim(line);if(line.empty()){if(end==source.size())break;continue;}
    if(line.size()>=3&&br_hex_digit(line[0])>=0&&br_hex_digit(line[1])>=0&&line[2]==':'){pc=(unsigned)(br_hex_digit(line[0])*16+br_hex_digit(line[1]));line=br_trim(line.substr(3));if(line.empty()){if(end==source.size())break;continue;}}
    for(;;){size_t colon=line.find(':');if(colon==std::string::npos)break;std::string candidate=br_trim(line.substr(0,colon));if(!br_symbol(candidate))break;std::string name=br_upper(candidate);if(br_mnemonic(name)||labels.count(name))throw std::runtime_error("line "+std::to_string(lineNo)+": reserved or duplicate label "+name);if(pc>255)throw std::runtime_error("line "+std::to_string(lineNo)+": label outside bank");labels[name]=(uint8_t)pc;line=br_trim(line.substr(colon+1));if(line.empty())break;}
    if(line.empty()){if(end==source.size())break;continue;}auto t=br_tokens(line);if(t.empty())continue;std::string op=br_upper(t[0]);
    if(op=="ORG"){if(t.size()!=2)throw std::runtime_error("line "+std::to_string(lineNo)+": ORG requires one address");pc=br_org(t[1],lineNo);continue;}
    const BrInstructionDef* def=br_mnemonic(op);if(def){if(strcmp(def->mode,"ANY")&&strcmp(def->mode,bank))throw std::runtime_error("line "+std::to_string(lineNo)+": wrong bank for "+op);if(pc&1)throw std::runtime_error("line "+std::to_string(lineNo)+": instruction at odd address");if(t.size()>2)throw std::runtime_error("line "+std::to_string(lineNo)+": too many arguments");if(t.size()==1&&strcmp(def->argument,"ignored"))throw std::runtime_error("line "+std::to_string(lineNo)+": missing operand for "+op);std::vector<std::string> vals;vals.push_back(def->mnemonic);vals.push_back(t.size()==2?t[1]:"00");records.push_back({lineNo,pc,op,vals,true});pc+=2;}
    else{std::vector<std::string> vals;if(op=="DB")vals.assign(t.begin()+1,t.end());else if(t.size()==2)vals=t;else throw std::runtime_error("line "+std::to_string(lineNo)+": use mnemonic, DB or raw HEX pair");if(vals.empty())throw std::runtime_error("line "+std::to_string(lineNo)+": DB needs data");records.push_back({lineNo,pc,op,vals,false});pc+=(unsigned)vals.size();}
    if(pc>256)throw std::runtime_error("line "+std::to_string(lineNo)+": memory overflow");if(end==source.size())break;
  }
  bool written[256]={};result.labels=labels;for(const auto&r:records){std::vector<uint8_t> data;if(r.instruction){const BrInstructionDef*def=br_mnemonic(r.op);data.push_back(def->opcode);data.push_back(br_value(r.values[1],labels,r.line));const char*arg=def->argument;if(!strcmp(arg,"even_address")&&(data[1]&1))throw std::runtime_error("line "+std::to_string(r.line)+": odd jump target");if(!strcmp(arg,"1..255")&&!data[1])throw std::runtime_error("line "+std::to_string(r.line)+": COUNT requires 01..FF");if(!strcmp(r.op.c_str(),"WAVE")&&data[1]>5)throw std::runtime_error("line "+std::to_string(r.line)+": WAVE range 00..05");if(!strcmp(r.op.c_str(),"PATTERN")&&data[1]>8)throw std::runtime_error("line "+std::to_string(r.line)+": PATTERN range 00..08");if(!strcmp(r.op.c_str(),"GATE")&&data[1]>1)throw std::runtime_error("line "+std::to_string(r.line)+": GATE range 00..01");}else for(const auto&v:r.values)data.push_back(br_value(v,labels,r.line));
    for(size_t i=0;i<data.size();i++){unsigned at=r.pc+i;if(at>=256)throw std::runtime_error("line "+std::to_string(r.line)+": memory overflow");if(written[at])throw std::runtime_error("line "+std::to_string(r.line)+": overlapping write");written[at]=true;result.bytes[at]=data[i];++result.written;}}
  return result;
}
