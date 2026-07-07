# InkPad — team constitution

Custom firmware for the Xteink X3/X4: a distraction-free BLE-keyboard word
processor ("Writer"), a flashcards app, and an SD-card firmware flasher so the
device can swap between this firmware and CrossPoint like a game console swaps
cartridges.

## Ground rules for AI-assisted changes

1. **`docs/HARDWARE.md` is the single source of truth for hardware facts.**
   Pins, panel controllers, flash size, partition layout, button wiring. Never
   invent a GPIO or a display driver class; if a fact is missing, derive it
   from the FreeInk SDK's `BoardConfig.h` and record it in HARDWARE.md first.
2. **`docs/INVARIANTS.md` rules are non-negotiable.** Read it before touching
   the Writer refresh path, the save path, or the flasher.
3. **Boring code.** Fixed-capacity buffers, no heap in hot paths, no clever
   abstractions. Match the FreeInk SDK's style (it is the substrate we build
   on). Files stay under ~800 lines; split before they grow past that.
4. **The SDK is a submodule, not a fork.** Never edit files under
   `freeink-sdk/`. If the SDK needs a change, note it in `docs/ROADMAP.md` and
   work around it in our layer.
5. **Flash safety is sacred.** Anything that writes flash (`src/flash/`) must
   validate the full image before touching otadata, exactly as the ported
   CrossPoint code does. Do not "simplify" the validation away, and do not
   replace the raw-partition-write path with `Update.h` — the stock X4
   bootloader accepts images that the running IDF's `esp_image_verify`
   rejects (see HARDWARE.md §Flashing).
6. **Build gate:** `pio run -e xteink` must compile clean before a change is
   done. There is no on-device CI; flashing and on-device behavior are the
   human's to verify — say so explicitly in handoffs.

## Layout

- `platformio.ini` / `partitions.csv` — build config (CrossPoint-compatible
  partition table; keep it identical so firmware swap works both ways).
- `src/main.cpp` — boot, device detect, main loop, sleep.
- `src/apps/` — one self-contained app per pair of files (Launcher, Writer,
  Flashcards, Flasher). Apps talk to hardware only through `AppContext`.
- `src/flash/` — OTA partition flashing, ported from CrossPoint (MIT).
- `docs/` — architecture, invariants, hardware truth, roadmap.
- `prompts/` — reusable role prompts for AI sessions (safe-vibe-coding).

## Conventions

- ActionId ranges: Launcher 1–9, Writer 10–29, Flashcards 30–39, Flasher
  40–49. Never reuse across apps.
- Font slots on the shared `DisplayTarget`: 0 small / 1 body / 2 title
  (UI chrome), 3–6 Writer sizes S/M/L/XL.
- SD layout: `/docs` writer output, `/decks` flashcards, `/firmware` app
  binaries. Created on boot if missing.
