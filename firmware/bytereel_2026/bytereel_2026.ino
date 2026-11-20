#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h>
#include <driver/dac.h>
#include <driver/i2s.h>
#include <ESP_8_BIT_composite.h>
#include <math.h>
#include <stdint.h>
#include <limits.h>
#include <limits.h>
#include <new>
#include <stdarg.h>
#include <esp_heap_caps.h>
#include <esp_system.h>
#include <esp_partition.h>
#include <esp_timer.h>
#include "vm.h"
#include "run_gate.h"
#include "panel_state.h"
#include "video_renderer.h"
#include "audio_synth.h"
#include "audio_transport_stats.h"
#include "audio_diagnostic.h"
#include "slot_envelope.h"
#include "instruction_catalog.h"
#include "assembler.h"
#include "source_validation.h"
#include "slot_upload_validation.h"
#include "web_ui.h"
#include "monitor_state.h"
#include "i2c_diagnostics.h"
#include "panel_display.h"
#include "slot_store.h"
#include "littlefs_slot_adapter.h"
#include "storage_boot_guard.h"
#include "web_routes.h"
#include "demo_programs.h"
#include "web_json.h"
#ifndef BR_AUDIO_DIAGNOSTIC
#define BR_AUDIO_DIAGNOSTIC 0
#endif

// ByteREEL 2026 hardware map. INPUT expander 0x22 is strictly read-only.
static constexpr uint8_t LED_ADDR=0x20, INPUT_ADDR=0x22;
static constexpr int I2C_SDA_PIN=21, I2C_SCL_PIN=22;
static constexpr uint32_t I2C_FREQUENCY_HZ=10000;
static constexpr int PIN_RUN=13, PIN_VIDEO=14, PIN_LIVE=16, PIN_AUTO=17,
  PIN_EDIT=18, PIN_SPEED=33, PIN_PIEZO=23, PIN_VIDEO_OUT=25,
  PIN_UART_RX=32, PIN_UART_TX=27;
static constexpr uint32_t UART_BAUD=9600;
WebServer http(80);
VM vm;
uint8_t videoRam[256], audioRam[256];
uint8_t panel[2]={0xff,0xff};
uint16_t switches=0xffff;
uint8_t editorVideo=0,editorAudio=0;
uint8_t selectedSlot=0;
bool panelRun=false;
bool previousEdit=true,editSeen=false;
bool dirtyVideo=false,dirtyAudio=false,storageBusy=false;
RunGate runGate;
InputDebouncer inputFilter;
volatile bool audioDriver=false;
volatile bool audioMuteRequested=false;
bool physicalPiezo=false,audioTaskReady=false;
SemaphoreHandle_t audioWriteMutex=nullptr;
TaskHandle_t audioTaskHandle=nullptr;
uint16_t physicalPiezoHz=0;
uint32_t piezoGenerationSeen=0;
uint32_t speedAt=0,panelAt=0,frameAt=0,piezoUntil=0;
uint32_t videoFrame=0;
portMUX_TYPE audioStatsMux=portMUX_INITIALIZER_UNLOCKED;
BrAudioTransportStats audioTransportStats;
#if BR_AUDIO_DIAGNOSTIC
BrAudioDiagnosticTone audioDiagnosticTone;
volatile bool diagnosticToneActive=false;bool diagnosticDacEnabled=false;
#endif
uint32_t maxHttpUs=0,maxSlotReadUs=0,maxSlotCommitUs=0,maxI2cReadUs=0,maxLoopUs=0,maxVideoStartUs=0;
float speedFiltered=0;
uint8_t ledWord[2]={0xff,0xff};
ESP_8_BIT_composite *video=nullptr;
uint8_t videoHistory[256*192];
static constexpr size_t SLOT_FILE_CAP=32+BR_SLOT_MAX_PAYLOAD;
uint8_t *slotA=nullptr,*slotB=nullptr;
bool storageHealthy=false;
enum BrStorageError:uint8_t { BR_STORAGE_OK,BR_STORAGE_PARTITION_MISSING,BR_STORAGE_NOT_STOP,BR_STORAGE_NOT_EMPTY,BR_STORAGE_VERIFY_FAILED,BR_STORAGE_MOUNT_FAILED,BR_STORAGE_INIT_FAILED };
BrStorageError storageError=BR_STORAGE_PARTITION_MISSING;
bool wifiAPReady=false;
uint32_t ledAt=0;
HoldGesture clearHold,saveHold,loadHold;
BrEditPreview editPreview;
uint8_t previewPaddles=0;
bool ledWritePending=false;
uint16_t pendingLedWord=0xffff,lastLedElectrical=0;
bool ledStateKnown=false;
uint32_t ledPulseAt=0;
BrErrorPulse errorPulse;
BrFetchFifo fetchFifo;
BrLogRing appLog;
BrRuntimeCounters runtimeCounters;
uint32_t bootId=0,observedFetchSerial=0,monitorAt=0,scanAddress=0;
esp_reset_reason_t bootResetReason=ESP_RST_UNKNOWN;
uint16_t monitorAddress=0;uint8_t monitorValue=0;
uint8_t slotValidMask=0;
uint8_t slotErrorMask=0;
bool inputFaultLatched=false,vmErrorLogged=false;
bool ledFaultLatched=false;
bool videoErrorLatched=false;
BrI2cDiagnostics i2cDiagnostics;
const char *webMutationContext=nullptr;uint8_t webMutationSlot=0;bool webMutationFailed=false;
bool webRunning=false,webRoutesConfigured=false;
uint32_t webRequestSequence=0,webRequestStartUs=0,webRequestBodyBytes=0;int webRequestStatus=0;char webRequestMethod[8]="?",webRequestPath[80]="/",webClientRequestId[48]="-";const char *webRequestPhase="ROUTE";
BrUiModeGate uiModeGate;
BrSlowBlink panelBlink;
enum BrWifiTaskState:uint8_t { BR_WIFI_OFF,BR_WIFI_STARTING,BR_WIFI_READY,BR_WIFI_ERROR_START,BR_WIFI_ERROR_IP };
volatile bool wifiDesiredMemory=false,wifiStopLogPending=false;
volatile uint32_t wifiRequestGeneration=0,wifiResultGeneration=0;
volatile BrWifiTaskState wifiTaskState=BR_WIFI_OFF;
TaskHandle_t wifiTaskHandle=nullptr;
bool wifiTaskReady=false;

static void appLogEvent(const char *type,const char *format,...){char line[104];va_list args;va_start(args,format);vsnprintf(line,sizeof(line),format,args);va_end(args);appLog.append(millis(),type,line);Serial.printf("[%s] %s\n",type,line);}
static void appLogf(const char *format,...){char line[104],type[16];va_list args;va_start(args,format);vsnprintf(line,sizeof(line),format,args);va_end(args);size_t n=0;while(line[n]&&line[n]!=' '&&n<sizeof(type)-1){type[n]=line[n];n++;}type[n]=0;if(!n)strcpy(type,"SYSTEM");appLog.append(millis(),type,line);Serial.println(line);}
static void webRequestBegin(){++webRequestSequence;if(!webRequestSequence)++webRequestSequence;webRequestStartUs=micros();webRequestStatus=0;webRequestPhase="ROUTE";String path=http.uri(),method=http.method()==HTTP_GET?"GET":http.method()==HTTP_POST?"POST":http.method()==HTTP_PUT?"PUT":http.method()==HTTP_DELETE?"DELETE":"OTHER";strncpy(webRequestPath,path.c_str(),sizeof(webRequestPath)-1);webRequestPath[sizeof(webRequestPath)-1]=0;strncpy(webRequestMethod,method.c_str(),sizeof(webRequestMethod)-1);webRequestMethod[sizeof(webRequestMethod)-1]=0;String clientId=http.header("X-byteREEL-Client-Request-Id");if(clientId.isEmpty())clientId="-";strncpy(webClientRequestId,clientId.c_str(),sizeof(webClientRequestId)-1);webClientRequestId[sizeof(webClientRequestId)-1]=0;String len=http.header("Content-Length");webRequestBodyBytes=len.length()?(uint32_t)len.toInt():(uint32_t)http.arg("plain").length();http.sendHeader("X-byteREEL-Request-Id",String(bootId)+"-"+String(webRequestSequence));http.sendHeader("X-byteREEL-Client-Request-Id",webClientRequestId);}
static void webRequestFinish(){uint32_t elapsed=micros()-webRequestStartUs;if(elapsed>maxHttpUs)maxHttpUs=elapsed;const bool poll=!strcmp(webRequestPath,"/api/v1/status")||!strcmp(webRequestPath,"/api/v1/log");if(!poll||elapsed>200000||webRequestStatus>=400)appLogEvent("WEB_REQUEST","id=%lu client=%s method=%s path=%s body=%lu phase=%s elapsedUs=%lu http=%d",(unsigned long)webRequestSequence,webClientRequestId,webRequestMethod,webRequestPath,(unsigned long)webRequestBodyBytes,webRequestPhase,(unsigned long)elapsed,webRequestStatus);}
struct BrWebTraceGuard{BrWebTraceGuard(){webRequestBegin();}~BrWebTraceGuard(){webRequestFinish();}};
static String jsonEscape(const char *text);

static void wifiControlTask(void*){
  bool apActive=false;uint32_t handledGeneration=0;
  for(;;){const uint32_t requested=wifiRequestGeneration;if(requested!=handledGeneration){handledGeneration=requested;const bool wanted=wifiDesiredMemory;if(wanted){wifiTaskState=BR_WIFI_STARTING;WiFi.mode(WIFI_AP);uint8_t mac[6];WiFi.softAPmacAddress(mac);char ssid[32];snprintf(ssid,sizeof(ssid),"byteREEL-2026-%02X%02X",mac[4],mac[5]);if(!WiFi.softAP(ssid,nullptr,1,false,2)){WiFi.mode(WIFI_OFF);apActive=false;wifiTaskState=BR_WIFI_ERROR_START;}else if(!WiFi.softAPConfig(IPAddress(192,168,4,1),IPAddress(192,168,4,1),IPAddress(255,255,255,0))){WiFi.softAPdisconnect(true);WiFi.mode(WIFI_OFF);apActive=false;wifiTaskState=BR_WIFI_ERROR_IP;}else{apActive=true;wifiTaskState=BR_WIFI_READY;}}
      else{if(apActive){WiFi.softAPdisconnect(true);WiFi.mode(WIFI_OFF);}apActive=false;wifiTaskState=BR_WIFI_OFF;}}
    wifiResultGeneration=handledGeneration;vTaskDelay(pdMS_TO_TICKS(10));}
}

static bool i2cReadInputs(uint16_t &v) {
  // The verified panel expander at 0x22 is read-only. Match the service
  // firmware: issue a direct read request, never an address+write probe.
  const uint8_t requested=2;
  const uint32_t started=micros();const uint8_t returned=(uint8_t)Wire.requestFrom(INPUT_ADDR,requested);const uint32_t elapsed=micros()-started;if(elapsed>maxI2cReadUs)maxI2cReadUs=elapsed;
  const int available=Wire.available();
  if(!i2cDiagnostics.recordReadResult(requested,returned,(uint8_t)(available<0?0:available))){
    while(Wire.available()>0) (void)Wire.read();
    return false;
  }
  const int lo=Wire.read(),hi=Wire.read();
  if(lo<0||hi<0){
    i2cDiagnostics.recordReadValueError(lo<0?lo:hi);
    while(Wire.available()>0) (void)Wire.read();
    return false;
  }
  const uint16_t sample=(uint16_t)(uint8_t)lo|((uint16_t)(uint8_t)hi<<8);
  i2cDiagnostics.commitValidRaw(sample,millis());
  v=sample;
  return true;
}
static void ledWrite(uint16_t logicalLit) {
  const uint16_t activeLow=(uint16_t)~br_panel_map_word(logicalLit);
  if(ledStateKnown&&activeLow==lastLedElectrical)return;
  Wire.beginTransmission(LED_ADDR); Wire.write((uint8_t)activeLow); Wire.write((uint8_t)(activeLow>>8));
  const uint8_t rc=Wire.endTransmission();
  if(!rc){lastLedElectrical=activeLow;ledStateKnown=true;}
  i2cDiagnostics.recordLedWrite(rc);
  if(rc&&!ledFaultLatched){appLogf("I2C_LED_WRITE_ERROR addr=0x20 rc=%u",rc);ledFaultLatched=true;}
  else if(!rc&&ledFaultLatched){appLogf("I2C_LED_WRITE_RECOVERED addr=0x20");ledFaultLatched=false;}
}
static void queueLed(uint16_t activeLow){pendingLedWord=activeLow;ledWritePending=true;}
static BrPartitionCheck checkPartitionErased(){
  const esp_partition_t *p=esp_partition_find_first(ESP_PARTITION_TYPE_DATA,ESP_PARTITION_SUBTYPE_DATA_SPIFFS,"spiffs");
  if(!p||p->size==0)return BR_PARTITION_READ_FAILED;
  uint8_t buf[256];
  for(size_t off=0;off<p->size;off+=sizeof(buf)){
    size_t n=p->size-off;if(n>sizeof(buf))n=sizeof(buf);
    if(esp_partition_read(p,off,buf,n)!=ESP_OK)return BR_PARTITION_READ_FAILED;
    for(size_t i=0;i<n;i++)if(buf[i]!=0xff)return BR_PARTITION_HAS_DATA;
  }
  return BR_PARTITION_ERASED;
}
static void ledsInit() {
  ledWrite(0x0000); delay(150);
  for(int n=0;n<2;n++){ledWrite(0xffff);delay(200);ledWrite(0x0000);delay(200);}
  uint16_t lit=0;
  for(int i=0;i<4;i++){lit|=(uint16_t)(1u<<i)|(uint16_t)(1u<<(7-i))|(uint16_t)(1u<<(8+i))|(uint16_t)(1u<<(15-i));ledWrite(lit);delay(150);}
  delay(1000);
  for(int i=0;i<4;i++){lit&=(uint16_t)~((1u<<i)|(1u<<(7-i))|(1u<<(8+i))|(1u<<(15-i)));ledWrite(lit);delay(150);}
  ledWrite(0x0000);
}
static void piezoOff(){ledcWrite(0,0);piezoUntil=0;}
static void piezoSet(uint16_t hz,uint8_t duration){ledcWriteTone(0,hz);ledcWrite(0,128);piezoUntil=duration?millis()+(uint32_t)duration*10:0;}
static void audioTask(void*){
  AudioSynth synth;uint32_t generation=0xffffffffu;int16_t samples[128];BrAudioBlockCursor cursor;uint32_t blockStartUs=0;
  for(;;){
    if(!audioDriver){cursor.finish();vTaskDelay(1);continue;}
    if(!cursor.pending){if(vm.synthGeneration!=generation){generation=vm.synthGeneration;synth.reset();}
#if BR_AUDIO_DIAGNOSTIC
      if(digitalRead(PIN_RUN)==LOW&&diagnosticToneActive)audioDiagnosticTone.render(samples,128);else for(int i=0;i<128;i++)samples[i]=(int16_t)0x8000;
#else
      if(vm.running&&vm.mode==VM::AUDIO)synth.render(vm,samples,128);else for(int i=0;i<128;i++)samples[i]=(int16_t)0x8000;
#endif
      cursor.begin();blockStartUs=(uint32_t)esp_timer_get_time();}
    if(audioWriteMutex)xSemaphoreTake(audioWriteMutex,portMAX_DELAY);
    bool completed=false,writeIncomplete=false;uint32_t finishUs=(uint32_t)esp_timer_get_time();
    if(audioDriver){
#if BR_AUDIO_DIAGNOSTIC
      const bool runPinActive=digitalRead(PIN_RUN)==LOW;if(!runPinActive)diagnosticToneActive=false;const bool diagnosticRun=runPinActive&&diagnosticToneActive;
      if(diagnosticRun&&!diagnosticDacEnabled){dac_output_enable(DAC_CHANNEL_2);diagnosticDacEnabled=true;}
      else if(!diagnosticRun&&diagnosticDacEnabled){dac_output_disable(DAC_CHANNEL_2);diagnosticDacEnabled=false;}
      if(audioMuteRequested||!diagnosticRun){for(int i=0;i<128;i++)samples[i]=(int16_t)0x8000;cursor.begin();blockStartUs=(uint32_t)esp_timer_get_time();audioMuteRequested=false;}
#else
      if(audioMuteRequested||!vm.running||vm.mode!=VM::AUDIO){for(int i=0;i<128;i++)samples[i]=(int16_t)0x8000;cursor.begin();blockStartUs=(uint32_t)esp_timer_get_time();audioMuteRequested=false;}
#endif
      const uint8_t *bytes=(const uint8_t*)samples;const int64_t writeDeadline=esp_timer_get_time()+10000;esp_err_t err=ESP_OK;
      while(cursor.remaining(sizeof(samples))){
        const int64_t remaining=writeDeadline-esp_timer_get_time();if(remaining<=0){err=ESP_ERR_TIMEOUT;portENTER_CRITICAL(&audioStatsMux);audioTransportStats.recordDeadline();portEXIT_CRITICAL(&audioStatsMux);break;}
        size_t written=0;const size_t requested=cursor.remaining(sizeof(samples));const int64_t callStarted=esp_timer_get_time();
        err=i2s_write(I2S_NUM_0,bytes+cursor.offset,requested,&written,pdMS_TO_TICKS((uint32_t)((remaining+999)/1000)));
        const uint32_t callUs=(uint32_t)(esp_timer_get_time()-callStarted);portENTER_CRITICAL(&audioStatsMux);audioTransportStats.recordWrite(requested,written,err==ESP_ERR_TIMEOUT,err!=ESP_OK,callUs);portEXIT_CRITICAL(&audioStatsMux);
        cursor.advance(written,sizeof(samples));if(err!=ESP_OK||!written)break;
      }
      finishUs=(uint32_t)esp_timer_get_time();completed=cursor.complete(sizeof(samples));writeIncomplete=!completed;
      portENTER_CRITICAL(&audioStatsMux);audioTransportStats.recordBlock(blockStartUs,finishUs,completed);portEXIT_CRITICAL(&audioStatsMux);
      if(completed)cursor.finish();
    }
    if(audioWriteMutex)xSemaphoreGive(audioWriteMutex);
    if(writeIncomplete)vTaskDelay(1);
  }
}
static void audioStart(){if(audioDriver)return;if(!audioTaskReady){appLogf("AUDIO_TASK_UNAVAILABLE");return;}i2s_config_t c={.mode=(i2s_mode_t)(I2S_MODE_MASTER|I2S_MODE_TX|I2S_MODE_DAC_BUILT_IN),.sample_rate=22050,.bits_per_sample=I2S_BITS_PER_SAMPLE_16BIT,.channel_format=I2S_CHANNEL_FMT_ONLY_RIGHT,.communication_format=I2S_COMM_FORMAT_STAND_MSB,.intr_alloc_flags=ESP_INTR_FLAG_LEVEL1,.dma_buf_count=8,.dma_buf_len=128,.use_apll=true,.tx_desc_auto_clear=true,.fixed_mclk=0};esp_err_t err=i2s_driver_install(I2S_NUM_0,&c,0,nullptr);if(err!=ESP_OK){appLogEvent("AUDIO_BACKEND_ERROR","phase=I2S_INSTALL code=%d",(int)err);return;}err=i2s_set_pin(I2S_NUM_0,nullptr);if(err==ESP_OK)err=i2s_set_dac_mode(I2S_DAC_CHANNEL_RIGHT_EN);if(err!=ESP_OK){esp_err_t cleanup=i2s_driver_uninstall(I2S_NUM_0);appLogEvent("AUDIO_BACKEND_ERROR","phase=I2S_CONFIG code=%d cleanup=%d",(int)err,(int)cleanup);return;}audioMuteRequested=true;audioDriver=true;
#if BR_AUDIO_DIAGNOSTIC
  diagnosticDacEnabled=digitalRead(PIN_RUN)==LOW&&diagnosticToneActive;if(!diagnosticDacEnabled)dac_output_disable(DAC_CHANNEL_2);
#endif
  appLogf("AUDIO_READY sampleRate=22050 dma=8x128 mono16 blockUs=5805 queueUs=46440");}
static bool audioStop(){if(!audioDriver)return true;audioMuteRequested=true;if(audioWriteMutex)xSemaphoreTake(audioWriteMutex,portMAX_DELAY);esp_err_t zero=i2s_zero_dma_buffer(I2S_NUM_0);esp_err_t err=i2s_driver_uninstall(I2S_NUM_0);if(err==ESP_OK){dac_output_disable(DAC_CHANNEL_2);audioDriver=false;}if(audioWriteMutex)xSemaphoreGive(audioWriteMutex);if(err!=ESP_OK){appLogEvent("AUDIO_BACKEND_ERROR","phase=I2S_UNINSTALL code=%d zero=%d",(int)err,(int)zero);return false;}return true;}
static void audioMuteNow(){audioMuteRequested=true;}
static void runtimeError(const char *type,const char *phase,int code){
  const bool videoFailure=!strcmp(type,"VIDEO_BACKEND_ERROR");if(videoFailure&&videoErrorLatched)return;
  vm.stop();runGate.reset();audioMuteNow();piezoOff();physicalPiezo=false;
  if(videoFailure)videoErrorLatched=true;
  appLogEvent(type,"phase=%s code=%d",phase,code);
  if(!inputFaultLatched&&!ledFaultLatched)errorPulse.trigger(millis());
}
static bool videoStart(){
  if(video&&video->isStarted())return true;
  const uint32_t startUs=micros();
  videoErrorLatched=false;
  if(audioDriver&&!audioStop()){const uint32_t elapsed=micros()-startUs;if(elapsed>maxVideoStartUs)maxVideoStartUs=elapsed;runtimeError("VIDEO_BACKEND_ERROR","AUDIO_TEARDOWN",ESP_ERR_INVALID_STATE);return false;}
  const uint32_t freeBefore=ESP.getFreeHeap(),dmaBefore=heap_caps_get_largest_free_block(MALLOC_CAP_DMA);
  video=new(std::nothrow) ESP_8_BIT_composite(0);
  if(!video){const uint32_t elapsed=micros()-startUs;if(elapsed>maxVideoStartUs)maxVideoStartUs=elapsed;appLogEvent("VIDEO_BACKEND_ERROR","phase=OBJECT_ALLOC code=%d requested=%u free=%u largestDma=%u",(int)ESP_ERR_NO_MEM,(unsigned)sizeof(ESP_8_BIT_composite),(unsigned)freeBefore,(unsigned)dmaBefore);runtimeError("VIDEO_BACKEND_ERROR","OBJECT_ALLOC",ESP_ERR_NO_MEM);return false;}
  video->begin();
  if(!video->isStarted()||!video->getFrameBufferLines()){
    const char *stage=video->startStage();const int rc=(int)video->startError();
    const uint32_t elapsed=micros()-startUs;if(elapsed>maxVideoStartUs)maxVideoStartUs=elapsed;
    appLogEvent("VIDEO_BACKEND_ERROR","phase=%s code=%d fbBytes=%u freeBefore=%u largestDmaBefore=%u freeAfter=%u largestDmaAfter=%u",stage,rc,2u*256u*240u,(unsigned)freeBefore,(unsigned)dmaBefore,(unsigned)ESP.getFreeHeap(),(unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_DMA));
    delete video;video=nullptr;runtimeError("VIDEO_BACKEND_ERROR",stage,rc);return false;
  }
  videoErrorLatched=false;const uint32_t elapsed=micros()-startUs;if(elapsed>maxVideoStartUs)maxVideoStartUs=elapsed;memset(videoHistory,0,sizeof(videoHistory));
  br_render_video(vm,video->getFrameBufferLines(),videoHistory,0);appLogf("VIDEO_READY standard=PAL stage=%s",video->startStage());return true;
}
static void videoStop(){if(video){appLogEvent("VIDEO_STOP","started=%u",video->isStarted()?1u:0u);video->end();delete video;video=nullptr;}}
static void modeReset(bool isVideo){
#if BR_AUDIO_DIAGNOSTIC
  diagnosticToneActive=false;
#endif
  vm.reset(isVideo?VM::VIDEO:VM::AUDIO);if(isVideo){if(!audioStop()){runtimeError("VIDEO_BACKEND_ERROR","AUDIO_TEARDOWN",ESP_ERR_INVALID_STATE);return;}videoStart();}else{videoStop();audioStart();}}
static void runStop(bool clearFifos){runGate.stoppedByHalt();vm.stop();audioMuteNow();piezoOff();physicalPiezo=false;appLogf("STOP physical");if(clearFifos){vm.rxHead=vm.rxTail=vm.txHead=vm.txTail=0;}}
static void storageInit(){
  storageHealthy=false;storageError=BR_STORAGE_PARTITION_MISSING;
  const esp_partition_t *p=esp_partition_find_first(ESP_PARTITION_TYPE_DATA,ESP_PARTITION_SUBTYPE_DATA_SPIFFS,"spiffs");
  if(!p){appLogf("STORAGE_ERROR partition spiffs missing");return;}
  if(ESP.getFlashChipSize()<4*1024*1024){appLogf("STORAGE_ERROR flash size below 4MiB");return;}
  const bool mounted=LittleFS.begin(false,"/littlefs",4,"spiffs");
  const bool stopped=digitalRead(PIN_RUN)==HIGH;
  BrPartitionCheck check=BR_PARTITION_READ_FAILED;if(!mounted&&stopped)check=checkPartitionErased();
  const BrStorageRecovery recovery=br_storage_recovery_checked(mounted,stopped,check);
  if(recovery==BR_STORAGE_RECOVERY_MOUNTED){storageHealthy=true;storageError=BR_STORAGE_OK;}
  else if(recovery==BR_STORAGE_RECOVERY_REQUIRE_STOP){storageError=BR_STORAGE_NOT_STOP;appLogf("STORAGE_MOUNT_FAILED; initialization requires physical STOP");return;}
  else if(recovery==BR_STORAGE_RECOVERY_REFUSE_DATA){storageError=check==BR_PARTITION_HAS_DATA?BR_STORAGE_NOT_EMPTY:BR_STORAGE_VERIFY_FAILED;appLogf(check==BR_PARTITION_HAS_DATA?"STORAGE_MOUNT_FAILED; partition contains data, left untouched":"STORAGE_PARTITION_VERIFY_FAILED; left untouched");return;}
  else if(!LittleFS.format()){storageError=BR_STORAGE_INIT_FAILED;appLogf("STORAGE_INIT_FORMAT_FAILED; blank partition only");return;}
  else if(!LittleFS.begin(false,"/littlefs",4,"spiffs")){storageError=BR_STORAGE_MOUNT_FAILED;appLogf("STORAGE_INIT_REMOUNT_FAILED");return;}
  else {storageHealthy=true;storageError=BR_STORAGE_OK;appLogf("STORAGE_INITIALIZED erased_partition=1");}
  if(storageHealthy&&!LittleFS.exists("/slots")&&!LittleFS.mkdir("/slots")){storageHealthy=false;storageError=BR_STORAGE_INIT_FAILED;appLogf("STORAGE_ERROR cannot create /slots");}
  if(storageHealthy)appLogf("STORAGE_READY flash=%lu partitionBytes=%lu",(unsigned long)ESP.getFlashChipSize(),(unsigned long)p->size);
}
static bool storageAcquire(){if(slotA&&slotB)return true;slotA=(uint8_t*)malloc(SLOT_FILE_CAP);slotB=(uint8_t*)malloc(SLOT_FILE_CAP);if(!slotA||!slotB){free(slotA);free(slotB);slotA=slotB=nullptr;return false;}return true;}
static void storageRelease(){free(slotA);free(slotB);slotA=slotB=nullptr;}
static String slotPath(uint8_t id,char side){char p[24];snprintf(p,sizeof(p),"/slots/%02u.%c.brp",id,side);return String(p);}
static size_t slotRead(uint8_t id,char side,uint8_t*out){if(!storageHealthy||id>7)return SIZE_MAX;String path=slotPath(id,side);if(!LittleFS.exists(path))return 0;File f=LittleFS.open(path,"r");if(!f)return SIZE_MAX;size_t n=f.size();if(n>SLOT_FILE_CAP){f.close();return SIZE_MAX;}size_t got=f.read(out,n);f.close();return got==n?n:SIZE_MAX;}
static BrSlotState slotCurrent(uint8_t id,BrSlotView&view,size_t&na,size_t&nb){if(!storageHealthy||!storageAcquire()){na=nb=0;view=BrSlotView{};return BR_SLOT_CORRUPT;}const uint32_t started=micros();na=slotRead(id,'a',slotA);nb=slotRead(id,'b',slotB);const uint32_t elapsed=micros()-started;if(elapsed>maxSlotReadUs)maxSlotReadUs=elapsed;BrSlotState state;if(na==SIZE_MAX||nb==SIZE_MAX){na=nb=0;view=BrSlotView{};state=BR_SLOT_CORRUPT;}else state=br_select_slot(slotA,na,slotB,nb,view);if(id<8){const uint8_t bit=(uint8_t)(1u<<id);if(state==BR_SLOT_VALID||state==BR_SLOT_DEGRADED){slotValidMask|=bit;slotErrorMask&=(uint8_t)~bit;}else{slotValidMask&=(uint8_t)~bit;if(state==BR_SLOT_CORRUPT&&!(slotErrorMask&bit)){slotErrorMask|=bit;appLogEvent("SLOT_DATA_ERROR","slot=%u code=CORRUPT_OR_READ_ERROR phase=READ_CANDIDATES; empty and damaged storage are distinct",(unsigned)id+1);if(!inputFaultLatched&&!ledFaultLatched)errorPulse.trigger(millis());}}}return state;}
static void refreshSlotValidity(){if(!storageAcquire())return;for(uint8_t i=0;i<8;i++){BrSlotView view;size_t na,nb;(void)slotCurrent(i,view,na,nb);}storageRelease();}
static BrCommitResult slotWrite(uint8_t id,const uint8_t*payload,size_t len,uint32_t expected,uint32_t&revision,BrCommitPhase *outPhase=nullptr){
  if(!storageHealthy||id>7||!payload||len>BR_SLOT_MAX_PAYLOAD||panelRun||vm.running||digitalRead(PIN_RUN)==LOW||storageBusy){if(outPhase)*outPhase=BR_COMMIT_PHASE_NONE;const char *reason=!storageHealthy?"FILESYSTEM_UNAVAILABLE":(panelRun||vm.running||digitalRead(PIN_RUN)==LOW)?"STOP_REQUIRED":storageBusy?"BUSY":"INVALID_ARGUMENT";appLogEvent("SLOT_COMMIT_ERROR","slot=%u code=%s phase=GUARD reason=%s",(unsigned)id+1,reason,reason);return BR_COMMIT_IO;}
  if(!storageAcquire()){if(outPhase)*outPhase=BR_COMMIT_PHASE_READ_CANDIDATES;appLogEvent("SLOT_COMMIT_ERROR","code=NO_MEMORY phase=BUFFER_ALLOC reason=two_candidate_buffers_unavailable");return BR_COMMIT_IO;}
  // LittleFS does not own I2S0. Keep PAL DMA alive through a commit: the old
  // end()/begin() pause could silently fail on resume and leave the VM with a
  // dead video backend after an otherwise successful SAVE.
  const uint32_t commitStarted=micros();storageBusy=true;runGate.reset();BrLittleFsStore store(LittleFS,"/slots",storageHealthy);BrCommitPhase phase=BR_COMMIT_PHASE_NONE;BrCommitResult result=br_commit_slot(store,id,payload,len,expected,revision,slotA,slotB,SLOT_FILE_CAP,&phase);storageBusy=false;const uint32_t commitElapsed=micros()-commitStarted;if(commitElapsed>maxSlotCommitUs)maxSlotCommitUs=commitElapsed;if(outPhase)*outPhase=phase;
  if(result==BR_COMMIT_OK){slotValidMask|=(uint8_t)(1u<<id);slotErrorMask&=(uint8_t)~(1u<<id);appLogEvent("SLOT_COMMIT","slot=%u revision=%lu verified=CRC+readback",(unsigned)id+1,(unsigned long)revision);return result;}
  appLogEvent("SLOT_COMMIT_ERROR","slot=%u code=%s phase=%s reason=%s",(unsigned)id+1,br_commit_result_name(result),br_commit_phase_name(phase),store.lastError);
  return result;
}
static bool slotGetPayload(uint8_t id,String&payload,uint32_t&revision){BrSlotView view;size_t na,nb;BrSlotState state=slotCurrent(id,view,na,nb);if((state!=BR_SLOT_VALID&&state!=BR_SLOT_DEGRADED)||!view.payload)return false;payload="";payload.reserve(view.length+24);for(uint32_t i=0;i<view.length;i++)payload+=(char)view.payload[i];revision=view.generation;if(payload.length()!=view.length||payload.length()<2||payload[payload.length()-1]!='}')return false;payload.remove(payload.length()-1);payload+=",\"revision\":"+String(revision)+"}";return true;}
static bool jsonStringField(const String&j,const char*key,String&out,bool&isNull){return br_json_string(j,key,out,isNull);}
static bool jsonObjectField(const String&j,const char*key,String&out){size_t start=0,end=0;if(!br_json_object_range(j,key,start,end))return false;out=j.substring((unsigned int)start,(unsigned int)end);return true;}
static bool parseHexBank(const String&hex,uint8_t*out,int *badOffset=nullptr){if(hex.length()!=512){if(badOffset)*badOffset=-1;return false;}for(int i=0;i<256;i++){auto hv=[](char x)->int{if(x>='0'&&x<='9')return x-'0';if(x>='A'&&x<='F')return x-'A'+10;if(x>='a'&&x<='f')return x-'a'+10;return -1;};int hi=hv(hex[i*2]),lo=hv(hex[i*2+1]);if(hi<0||lo<0){if(badOffset)*badOffset=hi<0?i*2:i*2+1;return false;}out[i]=(uint8_t)((hi<<4)|lo);}return true;}
static bool extractHexBank(const String&json,const char*bank,uint8_t*out){String object,hex;bool isNull=false;if(!jsonObjectField(json,bank,object)||!jsonStringField(object,"hex",hex,isNull)||isNull)return false;return parseHexBank(hex,out);}
static String jsonEscape(const char *text);
static String panelPriorSource(const BrSlotView&view,const char*bank){if(!view.payload)return "null";String record="";record.reserve(view.length);for(uint32_t i=0;i<view.length;i++)record+=(char)view.payload[i];size_t start=0,end=0;String source;bool isNull=false;if(!br_json_object_range(record,bank,start,end)||!br_json_string_range(record,start,end,"source",source,isNull)||isNull)return "null";return String("\"")+jsonEscape(source.c_str())+"\"";}
static String panelSlotName(uint8_t id,const BrSlotView&view){uint8_t number=(uint8_t)(id+1);String fallback=String("\"SLOT ")+((number<10)?"0":"")+String(number)+"\"";if(!view.payload)return fallback;String j="";j.reserve(view.length);for(uint32_t i=0;i<view.length;i++)j+=(char)view.payload[i];int k=j.indexOf("\"name\"");if(k<0)return fallback;int q=j.indexOf('"',j.indexOf(':',k)+1);if(q<0)return fallback;bool esc=false;for(int i=q+1;i<j.length();i++){char c=j[i];if(esc){esc=false;continue;}if(c=='\\'){esc=true;continue;}if(c=='"'){if(i-q-1<=64)return j.substring(q,i+1);return fallback;}}return fallback;}
static bool makePanelPayload(uint8_t id,String&out){BrSlotView v;size_t na,nb;BrSlotState state=slotCurrent(id,v,na,nb);if(state==BR_SLOT_CORRUPT)return false;String nameJson=panelSlotName(id,v),sourceJson[2]={panelPriorSource(v,"video"),panelPriorSource(v,"audio")};char*hex=(char*)malloc(1025);if(!hex)return false;static const char digits[]="0123456789ABCDEF";uint8_t* banks[2]={videoRam,audioRam};String json="{\"schema\":1,\"isa\":\"2.0\",\"slot\":"+String(id)+",\"name\":"+nameJson+",\"video\":{\"hex\":\"";json.reserve(1600);
  for(int bank=0;bank<2;bank++){for(int i=0;i<256;i++){hex[i*2]=digits[banks[bank][i]>>4];hex[i*2+1]=digits[banks[bank][i]&15];}hex[512]=0;json+=hex;json+="\",\"source\":"+sourceJson[bank]+",\"sourceMatches\":false,\"length\":null}";if(bank==0)json+=",\"audio\":{\"hex\":\"";}
  json+='}';free(hex);out=json;return out.length()<=BR_SLOT_MAX_PAYLOAD;}
static PanelSlotResult panelSave(uint8_t id){if(panelRun||vm.running||digitalRead(PIN_RUN)==LOW)return PANEL_SLOT_RUN;if(!storageHealthy)return PANEL_SLOT_FILESYSTEM;if(!storageAcquire())return PANEL_SLOT_MEMORY;String payload;uint32_t rev=0,newRev;BrSlotView v;size_t na,nb;BrSlotState state=slotCurrent(id,v,na,nb);if(state==BR_SLOT_CORRUPT){storageRelease();return PANEL_SLOT_CORRUPT;}if(state==BR_SLOT_VALID||state==BR_SLOT_DEGRADED)rev=v.generation;if(!makePanelPayload(id,payload)){storageRelease();return PANEL_SLOT_MEMORY;}BrCommitResult result=slotWrite(id,(const uint8_t*)payload.c_str(),payload.length(),rev,newRev);storageRelease();if(result==BR_COMMIT_OK){dirtyVideo=dirtyAudio=false;return PANEL_SLOT_OK;}return PANEL_SLOT_WRITE_FAILED;}
static PanelSlotResult panelLoad(uint8_t id){if(panelRun||vm.running||digitalRead(PIN_RUN)==LOW)return PANEL_SLOT_RUN;if(!storageHealthy)return PANEL_SLOT_FILESYSTEM;if(!storageAcquire())return PANEL_SLOT_MEMORY;BrSlotView view;size_t na=0,nb=0;BrSlotState state=slotCurrent(id,view,na,nb);if(state==BR_SLOT_EMPTY){storageRelease();return PANEL_SLOT_EMPTY;}if(state==BR_SLOT_CORRUPT){storageRelease();return PANEL_SLOT_CORRUPT;}String payload;uint32_t rev;uint8_t v[256],a[256];bool ok=slotGetPayload(id,payload,rev)&&extractHexBank(payload,"video",v)&&extractHexBank(payload,"audio",a);storageRelease();if(!ok)return PANEL_SLOT_CORRUPT;if(!br_publish_banks(true,v,a,videoRam,audioRam))return PANEL_SLOT_CORRUPT;dirtyVideo=dirtyAudio=false;editorVideo=editorAudio=0;vm.reset(vm.mode);runGate.reset();if(vm.mode==VM::VIDEO&&video){memset(videoHistory,0,sizeof(videoHistory));video->waitForFrame();br_render_video(vm,video->getFrameBufferLines(),videoHistory,0);}return PANEL_SLOT_OK;}
static void userOperationError(const char *code,const char *message){appLogf("USER_ERROR %s: %s",code,message);errorPulse.trigger(millis());}
static const char* panelSlotErrorText(PanelSlotResult r){switch(r){case PANEL_SLOT_RUN:return "Operace odmítnuta: přepni přístroj do STOP";case PANEL_SLOT_FILESYSTEM:return "LittleFS není dostupný; data zůstala beze změny";case PANEL_SLOT_EMPTY:return "Vybraný slot je prázdný";case PANEL_SLOT_CORRUPT:return "Vybraný slot je poškozený; RAM ani uložená data nebyla změněna";case PANEL_SLOT_MEMORY:return "Nedostatek paměti pro bezpečné zpracování slotu";case PANEL_SLOT_WRITE_FAILED:return "Zápis selhal; původní platná revize je zachována";default:return "Operace dokončena";}}
static void webModeTransition(bool memory);
static uint8_t *activeBank(){return vm.mode==VM::VIDEO?videoRam:audioRam;}
static uint8_t &activeEditor(){return vm.mode==VM::VIDEO?editorVideo:editorAudio;}
static bool loadDemo(VM::Mode mode,bool run){
  if(run||vm.running||digitalRead(PIN_RUN)==LOW){userOperationError("DEMO_RUN_REJECTED","DEMO odmítnuto: přístroj musí být ve STOP");return false;}
  const uint8_t *source=mode==VM::VIDEO?BR_DEMO_VIDEO:BR_DEMO_AUDIO;
  const size_t length=mode==VM::VIDEO?BR_DEMO_VIDEO_CODE_BYTES:BR_DEMO_AUDIO_CODE_BYTES;
  uint8_t staged[256];for(size_t i=0;i<sizeof(staged);i++)staged[i]=i<length?pgm_read_byte(source+i):0;
  if(!br_copy_demo_bank(activeBank(),staged,length)){userOperationError("DEMO_LOAD_FAILED","Vestavěný program má neplatnou délku");return false;}
  vm.stop();piezoOff();physicalPiezo=false;audioMuteNow();vm.reset(mode);runGate.reset();fetchFifo.clear();monitorAddress=monitorValue=0;activeEditor()=0;
  if(mode==VM::VIDEO){dirtyVideo=true;memset(videoHistory,0,sizeof(videoHistory));}else dirtyAudio=true;
  appLogEvent("DEMO_LOAD","mode=%s bytes=%u bank=active only",mode==VM::VIDEO?"VIDEO":"AUDIO",(unsigned)length);return true;
}
static void physicalReset(){vm.stop();piezoOff();physicalPiezo=false;vm.reset(vm.mode);runGate.reset();fetchFifo.clear();monitorAddress=monitorValue=0;memset(videoHistory,0,sizeof(videoHistory));if(video&&vm.mode==VM::VIDEO){video->waitForFrame();br_render_video(vm,video->getFrameBufferLines(),videoHistory,0);}appLogf("RESET physical"); }
static void panelActions(uint32_t now,bool run){
  const bool edit=inputFilter.activeLow(20);const uint8_t data=(uint8_t)(~inputFilter.buttons);uint8_t slot=0;const bool validSlot=br_one_hot_slot(data,slot);if(validSlot)selectedSlot=slot;panelRun=run;
  if(uiModeGate.observe(edit))webModeTransition(!edit);
  static bool panelModeKnown=false,lastEditMode=true;
  if(!panelModeKnown||lastEditMode!=edit){panelBlink.leave();if(!edit)panelBlink.enter(now);panelModeKnown=true;lastEditMode=edit;}
  static bool haveData=false,haveModes=false,haveSlotMask=false;static uint8_t lastData=0,lastModes=0,lastSlotMask=0;
  const uint8_t oldEditor=activeEditor();const bool oldPreview=editPreview.active;
  if(!haveData||data!=lastData){char bits[9];for(uint8_t i=0;i<8;i++)bits[i]=(data&(1u<<(7-i)))?'1':'0';bits[8]=0;appLogEvent("DATA_SWITCHES","mask=0x%02X bits=%s",data,bits);lastData=data;haveData=true;}
  const uint8_t modes=(uint8_t)((inputFilter.activeLow(16)?1:0)|(inputFilter.activeLow(17)?2:0)|(inputFilter.activeLow(18)?4:0)|(inputFilter.activeLow(19)?8:0)|(inputFilter.activeLow(20)?16:0));
  if(!haveModes||modes!=lastModes){appLogEvent("MODE_CHANGE","RUN=%s VIDEO=%s LIVE=%s AUTO=%s EDIT=%s",(modes&1)?"RUN":"STOP",(modes&2)?"VIDEO":"AUDIO",(modes&4)?"LIVE":"SCAN",(modes&8)?"AUTO":"STEP",(modes&16)?"EDIT":"MEMORY");lastModes=modes;haveModes=true;}
  if(!edit&&(!haveSlotMask||data!=lastSlotMask)){if(validSlot)appLogEvent("SLOT_SELECT","mask=0x%02X slot=%u",data,(unsigned)slot+1);else appLogEvent("SLOT_SELECT","mask=0x%02X %s",data,data==0?"NONE":"MULTIPLE");lastSlotMask=data;haveSlotMask=true;}else if(edit)haveSlotMask=false;
  if(!editSeen||previousEdit!=edit){editPreview.reset();editPreview.observed=data;previewPaddles=data;}
  bool stepMode=!inputFilter.activeLow(19);if(stepMode!=vm.step)vm.setStep(stepMode,now);
  const uint32_t edge=inputFilter.pressed;
  static const char *const keyNames[6]={"HOME","RESET/CLR","WRITE","WRITE+","EXP/EXAM","NEXT"};
  for(uint8_t k=0;k<6;k++){uint32_t bit=1u<<(8+k);if(edge&bit)appLogEvent("PANEL_KEY","%s pressed",keyNames[k]);if(inputFilter.released&bit)appLogEvent("PANEL_KEY","%s released",keyNames[k]);}
  const bool home=edge&(1u<<8),reset=edge&(1u<<9),write=edge&(1u<<10),writeNext=edge&(1u<<11),exam=edge&(1u<<12),next=edge&(1u<<13);
  const bool editStop=edit&&!run;
  if(!editStop||reset||home||next){editPreview.reset();editPreview.observed=data;previewPaddles=data;}
  if(editStop){if(data!=previewPaddles)editPreview.paddleChanged(data);previewPaddles=data;}
  if(home&&editStop)activeEditor()=0;
  if(reset){editPreview.reset();physicalReset();}
  if(!edit&&write&&!writeNext&&!reset)loadDemo(vm.mode,run);
  if(exam&&editStop){uint8_t addr=activeEditor();editPreview.examPress(data,addr);activeEditor()=addr;}
  else if(editStop&&(write||writeNext)){
    uint8_t old=activeBank()[activeEditor()],address=activeEditor();br_edit_store(activeBank(),activeEditor(),data,writeNext);appLogEvent("RAM_WRITE","bank=%s action=%s addr=0x%02X old=0x%02X new=0x%02X",vm.mode==VM::VIDEO?"VIDEO":"AUDIO",writeNext?"WRITE+":"WRITE",address,old,data);if(vm.mode==VM::VIDEO)dirtyVideo=true;else dirtyAudio=true;
    editPreview.active=true;
  }else if(exam&&edit) {activeEditor()=data;}
  if(next){if(run&&vm.step)vm.nextStep();else if(editStop)++activeEditor();}
  const uint8_t token=(uint8_t)((edit?1:0)|(run?2:0)|(vm.mode==VM::VIDEO?4:0));
  const uint8_t gestureSlot=data;
  const bool saveWasActive=saveHold.active,loadWasActive=loadHold.active,clearWasActive=clearHold.active;
  bool save=saveHold.update(!edit&&!run&&inputFilter.activeLow(12),gestureSlot,token,now,2000);
  bool load=loadHold.update(!edit&&!run&&inputFilter.activeLow(13),gestureSlot,token,now,2000);
  bool clear=clearHold.update(!run&&inputFilter.activeLow(9),gestureSlot,(uint8_t)(vm.mode==VM::VIDEO),now,3000);
  if(!saveWasActive&&saveHold.active)appLogEvent("PANEL_SAVE_BEGIN","selection=0x%02X",data);
  if(!loadWasActive&&loadHold.active)appLogEvent("PANEL_LOAD_BEGIN","selection=0x%02X",data);
  if(!clearWasActive&&clearHold.active)appLogEvent("CLEAR_BEGIN","bank=%s",vm.mode==VM::VIDEO?"VIDEO":"AUDIO");
  if(clear){memset(activeBank(),0,256);activeEditor()=0;if(vm.mode==VM::VIDEO)dirtyVideo=true;else dirtyAudio=true;vm.reset(vm.mode);runGate.reset();appLogEvent("CLEAR_RESULT","ok=1 bank=%s",vm.mode==VM::VIDEO?"VIDEO":"AUDIO");}
  if(save){if(!validSlot)userOperationError("PANEL_SAVE_INVALID_SELECTION","SAVE odmítnut: vyber právě jeden slot");else{PanelSlotResult result=panelSave(slot);appLogEvent("PANEL_SAVE_RESULT","slot=%u detail=%s",(unsigned)slot+1,panelSlotErrorText(result));if(result!=PANEL_SLOT_OK)userOperationError("PANEL_SAVE_FAILED",panelSlotErrorText(result));}}
  if(load){if(!validSlot)userOperationError("PANEL_LOAD_INVALID_SELECTION","LOAD odmítnut: vyber právě jeden slot");else{PanelSlotResult result=panelLoad(slot);appLogEvent("PANEL_LOAD_RESULT","slot=%u detail=%s",(unsigned)slot+1,panelSlotErrorText(result));if(result!=PANEL_SLOT_OK)userOperationError("PANEL_LOAD_FAILED",panelSlotErrorText(result));}}
  if(editPreview.active!=oldPreview){appLogEvent("EDIT_PREVIEW","%s",editPreview.active?"entered":"exited");if(editPreview.active)panelBlink.enter(now);else panelBlink.leave();}
  if(activeEditor()!=oldEditor)appLogEvent("EDITOR_ADDRESS","bank=%s old=0x%02X new=0x%02X",vm.mode==VM::VIDEO?"VIDEO":"AUDIO",oldEditor,activeEditor());
}

static void monitorUpdate(uint32_t now){
  if((uint32_t)(now-monitorAt)<50)return;monitorAt=now;
  uint8_t addr=0,value=0;const bool edit=inputFilter.activeLow(20),run=panelRun;
  if(!edit){uint8_t mask=inputFilter.ready?(uint8_t)(~inputFilter.buttons):0;uint8_t selected=0;addr=mask;value=panelBlink.update(now)?slotValidMask:0x00;if(br_one_hot_slot(mask,selected))selectedSlot=selected;}
  else if(!run){addr=activeEditor();value=br_edit_display_value(editPreview.active,(uint8_t)(~inputFilter.buttons),activeBank()[addr]);if(editPreview.active&&!panelBlink.update(now))value=0;}
  else if(inputFilter.activeLow(18)){
    if(vm.step){addr=(uint8_t)vm.lastFetchPc;value=vm.lastFetchOp;}
    else {BrFetchEvent event;if(fetchFifo.pop(event)){monitorAddress=event.address;monitorValue=event.value;}addr=(uint8_t)monitorAddress;value=monitorValue;}
  }else{scanAddress=(scanAddress+1)&255;addr=(uint8_t)scanAddress;value=activeBank()[addr];}
  uint16_t logical=(uint16_t)(((uint16_t)addr<<8)|value);
  // The expander has binary latched outputs; keep normal panel indications
  // steady. Only the independent three-flash error pulse is time-varying.
  if(errorPulse.update(now,!ledFaultLatched,!inputFaultLatched))logical=br_error_pulse_word(errorPulse,logical);
  queueLed(logical);
}

static void replyError(int status,const char* code,const char* msg,const char *phase="REQUEST_VALIDATION"){webRequestStatus=status;webRequestPhase=phase;if(webMutationContext){webMutationFailed=true;appLogEvent("WEB_MUTATION_REJECTED","id=%lu operation=%s slot=%u http=%d code=%s phase=%s",(unsigned long)webRequestSequence,webMutationContext,(unsigned)webMutationSlot+1,status,code,phase);}String s=String("{\"requestId\":\"")+String(bootId)+"-"+String(webRequestSequence)+"\",\"ok\":false,\"code\":\""+jsonEscape(code)+"\",\"phase\":\""+jsonEscape(phase)+"\",\"message\":\""+jsonEscape(msg)+"\"}";http.sendHeader("Cache-Control","no-store, no-cache, must-revalidate, max-age=0");http.send(status,"application/json; charset=utf-8",s);}
static void appendActiveControls(String &out){
  static const char *const expanderNames[14]={"D0","D1","D2","D3","D4","D5","D6","D7","HOME","RESET","WRITE","WRITE+","EXP/EXAM","NEXT"};
  static const int modePins[5]={PIN_RUN,PIN_VIDEO,PIN_LIVE,PIN_AUTO,PIN_EDIT};
  static const char *const modeNames[5]={"RUN","VIDEO","LIVE","AUTO","EDIT"};
  bool first=true;out+="[";
  if(i2cDiagnostics.hasValidRaw){
    const uint16_t active=(uint16_t)(~i2cDiagnostics.lastValidRaw);
    for(uint8_t i=0;i<14;i++)if(active&(1u<<i)){if(!first)out+=",";out+="\"";out+=expanderNames[i];out+="\"";first=false;}
  }
  for(uint8_t i=0;i<5;i++)if(digitalRead(modePins[i])==LOW){if(!first)out+=",";out+="\"";out+=modeNames[i];out+="\"";first=false;}
  out+="]";
}
static void appendI2cStatus(String &out,uint32_t now){
  char raw[7];snprintf(raw,sizeof(raw),"0x%04X",i2cDiagnostics.lastValidRaw);
  out+="\"i2c\":{\"sdaPin\":"+String(I2C_SDA_PIN)+",\"sclPin\":"+String(I2C_SCL_PIN)+",\"frequencyHz\":"+String(I2C_FREQUENCY_HZ);
  out+=",\"inputReadErrors\":"+String(i2cDiagnostics.inputReadErrors)+",\"ledWriteErrors\":"+String(i2cDiagnostics.ledWriteErrors);
  out+=",\"lastRead\":{\"address\":\"0x22\",\"requested\":"+String(i2cDiagnostics.lastReadRequested)+",\"returned\":"+String(i2cDiagnostics.lastReadReturned)+",\"received\":"+String(i2cDiagnostics.lastReadAvailable)+"}";
  if(i2cDiagnostics.lastErrorOperation==BR_I2C_OP_NONE)out+=",\"lastError\":null";
  else out+=",\"lastError\":{\"address\":\"0x"+String(i2cDiagnostics.lastErrorAddress,HEX)+"\",\"operation\":\""+brI2cOperationName(i2cDiagnostics.lastErrorOperation)+"\",\"returnCode\":"+String(i2cDiagnostics.lastErrorCode)+"}";
  if(i2cDiagnostics.hasValidRaw)out+=",\"lastValidInput\":{\"raw\":\""+String(raw)+"\",\"ageMs\":"+String((uint32_t)(now-i2cDiagnostics.lastValidAtMs))+"},\"activeControls\":";
  else out+=",\"lastValidInput\":null,\"activeControls\":";
  appendActiveControls(out);out+="}";
}
static void serveStatus(){BrWebTraceGuard trace;
  const uint32_t now=millis();
  BrAudioTransportStats audioStats;portENTER_CRITICAL(&audioStatsMux);audioStats=audioTransportStats;portEXIT_CRITICAL(&audioStatsMux);
  uint32_t freeHeap=ESP.getFreeHeap(),minHeap=ESP.getMinFreeHeap(),largest=heap_caps_get_largest_free_block(MALLOC_CAP_DMA);
  runtimeCounters.i2cErrors=i2cDiagnostics.inputReadErrors+i2cDiagnostics.ledWriteErrors;
  String s="{\"project\":\"byteREEL 2026\",\"isa\":\"2.0\",\"firmwareVersion\":\""+String(BR_FIRMWARE_VERSION)+"\",\"uiVersion\":\""+String(BR_UI_VERSION)+"\",\"bootId\":"+String(bootId)+",\"resetReason\":"+String((int)bootResetReason)+",\"uptimeMs\":"+String(now)+",\"flashBytes\":"+String(ESP.getFlashChipSize())+",\"mode\":\""+(vm.mode==VM::VIDEO?String("VIDEO"):String("AUDIO"))+"\",\"running\":"+(vm.running?"true":"false")+",\"pc\":"+String(vm.pc)+",\"a\":"+String(vm.a)+",\"c\":"+String(vm.c)+",\"z\":"+(vm.z?"true":"false")+",\"editorAddr\":"+String(vm.mode==VM::VIDEO?editorVideo:editorAudio)+",\"dirtyVideo\":"+(dirtyVideo?"true":"false")+",\"dirtyAudio\":"+(dirtyAudio?"true":"false")+",\"requestedIps\":"+String(runtimeCounters.requestedIps)+",\"measuredIps\":"+String(runtimeCounters.measuredIps)+",\"pending\":"+String((int)vm.pending)+",\"freeHeap\":"+String(freeHeap)+",\"minHeap\":"+String(minHeap)+",\"largestDmaBlock\":"+String(largest)+",\"i2cErrors\":"+String(runtimeCounters.i2cErrors)+",\"slotOccupancyMask\":"+String(slotValidMask)+",\"slotErrorMask\":"+String(slotErrorMask)+",\"wifiStations\":"+String(wifiAPReady?WiFi.softAPgetStationNum():0)+",\"audioWriteCalls\":"+String(audioStats.calls)+",\"audioBytesWritten\":"+String(audioStats.bytesWritten)+",\"audioPartialWrites\":"+String(audioStats.partialCalls)+",\"audioWriteTimeouts\":"+String(audioStats.timeouts)+",\"audioBlockDeadlineExpirations\":"+String(audioStats.deadlineExpirations)+",\"audioWriteErrors\":"+String(audioStats.writeErrors)+",\"audioIncompleteBlockAttempts\":"+String(audioStats.incompleteAttempts)+",\"audioBlocksCompleted\":"+String(audioStats.completeBlocks)+",\"audioMaxWriteUs\":"+String(audioStats.maxWriteUs)+",\"audioMaxBlockFillUs\":"+String(audioStats.maxBlockFillUs)+",\"audioMaxBlockPeriodUs\":"+String(audioStats.maxBlockPeriodUs)+",\"audioDmaUnderruns\":null,\"maxLoopUs\":"+String(maxLoopUs)+",\"maxHttpUs\":"+String(maxHttpUs)+",\"maxSlotReadUs\":"+String(maxSlotReadUs)+",\"maxSlotCommitUs\":"+String(maxSlotCommitUs)+",\"maxI2cReadUs\":"+String(maxI2cReadUs)+",\"maxVideoStartUs\":"+String(maxVideoStartUs)+",\"uartRx\":"+String(runtimeCounters.uartRx)+",\"uartTx\":"+String(runtimeCounters.uartTx)+",\"uartRxOverflow\":"+String(runtimeCounters.uartRxOverflow)+",\"uartTimeout\":"+String(runtimeCounters.uartTimeout)+",\"droppedFrames\":"+String(runtimeCounters.videoDroppedFrames)+",\"fetchDropped\":"+String(fetchFifo.dropped)+",";
  s+="\"storage\":{\"available\":"+(storageHealthy?String("true"):String("false"))+",\"state\":\""+(storageHealthy?String("READY"):storageError==BR_STORAGE_NOT_EMPTY?String("PARTITION_CONTAINS_DATA"):storageError==BR_STORAGE_VERIFY_FAILED?String("PARTITION_VERIFY_FAILED"):storageError==BR_STORAGE_NOT_STOP?String("STOP_REQUIRED"):storageError==BR_STORAGE_MOUNT_FAILED?String("MOUNT_FAILED"):storageError==BR_STORAGE_PARTITION_MISSING?String("PARTITION_MISSING"):String("INITIALIZATION_FAILED"))+"\"},";
  appendI2cStatus(s,now);s+="}";
  http.sendHeader("Cache-Control","no-store, no-cache, must-revalidate, max-age=0");webRequestStatus=200;http.send(200,"application/json",s);
}
static String jsonEscape(const char *text){String out;while(*text){uint8_t c=(uint8_t)*text++;if(c=='"'||c=='\\'){out+='\\';out+=(char)c;}else if(c=='\n')out+="\\n";else if(c=='\r')out+="\\r";else if(c=='\t')out+="\\t";else if(c<0x20)out+=' ';else out+=(char)c;}return out;}
static void serveLogs(){BrWebTraceGuard trace;
  const uint32_t requestedBoot=(uint32_t)http.arg("bootId").toInt();uint32_t after=(uint32_t)http.arg("after").toInt();int limit=http.arg("limit").toInt();if(limit<1)limit=16;if(limit>16)limit=16;
  const uint32_t oldest=appLog.oldest();BrLogCursor cursor;cursor.begin(requestedBoot,bootId,after,oldest);String entries="";bool first=true;
  for(uint8_t i=0;i<limit;i++){const BrLogEntry*e=appLog.find(cursor.sequence+1);if(!e)break;String item="{\"seq\":"+String(e->sequence)+",\"ms\":"+String(e->atMs)+",\"type\":\""+jsonEscape(e->type)+"\",\"text\":\""+jsonEscape(e->text)+"\"}";if(entries.length()+item.length()+32>1850)break;if(!first)entries+=',';first=false;entries+=item;cursor.advance(e->sequence);}
  String out="{\"bootId\":"+String(bootId)+",\"oldest\":"+String(oldest)+",\"next\":"+String(cursor.sequence)+",\"lost\":"+(cursor.lost?"true":"false")+",\"entries\":["+entries+"]}";http.sendHeader("Cache-Control","no-store, no-cache, must-revalidate, max-age=0");webRequestStatus=200;http.send(200,"application/json",out);
}
static bool jsonBalanced(const String&s){return br_json_balanced(s);}
static bool objectFromBody(const String&body,String&obj){return jsonObjectField(body,"slot",obj);}
static bool jsonUnsignedField(const String&j,const char*key,uint32_t&v){return br_json_unsigned(j,key,v);}
static bool sameSourceBytes(const String&source,const char*bank,const uint8_t*bytes,uint32_t *written=nullptr,unsigned *different=nullptr,uint8_t *expected=nullptr,uint8_t *supplied=nullptr,String *invalidReason=nullptr){if(source.length()>4096){if(different)*different=UINT_MAX;if(invalidReason)*invalidReason="assembler source exceeds 4096 bytes";return false;}BrSourceCheckResult check=br_check_source_bytes(std::string(source.c_str()),bank,bytes);if(written)*written=check.assembledLength;if(check.result==BR_SOURCE_MISMATCH){if(different)*different=check.address;if(expected)*expected=check.expected;if(supplied)*supplied=check.supplied;}else if(check.result==BR_SOURCE_INVALID){if(different)*different=UINT_MAX;if(invalidReason)*invalidReason=check.error.c_str();}return check.result==BR_SOURCE_MATCH;}
static bool validateBank(uint8_t slotId,const String&j,const char*bank,uint8_t*out,String&field,String&reason){String object,hex,source;bool isNull=false;field=String(bank);if(!jsonObjectField(j,bank,object)){reason="missing bank object";return false;}field=String(bank)+".hex";if(!jsonStringField(object,"hex",hex,isNull)||isNull){reason="expected a HEX string containing exactly 256 bytes";return false;}int bad=-1;if(!parseHexBank(hex,out,&bad)){reason=bad<0?String("expected 512 hexadecimal characters, got ")+String(hex.length()):String("non-HEX character at character ")+String(bad);return false;}field=String(bank)+".source";if(!jsonStringField(object,"source",source,isNull)){reason="expected assembler source text or null";return false;}bool matches=!isNull,matchField=false;size_t matchPos=0;if(br_json_top_field(object,"sourceMatches",matchPos)){if(!br_json_boolean(object,"sourceMatches",matches)){reason="expected sourceMatches boolean";return false;}matchField=true;}if(isNull&&matches){reason="sourceMatches=true requires assembler source text";return false;}field=String(bank)+".length";uint32_t length=0;bool hasLength=jsonUnsignedField(object,"length",length);int k=object.indexOf("\"length\"");if(k>=0&&!hasLength){int colon=object.indexOf(':',k);if(colon<0){reason="invalid JSON value";return false;}int p=colon+1;while(p<object.length()&&isspace((uint8_t)object[p]))p++;if(object.substring(p,p+4)!="null"){reason="expected integer 0..256 or null";return false;}}if(hasLength&&length>256){reason="program byte length must be 0..256";return false;}if(isNull||!matches)return true;uint32_t assembledLength=0;unsigned different=0;uint8_t expected=0,supplied=0;String assemblerError;field=String(bank)+".source";if(!sameSourceBytes(source,bank,out,&assembledLength,&different,&expected,&supplied,&assemblerError)){if(different==UINT_MAX){reason=String("slot ")+String((unsigned)slotId+1)+" bank "+bank+" assembler rejected source: "+assemblerError;return false;}reason=String("slot ")+String((unsigned)slotId+1)+" bank "+bank+" source/HEX mismatch at address 0x"+(different<16?String("0"):String(""))+String(different,HEX)+String(" expected=0x")+(expected<16?String("0"):String(""))+String(expected,HEX)+String(" supplied=0x")+(supplied<16?String("0"):String(""))+String(supplied,HEX);return false;}if(hasLength&&length!=assembledLength){field=String(bank)+".length";reason=String("expected assembler length ")+String(assembledLength)+" bytes, got "+String(length);return false;}return true;}
static bool validateUpload(uint8_t id,const String&j,uint8_t*v,uint8_t*a,String&field,String&reason){if(j.length()>BR_SLOT_MAX_PAYLOAD){field="slot";reason="serialized slot exceeds 12288 bytes";return false;}if(j.isEmpty()||!jsonBalanced(j)||j[0]!='{'||j[j.length()-1]!='}'){field="slot";reason="expected one complete JSON object";return false;}uint32_t schema=0,slot=99;String isa,name;bool null=false;field="schema";if(!jsonUnsignedField(j,"schema",schema)||schema!=1){reason="expected schema 1";return false;}field="slot";if(!jsonUnsignedField(j,"slot",slot)||slot>7||slot!=id){reason=String("route uses internal index ")+String(id)+" (UI numbering is 1–8); record slot must match";return false;}field="isa";if(!jsonStringField(j,"isa",isa,null)||null||isa!="2.0"){reason="expected ISA string 2.0";return false;}field="name";if(!jsonStringField(j,"name",name,null)||null||name.length()>32){reason="expected a string of at most 32 characters";return false;}if(!validateBank(id,j,"video",v,field,reason))return false;if(!validateBank(id,j,"audio",a,field,reason))return false;return true;}
static bool validateUploadShared(uint8_t id,const String&j,uint8_t*v,uint8_t*a,String&field,String&reason){BrSlotUploadValidation r=br_validate_slot_upload(std::string(j.c_str(),j.length()),id);field=r.field.c_str();reason=r.reason.c_str();if(!r.ok)return false;memcpy(v,r.video,256);memcpy(a,r.audio,256);return true;}
#define validateUpload validateUploadShared
static void apiAssemble(){String body=http.arg("plain"),bank,source;bool n=false;if(!http.header("Content-Type").startsWith("application/json")){replyError(400,"CONTENT_TYPE","Požadováno application/json");return;}if(body.length()>16384){replyError(413,"BODY_TOO_LARGE","Tělo je příliš velké");return;}if(!jsonBalanced(body)||!jsonStringField(body,"bank",bank,n)||n||!jsonStringField(body,"source",source,n)||n||source.length()>4096){replyError(400,"ASSEMBLY_INPUT","Očekávám bank VIDEO/AUDIO a source do 4096 B");return;}if(bank!="VIDEO"&&bank!="AUDIO"){replyError(400,"ASSEMBLY_BANK","Banka musí být VIDEO nebo AUDIO");return;}webRequestPhase="ASSEMBLER";try{BrAsmResult result=br_assemble(std::string(source.c_str()),bank.c_str());static const char digits[]="0123456789ABCDEF";String out="{\"bank\":\""+bank+"\",\"hex\":\"";out.reserve(1200);for(int i=0;i<256;i++){out+=digits[result.bytes[i]>>4];out+=digits[result.bytes[i]&15];}out+="\",\"bytes\":"+String(result.written)+",\"labels\":{";bool first=true;for(const auto&entry:result.labels){if(!first)out+=',';first=false;out+="\""+String(entry.first.c_str())+"\":"+String(entry.second);}out+="}}";webRequestStatus=200;webRequestPhase="ASSEMBLY_COMPLETE";http.send(200,"application/json",out);}catch(const std::exception&e){String reason=String("Assembler odmítl zdroj: ")+e.what();replyError(400,"ASSEMBLY_ERROR",reason.c_str(),"ASSEMBLER");}}
static void apiPutSlot(uint8_t id){if(panelRun||vm.running||digitalRead(PIN_RUN)==LOW){replyError(409,"STOP_REQUIRED","Přepni přístroj do STOP a ulož znovu","STATE_GUARD");return;}if(storageBusy){replyError(409,"BUSY","Úložiště právě zpracovává jiný zápis","STATE_GUARD");return;}if(!storageHealthy){replyError(503,"FILESYSTEM_UNAVAILABLE","LittleFS není připojené; data zůstala beze změny","FILESYSTEM");return;}String ct=http.header("Content-Type");ct.trim();String ctLower=ct;ctLower.toLowerCase();if(!ctLower.startsWith("application/json")||(ctLower.length()>16&&ctLower[16]!=';'&&!isspace((uint8_t)ctLower[16]))){replyError(400,"CONTENT_TYPE","Požadováno Content-Type application/json","REQUEST_HEADERS");return;}String origin=http.header("Origin");if(origin.length()&&origin!="http://192.168.4.1"){replyError(400,"ORIGIN","Origin odmítnut","REQUEST_HEADERS");return;}String body=http.arg("plain");if(body.length()>16384){replyError(413,"BODY_TOO_LARGE","Tělo je příliš velké; maximum je 16384 B","REQUEST_BODY");return;}if(!jsonBalanced(body)){replyError(400,"JSON_INVALID","Tělo není platný úplný JSON","REQUEST_PARSE");return;}uint32_t expected=0;if(!jsonUnsignedField(body,"expectedRevision",expected)){replyError(400,"REVISION_REQUIRED","Chybí integer expectedRevision","REQUEST_PARSE");return;}String obj;if(!objectFromBody(body,obj)){replyError(400,"SLOT_REQUIRED","Chybí objekt slot","REQUEST_PARSE");return;}uint8_t v[256],a[256];String field,validation;if(!validateUpload(id,obj,v,a,field,validation)){String reason=String("pole ")+field+": "+validation;replyError(400,"SLOT_INVALID",reason.c_str(),"SOURCE_BYTECODE_VALIDATION");return;}if(!storageAcquire()){replyError(503,"STORAGE_ERROR","Nedostatek RAM pro oba kandidáty","BUFFER_ALLOC");return;}uint32_t rev=0;BrSlotView cur;size_t na,nb;BrSlotState st=slotCurrent(id,cur,na,nb);storageRelease();if(st==BR_SLOT_CORRUPT){replyError(409,"SLOT_CORRUPT","Soubory slotu jsou poškozené nebo nelze ověřit CRC","READ_CANDIDATES");return;}if(st==BR_SLOT_VALID||st==BR_SLOT_DEGRADED)rev=cur.generation;if(expected!=rev){replyError(409,"REVISION_CONFLICT","Slot byl mezitím změněn; znovu jej načti před uložením","REVISION");return;}uint32_t outRev=0;BrCommitPhase phase=BR_COMMIT_PHASE_NONE;if(!storageAcquire()){replyError(503,"STORAGE_ERROR","Nedostatek RAM pro oba kandidáty","BUFFER_ALLOC");return;}BrCommitResult commit=slotWrite(id,(const uint8_t*)obj.c_str(),obj.length(),expected,outRev,&phase);storageRelease();if(commit!=BR_COMMIT_OK){const int status=commit==BR_COMMIT_CONFLICT||commit==BR_COMMIT_CORRUPT?409:503;String reason=String("code=")+br_commit_result_name(commit)+" phase="+br_commit_phase_name(phase)+"; ověřená původní kopie zůstala zachována";replyError(status,br_commit_result_name(commit),reason.c_str(),br_commit_phase_name(phase));return;}webRequestStatus=200;webRequestPhase="SAVE_COMMIT_VERIFIED";http.send(200,"application/json","{\"ok\":true,\"revision\":"+String(outRev)+"}");}

static void apiDeleteSlot(uint8_t id){
  if(panelRun||vm.running||digitalRead(PIN_RUN)==LOW){replyError(409,"STOP_REQUIRED","Mazání je povoleno pouze ve STOP");return;}
  if(!storageHealthy){replyError(503,"FILESYSTEM_UNAVAILABLE","Úložiště není připojeno; obsah zůstal beze změny");return;}
  if(storageBusy){replyError(409,"BUSY","Úložiště právě zpracovává jinou operaci");return;}
  String body=http.arg("plain");uint32_t expected=0;if(!jsonBalanced(body)||!jsonUnsignedField(body,"expectedRevision",expected)){replyError(400,"REVISION_REQUIRED","Mazání vyžaduje platný expectedRevision");return;}
  if(!storageAcquire()){replyError(503,"STORAGE_ERROR","Nedostatek RAM pro úložiště");return;}
  storageBusy=true;runGate.reset();BrLittleFsStore store(LittleFS,"/slots",storageHealthy);BrDeleteResult result=br_delete_slot(store,id,expected,slotA,slotB,SLOT_FILE_CAP);storageBusy=false;storageRelease();
  if(result==BR_DELETE_CORRUPT){replyError(409,"SLOT_CORRUPT","Slot je poškozený; mazání odmítnuto kvůli ochraně dat");return;}
  if(result==BR_DELETE_CONFLICT){replyError(409,"REVISION_CONFLICT","Slot byl mezitím změněn");return;}
  if(result!=BR_DELETE_OK){replyError(503,"DELETE_FAILED","Mazání selhalo; úplná platná kopie slotu zůstala zachována");return;}
  slotValidMask&=(uint8_t)~(1u<<id);slotErrorMask&=(uint8_t)~(1u<<id);appLogEvent("SLOT_DELETE","slot=%u verified=both_generations_removed",(unsigned)id+1);webRequestStatus=200;webRequestPhase="DELETE_VERIFIED";http.send(200,"application/json","{\"ok\":true,\"revision\":0}");
}

static String slotListJson(){
  if(!storageHealthy)return "";
  if(!storageAcquire())return "";
  String out="[";
  for(uint8_t i=0;i<8;i++){
    if(i)out+=',';BrSlotView v;size_t na=0,nb=0;BrSlotState state=slotCurrent(i,v,na,nb);
    const char *stateName=state==BR_SLOT_EMPTY?"EMPTY":state==BR_SLOT_CORRUPT?"CORRUPT":state==BR_SLOT_DEGRADED?"DEGRADED":"OK";
    String name=panelSlotName(i,v);String clean=name; if(clean.startsWith("\""))clean=clean.substring(1,clean.length()-1);
    String payload="";if(v.payload)for(uint32_t j=0;j<v.length;j++)payload+=(char)v.payload[j];
    auto lengthJson=[&](const char *key)->String{String bank;uint32_t n;if(!v.payload||!jsonObjectField(payload,key,bank))return "null";if(jsonUnsignedField(bank,"length",n)&&n<=256)return String(n);String source;bool isNull=false;if(!jsonStringField(bank,"source",source,isNull)||isNull)return "null";try{BrAsmResult assembled=br_assemble(std::string(source.c_str()),strcmp(key,"video")==0?"VIDEO":"AUDIO");return String(assembled.written);}catch(const std::exception&){return "null";}};
    out+="{\"slot\":"+String(i)+",\"name\":\""+jsonEscape(clean.c_str())+"\",\"state\":\""+stateName+"\",\"revision\":"+String(v.generation)+",\"videoBytes\":"+lengthJson("video")+",\"audioBytes\":"+lengthJson("audio")+",\"bankCapacity\":256}";
  }
  out+=']';storageRelease();return out;
}

static void webPutSlot(uint8_t id){webRequestPhase="SAVE_VALIDATE";webMutationContext="SAVE";webMutationSlot=id;webMutationFailed=false;appLogEvent("WEB_MUTATION_BEGIN","requestId=%lu operation=SAVE slot=%u",(unsigned long)webRequestSequence,(unsigned)id+1);apiPutSlot(id);if(!webMutationFailed){webRequestPhase="SAVE_COMMIT_VERIFIED";appLogEvent("WEB_MUTATION_OK","requestId=%lu operation=SAVE slot=%u",(unsigned long)webRequestSequence,(unsigned)id+1);}webMutationContext=nullptr;}
static void webDeleteSlot(uint8_t id){webRequestPhase="DELETE_VALIDATE";webMutationContext="DELETE";webMutationSlot=id;webMutationFailed=false;appLogEvent("WEB_MUTATION_BEGIN","requestId=%lu operation=DELETE slot=%u",(unsigned long)webRequestSequence,(unsigned)id+1);apiDeleteSlot(id);if(!webMutationFailed){webRequestPhase="DELETE_VERIFIED";appLogEvent("WEB_MUTATION_OK","requestId=%lu operation=DELETE slot=%u",(unsigned long)webRequestSequence,(unsigned)id+1);}webMutationContext=nullptr;}

#if 0 // Retired route setup; kept out of the compiled firmware.
static void webInit(){static const char* headerKeys[]={"Content-Type","Origin"};WiFi.mode(WIFI_AP);uint8_t mac[6];WiFi.softAPmacAddress(mac);char ssid[32];snprintf(ssid,sizeof(ssid),"byteREEL-2026-%02X%02X",mac[4],mac[5]);wifiAPReady=WiFi.softAP(ssid,nullptr,1,false,2);if(!wifiAPReady)appLogf("WIFI_AP_START_FAILED");if(!WiFi.softAPConfig(IPAddress(192,168,4,1),IPAddress(192,168,4,1),IPAddress(255,255,255,0)))appLogf("WIFI_AP_CONFIG_FAILED");http.collectHeaders(headerKeys,2);http.on("/",HTTP_GET,[]{http.send_P(200,"text/html; charset=utf-8",BR_PAGE);});http.on("/api/v1/status",HTTP_GET,serveStatus);http.on("/api/v1/log",HTTP_GET,serveLogs);http.on("/api/v1/slots",HTTP_GET,[]{if(!storageHealthy){replyError(503,"FILESYSTEM_UNAVAILABLE","LittleFS není dostupný; sloty nelze označit jako prázdné");return;}String out=slotListJson();if(out.isEmpty()){replyError(503,"STORAGE_ERROR","Nedostatek RAM pro čtení slotů");return;}http.send(200,"application/json",out);});http.on("/api/v1/assemble",HTTP_POST,apiAssemble);http.on("/api/v1/instructions",HTTP_GET,[]{http.send_P(200,"application/json",BR_INSTRUCTION_CATALOG);});http.onNotFound([]{String u=http.uri();if(u.startsWith("/api/v1/slots/")){String tail=u.substring(14);bool exp=tail.endsWith("/export");if(exp)tail.remove(tail.length()-7);int id=tail.toInt();if(id<0||id>7||tail.length()!=1){replyError(404,"NOT_FOUND","Neplatný slot");return;}if(http.method()==HTTP_PUT){apiPutSlot(id);return;}if(http.method()==HTTP_DELETE){apiDeleteSlot(id);return;}if(http.method()!=HTTP_GET){replyError(405,"METHOD_NOT_ALLOWED","Tato operace není podporována");return;}if(!storageHealthy){replyError(503,"FILESYSTEM_UNAVAILABLE","LittleFS není dostupný; slot nebyl označen jako prázdný");return;}if(!storageAcquire()){replyError(503,"STORAGE_ERROR","Nedostatek RAM");return;}String payload;uint32_t rev=0;BrSlotView view;size_t na=0,nb=0;BrSlotState state=slotCurrent(id,view,na,nb);bool found=(state==BR_SLOT_VALID||state==BR_SLOT_DEGRADED)&&slotGetPayload(id,payload,rev);storageRelease();if(!found){replyError(state==BR_SLOT_EMPTY?404:409,state==BR_SLOT_EMPTY?"SLOT_EMPTY":"SLOT_CORRUPT",state==BR_SLOT_EMPTY?"Slot je prázdný":"Slot je poškozený; data nebyla změněna");return;}if(exp){http.sendHeader("Content-Disposition","attachment; filename=bytereel-slot.json");http.send(200,"application/json",payload);}else http.send(200,"application/json","{\"revision\":"+String(rev)+",\"slot\":"+payload+"}");return;}replyError(404,"NOT_FOUND","Neznámá cesta");});http.begin();}
#endif

static void webConfigureRoutes(){
  if(webRoutesConfigured)return;
  static const char* headerKeys[]={"Content-Type","Origin","Content-Length","X-byteREEL-Client-Request-Id"};http.collectHeaders(headerKeys,4);
  http.on("/",HTTP_GET,[]{BrWebTraceGuard trace;http.sendHeader("Cache-Control","no-store, no-cache, must-revalidate, max-age=0");http.sendHeader("Pragma","no-cache");http.sendHeader("Expires","0");http.sendHeader("X-byteREEL-UI",BR_UI_VERSION);webRequestStatus=200;http.send_P(200,"text/html; charset=utf-8",BR_PAGE);});
  http.on("/api/v1/status",HTTP_GET,serveStatus);http.on("/api/v1/log",HTTP_GET,serveLogs);
  http.on("/api/v1/slots",HTTP_GET,[]{BrWebTraceGuard trace;if(!storageHealthy){replyError(503,"FILESYSTEM_UNAVAILABLE","LittleFS není dostupný; sloty nelze označit jako prázdné");return;}String out=slotListJson();if(out.isEmpty()){replyError(503,"STORAGE_ERROR","Nedostatek RAM pro čtení slotů");return;}http.sendHeader("Cache-Control","no-store, no-cache, must-revalidate, max-age=0");webRequestStatus=200;http.send(200,"application/json",out);});
  http.on("/api/v1/assemble",HTTP_POST,[]{BrWebTraceGuard trace;webRequestPhase="ASSEMBLY_INPUT";apiAssemble();});http.on("/api/v1/instructions",HTTP_GET,[]{BrWebTraceGuard trace;http.sendHeader("Cache-Control","no-store");webRequestStatus=200;http.send_P(200,"application/json",BR_INSTRUCTION_CATALOG);});
  http.onNotFound([]{BrWebTraceGuard trace;
    BrWebMethod method=http.method()==HTTP_GET?BR_WEB_GET:http.method()==HTTP_PUT?BR_WEB_PUT:http.method()==HTTP_DELETE?BR_WEB_DELETE:BR_WEB_OTHER;
    BrWebMatch match=br_match_slot_route(method,http.uri().c_str());
    if(match.route==BR_WEB_ROUTE_NONE){replyError(404,"NOT_FOUND","Neznámá cesta");return;}
    if(match.route==BR_WEB_ROUTE_INVALID){replyError(404,"SLOT_ROUTE_INVALID","Neplatná cesta, metoda nebo číslo slotu 1–8");return;}
    uint8_t id=match.slot;
    if(match.route==BR_WEB_ROUTE_SLOT_PUT){webPutSlot(id);return;}
    if(match.route==BR_WEB_ROUTE_SLOT_DELETE){webDeleteSlot(id);return;}
    if(!storageHealthy){replyError(503,"FILESYSTEM_UNAVAILABLE","LittleFS není dostupný; slot nebyl označen jako prázdný");return;}
    if(!storageAcquire()){replyError(503,"STORAGE_ERROR","Nedostatek RAM pro čtení slotu");return;}
    String payload;uint32_t rev=0;BrSlotView view;size_t na=0,nb=0;BrSlotState state=slotCurrent(id,view,na,nb);bool found=(state==BR_SLOT_VALID||state==BR_SLOT_DEGRADED)&&slotGetPayload(id,payload,rev);storageRelease();
    if(!found){replyError(state==BR_SLOT_EMPTY?404:409,state==BR_SLOT_EMPTY?"SLOT_EMPTY":"SLOT_CORRUPT",state==BR_SLOT_EMPTY?"Slot je prázdný":"Slot je poškozený; data nebyla změněna");return;}
    http.sendHeader("Cache-Control","no-store, no-cache, must-revalidate, max-age=0");webRequestStatus=200;if(match.route==BR_WEB_ROUTE_SLOT_EXPORT){http.sendHeader("Content-Disposition","attachment; filename=bytereel-slot.json");http.send(200,"application/json",payload);}else http.send(200,"application/json","{\"revision\":"+String(rev)+",\"slot\":"+payload+
      "}");
  });webRoutesConfigured=true;
}
static void webModeTransition(bool memory){
  if(memory==wifiDesiredMemory)return;
  if(memory){
    if(!wifiTaskReady){appLogEvent("WIFI_ERROR","phase=TASK_CREATE reason=worker_unavailable");return;}
    wifiDesiredMemory=true;wifiRequestGeneration++;appLogEvent("WIFI_START","reason=MEMORY freeHeap=%u largestDma=%u",(unsigned)ESP.getFreeHeap(),(unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_DMA));
  }else{
    if(webRunning){http.stop();webRunning=false;}
    if(wifiDesiredMemory){wifiStopLogPending=true;wifiDesiredMemory=false;wifiRequestGeneration++;wifiAPReady=false;}
  }
}
static void serviceWebLifecycle(){
  const uint32_t request=wifiRequestGeneration;if(wifiResultGeneration!=request)return;
  if(wifiDesiredMemory){
    if(wifiTaskState==BR_WIFI_READY&&!webRunning){webConfigureRoutes();http.begin();webRunning=true;wifiAPReady=true;uint8_t mac[6];WiFi.softAPmacAddress(mac);appLogEvent("WIFI_READY","ssid=byteREEL-2026-%02X%02X ip=192.168.4.1 freeHeap=%u largestDma=%u",mac[4],mac[5],(unsigned)ESP.getFreeHeap(),(unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_DMA));}
    else if(wifiTaskState==BR_WIFI_ERROR_START||wifiTaskState==BR_WIFI_ERROR_IP){wifiAPReady=false;appLogEvent("WIFI_ERROR","phase=%s reason=AP_start_failed",wifiTaskState==BR_WIFI_ERROR_START?"SOFTAP_START":"IP_CONFIG");wifiDesiredMemory=false;wifiRequestGeneration++;}
  }else if(wifiTaskState==BR_WIFI_OFF){wifiAPReady=false;if(wifiStopLogPending){appLogEvent("WIFI_STOP","reason=EDIT clients_disconnected=1 freeHeap=%u largestDma=%u",(unsigned)ESP.getFreeHeap(),(unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_DMA));wifiStopLogPending=false;}}
}

void setup(){
  runGate.boot();
  Serial.begin(115200);
  delay(50);
  bootId=esp_random();
  bootResetReason=esp_reset_reason();
  Serial.printf("BOOT byteREEL-2026 id=%lu reset=%d | I2C SDA=%d SCL=%d %lu Hz | USB UART0=115200 | peripheral UART2 RX=%d TX=%d %lu baud\n",
    (unsigned long)bootId,(int)bootResetReason,I2C_SDA_PIN,I2C_SCL_PIN,
    (unsigned long)I2C_FREQUENCY_HZ,PIN_UART_RX,PIN_UART_TX,(unsigned long)UART_BAUD);
  appLogf("BOOT byteREEL-2026 id=%lu",(unsigned long)bootId);
#if BR_AUDIO_DIAGNOSTIC
  audioDiagnosticTone.init();appLogEvent("AUDIO_DIAGNOSTIC_READY","tone=440Hz amplitude=%d tableSamples=%u blockSamples=%u; RUN plays, STOP mutes; VM disabled",BrAudioDiagnosticTone::DAC_AMPLITUDE,BrAudioDiagnosticTone::TABLE_SAMPLES,BrAudioDiagnosticTone::BLOCK_SAMPLES);
#endif
  audioWriteMutex=xSemaphoreCreateMutex();
  audioTaskReady=audioWriteMutex&&xTaskCreatePinnedToCore(audioTask,"bytereel-audio",4096,nullptr,3,&audioTaskHandle,0)==pdPASS;
  wifiTaskReady=xTaskCreatePinnedToCore(wifiControlTask,"bytereel-wifi",3072,nullptr,1,&wifiTaskHandle,0)==pdPASS;
  if(!wifiTaskReady)appLogEvent("WIFI_ERROR","phase=TASK_CREATE reason=worker_unavailable");
  Serial2.begin(UART_BAUD,SERIAL_8N1,PIN_UART_RX,PIN_UART_TX);
  Wire.begin(I2C_SDA_PIN,I2C_SCL_PIN);
  Wire.setClock(I2C_FREQUENCY_HZ);
  Wire.setTimeOut(25);
  pinMode(PIN_RUN,INPUT_PULLUP);pinMode(PIN_VIDEO,INPUT_PULLUP);pinMode(PIN_LIVE,INPUT_PULLUP);pinMode(PIN_AUTO,INPUT_PULLUP);pinMode(PIN_EDIT,INPUT_PULLUP);pinMode(PIN_SPEED,INPUT);
  ledcSetup(0,1000,8);ledcAttachPin(PIN_PIEZO,0);piezoOff();
  memset(videoRam,0,256);memset(audioRam,0,256);videoRam[0]=audioRam[0]=1;
  bool bootVideo=digitalRead(PIN_VIDEO)==LOW;vm.reset(bootVideo?VM::VIDEO:VM::AUDIO);speedFiltered=analogRead(PIN_SPEED);
  storageInit();refreshSlotValidity();ledsInit();
  if(bootVideo)videoStart();else audioStart();
  piezoSet(900,8);delay(100);piezoSet(1200,8);delay(100);piezoOff();
}
void loop(){const uint32_t loopStartedUs=micros();uint32_t now=millis();
  if(now-speedAt>=10){speedAt=now;float raw=analogRead(PIN_SPEED);speedFiltered+=.15f*(raw-speedFiltered);vm.intervalMs=(uint16_t)lroundf(1000.0f*powf(.005f,speedFiltered/4095.0f));static uint16_t lastLoggedInterval=0;static uint32_t lastSpeedLogAt=0;uint16_t delta=vm.intervalMs>lastLoggedInterval?vm.intervalMs-lastLoggedInterval:lastLoggedInterval-vm.intervalMs;if((!lastLoggedInterval||delta>=3)&&(uint32_t)(now-lastSpeedLogAt)>=1000){uint16_t requested=(uint16_t)lroundf(1000.0f/(vm.intervalMs?vm.intervalMs:1));appLogEvent("SPEED_CHANGE","intervalMs=%u requestedIPS=%u",vm.intervalMs,requested);lastLoggedInterval=vm.intervalMs;lastSpeedLogAt=now;}}
  if(now-panelAt>=5){
    panelAt=now;
    uint16_t raw=0;
    if(!i2cReadInputs(raw)){
      if(!inputFaultLatched)appLogf("INPUT_I2C_ERROR addr=0x22 read returned=%u available=%u",i2cDiagnostics.lastReadReturned,i2cDiagnostics.lastReadAvailable);
      inputFaultLatched=true;inputFilter.invalidate();
      clearHold.cancel();saveHold.cancel();loadHold.cancel();panelRun=false;
      runGate.inputFault();vm.stop();audioMuteNow();piezoOff();physicalPiezo=false;
    }else{
      inputFilter.update(now,raw,digitalRead(PIN_RUN),digitalRead(PIN_VIDEO),digitalRead(PIN_LIVE),digitalRead(PIN_AUTO),digitalRead(PIN_EDIT));
      if(!inputFilter.ready){clearHold.cancel();saveHold.cancel();loadHold.cancel();panelRun=false;}
      else{
        if(inputFaultLatched)appLogf("INPUT_I2C_RECOVERED after settle");
        inputFaultLatched=false;switches=inputFilter.buttons;
        bool run=inputFilter.activeLow(16);bool isVideo=inputFilter.activeLow(17);bool edit=inputFilter.activeLow(20);
        panelActions(now,run);
        if(editSeen&&previousEdit&&!edit&&vm.running)runStop(false);
        previousEdit=edit;editSeen=true;
        if(isVideo!=(vm.mode==VM::VIDEO)){editPreview.reset();editPreview.observed=(uint8_t)(~inputFilter.buttons);previewPaddles=editPreview.observed;runStop(false);runGate.modeChanged(!run);modeReset(isVideo);}
        if(!run&&vm.running)runStop(false);
        else if(runGate.sample(run)){
#if BR_AUDIO_DIAGNOSTIC
          // In this build RUN selects the fixed hardware-path test tone; VM
          // execution is intentionally unavailable, while STOP still mutes.
          diagnosticToneActive=true;appLogEvent("AUDIO_DIAGNOSTIC_RUN","fixed 440 Hz; VM held stopped");
#else
          bool backendReady=isVideo?(video&&video->isStarted()):audioDriver;
          if(!edit||storageBusy||!backendReady){runGate.reset();appLogf(!edit?"RUN_REJECTED_MEMORY":"RUN_REJECTED_BACKEND_OR_BUSY");}
          else{fetchFifo.clear();monitorAddress=monitorValue=0;vmErrorLogged=false;vm.start(isVideo?VM::VIDEO:VM::AUDIO);appLogf("RUN mode=%s",isVideo?"VIDEO":"AUDIO");}
#endif
        }
      }
    }
  }
  serviceWebLifecycle();
  if(webRunning){const uint32_t httpStarted=micros();http.handleClient();const uint32_t httpElapsed=micros()-httpStarted;if(httpElapsed>maxHttpUs)maxHttpUs=httpElapsed;}
  while(Serial2.available()){uint8_t b=(uint8_t)Serial2.read();++runtimeCounters.uartRx;if(!vm.uartReceive(b))++runtimeCounters.uartRxOverflow;}
  uint8_t txb;if(Serial2.availableForWrite()&&vm.uartTransmit(txb)){Serial2.write(txb);++runtimeCounters.uartTx;}
  if(vm.piezoOn&&(vm.piezoGeneration!=piezoGenerationSeen||vm.piezoHz!=physicalPiezoHz||!physicalPiezo)){piezoGenerationSeen=vm.piezoGeneration;physicalPiezoHz=vm.piezoHz;physicalPiezo=true;piezoSet(vm.piezoHz,vm.piezoDuration);}else if(!vm.piezoOn&&physicalPiezo){physicalPiezo=false;piezoOff();}
  if(vm.mode==VM::VIDEO&&vm.running&&(!video||!video->isStarted()))runtimeError("VIDEO_BACKEND_ERROR","PATTERN_OR_RENDER",ESP_ERR_INVALID_STATE);
  vm.tick(now,videoRam,audioRam);while(observedFetchSerial!=vm.fetchSerial){++observedFetchSerial;fetchFifo.push(vm.lastFetchPc,vm.lastFetchOp,0);fetchFifo.push((uint16_t)(vm.lastFetchPc+1),vm.lastFetchArg,1);}runtimeCounters.uartRxOverflow=vm.uartOverflowCount;runtimeCounters.uartTimeout=vm.uartTimeoutCount;runtimeCounters.sample(now,vm.executed);runtimeCounters.requestedIps=(uint16_t)lroundf(1000.0f/(vm.intervalMs?vm.intervalMs:1));if(vm.error!=VM::OK&&!vmErrorLogged){char detail[48];snprintf(detail,sizeof(detail),"VM code=%u pc=0x%02X",(unsigned)vm.error,vm.errorPc);appLogEvent("VM_ERROR","%s",detail);if(!inputFaultLatched&&!ledFaultLatched)errorPulse.trigger(now);audioMuteNow();piezoOff();physicalPiezo=false;runGate.reset();vmErrorLogged=true;}else if(vm.error==VM::OK)vmErrorLogged=false;
  if(vm.mode==VM::VIDEO&&vm.running&&(uint32_t)(now-frameAt)>=40){uint32_t elapsed=now-frameAt;if(elapsed>=80)runtimeCounters.videoDroppedFrames+=elapsed/40-1;frameAt=now;if(video&&video->isStarted()){video->waitForFrame();uint8_t **lines=video->getFrameBufferLines();if(lines)br_render_video(vm,lines,videoHistory,++videoFrame);else runtimeError("VIDEO_BACKEND_ERROR","FRAMEBUFFER_ACCESS",ESP_ERR_INVALID_STATE);}}
  monitorUpdate(now);
  if(ledWritePending){uint16_t word=pendingLedWord;ledWritePending=false;if(!inputFaultLatched)ledWrite(word);}
  if(!vm.running&&runGate.running){appLogEvent("HALT","pc=0x%02X",vm.pc);audioMuteNow();if(physicalPiezo){piezoOff();physicalPiezo=false;}runGate.stoppedByHalt();}
  const uint32_t loopElapsed=micros()-loopStartedUs;if(loopElapsed>maxLoopUs)maxLoopUs=loopElapsed;
}
