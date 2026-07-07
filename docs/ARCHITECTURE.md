# Architecture

## The one decision that shapes everything

The original plan (from the seeding conversation) was a standalone bootloader
firmware that reflashes `.bin` "cartridges" from SD. Research against real
hardware changed the shape:

- **CrossPoint already ships the cartridge mechanism.** Its settings menu
  flashes any `.bin` from the SD card into the inactive OTA partition and
  reboots into it. Any firmware that carries the same flasher can swap back.
- **The ESP32-C3 runs one image at a time**, but our three "apps" (Writer,
  Flashcards, Launcher) are small enough to live in one image with instant
  switching — no reflash, no reboot.

So InkPad is **one firmware** = launcher menu + Writer + Flashcards + SD
flasher, built as a peer of CrossPoint:

```
SD card /firmware/*.bin
   ┌─────────────────┐   flash + otadata flip   ┌──────────────────┐
   │  CrossPoint      │ ───────────────────────▶ │  InkPad (this)   │
   │  (reader)        │ ◀─────────────────────── │  Writer/Cards    │
   └─────────────────┘                           └──────────────────┘
        ota_0 / ota_1 — whichever is inactive receives the new image
```

## Layers

```
src/apps/*        Launcher · Writer · Flashcards · Flasher   (App interface)
src/AppContext.h  one struct of references: display, UI, input, battery,
                  settings — apps never touch globals directly
src/main.cpp      boot (detect X3/X4 → SD → display → UI), app switching,
                  power-button + idle sleep, autosave-on-sleep hook
src/flash/        image validation + raw OTA flash + otadata switch
                  (ported from CrossPoint, MIT)
freeink-sdk/      submodule — display drivers (SSD1677/UC8253 + LUTs),
                  InputManager (ADC ladder), SDCardManager (SdFat),
                  BatteryMonitor, PowerManager, XteinkDetect,
                  BleKeyboardHost (NimBLE central), FreeInkUI
```

## UI model

One `freeink::ui::FreeInkApp<48, 32>` instance owns the immediate-mode UI.
Each app is a screen function + a state struct + action handlers registered
once at boot (ActionId ranges per app, see CLAUDE.md). Dark mode is a single
`InvertedDrawTarget` wrapper — no per-widget flags.

Rendering never touches the panel by itself: the main loop pushes the
framebuffer with the refresh mode mapped from the UI's `RefreshHint`
(`Fast → FAST_REFRESH`, `Full/Clean → FULL_REFRESH`). The Writer intentionally
narrows this further (see INVARIANTS).

## Writer data flow

```
BLE keyboard ──NimBLE──▶ BleKeyboardHost ring ──popKey()──▶ WriterApp::tick()
                                                   │ edits fixed 32 KB buffer
                                                   ▼
                              trigger key? (Enter / Tab / '.')
                                   │ yes                    │ no
                                   ▼                        ▼
                    invalidate(Fast) [+ autosave]      buffer only,
                    → render → FAST_REFRESH            screen untouched
```

Documents are plain `.txt` under `/docs` on the SD card. Saves are
whole-buffer rewrites through SdFat (a 32 KB file writes in well under a
second); autosave fires on sleep, on app exit, and optionally on every
refresh trigger.

## Sleep

Short power press (or 15 min idle) → current app's `onExit()` (Writer saves) →
display deep-sleep → `PowerManager::deepSleepUntilPowerButton()`. Wake is a
chip reset; `Settings` remembers the last app and the Writer's last document,
so the device resumes where it left off.
