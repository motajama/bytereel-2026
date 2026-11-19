# BITBOX SERVICE 1.0 — servisní firmware

Pro konkrétní oživený BITBOX s ESP32-WROOM-32, LED expanderem 0x20 a vstupním expanderem 0x22. Verze zapojení: 19. 11. 2026. Slouží k opakovanému hledání přerušených vodičů, chybných kontaktů a nesprávných poloh ovladačů. Není to interpreter BASIC. Nezapisuje do programových bank ani do Preferences/EEPROM; výsledky testů jsou pouze v RAM.

## Nahrání

1. Rozbal ZIP. Otevři `bitbox_service/bitbox_service.ino` v Arduino IDE.
2. Správce desek: **esp32 by Espressif Systems 2.0.14**. Vyber **ESP32 Dev Module** (klasický ESP32, nikoli S2/S3/C3). CPU 240 MHz, Flash 4 MB, výchozí partition, PSRAM vypnuta. Tento balíček je záměrně vázán na core 2.x kvůli composite video knihovně; na core 3.x se nepřeloží.
3. Nainstaluj knihovny **ESP_8_BIT Color Composite Video Library 1.3.2**, **Adafruit GFX Library 1.11.9**, **Adafruit BusIO 1.16.1**. Je-li knihovna ve správci nedostupná, použij přiložené ZIPy v `libraries` přes Sketch → Include Library → Add .ZIP Library. Není třeba instalovat AnimatedGIF.
4. Pro nahrávání napájej ESP pouze přes USB; externí napájení vypni a odpoj kladný výstup měniče od ESP 5V, jako při předchozích testech.
5. Nahraj program, otevři Serial Monitor **115200 baud**. Příkazy jsou jednopísmenné, lze poslat s Enterem.

Nahrání nahradí běžící firmware. Nedělej „Erase All Flash“, pokud chceš uchovat případná uložená data jiného firmwaru. Samotný servisní program trvalá data nemaže.

## Co se děje při každém startu

Nezávisle na RUN: dvakrát bliknou všechny LED, obě řady se naplní od krajů ke středu, sekundu svítí a od krajů ke středu zhasnou. Následují tři tóny piezo a scan I2C. Potom program čte přepínače a každou sekundu vypisuje stav. Při každém resetu se tato animace opakuje, i při přechodu do testu videa.

- Spodní LED ukazují datové přepínače D7…D0.
- Horní A0…A5 ukazují HOME, RESET, WRITE, WRITE+, EXP/EXAM, NEXT; A6/A7 jsou bez stisku zhasnuté, pokud není RUN.
- V RUN a bez stisku funkční páčky běží bod v horní řadě rychlostí SPEED. Stisk páčky má přednost.
- Přepínače VIDEO/SOUND, LIVE, AUTO a EDIT se v tomto servisním firmwaru pouze vypisují; **nezapínají automaticky výstupy**.
- Žádná páčka nemaže program, neukládá data ani neresetuje ESP. RESET je testovaný vstup; fyzické tlačítko EN na ESP stále resetuje desku.

## Ovládání

| Příkaz | Význam |
|---|---|
| `h` | Nápověda |
| `r` | Úplný servisní protokol včetně dosud neověřených poloh |
| `s` | Nový scan I2C; očekává se 0x20 a 0x22 |
| `x` | Vynulování výsledků aktuální relace bez restartu |
| `l` | Povinná úvodní animace a poté test jednotlivých LED |
| `p` | Tři tóny samostatného piezo na GPIO23 |
| `a` | Zvukový test RCA na GPIO26; 250/500/1000 Hz, po 2 sekundách |
| `u` | Jednorázový UART loopback na GX12, 9600 baud |
| `v` | Vypíše protokol a restartuje do PAL video testu |
| `n` | Vypíše protokol a restartuje do běžného servisního režimu |
| `q` | Ukončí testovací zvuk a sekvenci LED; LED se vrátí k zobrazování panelu |
| `1` | Ruční potvrzení, že LED vizuálně fungují |
| `2` | Ruční potvrzení, že piezo bylo slyšet |
| `3` | Ruční potvrzení, že audio RCA bylo slyšet |
| `4` | Ruční potvrzení správného PAL obrazu (ve video režimu) |

Potvrzení 1–4 nejsou automatická měření. Zadávej je pouze po pozorování daného výstupu. `x` je vymaže. Před každým restartem si zkopíruj protokol `r`: výsledky se mezi režimy neuchovávají. Video režim je jednorázový; hardwarový reset EN nebo odpojení napájení vrátí běžný servisní režim.

## Doporučený kompletní test

### 1. Panel a mechanické kontakty

Zkontroluj POWER LED. Dej RUN do STOP. Pošli `x`. Každý datový přepínač jednotlivě přepni na 1 a zpět na 0. Sleduj správné jméno D0…D7 v terminálu a odpovídající LED. Potom zkus všechny páčky, vždy oba směry a uvolnění do středu, a všechny režimové přepínače v obou polohách. Stav musí odpovídat skutečnému popisku panelu.

`r` musí u všech 19 vstupů ukázat „OBE POLOHY ZAZNAMENANY“. Počáteční klidová poloha se také počítá. Záznam dvou poloh sám o sobě nepozná prohozené vodiče, proto je nutné sledovat názvy. Počítadlo změn zahrnuje změny po odrušení 30 ms, nikoli každý elektrický zákmit. Při přerušení I2C se vstupy označí jako neplatné, místo aby zůstala zobrazená stará „dobrá“ data.

Pro hledání mechanického problému opakuj jednotlivé pohyby a sleduj nečekané události nebo růst chyb I2C. Neprováděj zkraty ani nemanipuluj holými vývody tak, aby se dotýkaly kovového víka. Porucha může být přerušovaná; krátký úspěšný test ji zcela nevylučuje.

### 2. Potenciometr

Pomalu otoč úplně doleva a doprava. Sleduj RAW ADC, vyhlazenou hodnotu, ms a požadované IPS. Doleva přibližně **1000 ms / 1 IPS**, doprava **5 ms / 200 IPS**. Směr je ponechán podle posledního funkčního zapojení, bez softwarové inverze. Čas se mění exponenciálně. Protokol orientačně vyžaduje dosažení ADC ≤150 a ≥3945. Pokud jeden konec chybí, není to samo o sobě důkaz závady: ověř rozsah a zapojení běžce.

IPS = 1000 / nastavené ms, **nikoli naměřená rychlost vykonávání instrukcí**. Při I2C 10 kHz, výpisech a videu je skutečná frekvence obnovování LED nižší. Servisní program žádné BASIC instrukce nevykonává.

### 3. LED

Pošli `l`. Po úvodní animaci se postupně rozsvítí vždy jedna z 16 LED, poté je vždy jedna z 16 zhasnutá mezi ostatními rozsvícenými. Každá poloha trvá 300 ms. Tím lze odhalit nesvítící LED i spojené vývody. Test postupuje D0→D7, A0→A7, tedy v každé řadě zprava doleva při pohledu na panel. Po kontrole pošli `1`. Během samotné úvodní animace se vstupy nečtou.

### 4. Piezo

Pošli `p`: má zaznít trojice stoupajících tónů (900, 1500, 2300 Hz). Potvrď `2`. Hlasitost pasivního piezo závisí na jeho rezonanci a mechanickém upevnění. Samotný program neumí poznat, zda piezo skutečně vydalo zvuk.

### 5. AUDIO RCA

Připoj linkový vstup zesilovače nebo aktivních reproduktorů, hlasitost nejprve nízkou. Žádné přímé sluchátko ani pasivní reproduktor. Pošli `a`: dva sekundy 250 Hz, dvě 500 Hz, dvě 1000 Hz, potom ticho. Má být slyšet změna výšky; potvrď `3`.

Generátor používá 8kHz softwarově časovaný DAC a malou amplitudu. Je to funkční test cesty, nikoli měření zkreslení, kalibrace hlasitosti nebo profesionální audio analyzátor. Výstupní RC filtr a vazební kondenzátor musí být správně zapojené. Neověřenou součástku označenou „10K“ nelze bez identifikace považovat za plánovaných 10 nF (`103`).

### 6. GX12 UART

Odpoj případnou periferii. Na protikusu GX12 **spoj jen pin 1 s pinem 2**. Pin 3 je GND; nezapojuje se do této propojky. Pošli `u`. Program vyšle 32 bajtů včetně 00, FF, 55, AA a porovná celý přijatý rámec. Očekávej `UART PASS`. Opakuj při podezření na přerušovaný spoj. Bez propojky je timeout/FAIL očekávaný a sám neznamená poruchu.

Tento loopback ověřuje TX, RX a cestu přes kontakty 1 a 2, **ne kontinuitu GND na pinu 3**. Tu ověř multimetrem proti ESP GND při vypnutém napájení. Použij čísla na konektoru; pohled zezadu je zrcadlový. Signály jsou 3,3V UART, nikoli RS-232, a konektor neposkytuje napájení. Pro test nepřipojuj magnetofon přímo.

### 7. VIDEO RCA

Nejprve si ulož `r`. Připoj VIDEO RCA do kompozitního PAL vstupu monitoru/TV a zvol správný AV vstup. Pošli `v`. ESP restartuje, provede init LED a začne generovat PAL přes GPIO25.

Očekávej barevné pruhy, odstíny šedé, okrajový rámeček, čitelné údaje, rostoucí čas a pohybující se bílý čtvereček. Malý ořez okraje u CRT může být normální overscan. Černobílý obraz může souviset s nastavením normy nebo kompatibilitou přijímače. Potvrď `4`, ulož `r`; příkaz `n` vrátí panel/audio režim. Při zaseknutí video knihovny použij EN: další start bude běžný.

Video je úmyslně oddělené od audio DAC testu. Knihovna používá I2S/DMA/APLL a upozorňuje na konflikty druhého DAC při současném použití. V PAL režimu `a` odmítne spuštění; piezo na GPIO23 zůstává samostatné. Tento firmware nepotvrzuje současný provoz analogového audia a videa v budoucím hlavním firmwaru. Některé rané revize klasického ESP32 nejsou s knihovnou kompatibilní; neúspěch PAL testu proto nemusí znamenat chybu pájení.

## Kanonické zapojení

| Funkce | Připojení |
|---|---|
| LED expander / vstupní expander | 0x20 / 0x22, SDA21, SCL22, 10 kHz |
| Dolní LED D0…D7 | LED expander P00…P07, aktivní LOW |
| Horní LED A0…A7 | LED expander P10…P17, aktivní LOW |
| Každá LED | 3V3 → vlastní 1k → anoda; katoda do expanderu |
| Data D0…D7 | Vstupní expander P00…P07; externí 10k pull-up, sepnutí do GND |
| HOME / RESET | Vstupní P10 / P11 |
| WRITE / WRITE+ | Vstupní P12 / P13 |
| EXP/EXAM / NEXT | Vstupní P14 / P15 |
| RUN / VIDEO / LIVE / AUTO / EDIT | GPIO13 / 14 / 16 / 17 / 18, INPUT_PULLUP, sepnutí do GND |
| SPEED | Běžec GPIO33, kraje 3V3/GND |
| Piezo | GPIO23 → 1k → piezo; druhý vývod GND |
| VIDEO RCA | GPIO25 střed, GND plášť |
| AUDIO RCA | GPIO26 → 1k → uzel A; A → 10nF → GND; A → +10µF− → uzel B; B → 100k → GND; B → střed RCA; plášť GND |
| GX12 pin 1 / 2 / 3 | TX GPIO27 / RX GPIO32 / GND |

P16/P17 vstupního expanderu jsou volné a nehodnotí se. Do vstupního expanderu firmware **nezapisuje**, respektuje tak poslední ověřené zapojení s externími pull-upy. Scan posílá pouze adresu, žádná datová slova. Převod aktivní LOW a popisky funkčních páček odpovídají poslednímu potvrzenému uspořádání, nikoli starší obrácené verzi.

## Meze ověření

ACK z LED expanderu nepotvrzuje světlo LED. UART PASS bez odděleného měření neověří zemní vodič. Firmware bez dalších měřicích obvodů neměří 5V/3V3 napájení, zvuk ani analogový obraz. POWER LED, napájení měniče a zemní vedení proto vyžadují vizuální kontrolu/multimetr. Pokud se zařízení samo resetuje, uchovej úvodní výpis důvodu resetu a hledej i napájení.

## Závislosti a zdroje

- Video: https://github.com/Roger-random/ESP_8_BIT_composite (1.3.2)
- Grafika: https://github.com/adafruit/Adafruit-GFX-Library (1.11.9)
- BusIO: https://github.com/adafruit/Adafruit_BusIO (1.16.1)
- ESP32 core: https://github.com/espressif/arduino-esp32 (2.0.14)

Přiložené knihovny mají vlastní licence uvnitř archivů. Výsledek kontroly překladu je v `VALIDATION.txt`; fyzický test tvého přístroje provedeš výše uvedeným postupem.
