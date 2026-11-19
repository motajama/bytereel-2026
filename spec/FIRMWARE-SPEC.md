# byteREEL 2026 — závazná specifikace firmwaru 1.0 / ISA 2.0

Datum: 19. 11. 2026. Toto je zadání k implementaci, nikoli tvrzení, že výsledný hlavní firmware již existuje. Uživatelem je hlášena funkčnost servisního firmwaru. Souběh generátorů s Wi-Fi, perzistence a nový VM se musí teprve implementovat a ověřit.

## 1. Identita a precedence

Všechna nová uživatelská rozhraní, logy a dokumentace nesou přesný název **byteREEL 2026**. Technické názvy složek/SSID používají `bytereel` / `byteREEL-2026`. Jazyk je vlastní dvoubajtový bytecode s mnemonikami, nikoli implementace textového Microsoft BASICu. ISA 2.0 zachovává všech 45 starých opcode a přidává 14 nových; časy nového VM se liší od HTML emulátoru.

Pořadí autority: tato specifikace a HARDWARE.md + instructions.json → aktuální testovací příklady → ověřený servisní firmware pro piny/init → starý emulátor jako algoritmická reference. Historické dokumenty mají zastaralé piny GX12, registry 74HC165/595, rychlosti a některé páčky; nekopírovat je jako dnešní hardware. Staré soubory pod `reference/` ponechat označené historickým jménem kvůli dohledatelnosti. Rename neopravňuje přepsat staré uživatelské programy bez migrace.

## 2. Hardware a verze sestavení

ESP32-WROOM-32, klasický ESP32, 240 MHz, bez předpokladu PSRAM. Cíl 4 MiB flash; ověřit `ESP.getFlashChipSize()` a uvést údaj v logu. Při menší flash neformátovat ani nezapisovat sloty. Nahrávač/build musí kontrolovat skutečnou velikost před flashováním, boot kontrola sama nemůže zabránit špatnému nahrání.

Základ: Arduino ESP32 od Espressif Systems **2.0.14**, ESP_8_BIT Color Composite Video Library 1.3.2, Adafruit GFX 1.11.9, BusIO 1.16.1. Neměnit automaticky na core 3.x. Preferovaný build Arduino CLI a Arduino IDE; případný PlatformIO musí skutečně pinovat tentýž core. Program obsahuje partitions.csv a přesné instalační/build instrukce. WebServer/WiFi/LittleFS z core, bez frameworku frontendu. JSON knihovnu, bude-li potřeba, pinovat a uvést do locku; žádné plovoucí latest.

Použít `partitions.csv` tohoto balíčku, shodné se stock Huge APP core 2.0.14. Žádné OTA, aktualizace přes USB. Další rozdělení v PARTITIONS.md.

## 3. Paměť a start

- Pracovní RAM: VIDEO[256] a AUDIO[256], nezávislé banky, editorová adresa samostatná pro každou banku. 128 dvoubajtových instrukcí v každé bance.
- 8 perzistentních slotů 0–7. Každý je jeden celek obsahující **obě** banky (512 B bytecode), název a volitelný zdroj obou bank. Sloty nejsou další vykonávané VM.
- Aktivní je jediný VM: vybraná banka určuje VIDEO nebo AUDIO. Piezo a UART fungují v obou režimech.
- Po bootu pracovní banky = samé 00 kromě `[0]=01` (HALT). Sloty se automaticky nenačítají ani nepřepisují. Zobrazit EMPTY WORKSPACE. Poslední slot lze pouze zobrazit, nikdy podle něj automaticky spouštět.
- Nutná sekvence init LED na každém bootu: 2× všechny on/off po 200 ms, obě řady naplnit od krajů ke středu po 150 ms, 1000 ms držet, po 150 ms mazat od krajů ke středu, zhasnout; úvodních 150 ms tma. Nezávisle na RUN. Poté krátký identifikační dvojtón piezo. Identifikační tón skončí před případným spuštěním VM.
- SoftAP spustit v setup bez úmyslné časové prodlevy (před animací); HTTP obsluha musí být dostupná nejpozději po init. Síť zůstává aktivní i v RUN/VIDEO a při chybě VM.
- Má-li fyzický RUN při startu polohu RUN, **nespouštět**. Vyžadovat STOP→RUN. Stejné pravidlo po HALT, chybě, RESET a změně VIDEO/AUDIO za RUN.
- Žádné automatické uložení pracovních bank do flash po každém WRITE nebo zastavení. Dirty příznaky samostatně pro obě banky.

## 4. Panel — přesná pravidla

Všechny vstupy aktivní LOW, debounce po jednotlivých vstupech 30 ms. Momentové příkazy jednou za stisk, bez autorepeat. Pro dlouhé stisky akci spustit jen jednou, bez souběžné krátké akce při uvolnění. Současné oba směry stejné páčky => potlačit oba příkazy a hlásit neplatný stav, nikdy SAVE/CLR.

| Stav / prvek | Chování |
|---|---|
| RUN náběžná hrana ze STOP | reset VM (PC=0,A=0,C=0,Z=1), reset aktivního AV, start. Podmínka: EDIT, validní vstupy a připravený backend. MEMORY RUN odmítnout, nejprve STOP a EDIT. |
| STOP | ihned zastavit VM i čekání, umlčet audio/piezo, zrušit čekající TX; již odeslané bajty nelze vzít zpět. VIDEO drží poslední obraz. |
| VIDEO/AUDIO změna | zastavit, reset aktivního VM, přepnout backend; zachovat obě pracovní banky i adresy; nový běh až STOP→RUN. Žádný skrytý reset MCU. |
| AUTO | VM provádí kroky podle SPEED. |
| STEP | při RUN provede NEXT jednu instrukci. Přepnutí AUTO→STEP nedělá krok; STEP→AUTO pokračuje z PC. |
| EDIT + EXAM | editorová adresa = D7..D0; za RUN nemění PC. |
| EDIT + WRITE | atomicky zapíše jeden bajt D7..D0 na editorovou adresu aktuální banky, dirty=true. Povoleno i RUN pro fyzické live coding. |
| EDIT + WRITE+ | totéž, potom adresa=(adresa+1) mod 256. |
| EDIT + NEXT ve STOP | editorová adresa++ mod 256. |
| EDIT + NEXT v RUN STEP | právě jeden VM krok, viz časování. |
| EDIT + NEXT v RUN AUTO | bez akce. |
| HOME | editorová adresa aktivní banky=00; PC se nemění. |
| RESET krátce | zastaví a resetuje VM a generátory, vymaže čekání/FIFO UART, ponechá pracovní banky a sloty; znovu vyžaduje STOP→RUN. |
| RESET držený >=3 s, fyzický STOP | CLR právě vybrané banky: 256 nul, dirty=true, editor=00, VM reset; sloty a druhá banka zůstávají. |
| RESET držený za RUN | pouze krátká reset funkce při stisku; CLR se nikdy neaktivuje. |
| Přechod EDIT→MEMORY | pokud běží, zastaví VM; neprovádí save/load. V režimu MEMORY se pracuje pouze s fyzickým STOP. |
| MEMORY | D2..D0 vybírají 0..7, D7..D3 ignorovat. WRITE/WRITE+ neaktivní. |
| MEMORY + EXAM/SAVE držený 2 s | uloží snapshot obou pracovních bank do vybraného slotu; nahrazení starého slotu je záměrné gesto. Slot/banky zachytit na začátku držení, změna volby/STOP/modu během držení akci ruší. |
| MEMORY + NEXT/LOAD držený 2 s | načte obě banky z vybraného platného slotu; reset VM/editorů, dirty=false. Přepíše neuloženou pracovní paměť. Stejné rušení držení jako SAVE. |

Délka LOAD 2 s je nové upřesnění, aby nedošlo k nechtěnému přepsání pracovního programu. CLR naplní nuly (na rozdíl od boot výchozího HALT); RUN nad prázdnou bankou nakonec skončí chybou PC, nikdy nepřeteče do nuly. Chyba a prázdný slot nechávají obě pracovní banky nedotčené. LOAD validuje celý pár před jedním atomickým přepnutím.

### LED monitor

STOP+EDIT: horní řada editorová adresa, dolní skutečný bajt RAM, ne nastavení přepínačů. RUN+LIVE AUTO: bounded FIFO fetch událostí [adresa,bajt] pro opcode i argument, 100 ms/událost; při zaplnění zahazovat nejstarší a čítat dropped. RUN+STEP LIVE: přímo adresa a opcode právě provedené instrukce. RUN+SCAN: jedna adresa za 100 ms cyklicky 00..FF bez vlivu na VM. FIFO 48 událostí, ne neomezená historie.

STOP+MEMORY: horní řada číslo slotu 0..7; dolní bit0 slot platný, bit1 VIDEO dirty, bit2 AUDIO dirty, bit7 probíhá úložiště. Po save/load krátké úspěšné pípnutí; při chybě dvojité nižší. Tyto servisní zvuky jen STOP, nikdy nepřekrývat programové PPLAY za RUN. Chyby jsou také v monitoru. Nezavádět nevyžádané pípání každé instrukce.

## 5. VM a nové kanonické časování

Formát vždy [opcode,arg], včetně HALT/NOP. PC musí být **alespoň 16bit**, validní fetch pouze sudé 00..FE. Po fetchi PC+=2, tedy FE→100; bez skoku/ukončení další fetch vyvolá chybu, nikoli wrap. Skoky validují sudý cíl **i u neprovedené podmíněné větve**, tj. `JZ 03` je chyba nezávisle na Z. Paměť může obsahovat DB data, proto validace při uložení nesmí zakázat každou neznámou dvojici v datové oblasti; při fetchi neznámý opcode vždy chyba.

Fetch načte oba bajty ze stejného krátce zamčeného snapshotu. WRITE běžícího programu mění až další fetch; dvě samostatná fyzická WRITE mohou záměrně vytvořit přechodný program. Web do pracovní RAM nezapisuje.

SPEED: ADC33 (0..4095) roste doprava. Filtr `filtered += .15*(raw-filtered)` vzorkovat pravidelně po 10 ms, počáteční filtered=raw. Interval `round(1000 * pow(0.005, filtered/4095))` ms, tedy **1000..5 ms = 1..200 požadovaných IPS**. Tato stupnice nahrazuje starý násobič 0.25×..4× i nerealizovaný návrh 1000 IPS. Do monitoru dávat požadované IPS i skutečné průměrné dokončené instrukce/s za 1 s. WAIT a UART mohou reálné IPS snížit; nejde o takt CPU.

AUTO: po dokončení každé instrukce naplánovat další nejdříve `doneTime + intervalSnapshot + explicitWait`. IntervalSnapshot se odebere při dokončení této instrukce. `WAIT p` přidá p×10 ms, `AWAIT p` p×20 ms, `VWAIT p` p×40 ms. Žádné násobení/dělení těchto WAIT časů potenciometrem. Žádné catch-up dávky zmeškaných instrukcí. Na RUN první fetch ihned, poté toto tempo. Instrukce čekající na UART se dokončí až při úspěchu/timeoutu; potom následuje další interval. PPLAY je neblokující, jeho délka běží v reálném čase nezávisle na SPEED.

STEP: každý nový NEXT přeskočí zbývající VWAIT/AWAIT/WAIT a provede jednu instrukci; nesmí ignorovat RX čekající na data. Je-li UART instrukce pending, NEXT se zahodí a zaloguje BUSY (nefrontuje se). Při příjmu se dokončí tatáž instrukce, další až dalším NEXT. STOP přeruší i UTIME 00. Přepnutí do AUTO při pending nemění její deadline.

Původní watchdog „64 instrukcí bez WAIT“ **odstranit**: plánovaný VM i v `JMP 00` pravidelně uvolňuje CPU. Místo něj bounded work za tick, systémový watchdog a detekce chyb adres/kódů. ARM/STOP latch nepřepisovat automaticky podle trvale zapnuté RUN páčky po HALT.

A,C unsigned8; Z bool. COUNT/LOOP nemění Z. ALU modulo256. RNG xorshift32 (<<13,^,>>17,^,<<5, unsigned32) začíná 0x12345678 při RUN. RAND p = uint64(rng)*(p+1)>>32; nesmí přetéct pro p=FF. SEED p: VM RNG=`0x5f356495 XOR uint32((p+1)*0x27d4eb2d)`, video RNG=`0x9e3779b9 XOR uint32((p+1)*0x045d9f3b)`. Audio šum má vlastní RNG, nemění posloupnost RAND.

## 6. Audio, video a piezo

AV je výhradně **VIDEO nebo AUDIO**. Nevyžaduje se současné RCA video a RCA audio. Piezo a UART + Wi-Fi jsou použitelné v obou režimech. Instrukce nesprávné banky vyvolá VM error. Přechod mezi backendy má jediného vlastníka periferií; neaktivní DAC se vypne nebo drží v klidové úrovni podle bezpečné sekvence driveru.

### VIDEO

Generátor 256×192 v monochromatickém logickém rastru, prezentovaný uprostřed PAL frame bufferu knihovny (256×240, černé okraje nahoře/dole 24 px). Paleta převádí šedou na RGB332 aproximaci; nelze slibovat přesně 256 analogových úrovní jasu. Aktualizace generativního obrazu 25 Hz, fyzické PAL časování řídí DMA knihovny. Při nestíhání přeskočit render, zachovat synchronizaci a zvednout droppedFrames. Vzory a parametry podle legacy drawVideoFrame, ale fáze pohybu přibude o 1 za každý 40ms krok, nezávisle na potenciometru. Historie FEEDBACK 256×192 8bit (49 152 B), inicializovaná černě. PATTERN 07 zůstává generovaná textura, ne čtení programové RAM. Nezavádět nezdokumentovanou změnu tohoto vzoru.

Dvojitý PAL framebuffer knihovny má cca 122 880 B + režie. Generátor pracuje rovnou do backbufferu a historie, žádné další RGB/RGBA framebuffer kopie. Připrav testovací šedou rampu a všechny vzory. STOP podrží obraz, ale synchronizace pokračuje. RESET obraz vyčistí. Web/RAM není framebuffer.

Při přepnutí na AUDIO **nejdříve zastavit DMA a zakázat IRQ, až potom uvolňovat buffery a APLL/I2S/DAC**. Historický destructor knihovny uvolňuje buffery před IRQ teardown a obsahuje TODO k interrupt free; auditovat/patchovat nebo použít vlastní bezpečný backend. Nelze bez kontroly donekonečna new/delete původní wrapper. Opakovat 100 přepnutí v akceptačním testu bez úniku RAM. Tuto část neřešit restartem celého ESP: RAM programy by zmizely.

### AUDIO

Monofonní 8bit DAC GPIO26, sample rate 22050 Hz, bufferované DMA/I2S nebo ekvivalentně prokázaně stabilní hardware. Servisní esp_timer + dacWrite není cílový syntezátor. Při AUDIO je video I2S driver vypnut. FREQ/FREQ_A 200+10p Hz; ostatní parametry katalog. Oscilátor fáze 0..1, PULSE clamp, obálka linear, šum vlastní PRNG. FILTER definovat jako jednopolový lowpass `y += alpha*(x-y)`, `alpha=1-exp(-2*pi*fc/Fs)`. LFO sinus, výsledná f clamp 0..0.45Fs; střída pevně omezena. Výstup DAC klid128, maximum bipolar ±96 před finálním clamp0..255, nikoli plný rozkmit bez rezervy. Úroveň VOLUME p/255; smíchání noise a wave: wave*(1-n)+noise*n.

Výchozí stav RUN/RESET generátoru: freq200, wave0, volume0, attack0, decay0, gate1, pulse0.5, crush8, noise0, lfoRate0, lfoDepth0, filter6000, phase0,lfoPhase0,env1. GATE01 po zavření restartuje env0; GATE00 použije DECAY. HALT/STOP/chyba/bank change ztiší nejpozději 50 ms, bez čekání na dlouhé DECAY. DMA underrun počítat a logovat agregovaně. Za RUN žádné dynamické alokace vzorkových bufferů ani blokující logy.

### PIEZO

GPIO23 přes1k, samostatné LEDC PWM 50 %, rezervovaný kanál0, pin nepatří RCA. PFREQ=200+10p Hz, výchozí1000 Hz; PDUR=100 ms, 00 nepřetržitý; PPLAY obnoví časovač, PSTOP zhasne. PFREQ_A se řídí A. HALT/STOP/chyba/změna banky/fyzický RESET piezo vypne; opcode RESET02 mění jen AV. Délka řízená deadline bez delay; skončí i během RX čekání. Ovládání hlasitosti piezo se v této verzi nepřidává.

## 7. UART GX12

Serial2, RX32, TX27, **9600 8N1, 3.3 V TTL**, bez hardwarového flow control. GX12 pin1 TX, pin2 RX, pin3 GND. USB Serial 115200 je monitor, nesmí se směšovat se Serial2. Wi-Fi monitor není bridge UART a nemá pole pro vysílání.

RX a TX fronta po256 B, bounded; RX overflow zahodí nově příchozí bajt a nastaví sticky status. Driver/hardware chyby také evidovat, ne vydávat chybový čtecí kód za platné 0xFF. Software/driver čítače popsat; nepřijímat data do několika neomezených front. UART task běží i během WAIT a STEP. RX/TX dle katalogu: **Z značí úspěch přenosu, ne nulový obsah A**. Úspěšný příjem bajtu00 => A=00, Z=0. Timeout A zachová a Z=1, nastaví sticky bit. TRYRX při prázdné frontě nastaví Z=1, ale ne sticky timeout. TX success potvrzuje enqueue, ne přijetí protistranou. UART fyzické errors mohou nastat i po enqueue.

Výchozí UTIME=1000ms; 00 znamená bez timeoutu, STOP stále okamžitě funkční. Příchozí data ve STOP lze bufferovat; RUN a fyzický RESET začínají s prázdnými RX/TX frontami, čistými sticky flags a default UTIME. HALT a STOP ruší pending instrukci i neodeslanou software TX frontu; dokončení hardwarově vysílaného bajtu připustit. Nevysílat později starou TX frontu při novém RUN. Přenosové status bity jsou v instructions.json.

Tato verze přenáší surové bajty do/z A, například data děrného štítku, řízení parametrů, echo. **Nepředstírat existující protokol nahrávání celých programů ani přímý magnetofonový modem**; to bude protokol externí periferie. Pro test použít GX12 loopback1–2 nebo externí3.3V UART. Detekci připojené periferie bez handshake nevyhlašovat.

## 8. Úložiště a web

Detailní smlouva v STORAGE-WEB.md. Pracovní RAM a perzistentní slot jsou oddělené objekty. Uložení webového editoru nemění běžící/načtený program. Tlačítko je „ULOŽIT SLOT“, nikoli RUN ani APPLY. Panel SAVE/LOAD oba programy v jednom slotu. Žádné LOAD/RUN/STOP/STEP/RESET/GPIO/uart-send endpointy.

Otevřený AP při každém startu: `byteREEL-2026-XXXX` (poslední4 hex MAC), bez hesla, IP192.168.4.1/24, max2 klienti, žádné STA/cloud. HTTP dostupné přímo na http://192.168.4.1. Captive portal není podmínka; do projektu nepřidávat internetové závislosti. Web pouze Sloty / Monitor / Instrukce, jednoduchý monospace code-style, čeština, bez CDN/font downloadů/frameworku. Otevřená síť znamená, že sloty může upravit každý připojený člověk; to je zvolený provozní režim, nepřidávat nevyžádané login.

## 9. Souběh, čas a chyby

Panel/I2C má jednoho vlastníka, I2C10kHz, timeout25ms. Žádný zápis FF FF do vstupního expanderu. Výpadek vstupů během RUN zastaví VM a umlčí zvuk; po obnově vyžaduje STOP→RUN. Chyba LED log/indikace, nezaměňovat ACK s vizuálním testem. Periodicky re-probe adres; ne scan celého bus v realtime.

Smyčka bez delay mimo neškodnou init sekvenci. VM, UART, panel mají bounded kroky. Web obsluha nesmí držet VM mutex během socket write nebo filesystem operace; překopírovat malé snapshoty. Long-running HTTP request nemá blokovat STOP. Doporučené core/task rozdělení zdokumentovat po profilování; nepředpokládat, že samotné pinning garantuje determinismus. IRQ bez printf, alokací a FS.

**Zápis flash a PAL jsou integrační riziko:** flash cache se při zápisu může vypínat; auditovat IRAM/DMA a skutečný driver. Web i panel SAVE commit povolit jen při fyzickém STOP. Je-li nutné, během uloženého commit bezpečně pozastavit PAL, zalogovat `VIDEO_PAUSED_FOR_SAVE` a obnovit poslední obraz. Neformátovat/nezapisovat z ISR. Uložení musí mít test výpadku proudu. Zákaz flash zápisu za RUN neznamená zákaz editace textu v prohlížeči nebo čtení uloženého slotu.

Status: projekt, verzefirmware/ISA, bootId, uptime, reset reason, flashsize, mode, PC,A,C,Z, editorAddr, dirty obou bank, requested IPS/measured IPS, wait/pending, heap/minHeap/largestDMAblock, I2Cerrors, underruns,droppedFrames, UARTcounts/errors, storagehealth. Logy do USB a omezeného8KiB ringbufferu pro web; ne celé serial debug/wifi SDK garbage. WiFi Serialmonitor znamená aplikační log, ne zpětný odposlech ROM boot logu. Startup records přidat do ringbufferu před připojením klienta.

## 10. Hotovo znamená

Žádné TODO/stub u veřejné instrukce nebo webové funkce. Bez hardwaru ověřit build, čisté VM testy, assembler, API a fault-injection úložiště; hardware testy výslovně NOT TESTED, ne vymyšlené PASS. Dodání `.ino`/zdrojů, build skript, partition, dependency lock, web assets generované do PROGMEM, instrukční JSON, ukázky, český manuál, migrační poznámky a výsledky testů. Neprovádět nahrání na uživatelovo zařízení bez přístupu/jeho akce. Nevyžadovat předem novou elektroniku: současný panel je autorita.
