# Hardware truth — Xteink X3 / X4

Every fact here was verified against the FreeInk SDK's board registry
(`freeink-sdk/libs/hardware/BoardConfig/include/BoardConfig.h`, profiles
`XTEINK_X3` / `XTEINK_X4`) — the same code CrossPoint ships on both devices.
Do not contradict this file from memory or from chat transcripts.

## The two devices

| | Xteink X4 | Xteink X3 |
|---|---|---|
| MCU | ESP32-C3 (RISC-V, single core) | same |
| Usable RAM | ~380 KB | same |
| Flash | **16 MB** (DIO) | same |
| Panel | 4.26" **800×480**, **SSD1677** controller | 3.68" **792×528**, **UC8253** controller |
| Display SPI | 5 MHz (stock bus speed) | 16 MHz (driver default) |
| Battery | ADC on GPIO0, 2:1 divider | **BQ27220 fuel gauge** over I²C (0x55) |
| X3-only I²C (SDA=20, SCL=0) | — | BQ27220 (0x55), DS3231 RTC (0x68), QMI8658 IMU (0x6B/0x6A) |
| Radio | BLE only (no Bluetooth Classic) | same |

**One binary drives both.** X3 and X4 share the same board/pinout; the firmware
fingerprints the X3-only I²C peripherals at boot (`freeink::selectXteinkDevice()`)
and calls `display.setDisplayX3()` when ≥2 of them answer. Anything else is
treated as an X4.

## Shared pinout (both devices)

| Function | GPIO |
|---|---|
| Display SCLK / MOSI | 8 / 10 |
| Display CS / DC / RST / BUSY | 21 / 4 / 5 / 6 |
| SD card | shares the display SPI bus; MISO=7, CS=12 |
| Buttons | **ADC resistor ladder** on GPIO1 + GPIO2 (not per-button GPIOs) |
| Power button | GPIO3, active-low, deep-sleep wake source |
| I²C (X3 peripherals) | SDA=20, SCL=0 |
| USB | native USB-CDC (GPIO18/19), used for serial + esptool flashing |

The ladder decodes to six logical buttons (BACK, CONFIRM, LEFT, RIGHT, UP,
DOWN) plus POWER — all handled by the SDK's `InputManager`.

Physical layout (first X4 hardware test, 2026-07): the bottom bezel row is,
left to right, **BACK, CONFIRM, then the two buttons the ladder reports as
LEFT and RIGHT**; UP and DOWN are the side buttons. Which of the right two is
LEFT vs RIGHT is **not yet bench-verified** — the firmware maps both onto
focus/selection moves (LEFT=up, RIGHT=down), so a swap would show up as the
right two buttons scrolling in the opposite direction of their on-screen
labels (and as swapped Prev/Next in the Flashcards card view).

## Partition table (16 MB, CrossPoint-compatible)

```
nvs      data nvs      0x9000   0x5000
otadata  data ota      0xe000   0x2000
app0     app  ota_0    0x10000  0x640000   (6.25 MB)
app1     app  ota_1    0x650000 0x640000   (6.25 MB)
spiffs   data spiffs   0xc90000 0x360000
coredump data coredump 0xFF0000 0x10000
```

Keeping this **byte-identical to CrossPoint's `partitions.csv`** is what makes
two-way firmware swapping work: each firmware flashes the other into the
inactive OTA slot and flips `otadata`.

## Flashing (why not `Update.h`)

The stock X3/X4 bootloader boots images produced by CrossPoint's
`patch_firmware_image.py` / web-flasher pipeline, but the *running* ESP-IDF's
`esp_image_verify()` rejects those images with bogus efuse-block-revision
errors on X4 silicon. CrossPoint therefore bypasses the Arduino `Update` class
entirely: it validates the image itself (magic, segment walk, XOR checksum,
SHA-256 trailer), streams it into the next OTA partition with raw
`esp_partition_erase_range`/`esp_partition_write`, and writes a fresh otadata
`SelectEntry` directly. `src/flash/` ports that approach (MIT, attribution in
the headers). **Do not replace it with `Update.h`** — it will brick the swap
flow on real X4 hardware even though it works in naive testing.

First-time install still goes over USB with esptool/PlatformIO; the SD-card
swap flow is for switching between already-installed-once firmwares.

## Corrections to the prior LLM conversation

The chat transcript that seeded this project contained errors. Recorded here
so they don't reenter the codebase:

| Claim in transcript | Reality |
|---|---|
| "4 MB flash, `default.csv` partitions" | 16 MB flash; CrossPoint's custom table above |
| "X3 display is `GxEPD2_370`" | X3 is a UC8253 at 792×528; **no GxEPD2 class matches either panel** — that's why CrossPoint/FreeInk carry their own drivers |
| "GxEPD2 removes the need to write waveform LUTs" | GxEPD2 has no driver for these panels; the community-reverse-engineered LUTs live in the FreeInk SDK |
| "X4 display pins CS=5, DC=0, RST=2, BUSY=15" (first code sample) | CS=21, DC=4, RST=5, BUSY=6 (later message had these right) |
| "X3 buttons are standard digital pull-ups" | Both devices use the ADC resistor ladder on GPIO1/2 |
| "Buttons: Page Up, Page Down, Power only" | Six ladder buttons + power |
| "SD CS pin 7" / "SPIFFS for X3 storage" | SD CS=12 (MISO=7); documents belong on the SD card, not SPIFFS |
| "`Update.begin()`/`writeStream()` for app swap" | Rejected on X4 silicon; see §Flashing |
| "Launcher must reflash itself from `Launcher.bin` to switch back" | With two OTA slots the previous firmware is still in the other slot; switching back is an otadata flip + reboot when it hasn't been overwritten, or a normal SD flash when it has |
| "esp32-c3-devkitm-1 + stock espressif32 platform" | Board id is right, but the SDK toolchain is pinned to **pioarduino 55.03.37** (Arduino core 3.3 / IDF 5.5); the stock platform's older core breaks the SDK |
| "800×480 is 480×800 portrait-native" | Panels are landscape-native; the UI layer rotates logically (default portrait) |
