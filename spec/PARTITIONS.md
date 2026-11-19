# Flash partition and RAM budget

Selected layout: **Huge APP (3 MB No OTA / 1 MB SPIFFS)** for classic ESP32
with **4 MiB flash**. The included `partitions.csv` is the exact table from
Arduino ESP32 core 2.0.14 and belongs next to the main sketch. Select Huge APP
in Arduino IDE as well; the build checks the application limit. Verify the
table emitted by the build.

| Partition | Offset | Length | Purpose |
|---|---:|---:|---|
| nvs | 0x009000 | 0x005000 = 20 KiB | Small settings / Wi-Fi |
| otadata | 0x00E000 | 0x002000 = 8 KiB | Stock layout component; there is no second OTA image |
| app0 | 0x010000 | 0x300000 = 3 MiB | Firmware, web assets, catalogue in PROGMEM |
| spiffs | 0x310000 | 0x0E0000 = 896 KiB | **LittleFS**, slots and source |
| coredump | 0x3F0000 | 0x010000 = 64 KiB | Reserved; actual dumps require the relevant configuration |

The final address is 0x400000 = 4 MiB. Arduino IDE labels the option “1MB
SPIFFS”, but the actual file area is **896 KiB**. LittleFS is mounted through
the `spiffs` label/subtype; the label does not require use of the SPIFFS
driver. Core 2.0.14 LittleFS defaults to partition label `spiffs`.

There is only one app slot, so OTA updates are unavailable. The Wi-Fi slot
editor does **not** need OTA. A USB upload preserves the filesystem only when
the partition layout is unchanged and the whole flash is not erased. A first
migration from another table may make data at the old offsets inaccessible.
Export data before migration; never format on a mount failure.

Eight program banks of 512 B total only 4096 B. With source, metadata, and
both generations, the persistent file limit is 197,120 B, leaving room in the
partition. The final firmware size cannot be promised before linking: provide
the actual `.bin` size. Target below 2.8 MiB; hard limit is 0x300000. Keep HTML
and JavaScript compact, with no framework or external fonts.

Flash is not RAM. The largest risk is RAM when VIDEO and Wi-Fi run together:

- Two PAL buffers use about 122,880 B plus line pointers and DMA.
- FEEDBACK history uses 49,152 B.
- Working programs use 512 B; RX/TX 256 B each; log 8 KiB.
- Wi-Fi, TCP/IP, task stacks, and the web parser add implementation-dependent
  memory use.

Do not allocate full RGB/RGBA frames, huge JSON DOMs, or all slot sources at
once. Keep web assets in PROGMEM; stream source/export data, limit open files
and requests, and bound parser allocations. Report free heap, minimum heap,
and largest DMA-capable block in tests. Evaluate memory with two connected
clients, not only after an idle boot.

References:

- <https://docs.espressif.com/projects/arduino-esp32/en/latest/tutorials/partition_table.html>
- <https://github.com/espressif/arduino-esp32/blob/2.0.14/tools/partitions/huge_app.csv>
- <https://github.com/espressif/arduino-esp32/blob/2.0.14/libraries/LittleFS/src/LittleFS.h>
