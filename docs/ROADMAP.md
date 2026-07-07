# Roadmap

## v0.1 (this codebase) — scope

- Launcher menu (UP/DOWN + CONFIRM), battery in status bar
- Writer: BLE keyboard, trigger-key refresh (Enter/Tab/'.'), Esc redraw,
  Ctrl+L ghost clear, Ctrl+S save, Ctrl+N new file, cursor editing
  (arrows/Home/End/Backspace/Delete), dark mode, 4 font sizes, word count +
  battery + filename status bar, `.txt` to `/docs`, autosave on sleep +
  optional autosave-on-refresh, resume last document
- Flashcards: `/decks/*.txt`, `question|answer` lines, flip/next/prev
- Flasher: list `/firmware/*.bin`, validate, flash inactive OTA slot, reboot
  (two-way swap with CrossPoint)
- Deep sleep on power button / 15 min idle, wake on power button

## Deliberately out of v0.1 (with reasons)

- **.epub export** — user cut it; `.txt` only.
- **Wired keyboards** — user cut it; X4 USB-C is device-mode CDC, X3 has pogo
  pins only. BLE only.
- **Runtime custom fonts from SD** — needs a font-file format + loader
  (CrossPoint has one; port later). v0.1 fonts are compile-time: drop any TTF
  through `freeink-sdk/libs/ui/FreeInkUI/tools/gen_font.py` and rebuild.
- **Landscape writing mode** — DisplayTarget supports it at construction;
  needs a settings-driven re-init pass.
- **Mesh broadcast (Meshtastic/Reticulum)** — honest assessment: the C3 radio
  can't hold a HID-host link and a second BLE central session comfortably in
  RAM, and Meshtastic's BLE API is a protobuf protocol, not "BLE serial" as
  the seed conversation claimed. Feasible as a *separate mode* (disconnect
  keyboard → connect to node → push file). Design doc before code.
- **Word-processor niceties** (find, selection, clipboard) — keep it Notepad.

## Known v0.1 limitations

- 32 KB per document (~5,000 words). Status bar warns at 90%; FULL at cap.
- No RTC-based filenames on X4 (no clock); documents are `draft-NNN.txt`.
- Keyboard layout is US QWERTY (HID usage translation in the SDK).
- Tab inserts two spaces (the panel fonts have no tab glyph; buffer, file,
  and screen stay byte-identical). Tabs in externally created files render
  one space wide.
- Buttons are polled synchronously: a press landing during a panel refresh
  (~0.3-2 s) is dropped. BLE keystrokes are queued and never lost. Fix path:
  InputManager::beginAsync + popPress.
- The keyboard-pairing list shows the first 8 devices found (focus-driven
  lists can't scroll; unnamed devices are filtered out at build level, so 8
  is plenty in practice). Deck/bin lists scroll and have no such cap.
