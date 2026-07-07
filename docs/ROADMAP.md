# Roadmap

## v0.1 (this codebase) — scope

- Launcher menu (UP/DOWN + CONFIRM), battery in status bar
- Writer: BLE keyboard, trigger-key refresh (Enter/Tab/'.'), Esc redraw,
  Ctrl+L ghost clear, Ctrl+S save, Ctrl+N new file, cursor editing
  (arrows/Home/End/Backspace/Delete), dark mode, 4 font sizes, word count +
  battery + filename status bar, `.txt` to `/docs`, autosave on sleep +
  optional autosave-on-refresh, resume last document; "Open document" picker
  over `/docs` + `/decks`; "Save folder" moves the document between `/docs`
  and `/decks` (write-new-then-delete-old, so a failed move loses nothing)
- Flashcards: `/decks/*.txt`, `question|answer` lines, flip/next/prev —
  decks can be typed on-device in the Writer (Save folder → /decks)
- Flasher: list `/firmware/*.bin`, validate, flash inactive OTA slot, reboot
  (two-way swap with CrossPoint)
- Deep sleep on power button / 15 min idle, wake on power button
- Bottom-row LEFT/RIGHT mirror the side buttons (focus/selection moves only,
  never action dispatch); on-screen button legend (CrossPoint style) on list
  screens; landscape/portrait toggle in the Writer menu (applies via restart)

## Deliberately out of v0.1 (with reasons)

- **.epub export** — user cut it; `.txt` only.
- **Wired keyboards** — user cut it; X4 USB-C is device-mode CDC, X3 has pogo
  pins only. BLE only.
- **Runtime custom fonts from SD** — needs a font-file format + loader
  (CrossPoint has one; port later). v0.1 fonts are compile-time: drop any TTF
  through `freeink-sdk/libs/ui/FreeInkUI/tools/gen_font.py` and rebuild.
- **Mesh broadcast (Meshtastic/Reticulum)** — honest assessment: the C3 radio
  can't hold a HID-host link and a second BLE central session comfortably in
  RAM, and Meshtastic's BLE API is a protobuf protocol, not "BLE serial" as
  the seed conversation claimed. Feasible as a *separate mode* (disconnect
  keyboard → connect to node → push file). Design doc before code.
- **Word-processor niceties** (find, selection, clipboard) — keep it Notepad.

## SDK change candidates (worked around in our layer)

- **`InvertedDrawTarget` gray dithers are invisible in dark mode.**
  DisplayTarget inks only the black dots of a dither ("off" pixels leave the
  background alone), so the inverted focus-highlight dither drew
  black-on-black. Workaround: `src/DarkModeTarget.h` under-fills dither fills
  with white when inversion is on. Upstream fix would live in the SDK's
  wrapper (or a white-speckle paint kind).
- **`DisplayTarget` orientation is constructor-only.** A runtime
  `setOrientation()` would let the landscape toggle apply without the reboot
  `requestRestart()` currently performs.

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
- SD hot-insert is detected on app entry (ensureSdMounted in SdMount.h), not
  instantly — insert the card, then re-open the app from the launcher.
- The keyboard-pairing list shows the first 8 devices found (focus-driven
  lists can't scroll; unnamed devices are filtered out at build level, so 8
  is plenty in practice). The selection-driven lists (Writer menu, document
  picker, decks, bins) scroll and have no such cap.
- The document picker lists the first 32 `.txt` files across `/docs` +
  `/decks` (alphabetical per folder). Past that, manage files on a PC.
- No on-device rename: a draft moved to `/decks` keeps its `draft-NNN.txt`
  name in the Flashcards list. Rename on a PC, or wait for a text-input
  screen (candidate for v0.2 — the BLE keyboard is right there).
