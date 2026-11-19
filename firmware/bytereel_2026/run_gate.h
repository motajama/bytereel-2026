#pragma once
#include <stdint.h>
// RUN input is active-low. Boot never counts as a STOP observation.
struct RunGate {
  bool sawStop=false;
  bool running=false;
  void boot(){sawStop=false;running=false;}
  bool sample(bool runActive){
    if(!runActive){sawStop=true;running=false;return false;}
    if(!running&&sawStop){sawStop=false;running=true;return true;}
    return false;
  }
  void stoppedByHalt(){running=false;sawStop=false;}
  void reset(){running=false;sawStop=false;}
  void modeChanged(bool physicallyStopped){running=false;sawStop=physicallyStopped;}
  void inputFault(){running=false;sawStop=false;}
};
