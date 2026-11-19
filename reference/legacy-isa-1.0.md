# BITBOX BASIC 1.0

**Specifikace interpretru, instrukčního jazyka a webového emulátoru**  
Verze návrhu: 1.0 (11. listopadu 2026)  
Určení: jednoduchý audiovizuální počítač s ESP32, čelním panelem inspirovaným Altairem 8800, 16 LED a páčkovými přepínači.

> **Stav:** Tento dokument přesně popisuje logiku přiloženého funkčního HTML emulátoru (`bitbox-emulator.html`). Jde o **vlastní osmibitový virtuální stroj**, ne o historický Microsoft Altair BASIC ani emulaci procesoru Intel 8080. Skutečný firmware pro ESP32, kompozitní PAL a fyzický panel zatím nejsou implementovány. Obraz v HTML vzniká na plátně Canvas; zvuk pomocí Web Audio API.

## 1. Myšlenka

BITBOX vytváří obraz nebo zvuk na základě uživatelského programu, zadaného pomocí osmibitových přepínačů nebo textovým assemblerem v emulátoru. Jeden program obsahuje posloupnost **dvoubajtových instrukcí**. Každá říká, *co vykonat*, a případně *s jakou hodnotou*.

| Vlastnost | Specifikace |
|---|---|
| Paměť | Dvě nezávislé banky, `VIDEO` a `AUDIO`, každá **256 bajtů** (`00`–`FF`) |
| Formát instrukce | **2 bajty**: `[opcode] [argument]` |
| Maximální program | **128 instrukcí** na jednu banku |
| Adresy instrukcí | Pouze **sudé**: `00`, `02`, … `FE` |
| Číselná soustava | **Hexadecimální**, `00`–`FF` (0–255) |
| Pracovní registr | Osmibitový **A** (0–255) |
| Počítadlo smyčky | Osmibitový **C** (0–255) |
| Programový čítač | **PC** (adresa dvoubajtové instrukce) |
| Stavový příznak | **Z** (1, pokud výsledek porovnání nebo operace je nula) |
| Výstup VIDEO | 256 × 192 bodů, černobílá simulace 25 snímků/s |
| Výstup AUDIO | Jednohlasý monofonní syntezátor, Web Audio API |
| Monitor | 8 LED adresy + 8 LED hodnoty v paměti |
| Časování | `VWAIT` po 40 ms, `AWAIT` po 20 ms, ovladač SPEED 0,25×–4× |

**Příklad:** dvojice `50 02` = `PATTERN 02` = vybrat generátor vodorovných pruhů. Následující instrukce bude na adrese `02`.

## 2. Vykonávací model interpretru

### Cyklus *fetch – decode – execute*

1. Na adrese `PC` se přečte bajt instrukce a bajt argumentu na `PC + 1`.
2. Do fronty monitoru `LIVE` se vloží skutečně načtené adresy a hodnoty obou bajtů.
3. Interpret nastaví `PC = PC + 2`, vyhodnotí instrukci a případně nastaví registry, parametry generátoru nebo novou adresu skoku.
4. Pokud instrukce čeká (`VWAIT`, `AWAIT`), interpret přestane dočasně načítat další instrukce. Obrazový/zvukový generátor mezitím **pokračuje** podle posledních nastavených hodnot.
5. Není-li čekání, vykoná další instrukci. Ochranný limit je 64 instrukcí za sebou bez kladné časové prodlevy; potom emulátor program zastaví a vypíše chybu.

Po `RUN` se nastaví `PC=00`, `A=00`, `C=00`, `Z=1`, resetuje se generátor a spustí se vybraná banka. Během jednoho běhu je aktivní vždy jen jedna banka. Přepnutí `VIDEO/AUDIO` zastaví předchozí program a případně ihned spustí program v nové bance od adresy `00`.

`JMP`, `JZ`, `JNZ`, `LOOP` a `VJMP` přijímají **absolutní adresu** (`00`–`FE`), která musí být sudá. Chybná adresa, neznámá instrukce, příkaz nesprávného režimu nebo neplatný parametr některých instrukcí způsobí zastavení a hlášení chyby.

### Časování

- `VWAIT 10` = `0x10 = 16` video snímků × 40 ms = **640 ms** při `SPEED=1×`.
- `AWAIT 08` = `0x08 = 8` kroků × 20 ms = **160 ms** při `SPEED=1×`.
- `VWAIT 00` a `AWAIT 00` **nečekají**; nekonečný rychlý cyklus zastaví watchdog.
- Potenciometr `SPEED` mění časy čekání dělením rychlostí (0,25× až 4×) a rychlost video pohybu. **Nemění samotnou frekvenci zvukového oscilátoru.**
- Video se překresluje přibližně **25× za sekundu** nezávisle na tom, zda interpret právě čeká. Při `STOP` zůstane poslední snímek zobrazen.
- Jako každý HTML emulátor závisí přesné časování na plánování prohlížeče; běh skrytého panelu může být omezen prohlížečem.

## 3. Adresový panel a LED

Fyzický návrh počítá s osmi datovými přepínači, šestnácti LED a sedmi ovládacími přepínači:

- **D7…D0 (8 kusů):** nastaví osmibitovou hodnotu od `00` do `FF`.
- **EXAM:** nastaví adresu editoru podle aktuální hodnoty D7…D0; LED v režimu STOP ukáží obsah této adresy.
- **WRITE:** uloží hodnotu D7…D0 na vybranou adresu editoru; funguje i za RUN, změna se projeví při dalším načtení instrukce.
- **NEXT:** ve STOP posune adresu editoru o jedna; v RUN + STEP vykoná **jednu dvoubajtovou instrukci**.
- **RUN/STOP:** spouští program od `00`, případně jej zastaví.
- **VIDEO/AUDIO:** přepíná programovou banku a aktuální výstup.
- **LIVE/SCAN:** mění funkci LED monitoru během běhu.
- **AUTO/STEP:** automatický běh nebo ruční vykonávání po instrukcích.

Celkem **15 páčkových přepínačů**: 8 datových + 7 ovládacích. V HTML mají ovládací přepínače podobu tlačítek; jsou virtuální náhradou plánovaného hardwaru.

### LED monitor

| Stav | A7–A0 (8 LED) | D7–D0 (8 LED) |
|---|---|---|
| STOP | Vybraná adresa editoru | Obsah paměti na této adrese |
| RUN + LIVE | Adresa skutečně načteného bajtu | Hodnota tohoto bajtu |
| RUN + SCAN | Postupně procházená adresa od `00` po `FF` | Obsah této adresy |
| RUN + STEP + LIVE | Adresa právě vykonané instrukce | Její operační kód |

V `LIVE` se oba načtené bajty (`opcode` a `argument`) ukládají do fronty. LED zobrazují **jednu zaznamenanou událost za 100 ms**. Při velmi rychlém programu mohou starší události z fronty vypadnout; interpret se **nezpomaluje** jen kvůli LED. `SCAN` zobrazuje jeden bajt paměti za 100 ms (všech 256 bajtů asi za **25,6 s**) a nijak neovlivňuje vykonávání programu.

V HTML je navíc paměťový inspektor v mřížce 16 × 16. Kliknutí na buňku vybere adresu pro `WRITE`.

## 4. Instrukční sada: základ

Každá instrukce má **přesně dva bajty**, a to i tehdy, pokud svůj druhý bajt nevyužívá (v tom případě používej `00`). Hodnoty níže jsou v HEX.

| Kód | Mnemonika | Význam | Argument |
|---|---|---|---|
| `00` | `NOP` | Bez operace | ignorován |
| `01` | `HALT` | Konec programu, zastaví výstup audio | ignorován |
| `02` | `RESET` | Reset parametrů aktuálního video/audio generátoru, nikoli registrů A, C, PC | ignorován |
| `30` | `JMP` | Nepodmíněný skok | sudá adresa |
| `31` | `JZ` | Skok, pokud `Z=1` | sudá adresa |
| `32` | `JNZ` | Skok, pokud `Z=0` | sudá adresa |
| `33` | `COUNT` | Nastaví počítadlo smyčky `C` | `01`–`FF` |
| `34` | `LOOP` | Sníží `C` o 1 a skočí, pokud `C != 0` | sudá adresa |

`COUNT 04` a smyčka končící `LOOP xx` vykonají tělo **celkem čtyřikrát**, pokud uvnitř nesměrují jinam skoky. Existuje jen jedno počítadlo `C`; **vnořené smyčky COUNT/LOOP** bez jiné pomocné logiky nejsou podporované.

### Aritmetika a logika

| Kód | Mnemonika | Efekt |
|---|---|---|
| `40` | `SET xx` | `A = xx` |
| `41` | `ADD xx` | `A = (A + xx) mod 256` |
| `42` | `SUB xx` | `A = (A - xx) mod 256` |
| `43` | `XOR xx` | `A = A XOR xx` |
| `44` | `RAND xx` | Náhodné celé `A` v rozsahu **0 až xx včetně** |
| `45` | `CMP xx` | Změní pouze `Z` podle `A == xx` |
| `46` | `AND xx` | `A = A AND xx` |
| `47` | `OR xx` | `A = A OR xx` |

Operace kromě `CMP` nastaví `Z=1`, pokud je jejich výsledkem `A=0`, jinak `Z=0`. Při `CMP` se registr `A` nemění. `RAND` využívá deterministický pseudonáhodný generátor, jehož stav lze inicializovat pomocí `SEED` ve VIDEO režimu; při každém novém RUN se výchozí stav obnoví.

Příklad:

```asm
SET 0A   ; A = 10 decimal
ADD 05   ; A = 15 decimal
CMP 0F   ; Z = 1
JZ 00    ; opakuj
```

## 5. Instrukční sada: VIDEO

Video emulátor vykresluje monochromatický rastr **256 × 192 pixelů** na HTML Canvas. Tato animace je vizuální aproximace zamýšleného výstupu. **Nevysílá elektrický PAL signál** a nepředstírá přesnou časovou simulaci DAC.

| Kód | Mnemonika | Efekt a mapování argumentu |
|---|---|---|
| `50` | `PATTERN xx` | Vyber algoritmus (`00`–`08`) |
| `51` | `MOTION xx` | Podepsané 8bitové číslo dělené 16 = px/snímek při 1×; `04` = +0,25, `FC` = −0,25 |
| `52` | `DENSITY xx` | Základní hustota; u pruhů přibližně `xx × 4` period/rastr, nejméně 1 |
| `53` | `LEVEL xx` | Jas 0–255 (`00` černá, `FF` plný jas) |
| `54` | `SCALE xx` | Faktor zmenšení/roztažení struktury `1 + floor(xx/32)` (1–8) |
| `55` | `GLITCH xy` | Vyšší nibble `x` = síla 0–15; nižší `y` = pravděpodobnost zásahu za snímek `y/16` |
| `56` | `INVERT xx` | `00` normálně, nenulový parametr = invertovat |
| `57` | `SHIFT xx` | Horizontální posun podle znaménkového bajtu −128 až +127 px |
| `58` | `FEEDBACK xx` | Podíl předchozího snímku `xx/255` (např. `80` přibližně 50 %) |
| `59` | `SEED xx` | Nový deterministický stav video šumu a náhodného registru |
| `5E` | `DENS_A 00` | Nastav hustotu podle aktuálního `A`; argument se ignoruje |
| `5F` | `GLITCH_A 00` | Nastav glitch podle aktuálního `A`; argument se ignoruje |
| `60` | `VWAIT xx` | Čekej `xx × 40 ms / SPEED` |
| `70` | `VJMP xx` | Videové synonymum `JMP xx`; cíl musí být sudý |

### Režimy `PATTERN`

| Parametr | Obrazový generátor |
|---|---|
| `00` | Černá obrazovka |
| `01` | Binární náhodný šum |
| `02` | Horizontální pruhy |
| `03` | Vertikální pruhy |
| `04` | Šachovnice |
| `05` | Bloky s pseudonáhodnou strukturou |
| `06` | Bitové XOR souřadnic |
| `07` | Generovaná bitová mapa (v 1.0 zatím **nečte programovou RAM**) |
| `08` | Diagonální pruhy |

`DENSITY` a `SCALE` ovlivňují jednotlivé režimy různě; na jejich přesné obrazové podobě je založen vizuální algoritmus emulátoru, nikoli norma televizního signálu. `FEEDBACK FF` použije pouze předchozí obraz — při startu z černé tím může obraz zůstat černý.

#### Program: GLITCH MACHINE

```asm
ORG 00
PATTERN 02   ; vodorovné pruhy
DENSITY 06   ; hustota 24
MOTION 04    ; posun +0,25 px/snímek
GLITCH 9A    ; silné deformace s pravděpodobností 10/16
VWAIT 10     ; 16 PAL snímků
VJMP 00      ; zpět na začátek
```

Tytéž bajty v HEX (adresy vlevo):

```text
00: 50 02
02: 52 06
04: 51 04
06: 55 9A
08: 60 10
0A: 70 00
```

#### Program: generativní pruhy

```asm
PATTERN 02
SMYCKA:
RAND 1F
DENS_A 00
VWAIT 04
VJMP SMYCKA
```

Program má 10 bajtů. Každých 160 ms vybírá náhodnou hustotu od 0 do 31; jednoduchou změnou `RAND 1F` na `RAND 07` jej učiníš méně proměnlivým.

## 6. Instrukční sada: AUDIO

Emulátor používá jeden oscilátor, obálku náběhu a odeznění, příměs šumu, modulaci LFO, bitcrush a dolní propust. Výstup je **monofonní**; používá nativní zvukovou kartu přes **Web Audio API**.

Frekvenční mapa je záměrně jednoduchá a lineární:

\[ f(p) = 200 + 10p\ \text{Hz},\quad p\in\{0,\ldots,255\}. \]

Například `FREQ 40` nastaví `0x40 = 64`, tedy **840 Hz**. `FREQ 80` = **1480 Hz**.

| Kód | Mnemonika | Efekt a mapování argumentu |
|---|---|---|
| `10` | `FREQ xx` | Frekvence `200 + 10 × xx` Hz |
| `11` | `WAVE xx` | Průběh `00` sin, `01` pila, `02` obdélník, `03` trojúhelník, `04` bílý šum, `05` pulz |
| `12` | `VOLUME xx` | Hlasitost `xx / 255`, před hlavní bezpečnostní úrovní zesílení |
| `13` | `ATTACK xx` | Lineární náběh `xx × 4 ms` |
| `14` | `DECAY xx` | Lineární doba vypínání `xx × 4 ms` |
| `15` | `GATE xx` | `00` zavřít tón, `01` otevřít tón (jiná hodnota je chyba) |
| `16` | `PULSE xx` | Střída pulzu `xx/255`, omezená na 1–99 % |
| `17` | `CRUSH xx` | Kvantizace 1–8 bitů; `00` = bez redukce (8 bitů) |
| `18` | `NOISE xx` | Poměr přimíchaného bílého šumu 0–100 % |
| `19` | `LFO_RATE xx` | `00` = bez LFO; jinak `0,1 + 0,05×(xx−1)` Hz (nejvýše 12,8 Hz) |
| `1A` | `LFO_DEPTH xx` | Frekvenční modulace přibližně ±`xx/255` výchozí frekvence |
| `1B` | `FILTER xx` | Frekvence dolní propusti `100 + (xx/255)×5900` Hz |
| `1E` | `FREQ_A 00` | Frekvence podle registru `A`: `200 + 10×A` Hz |
| `1F` | `VOL_A 00` | Hlasitost podle registru `A`: `A / 255` |
| `20` | `AWAIT xx` | Čekej `xx × 20 ms / SPEED` |

Po `RUN` je `GATE` implicitně otevřený, ale hlasitost je **nula**, dokud ji program nenastaví. To dovoluje krátké programy bez explicitního `GATE 01`. `GATE 00` použije aktuální `DECAY`; `GATE 01` po zavřeném tónu znovu spustí obálku `ATTACK`. `HALT`, `STOP` a odchod z AUDIO režimu výstup ztiší.

**Pozor na audio:** Kvůli bezpečnostním pravidlům prohlížečů musí uživatel samostatně kliknout na **ZAPNOUT ZVUK**, i když audio program již běží. Začni s nízkou systémovou hlasitostí. Implementace využívá starší `ScriptProcessorNode`, protože funguje jako **samostatný HTML soubor bez serveru**. V budoucím vydání je možné ho nahradit modernějším AudioWorklet.

#### Program: generativní tóny

```asm
ORG 00
WAVE 01      ; pila
VOLUME 80    ; přibližně 50 %
SMYCKA:
RAND 90      ; A = 0..144
ADD 20       ; A = A + 32
FREQ_A 00    ; 520..1960 Hz
AWAIT 08     ; 160 ms
JMP SMYCKA
```

#### Program: čtyři údery a pauza

```asm
ORG 00
WAVE 02      ; obdélník
FREQ 40      ; 840 Hz
VOLUME A0
OPAKUJ:
COUNT 04
PULZ:
GATE 01
AWAIT 04     ; 80 ms
GATE 00
AWAIT 04     ; 80 ms
LOOP PULZ   ; čtyři pulzy
AWAIT 20     ; pauza 640 ms
JMP OPAKUJ
```

## 7. Programování v HTML emulátoru

### Nejrychlejší postup

1. Otevři soubor `bitbox-emulator.html` v běžném moderním prohlížeči (bez instalace a bez internetu).
2. Zvol některou ukázku a klikni **Načíst ukázku**. Změní se i banka `VIDEO` nebo `AUDIO`.
3. Klikni **RUN**. Na obrazovce vpravo se zobrazí generovaný obraz. U AUDIO navíc klikni **ZAPNOUT ZVUK**, aby bylo něco slyšet.
4. Přepínačem **LIVE/SCAN** změň způsob zobrazení RAM na šestnácti LED. Přepínačem **AUTO/STEP** přejdi na krokování a pomocí **NEXT** vykonávej jednotlivé instrukce.
5. Uprav zdrojový kód v editoru a potvrď **ASSEMBLE → RAM**. Program se uloží do zvolené banky a změna se projeví při dalším načtení změněného bajtu. S běžícím programem můžeš manipulovat i přes **EXAM/WRITE**.
6. Příkazem **EXPORT .BIN** stáhni celou 256bajtovou banku. **IMPORT .BIN** ji znovu načte. Po zavření stránky se paměť emulátoru běžně vymaže — **uložení přes .BIN** je spolehlivý způsob archivace.

### Assembler

HTML obsahuje jednoduchý dvouprůchodový assembler s návěštími a komentáři.

```asm
; komentář
ORG 00
START:
SET 0A
ADD 01
CMP 0F
JNZ START
HALT 00
```

Pravidla:

- Formát `INSTRUKCE PARAMETR`, například `PATTERN 02`, `ADD FF`, `JMP START`. U instrukcí s ignorovaným parametrem lze napsat `NOP`, `HALT` atd. (automaticky se připojí `00`).
- Hexadecimální bajty lze zapsat i přímo: `50 02`, případně s adresou: `00: 50 02`.
- `ORG 40` nastaví absolutní adresu pro následně zapisované bajty; `DB 01 02 FF` vloží surové bajty.
- `NAZEV:` definuje návěští; `JMP NAZEV` nebo `JZ NAZEV` používá jeho adresu. Assembler podporuje i odkazy **dopředu**.
- Čísla jsou standardně **HEX**: `10` znamená šestnáct; další přijímané tvary jsou `0x10`, `$10`, `%00010000` a `0d16` (dekadicky šestnáct).
- Bajtové adresy `00:` fungují jako nastavování absolutního místa zápisu v RAM.
- Assembler zastaví zápis při překročení 256 B nebo při pokusu zapsat jednu adresu dvakrát. **Nekontroluje veškerou logiku programu**, například zda se skok nedostane doprostřed instrukce; toto kontroluje interpret při běhu.
- Komentáře začínají znakem `;`; názvy příkazů ani návěští nerozlišují velikost písmen.

### Ruční zadávání na předním panelu

Příklad prvních dvou bajtů `PATTERN 02`:

1. V režimu `STOP` nastav na datových přepínačích `01010000` (`50`) a použij **WRITE**; na adresu `00` se zapíše `50`.
2. Použij **NEXT**, čímž zvolíš adresu `01`.
3. Nastav `00000010` (`02`) a použij **WRITE**.
4. Dalšími kroky vlož zbytek programu a klikni **RUN**.
5. Chceš-li upravit určitou adresu, nastav na přepínačích její osmibitové číslo, klikni **EXAM**, přepni data na novou hodnotu a klikni **WRITE**.

Používáš-li `EXAM` za běhu, adresu editoru uvidíš zejména v poli `EDIT` a paměťovém inspektoru: LED v LIVE režimu nadále sledují interpret a nepřepnou se na editor.

### Klávesové zkratky emulátoru

`mezerník` = RUN/STOP · `N` = NEXT · `E` = EXAM · `W` = WRITE. Zkratky se neaktivují při psaní do textových polí.

## 8. Chyby, bezpečnost a hranice emulace

**Ošetřené chyby:** neznámá instrukce, nesprávný režim instrukce, neplatný parametr PATTERN/WAVE/GATE/COUNT, smyčka bez COUNT, skok na lichou adresu, PC mimo adresní prostor, 64 instrukcí bez kladné pauzy.

**Známá omezení:**

- Nepřesná emulace analogového PAL a synchronizace. Pro vytvoření skutečného televizního výstupu na ESP32 bude potřeba samostatný hardware a časově deterministický video firmware.
- `PATTERN 07` zatím vytváří generovanou texturu a **nečte programovou paměť jako obsah**; plánovaná vizualizace vlastních bajtů je rozšíření 1.1.
- Zvuk je monofonní a běží přes Web Audio, nikoli přes GPIO/DAC reálného ESP32.
- `SCRIPT` (`ScriptProcessorNode`) je funkční, ale zastarávající webové API. Chování závisí na prohlížeči a oprávnění ke zvuku.
- LED `LIVE` neukazují fyzické adresy na externí sběrnici procesoru, ale skutečné čtení **virtuální paměti interpretru**; zobrazovací fronta je omezená.
- Paměť ve webovém emulátoru není automaticky trvale uložena do souboru. Před zavřením stránky používej **EXPORT .BIN**.
- Náhodnost `RAND` je deterministická a po novém RUN se ve stejném programu opakuje stejná sekvence, pokud program výchozí stav nezmění.

## 9. Převod na fyzický BITBOX

Plánované zapojení: ESP32-WROOM-32, 16 LED se sériovými posuvnými registry **2× 74HC595**, vstup osmi datových a sedmi ovládacích páček přes **2× 74HC165**, potenciometr SPEED, analogový audio výstup a kompozitní video výstup. Mezi úlohy pro fyzický firmware patří oddělení interpretační smyčky od video/audio výstupu, korektní PAL časování, bezpečné zpracování přepínání a zápis obou bank do nevolatilní paměti. Pinout a napájecí návrh je nutné ověřit podle konkrétní vývojové desky.

### Navržené budoucí rozšíření 1.1

Nové instrukce čtoucí bajty vlastního programu jako data, nástroje pro převod paměti na obrazovou strukturu, případně na sekvenci zvukových vzorků. Tyto funkce **nejsou** součástí implementované verze 1.0.

---

**Soubory:** `bitbox-basic-1.0.md` (tato specifikace), `bitbox-emulator.html` (samostatný funkční webový emulátor). Žádné knihovny, CDN, internetové API ani server nejsou potřeba.
