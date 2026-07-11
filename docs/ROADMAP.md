# Roadmap

## v0.1 (this codebase) — scope

- Launcher menu (UP/DOWN + CONFIRM), battery in status bar
- Writer: BLE keyboard, trigger-key refresh (Enter/Tab/'.'), Esc redraw,
  Ctrl+L ghost clear, Ctrl+S save, Ctrl+N new file, cursor editing
  (arrows/Home/End/Backspace/Delete), dark mode, 4 font sizes, word count +
  battery + filename status bar, `.txt` to `/docs`, autosave on sleep +
  optional idle autosave (rides the ~2 s catch-up refresh, never a
  keystroke), resume last document; "Open document" picker
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

- **Xteink SD MISO is never attached to the SPI bus.** The X3/X4 profiles
  leave `sd.sclk`/`sd.mosi` unassigned (shared display bus), so
  `SDCardManager::begin()` never calls `SPI.begin()`; the bus is started by
  `EpdBus::begin()` with the panel driver's `spiMiso()`, which the
  SSD1677/UC8253 drivers leave at the base-class -1 (only `Ed2208M5Driver`
  overrides it, for the M5's shared bus). First `SPI.begin()` wins in the
  Arduino core, so MISO=7 is never routed and every SD mount fails at every
  clock — the real cause of the first-hardware-test "SD not detected" (the
  earlier 40→2 MHz clock walk could never help). Upstream fix: SSD1677/UC8253
  override `spiMiso()`/`coCs()` from `BoardConfig::ACTIVE.sd` exactly like the
  M5 driver. Workaround: `src/main.cpp` setup() calls `SPI.begin()` with the
  SD MISO (and parks SD CS HIGH) before `display.begin()` (HARDWARE.md §SD on
  the shared bus).
- **`InvertedDrawTarget` gray dithers are invisible in dark mode.**
  DisplayTarget inks only the black dots of a dither ("off" pixels leave the
  background alone), so the inverted focus-highlight dither drew
  black-on-black. Workaround: `src/DarkModeTarget.h` under-fills dither fills
  with white when inversion is on. Upstream fix would live in the SDK's
  wrapper (or a white-speckle paint kind).
- **`DisplayTarget` orientation is constructor-only.** A runtime
  `setOrientation()` would let the landscape toggle apply without the reboot
  `requestRestart()` currently performs.
- **`BleKeyboardHost` security policy is hardcoded** (legacy-only pairing,
  ENC-only key distribution — tuned for page-turner remotes). Modern
  keyboards expect LE Secure Connections and IRK exchange (they advertise
  with rotating resolvable private addresses); a 2025 Logitech K250 reached
  the passkey prompt and then failed legacy pairing. Workaround:
  `src/BleCompat.h` rewrites the NimBLE `ble_hs_cfg` globals right after
  `BleHid.begin()`. Upstream fix: `FREEINK_BLE_HID_*` flags for sc/key-dist.
  Related suspect if keyboards still fail: `ClientCB::onConnParamsUpdateRequest`
  returns false (rejects all peripheral conn-param requests); some keyboards
  drop the link when their update is rejected — not overridable from our
  layer, needs an SDK knob.
- **`BleKeyboardHost` doubles letters on rollover typing.** The generic
  page-turner fallback in `onReportIngest` runs whenever a keyboard-shaped
  report emits no NEW key and the report map has a consumer page (all modern
  keyboards: media keys) — so a rollover release frame re-emits the still-held
  key ("the" → "tthhe", confirmed on the K250). Upstream fix is one line:
  skip the fallback for keyboard-shaped reports when the map has a keyboard
  page. Workaround: `src/BleKeyFilter.h` re-subscribes the input reports with
  a filter that forwards only newly-pressed keys.
- **Keyboard navigation outside the Writer.** The Writer's menus, picker, and
  pairing screens are keyboard-drivable (AppContext kb* snapshot flags). The
  launcher/Flashcards/Flasher are not, because `BleHid` only runs inside the
  Writer (RAM: NimBLE is torn down on exit). System-wide keyboard nav needs a
  deliberate BLE lifetime redesign first.

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
- List screens skip filenames too long for their fixed slots (39 chars for
  docs/decks, 47 for .bins) — a truncated name couldn't be opened anyway.
  At 999 drafts in a folder the Writer refuses new files/moves instead of
  overwriting `draft-999.txt`.
- If the device goes to sleep (power button or idle) while the SD card is
  missing AND there is unsaved text, that text is lost — deep sleep powers
  down RAM and the save has nowhere to go. The Writer refuses *menu*
  actions that would drop text (new/open/rotate) on a failed save, but
  sleep cannot be refused indefinitely. Keep a card in the slot while
  writing.
