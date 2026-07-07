# InkPad

Custom firmware for the **Xteink X3 and X4** e-readers: a distraction-free
word processor driven by a Bluetooth keyboard, plus flashcards, in one
firmware that can swap itself with [CrossPoint](https://crosspointreader.com/)
from the SD card — cartridge-style.

Built on the MIT-licensed [FreeInk SDK](https://github.com/Free-Ink/freeink-sdk)
(display drivers, input, SD, battery, BLE HID host), the same substrate as
CrossPoint. One binary runs both devices; the firmware fingerprints X3 vs X4
at boot.

## The Writer

A digital typewriter, deliberately dumber than Notepad:

- Type on any **BLE keyboard**; text goes to a buffer, the e-ink panel stays
  still. The screen refreshes **only** on `Enter`, `Tab`, or `.` — end of a
  thought, not every keystroke. No flashing, tiny battery draw. (`Tab`
  inserts two spaces — the saved `.txt` matches the screen byte for byte.)
- `Esc` redraws on demand (see where you are after backspacing blind);
  `Ctrl+L` runs a full ghost-clearing refresh; every 15th refresh is promoted
  to a full one automatically.
- `Ctrl+S` save · `Ctrl+N` new file · `Ctrl+D` dark mode (white-on-black).
- Plain `.txt` into `/docs` on the SD card. Autosave on sleep, on exit, and
  (default on) on every screen refresh. Sleep leaves your words on the
  panel — e-ink keeps the last frame at zero power.
- Bottom status bar: filename, keyboard state, word count, battery.
- Four font sizes; swap in any TTF at build time via the SDK's `gen_font.py`
  (see `src/fonts/WriterFonts.h`).

BACK button opens the menu (save / new / pairing / font size / dark mode /
autosave / exit). Physical navigation: UP/DOWN + CONFIRM.

## Flashcards

Drop `/decks/anything.txt` on the SD card, one card per line:

```
What is the powerhouse of the cell?|Mitochondria
Speed of light?|299,792 km/s
```

DOWN flips and advances, UP goes back, BACK returns to the deck list.

## Firmware swap ("Swap firmware" in the launcher)

Copy another Xteink firmware image (e.g. CrossPoint's `firmware.bin`) into
`/firmware` on the SD card, pick it, confirm. The image is fully validated
(magic, segments, checksum, SHA-256), streamed into the inactive OTA slot,
and booted. CrossPoint's own *Settings → SD firmware update* flashes InkPad
back the same way — the partition table is identical by design.

## Building

```sh
pip install platformio     # needs Python 3.10-3.13 (the pioarduino platform
                           # rejects 3.14; on Windows: py -3.13 -m platformio ...)
pio run -e xteink          # build
pio run -e xteink -t upload  # first-time install over USB-C
```

The first-time USB flash uses the standard PlatformIO/esptool path. After
that, updates can ride the SD-swap flow.

New to flashing? **`docs/TESTING-LINUX.md`** is a zero-assumed-knowledge
walkthrough (Linux Mint): install the tools, build, flash, prepare the SD
card, pair a keyboard, and run the full on-device test checklist.

## Repo map

- `docs/HARDWARE.md` — verified hardware facts (and a table of corrections to
  the AI conversation that seeded this project — read it before trusting any
  pin number from a chat log).
- `docs/ARCHITECTURE.md`, `docs/INVARIANTS.md`, `docs/ROADMAP.md`
- `docs/TESTING-LINUX.md` — beginner-friendly build/flash/test walkthrough
- `src/` — firmware (apps in `src/apps/`, flash swap in `src/flash/`)
- `freeink-sdk/` — git submodule (`git submodule update --init` after clone)
- `prompts/` — reusable AI-session prompts (safe-vibe-coding workflow)

## License

MIT. `src/flash/` is ported from CrossPoint Reader (MIT, © 2025 Dave Allie);
the FreeInk SDK is MIT with its own NOTICE.
