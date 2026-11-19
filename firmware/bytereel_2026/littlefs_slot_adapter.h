#pragma once
#include <FS.h>

struct BrLittleFsStore {
  fs::FS &fs; const char *directory; bool available;const char *lastError="NONE";
  BrLittleFsStore(fs::FS &filesystem,const char *dir,bool mounted):fs(filesystem),directory(dir),available(mounted){}
  String path(uint8_t id,char side)const{char name[32];snprintf(name,sizeof(name),"%s/%02u.%c.brp",directory,id,side);return String(name);}
  int read(uint8_t id,char side,uint8_t*out,size_t cap){
    lastError="NONE";if(!available||id>7){lastError="READ_NOT_AVAILABLE";return -2;}String filePath=path(id,side);if(!fs.exists(filePath))return -1;
    File f=fs.open(filePath,"r");if(!f){lastError="READ_OPEN_FAILED";return -2;}size_t n=f.size();if(n>cap){f.close();lastError="READ_OVERSIZE";return -2;}
    size_t got=f.read(out,n);f.close();if(got!=n){lastError="READ_SHORT";return -2;}return (int)n;
  }
  bool write(uint8_t id,char side,const uint8_t*data,size_t n){
    lastError="NONE";if(!available||id>7){lastError="WRITE_NOT_AVAILABLE";return false;}if(!fs.exists(directory)){lastError="WRITE_DIRECTORY_MISSING";return false;}File f=fs.open(path(id,side),"w");if(!f){lastError="WRITE_OPEN_FAILED";return false;}
    size_t wrote=f.write(data,n);f.flush();f.close();if(wrote!=n){lastError="WRITE_SHORT";return false;}return true;
  }
  bool remove(uint8_t id,char side){
    lastError="NONE";if(!available||id>7){lastError="DELETE_NOT_AVAILABLE";return false;}String filePath=path(id,side);
    if(!fs.exists(filePath))return true;if(!fs.remove(filePath)){lastError="DELETE_FAILED";return false;}return true;
  }
};
