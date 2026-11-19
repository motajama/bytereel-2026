# Úložiště a Wi-Fi — implementační smlouva

## Uživatelský tok

1. Stroj zapne otevřený AP `byteREEL-2026-XXXX`. Uživatel zůstane připojen i přes hlášku telefonu „bez internetu“ a otevře http://192.168.4.1.
2. Stránka Sloty nabídne osm položek 00..07. V každém slotu jsou záložky VIDEO / AUDIO, jméno a revision.
3. Uživatel edituje assembler nebo hex bajty, vidí překlad do 256 B a chyby řádků. „Přeložit“ nemění paměť stroje. „Uložit slot“ ukládá obě banky jako celek do perzistentního slotu; neprovádí LOAD ani RUN.
4. Při RUN lze text upravovat lokálně a číst monitor/katalog; fyzické uložení odmítne HTTP 409 `STOP_REQUIRED`. Stránka řekne „Přepni přístroj do STOP a ulož znovu“. Žádná skrytá fronta pozdějšího uložení.
5. Do pracovní RAM se program dostane až přes fyzické STOP+MEMORY, D2..D0=slot, NEXT/LOAD na 2 s. Potom EDIT a STOP→RUN.
6. Monitor lze pozastavit pouze v prohlížeči, lokálně vymazat jeho zobrazení a stáhnout log. Nemá pole pro odeslání příkazu. Katalog nabízí hledání, HEX/BIN, parametry, banku a příklady.

## Velikosti a model

8 slotů, název max. 32 UTF-8 bajtů (ne znaků), zdroj každé banky max. 4096 UTF-8 B. Bytecode každé banky přesně 256 B. Celkový persistent payload max. 12 KiB. Bytecode je autorita; text musí odpovídat výsledku assembleru nebo být null. Při disassemblování není nutné obnovovat původní názvy návěští.

Slot payload JSON:
```json
{"schema":1,"isa":"2.0","slot":0,"name":"PIEZO PULSE","video":{"hex":"512 HEX znaku bez mezer","source":"PFREQ 50\nPDUR 0A\nPPLAY\nWAIT 14\nJMP 04\n"},"audio":{"hex":"512 HEX znaku bez mezer","source":null}}
```
Zde je `hex` jen popis délky; platný fixture je `examples/slot-00.json`. ID slotu musí souhlasit s cestou. Export je běžný JSON schema 1, nikoli interní CRC envelope. Obsahuje revision jako metadata; import ji nepoužívá k oprávnění přepsat novější data.

Pracovní RAM uchovává bytecode a dirty flag. Panel SAVE uloží obě banky a source=null; web zrekonstruuje adresovaný hex dump. Zachová se dosavadní název slotu; nový slot se pojmenuje `SLOT 00` atd. Původní source lze zachovat pouze po prokázané shodě překladu s bajty. Web SAVE zachovává source po serverové kontrole shody. V RAM se nesmějí trvale držet zdroje všech osmi slotů.

## Dvě generace a výpadek proudu

Každý slot má soubory `/slots/00.a.brp` a `/slots/00.b.brp`. Každý obsahuje celý pár bank a metadata. Nikdy necommitovat dvě banky do nezávislých souborů. Při startu zvalidovat oba kandidáty a vybrat platného s nejvyšší generation.

- Oba chybí: EMPTY.
- Jeden platný: použít jej; pokud druhý existuje a je poškozený, navíc DEGRADED.
- Oba poškozené: CORRUPT, bez automatického přepsání.
- Stejná generation s různými validními daty: CORRUPT, nevybírat náhodně.
- Generation je uint32, začíná 1. Přetečení zakázat s chybou, nikoli wrap na 0.

Envelope má 32 B, little-endian:

| Offset | Délka | Obsah |
|---|---|---|
| 0 | 8 | ASCII `BR26SLT` a bajt 00 |
| 8 | 2 | format=1 |
| 10 | 2 | ISA major=2 |
| 12 | 4 | generation 1..FFFFFFFF |
| 16 | 4 | payloadLen ≤12288 |
| 20 | 4 | CRC32/IEEE |
| 24 | 8 | reserved=0 |
| 32 | payloadLen | UTF-8 JSON payload |

CRC32/IEEE: reflected polynomial 0xEDB88320, init FFFFFFFF, xorout FFFFFFFF. Počítat přes **celý 32B header s vynulovaným CRC polem + payload**. Chrání se tedy i generation a délka. Trailing data odmítnout. Vedle CRC validovat JSON, schema, ISA, slot, délky i zdroje. CRC není kryptografická autentizace.

Commit: zamknout úložiště, ověřit expectedRevision a znovu STOP, vytvořit validovaný snapshot mimo VM mutex. Zapsat do neaktivní/nižší generace; nejnovější validní soubor nikdy netruncovat. Flush, close, znovu přečíst a ověřit CRC/schema. Teprve pak oznámit úspěch a novou revision. Při chybě zůstane stará verze. Souběh web/panel SAVE serializovat; webový požadavek se starou revision vrací 409. Neplatný nový soubor nesmí zastínit platný starý.

Jedna generace max. 12320 B; všechny dvojice max. 197120 B před režií FS. LittleFS 896 KiB má rezervu pro bloky a GC. Max. čtyři otevřené soubory, požadovaná volná rezerva 128 KiB. ENOSPC vrací chybu, nemaže jiný slot. Žádné trvalé flash logy ani opakované ukládání v loop.

Mount: `LittleFS.begin(false, "/littlefs", 4, "spiffs")`. Nikdy automaticky neformátovat při běžné chybě mount. Pro první prázdnou flash: po mount failure ověřit **celou** FS partition po malých blocích na FF; pouze pokud je vše FF a přístroj STOP, lze jednou formátovat. Jinak readonly STORAGE_ERROR. Destruktivní recovery jen přes dokumentovaný USB servisní příkaz `FORMAT_FS CONFIRM` ve STOP, nikdy přes web ani obyčejnou páčku RESET/CLR. Změna partition vyžaduje předem export existujících slotů.

Flash commit jen STOP. Po zahájení transakce BUSY: hranu RUN odmítnout, nezapamatovat automatický start po dokončení. Přepne-li uživatel RUN během commitu, dokončit atomický zápis a potom vyžadovat STOP→RUN. STOP musí umlčet zvuk bez čekání na FS mutex. Případné pozastavení PAL při zápisu je řešeno ve FIRMWARE-SPEC.md.

## API v1

Odpovědi JSON UTF-8 kromě HTML a attachmentu. Neznámá route 404. Žádné CORS `*`; mutace vyžadují application/json, kontrolu Origin, je-li přítomen, omezení těla na 16384 B a timeout příjmu. Otevřený AP je záměrný bez loginu; tato opatření nejsou autentizace. Nepřidávat remote command console.

| Metoda/cesta | Účel |
|---|---|
| GET `/` | HTML/CSS/JS v PROGMEM/gzip, žádné externí zdroje |
| GET `/api/v1/status` | Read-only provozní stav |
| GET `/api/v1/slots` | Metadata osmi slotů, EMPTY/CORRUPT/OK, revision |
| GET `/api/v1/slots/{id}` | Celý uložený slot a revision; EMPTY→404 |
| PUT `/api/v1/slots/{id}` | Validovaný pár bank, expectedRevision, jen STOP |
| GET `/api/v1/slots/{id}/export` | JSON attachment celého slotu |
| POST `/api/v1/assemble` | Čistý překlad `{bank,source}`, bez mutace paměti; dostupný i RUN |
| GET `/api/v1/instructions` | Katalog ze stejné instrukční tabulky jako VM |
| GET `/api/v1/log?bootId=...&after=seq&limit=...` | Read-only poll, max. 16 záznamů / 2048 B |

Import: prohlížeč přečte JSON soubor, zobrazí náhled a potvrzené uložení použije stejný PUT. Export .bin jedné banky o 256 B nebo čitelného hex lze vytvořit v prohlížeči z GET slot. DELETE se nepřidává; uživatel může uložit prázdný program. Web nemusí mít endpointy pracovních bank. Žádné endpointy RUN, STOP, LOAD, STEP, RESET, GPIO nebo uart-send.

PUT body: `{"expectedRevision":0,"slot":{...payload...}}`; 0 znamená dosud EMPTY. Odpověď 200/201 `{"ok":true,"revision":1}` až po ověřeném commitu. Chyby: 400 syntax/shoda hex-source, 404 nenalezený slot, 409 REVISION_CONFLICT / STOP_REQUIRED / BUSY, 413 BODY_TOO_LARGE, 422 nepodporované schema/ISA, 503 STORAGE_ERROR. Chyba má kód a zprávu, konflikt i aktuální revision. Vždy ukládat celý pár z aktuálně načtené revize. Při konfliktu zachovat lokální neuložený text a nabídnout jeho stažení, ne potichu přepsat.

Log ring 8 KiB, seq monotónní 64bit jako desítkový JSON string, bootId náhodný hex. Při přetečení vrátit `gap:true` a nejstarší dostupné seq. Po změně bootId klient začne nový log. Poll monitoru 500 ms, status 1000 ms, skrytá stránka 2 s. Max. dva klienti, DOM max. 500 řádků. Zdrojové kódy neposílat v každém statusu. Uživatelské názvy, texty a logy renderovat přes textContent, nikoli innerHTML.

## Assembler

Výstup přesně 256 B, mezery vyplnit 00. Dvoubajtové instrukce, case-insensitive mnemoniky/návěští, komentář `;`, dopředná návěští, ORG adresa, DB bajty, adresovaný hex `0A: 90 55`, raw hex `90 55`. Čísla: HEX default, `0xFF`, `$FF`, `%11111111`, `0b11111111`, `0d255`.

Symboly `[A-Za-z_][A-Za-z0-9_]*`, zákaz kolize s mnemonikou. HEX číslo má při ambiguálním `AB` přednost; návěští typu START nebo LOOP1 jsou jednoznačná. Instrukce musí začínat na sudé adrese, DB může na liché. Skok musí být absolutní sudá adresa. Neznámé instrukce, duplicity zápisu/návěští, overflow banky, rozsah argumentu, chybějící argument, lichý skok a nesprávná banka v assemblerovém zápisu mají chybu s řádkem a zabrání uložení.

Ignorovaný argument lze vynechat (doplní se 00); explicitní nenulový argument zachovat pro binární kompatibilitu. U DB/raw hex neprovádět lineární kontrolu všech dvojic jako instrukcí — smějí obsahovat data; runtime chyby zůstávají možné. Disassembler nesmí ztratit bajty neznámé instrukce nebo dat. Export starých 256B .bin zachovat.

Jednotná tabulka instrukcí generuje C++ enum/metadata, JSON katalog a webový help. Neudržovat tři ručně rozdílné seznamy. Server validuje i při klientském náhledu. `web-preview` je pouze návrh vzhledu, nikoli implementace API nebo assembleru.
