# byteREEL 2026 — runtime memory budget

Latest linked image: 938,581 B flash, 107,152 B statically allocated RAM,
220,528 B remaining from the ESP32 320 KiB linker RAM budget. The static
number includes the 49,152 B VIDEO feedback history, 512 B work RAM, VM and
panel state, 8,192 B log ring, 192 B fetch-event FIFO, plus Arduino globals.
It is not a measurement of free heap after Wi-Fi and tasks start.

## Additional allocations

| Allocation | Approximate bytes | Lifetime / allocator |
|---|---:|---|
| PAL framebuffer A+B | 124,800 | Runtime; 15 × 4,096 B chunks and 240 pointers per buffer, normal heap |
| PAL I2S DMA | 4,544 | Runtime DMA heap; 2 × (1,136 samples × 2 B) |
| PAL burst tables | 176 | Runtime normal heap; two 44-sample `int16_t` arrays |
| Audio I2S DMA | 2,048 + driver metadata | Runtime DMA heap; 8 × 128 × 2 B, core driver |
| Audio task stack | 4,096 | Created at setup; FreeRTOS task stack (core-defined units) |
| Slot pair scratch | 24,640 | Lazy transient allocation; two × (12,288-byte payload + 32-byte header) |
| Panel SAVE hex scratch | 1,025 | Transient normal heap |
| HTTP body/JSON/assembler | up to 16,384 body bytes plus response/parser objects | Transient; Arduino `String` and C++ assembler allocations |

The PAL figures are derived from the included library's constants and allocation
code. The linker does not account for the composite library framebuffers, DMA,
Wi-Fi/LwIP tasks, Arduino task stacks, or HTTP request Strings. The standard
runtime reports current free heap, minimum free heap, and largest DMA-capable
block at `/api/v1/status`. Firmware checks slot-buffer, video-object/frame,
PAL DMA and audio task/driver allocation failures. A successful link does not
prove that Wi-Fi + PAL + HTTP buffers fit together; record the reported values
on the physical board before sustained-load acceptance.

No heap/fragmentation measurements were possible without the ESP32. The status
endpoint's runtime values are therefore implemented but NOT TESTED here.
