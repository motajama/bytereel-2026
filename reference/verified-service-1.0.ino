/* BITBOX SERVICE 1.0 -- ESP32-WROOM-32, Arduino ESP32 core 2.0.14
   Canonical wiring 2026-11-19. No BASIC bank / flash writes.
   Input expander: READ ONLY. External 10k pullups required.
   Serial 115200, h = help. See README-CS.md before connector tests.
*/
#include <Arduino.h>
#include <Wire.h>
#include <math.h>
#include <esp_timer.h>
#include <esp_system.h>
#include <ESP_8_BIT_GFX.h>
#include <esp_arduino_version.h>
#if ESP_ARDUINO_VERSION_MAJOR != 2
#error "Select esp32 by Espressif Systems version 2.0.14 (Tools / Boards Manager)."
#endif

constexpr uint8_t LED_ADDR=0x20, INPUT_ADDR=0x22;
constexpr int POT=33, PIEZO=23, AUDIO=26, TX_PIN=27, RX_PIN=32;
constexpr uint8_t MODE_PINS[5]={13,14,16,17,18};
constexpr uint32_t ALL_INPUTS=(1UL<<19)-1;
constexpr uint32_t VIDEO_MAGIC=0xB17B025;
RTC_DATA_ATTR uint32_t nextBoot=0;
// Bits 0..7 D0..D7, bits 8..13 P10..P15; no historical pair swap needed.
const char* const NAMES[19]={"D0","D1","D2","D3","D4","D5","D6","D7",
  "HOME","RESET","WRITE","WRITE+","EXP/EXAM","NEXT",
  "RUN","VIDEO","LIVE","AUTO","EDIT"};
ESP_8_BIT_GFX screen(false,8); // PAL, RGB332, GPIO25. begin() only in video mode.
bool videoMode=false, validInputs=false, haveCandidate=false;
uint32_t stable=0, candidate=0, since[19]={0};
uint32_t seen0=0,seen1=0,changes[19]={0};
uint32_t readErrors=0,writeErrors=0,lastReport=0,lastLedWrite=0;
bool ledOK=false,inputOK=false;
uint16_t rawInput=0, lastLeds=0;
uint16_t potMin=4095,potMax=0,potRaw=0;
float potFiltered=0;
uint32_t intervalMs=1000;
uint32_t scanStart=0;
bool ledScan=false;
uint8_t chase=0;
uint32_t lastChase=0;
bool observedLED=false,observedPiezo=false,observedAudio=false,observedVideo=false;
bool triedLED=false,triedPiezo=false,triedAudio=false;
uint32_t piezoStart=0;
bool piezoActive=false;
int lastPiezoHz=-1;
esp_timer_handle_t audioTimer=nullptr;
uint8_t sineLut[32];
volatile uint8_t audioStep=1,audioPhase=0;
bool audioActive=false;
uint32_t audioStart=0;
bool uartActive=false;
uint8_t uartSent[32],uartGot[64];
size_t uartCount=0;
uint32_t uartStart=0,uartPass=0,uartFail=0;

bool leds(uint16_t lit) {
  uint16_t electrical=~lit;
  Wire.beginTransmission(LED_ADDR);
  Wire.write(uint8_t(electrical)); Wire.write(uint8_t(electrical>>8));
  uint8_t e=Wire.endTransmission();
  bool ok=(e==0);
  if(!ok) {
    ++writeErrors;
    if(ledOK || writeErrors==1) Serial.printf("CHYBA LED I2C: %u (nepotvrzuje stav samotnych LED)\n",e);
  } else if(!ledOK && writeErrors) Serial.println("LED I2C: komunikace obnovena; chyba zustava v souhrnu.");
  ledOK=ok; lastLedWrite=millis(); if(ok) lastLeds=lit;
  return ok;
}
uint16_t pairMask(uint8_t step) {
  uint8_t row=(1U<<step)|(1U<<(7-step));
  return uint16_t(row)|(uint16_t(row)<<8);
}
void initLEDs() {
  triedLED=true;
  leds(0); delay(150);
  for(int i=0;i<2;i++){leds(0xFFFF);delay(200);leds(0);delay(200);}
  uint16_t mask=0;
  for(int i=0;i<4;i++){mask|=pairMask(i);leds(mask);delay(150);}
  delay(1000);
  for(int i=0;i<4;i++){mask&=~pairMask(i);leds(mask);delay(150);}
  leds(0);
}
void scanBus() {
  Serial.println("I2C scan (vstupnimu expanderu neposilam zadna data):");
  for(uint8_t a=0x08;a<0x78;a++) {
    Wire.beginTransmission(a);uint8_t e=Wire.endTransmission();
    if(e==0) Serial.printf("  0x%02X %s\n",a,a==LED_ADDR?"LED":a==INPUT_ADDR?"PREPINACE":"NEOCEKAVANA ADRESA");
  }
  Serial.println("Ocekavam 0x20 a 0x22. Ani nalez adresy neoveruje vsechny spoje.");
}
void chirpTest() {triedPiezo=true;piezoStart=millis();piezoActive=true;}
void updatePiezo() {
  uint32_t elapsed=millis()-piezoStart;
  int hz=0;
  if(piezoActive) {
    if(elapsed<180) hz=900;
    else if(elapsed>=280 && elapsed<460) hz=1500;
    else if(elapsed>=560 && elapsed<800) hz=2300;
    else if(elapsed>=800) piezoActive=false;
  }
  if(hz!=lastPiezoHz){ledcWriteTone(0,hz);lastPiezoHz=hz;}
}
void audioTick(void*) {
  audioPhase=(audioPhase+audioStep)&31;
  dacWrite(AUDIO,sineLut[audioPhase]);
}
void stopAudio() {
  if(audioActive){esp_timer_stop(audioTimer);audioActive=false;}
  if(!videoMode) dacWrite(AUDIO,128);
}
void startAudio() {
  if(videoMode){Serial.println("Audio DAC se testuje v normalnim rezimu: prikaz n.");return;}
  stopAudio(); audioStep=1;audioPhase=0;
  esp_err_t e=audioTimer?esp_timer_start_periodic(audioTimer,125):ESP_FAIL;
  if(e!=ESP_OK){Serial.printf("CHYBA casovace audio: %d\n",int(e));return;}
  audioActive=true;triedAudio=true;audioStart=millis();
  Serial.println("AUDIO RCA: 250 / 500 / 1000 Hz, kazdy 2 s, pak ticho. Potvrd 3 az po poslechu.");
}
void updateAudio() {
  if(!audioActive)return;
  uint32_t e=millis()-audioStart;
  if(e>=6000){stopAudio();Serial.println("Audio test skoncil.");}
  else audioStep=e<2000?1:e<4000?2:4;
}
void startUART() {
  if(uartActive)return;
  Serial.println("UART: vyzaduje propojku GX12 pin 1--2, periferie odpojene. GND je pin 3.");
  while(Serial2.available())Serial2.read();
  for(int i=0;i<32;i++) uartSent[i]=uint8_t((i*73)^0xA5);
  uartSent[0]=0;uartSent[1]=0xFF;uartSent[2]=0x55;uartSent[3]=0xAA;
  uartCount=0;uartStart=millis();uartActive=true;
  if(Serial2.write(uartSent,sizeof(uartSent))!=sizeof(uartSent)) {
    uartActive=false;++uartFail;Serial.println("UART FAIL: neodeslan cely ramec.");
  }
}
void updateUART() {
  if(!uartActive)return;
  while(Serial2.available()) {
    int b=Serial2.read();
    if(uartCount<sizeof(uartGot))uartGot[uartCount++]=uint8_t(b);
    else {++uartFail;uartActive=false;Serial.println("UART FAIL: prilis mnoho dat.");return;}
  }
  if(millis()-uartStart<500)return;
  bool ok=uartCount==sizeof(uartSent) && memcmp(uartSent,uartGot,sizeof(uartSent))==0;
  if(ok)++uartPass;else ++uartFail;
  Serial.printf("UART %s: prijato %u/32 bajtu.\n",ok?"PASS (shoda dat)":"FAIL / chybi propojka",unsigned(uartCount));
  uartActive=false;
}
void sampleInputs() {
  uint8_t count=Wire.requestFrom(INPUT_ADDR,uint8_t(2));
  if(count!=2 || Wire.available()!=2) {
    while(Wire.available())Wire.read();
    if(inputOK || readErrors==0)Serial.println("CHYBA: cteni vstupu 0x22. Zobrazovane polohy nejsou platne.");
    ++readErrors;inputOK=false;validInputs=false;haveCandidate=false;return;
  }
  if(!inputOK && readErrors)Serial.println("Vstupni I2C obnoveno; cekam na ustaleni prepinacu.");
  inputOK=true;
  rawInput=uint8_t(Wire.read());rawInput|=uint16_t(uint8_t(Wire.read()))<<8;
  uint32_t value=(~rawInput)&0x3FFF;
  for(int i=0;i<5;i++)if(digitalRead(MODE_PINS[i])==LOW)value|=1UL<<(14+i);
  uint32_t now=millis();
  if(!haveCandidate){candidate=value;for(int i=0;i<19;i++)since[i]=now;haveCandidate=true;return;}
  if(!validInputs) {
    if(value!=candidate){candidate=value;for(int i=0;i<19;i++)since[i]=now;return;}
    if(now-since[0]<30)return;
    stable=value;validInputs=true;
    Serial.println("Vstupy ustaleny. Preklop kazdy prepinac do obou poloh; vratne packy uvolni.");
  }
  for(int i=0;i<19;i++) {
    uint32_t bit=1UL<<i;
    if((value&bit)!=(candidate&bit)){candidate^=bit;since[i]=now;}
    if(now-since[i]>=30) {
      if((candidate&bit)!=(stable&bit)) {
        stable^=bit;++changes[i];
        Serial.printf("%-9s : %s\n",NAMES[i],i<8?((stable&bit)?"1":"0"):
          i<14?((stable&bit)?"STISKNUTO":"UVOLNENO"):
          i==14?((stable&bit)?"RUN":"STOP"):
          i==15?((stable&bit)?"VIDEO":"SOUND"):((stable&bit)?"ZAP":"VYP"));
      }
      if(stable&bit)seen1|=bit;else seen0|=bit;
    }
  }
}
void samplePot() {
  potRaw=analogRead(POT);
  if(potRaw<potMin)potMin=potRaw;if(potRaw>potMax)potMax=potRaw;
  potFiltered+=0.15f*(potRaw-potFiltered);
  intervalMs=lroundf(1000.0f*powf(0.005f,potFiltered/4095.0f));
}
void printStatus() {
  Serial.printf("%s | DATA ",validInputs?"PANEL":"NEPLATNE VSTUPY");
  if(validInputs) {
    for(int i=7;i>=0;i--)Serial.print((stable>>i)&1);
    Serial.printf(" = 0x%02X = %u | %s %s LIVE:%u AUTO:%u EDIT:%u\n",unsigned(stable&255),unsigned(stable&255),
      stable&(1UL<<14)?"RUN":"STOP",stable&(1UL<<15)?"VIDEO":"SOUND",
      unsigned((stable>>16)&1),unsigned((stable>>17)&1),unsigned((stable>>18)&1));
    Serial.print("PACKY: ");bool any=false;
    for(int i=8;i<14;i++)if(stable&(1UL<<i)){Serial.print(NAMES[i]);Serial.print(' ');any=true;}
    if(!any)Serial.print("uvolnene");Serial.println();
    for(int i=8;i<14;i+=2)if(((stable>>i)&3)==3)Serial.printf("CHYBA / zkrat? Soucasne %s a %s!\n",NAMES[i],NAMES[i+1]);
  } else Serial.println("---");
  Serial.printf("SPEED ADC:%u filt:%.0f | %lu ms | %.1f IPS zadano | ADC rozsah:%u..%u\n",
    potRaw,potFiltered,(unsigned long)intervalMs,1000.0f/intervalMs,potMin,potMax);
  Serial.printf("I2C LED:%s IN:%s | chyby zapis:%lu cteni:%lu | uptime:%lus | heap:%u\n",
    ledOK?"OK":"CHYBA",inputOK?"OK":"CHYBA",(unsigned long)writeErrors,(unsigned long)readErrors,
    (unsigned long)(millis()/1000),ESP.getFreeHeap());
}
void report() {
  Serial.println("\n=== SERVISNI PROTOKOL BITBOX 1.0 ===");printStatus();
  for(int i=0;i<19;i++) {
    uint32_t bit=1UL<<i;
    Serial.printf("%-9s : %s | zmen:%lu\n",NAMES[i],(seen0&seen1&bit)?"OBE POLOHY ZAZNAMENANY":
      !(seen0&bit)?"CHYBI 0 / UVOLNENO":"CHYBI 1 / STISKNUTO",(unsigned long)changes[i]);
  }
  Serial.printf("Potenciometr: %s (orientacni meze <=150 a >=3945).\n",potMin<=150 && potMax>=3945?"OBA KONCE DOSAZENY":"NEPROVEREN CELY ROZSAH");
  Serial.printf("LED vizualne:%s | PIEZO poslech:%s | AUDIO poslech:%s | VIDEO obraz:%s\n",
    observedLED?"POTVRZENO":"NEPOTVRZENO",observedPiezo?"POTVRZENO":"NEPOTVRZENO",
    observedAudio?"POTVRZENO":"NEPOTVRZENO",observedVideo?"POTVRZENO":"NEPOTVRZENO");
  Serial.printf("UART loopback PASS:%lu FAIL:%lu\n",(unsigned long)uartPass,(unsigned long)uartFail);
  Serial.println("Zaznam poloh neni dukaz spravneho popisku: porovnej jmeno s fyzickou packou.");
  Serial.println("Napajeni, POWER LED, kvalitu zvuku/obrazu a GX12 GND over clovek. Zadny automaticky celkovy PASS.");
}
void help() {
  Serial.println("\nBITBOX SERVICE 1.0 | 115200 baud | posli jedno pismeno + Enter");
  Serial.println("h napoveda | r protokol | s I2C scan | x vymazat vysledky teto relace");
  Serial.println("l init LED + postupny test kazde LED | p tri tony piezo");
  Serial.println("a audio RCA 250/500/1000 Hz (6 s) | u UART loopback (GX12 1--2)");
  Serial.println("v restart do PAL VIDEO testu | n restart do normalniho testu");
  Serial.println("q zastavit audio/piezo/LED sekvenci (PAL vypnes prikazem n)");
  Serial.println("Potvrzeni clovekem: 1 LED viditelne OK, 2 piezo slyset, 3 audio slyset, 4 PAL obraz OK");
  Serial.println("Kazdy restart maze protokol: pred v/n si uloz vystup r. Programove banky se nemeni.");
}
void resetResults() {
  seen0=seen1=0;memset(changes,0,sizeof(changes));potMin=4095;potMax=0;
  readErrors=writeErrors=uartPass=uartFail=0;
  observedLED=observedPiezo=observedAudio=observedVideo=false;
  triedLED=triedPiezo=triedAudio=false;
  Serial.println("Statistiky nove relace vynulovany. Drzene polohy budou znovu zaznamenany.");
}
void commands() {
  while(Serial.available()) {
    char c=Serial.read();
    switch(c) {
      case 'h':help();break;
      case 'r':report();break;
      case 's':scanBus();break;
      case 'x':resetResults();break;
      case 'l':stopAudio();initLEDs();ledScan=true;scanStart=millis();break;
      case 'p':chirpTest();break;
      case 'a':startAudio();break;
      case 'u':startUART();break;
      case 'q':stopAudio();piezoActive=false;ledScan=false;updatePiezo();leds(0);break;
      case 'v':case 'n':
        report();stopAudio();ledcWriteTone(0,0);leds(0);
        Serial.println(c=='v'?"Restartuji do PAL testu.":"Restartuji do paneloveho testu.");
        Serial.flush();nextBoot=c=='v'?VIDEO_MAGIC:0;ESP.restart();break;
      case '1':if(triedLED){observedLED=true;Serial.println("Clovek potvrdil LED.");}else Serial.println("Nejprve l.");break;
      case '2':if(triedPiezo){observedPiezo=true;Serial.println("Clovek potvrdil piezo.");}else Serial.println("Nejprve p.");break;
      case '3':if(triedAudio){observedAudio=true;Serial.println("Clovek potvrdil audio.");}else Serial.println("Nejprve a.");break;
      case '4':if(videoMode){observedVideo=true;Serial.println("Clovek potvrdil PAL obraz.");}else Serial.println("Nejprve v.");break;
      case '\r':case '\n':case ' ':break;
      default:Serial.println("Neznamy prikaz; h = napoveda.");
    }
  }
}
void panelLEDs() {
  uint16_t pattern=0;
  if(ledScan) {
    uint32_t stage=(millis()-scanStart)/300;
    if(stage<16)pattern=1UL<<stage;
    else if(stage<32)pattern=uint16_t(~(1UL<<(stage-16)));
    else {ledScan=false;Serial.println("LED test hotov. Pokud kazda svitila i zhasla, posli 1.");}
  } else if(validInputs) {
    uint8_t upper=(stable>>8)&0x3F;
    if((stable&(1UL<<14)) && upper==0) {
      if(millis()-lastChase>=intervalMs){lastChase=millis();chase=(chase+1)&7;}
      upper=1U<<chase;
    }
    pattern=(stable&255)|(uint16_t(upper)<<8);
  }
  if(pattern!=lastLeds || millis()-lastLedWrite>=100)leds(pattern);
}
void drawVideo() {
  screen.waitForFrame();
  screen.fillScreen(0);
  const uint8_t colors[8]={0xFF,0xFC,0x1F,0x1C,0xE3,0xE0,0x03,0x00};
  for(int i=0;i<8;i++)screen.fillRect(i*32,28,32,56,colors[i]);
  for(int i=0;i<8;i++)screen.fillRect(i*32,90,32,20,uint8_t((i*7/7)<<5 | (i*7/7)<<2 | (i*3/7)));
  screen.drawRect(8,8,240,224,0xFF);
  screen.setTextColor(0xFF);screen.setTextSize(1);screen.setCursor(20,16);screen.print("BITBOX SERVICE - PAL");
  screen.setCursor(20,122);screen.printf("UPTIME %lus",(unsigned long)(millis()/1000));
  screen.setCursor(20,138);screen.printf("DATA %02X RAW %04X",unsigned(stable&255),rawInput);
  screen.setCursor(20,154);screen.printf("SPEED %lu ms %.1f IPS",(unsigned long)intervalMs,1000.0f/intervalMs);
  screen.setCursor(20,170);screen.print(validInputs?"INPUT OK":"INPUT ERROR");
  screen.fillRect(16+(millis()/20)%210,190,8,12,0xFF);
  screen.setCursor(20,214);screen.print("SERIAL: 4=OK  n=EXIT");
}
void setup() {
  Serial.begin(115200);delay(500);
  videoMode=(esp_reset_reason()==ESP_RST_SW && nextBoot==VIDEO_MAGIC);nextBoot=0;
  Serial.printf("\nBITBOX SERVICE 1.0 | reset=%d | CPU=%u MHz | rezim:%s\n",int(esp_reset_reason()),ESP.getCpuFreqMHz(),videoMode?"PAL":"PANEL/AUDIO");
  for(uint8_t pin:MODE_PINS)pinMode(pin,INPUT_PULLUP);
  analogReadResolution(12);pinMode(POT,INPUT);potFiltered=analogRead(POT);
  pinMode(RX_PIN,INPUT_PULLUP);Serial2.begin(9600,SERIAL_8N1,RX_PIN,TX_PIN);
  Wire.begin(21,22);Wire.setClock(10000);Wire.setTimeOut(25);
  ledcSetup(0,2000,10);ledcAttachPin(PIEZO,0);ledcWriteTone(0,0);
  initLEDs();chirpTest();scanBus();
  if(videoMode) {
    Serial.println("PAL start GPIO25. Sleduj pruhy, odstiny, ramecek, pohyb a cas. Audio DAC je vypnuto.");
    screen.begin();
  } else {
    dacWrite(AUDIO,128);
    for(int i=0;i<32;i++)sineLut[i]=uint8_t(lroundf(128+24*sinf(2*PI*i/32)));
    esp_timer_create_args_t args={};args.callback=&audioTick;args.name="bitbox_audio";
    esp_err_t e=esp_timer_create(&args,&audioTimer);
    if(e!=ESP_OK){audioTimer=nullptr;Serial.printf("CHYBA vytvoreni audio timeru: %d\n",int(e));}
  }
  help();
}
void loop() {
  commands();sampleInputs();samplePot();updatePiezo();updateAudio();updateUART();panelLEDs();
  if(millis()-lastReport>=1000){lastReport=millis();printStatus();}
  if(videoMode)drawVideo();else delay(1);
}
