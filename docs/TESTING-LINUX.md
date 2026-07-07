# Testing InkPad from Linux Mint — the complete beginner's guide

This walks you from a fresh Linux Mint computer to InkPad running on your
Xteink X3 or X4, step by step, assuming you have never used PlatformIO or
flashed a microcontroller before. Every command is meant to be copied and
pasted exactly. Expect the whole thing to take 30–60 minutes, most of it
waiting for downloads.

> **Nothing here can brick the device.** The ESP32-C3 chip inside the Xteink
> has a permanent, read-only bootloader in ROM. Whatever goes wrong with the
> firmware, plugging in USB and flashing again always works. The worst case
> is "it shows a blank screen until you re-flash."

---

## What you need

- **The e-reader**: an Xteink X3 or X4 (one firmware runs both; it detects
  which one it's on at boot).
- **A USB-C cable that carries data.** Charge-only cables are the #1 cause
  of "my computer doesn't see the device." If in doubt, use the cable that
  came with a phone.
- **A microSD card**, formatted FAT32 (most cards ≤32 GB already are).
- **A Bluetooth keyboard** — it must be Bluetooth **Low Energy** (BLE,
  "Bluetooth 4.0+"/"Bluetooth Smart"). The chip has no classic Bluetooth, so
  very old BT keyboards won't connect. Anything sold in the last ~8 years is
  almost certainly fine.
- **A Linux Mint computer** (Mint 21 or 22, either edition). Mint ships
  Python 3.10–3.12, which is exactly the range the build toolchain wants —
  no Python juggling needed (unlike Windows, where the system Python 3.14 is
  too new).

---

## The big picture

You will: (1) copy this project onto the Mint machine, (2) install the
`pio` build tool, (3) build the firmware, (4) flash it over USB-C,
(5) prepare the SD card, (6) pair a keyboard and run the test checklist.

Everything happens in the **terminal**. Open one with `Ctrl+Alt+T` (or
Menu → Terminal). You type a command, press Enter, read what comes back.
Commands starting with `sudo` will ask for your login password; typing it
shows nothing on screen — that's normal, just type it and press Enter.

---

## Step 1 — Get the project onto the Mint machine

Clone it *with its submodule* — the `freeink-sdk` folder is a separate
repository that provides the display drivers, and a plain download won't
include it:

```sh
sudo apt update && sudo apt install -y git
git clone --recurse-submodules https://github.com/Frostbite1536/ereader_firmware.git
cd ereader_firmware
```

If you already cloned without `--recurse-submodules`, fix it with:

```sh
git submodule update --init
```

**If you copied the folder on a USB stick** instead: copy the whole
`ereader_firmware` folder into your home folder, then verify the submodule
came along:

```sh
cd ~/ereader_firmware
ls freeink-sdk/libs
```

You should see folders like `display`, `hardware`, `ui`. If `freeink-sdk`
is empty, the copy missed it — get it from
`https://github.com/Free-Ink/freeink-sdk` and put its contents in that
folder. (A `.pio` folder, if present from a Windows build, can be deleted;
it's just build output and will be regenerated.)

**Sanity check** — this must print a table of partitions:

```sh
cat partitions.csv
```

---

## Step 2 — Install the build tool (PlatformIO)

PlatformIO ("pio") downloads the right compiler and libraries by itself; you
only install pio. Mint's Python is externally managed, so the clean way is
`pipx`, which puts pio in its own sandbox:

```sh
sudo apt install -y python3 python3-venv pipx
pipx install platformio
pipx ensurepath
```

Now **close the terminal and open a new one** (so the `pio` command is
found), and check:

```sh
pio --version
```

You should see something like `PlatformIO Core, version 6.x`. If the
command isn't found, run `pipx ensurepath` again and open yet another new
terminal.

**Give yourself permission to talk to USB serial devices** (one time only):

```sh
sudo usermod -a -G dialout $USER
```

Then **log out of Mint and log back in** — the permission only takes effect
on a fresh login. (Skipping this is the #2 cause of upload failures.)

Optional but recommended — PlatformIO's official udev rules, which smooth
over various USB device quirks:

```sh
curl -fsSL https://raw.githubusercontent.com/platformio/platformio-core/develop/platformio/assets/system/99-platformio-udev.rules | sudo tee /etc/udev/rules.d/99-platformio-udev.rules > /dev/null
sudo udevadm control --reload-rules && sudo udevadm trigger
```

---

## Step 3 — Build the firmware

From inside the project folder:

```sh
cd ~/ereader_firmware
pio run -e xteink
```

**The first build downloads the whole toolchain (several hundred MB) and
can take 5–15 minutes.** Later builds take under a minute. Success looks
like:

```
RAM:   [====      ]  40.8% (used 133748 bytes from 327680 bytes)
Flash: [=         ]  12.7% (used 829177 bytes from 6553600 bytes)
========================= [SUCCESS] =========================
```

If it ends in `[SUCCESS]`, the firmware is built. If it fails with errors
about missing `BoardConfig.h` or similar, the `freeink-sdk` submodule is
missing — go back to Step 1.

---

## Step 4 — Flash it onto the device

1. Turn the e-reader **on** (its stock firmware or CrossPoint should be
   showing something).
2. Plug it into the computer with the USB-C data cable.
3. Check Mint sees it:

   ```sh
   pio device list
   ```

   You should see a port such as `/dev/ttyACM0`. (Nothing listed? See
   Troubleshooting below.)
4. Flash:

   ```sh
   pio run -e xteink -t upload
   ```

   The tool resets the chip into its ROM bootloader over USB automatically,
   writes the firmware (a minute or so), and reboots it. You'll see
   percentage progress, then `[SUCCESS]`.

The screen will flash and settle on the **InkPad launcher**: a menu with
Writer, Flashcards, Swap firmware, and Sleep, with the device model and
battery percentage in the header. If the header says "Xteink X4" on an X4
(or X3 on an X3), device auto-detection works — that's your first test
passed.

**Optional — watch the boot log.** This is the firmware narrating what it's
doing, useful for bug reports:

```sh
pio device monitor -b 115200
```

Press the device's power button briefly twice (sleep, wake) and you should
see lines like `[MAIN] InkPad up on X4 (800x480)`. Exit the monitor with
`Ctrl+C`. If you ever report a bug, a copy-paste of this output is gold.

---

## Step 5 — Prepare the SD card

InkPad creates its folders (`/docs`, `/decks`, `/firmware`) on the card at
boot, so a freshly formatted FAT32 card works as-is. To have something to
test with, plug the card into the computer and copy:

- `sd-card/decks/example.txt` from this project → the card's `decks`
  folder (create it if the card hasn't been in the device yet). This is a
  sample flashcard deck: one `question|answer` per line.
- *(Optional, for the firmware-swap test)* CrossPoint's `firmware.bin` from
  <https://github.com/crosspoint-reader/crosspoint-reader/releases>
  (download the `firmware.bin` asset of the latest release) → the card's
  `firmware` folder.

Eject the card properly and put it in the device.

---

## Step 6 — Pair the keyboard and write

1. On the launcher, press **DOWN** until *Writer* is highlighted, press
   **CONFIRM** (the device's confirm button).
2. You're in an empty document. Press the **BACK** button — the Writer menu
   opens. Select **Keyboard pairing**.
3. Put your keyboard in pairing mode (usually holding a Bluetooth/Fn key —
   check its manual; a light starts blinking).
4. The device scans for ~5 seconds and lists what it finds. Highlight your
   keyboard with UP/DOWN, press CONFIRM.
   - Most keyboards pair silently ("Just Works").
   - Some show a popup: *"Type 123456 on the keyboard, then press Enter"* —
     do exactly that on the Bluetooth keyboard.
5. On success you land back in the document with a "Keyboard connected"
   note in the bottom bar. The pairing is remembered; next time the
   keyboard just reconnects.

Now write. The crucial thing to *feel*: **the screen does not move while
you type**. Letters go into memory; the panel updates only when you type
`.` or press Enter or Tab — the end of a thought. `Esc` redraws on demand
(after backspacing blind, say), `Ctrl+S` saves, `Ctrl+L` does a deep
"ghost-clearing" refresh, `Ctrl+D` flips dark mode.

Your text is saved to the SD card as plain `.txt` in `/docs` — pull the
card afterwards and open `draft-001.txt` on the computer to confirm.

---

## Step 7 — The test checklist

This is the full smoke test (same list as `test/README.md`, expanded). Work
through it in order; anything that misbehaves, note *exactly* what you did
and what the screen and (if running) the serial monitor showed.

1. **Launcher basics.** Battery % plausible? Model name right? DOWN moves
   the highlight *visibly*. **LEFT and RIGHT must do nothing** — if a stray
   LEFT/RIGHT press opens an app, that's a regression.
2. **Refresh discipline.** In the Writer, type a full sentence without any
   trigger key: the panel must hold perfectly still. Then type `.` — one
   quick update. Every ~15th update is deliberately a slower full-screen
   flash (that's ghost-clearing, not a bug).
3. **Long-document scrolling.** Keep writing (or hold Enter) until the text
   is 3–4 screens long. The view must follow the caret line by line, with
   every line rendered. Hold the ← arrow (or Home + ↑) to walk the caret
   back up: the view must scroll back up too. *(This exercises the newly
   rewritten canvas — the old one silently stopped at 16 lines.)*
4. **Nothing lost on power-cut.** Mid-sentence, press the power button
   (device sleeps — your text stays visible on the panel, that's e-ink).
   Press power again to wake: the Writer resumes with the same text. Now
   the harsher version: type a few words, wait a beat, and hold/slide the
   physical power off if the device has one — on next boot the words must
   still be there (autosave on refresh + on sleep).
5. **Menu round-trip.** BACK → menu → *Font size* cycles S/M/L/XL; *Dark
   mode* inverts the whole screen; *Resume writing* returns; nothing loses
   text.
6. **Flashcards.** Launcher → Flashcards → `example` deck. DOWN shows the
   answer, DOWN again advances, UP goes back, BACK returns to the deck
   list. If you drop 12+ deck files in `/decks`, the list must scroll all
   the way to the last one.
7. **Firmware swap** (if you copied CrossPoint's `firmware.bin`). Launcher
   → *Swap firmware* → select the .bin → CONFIRM. In the dialog, the
   **first DOWN press must land on "Cancel"** (never on Flash), and
   LEFT/RIGHT must do nothing. Select *Flash + reboot* with CONFIRM: a
   percentage counts up over a minute or two — **don't power off** — then
   the device reboots into CrossPoint. To come back: CrossPoint →
   Settings → *SD firmware update* → pick InkPad's `firmware.bin` (put
   `.pio/build/xteink/firmware.bin` from your build onto the card's
   `/firmware` folder first). Both directions must work.
8. **Sleep and battery.** Launcher → Sleep: the device sleeps with your
   screen contents still showing. It also sleeps by itself after 15 idle
   minutes. Power button wakes it back into whatever app you were in.

---

## Troubleshooting

**`pio device list` shows nothing / upload says "no serial port".**
- Nine times out of ten: the cable is charge-only. Try another cable.
- You skipped the log-out/log-in after `usermod -a -G dialout`. Check with
  `groups` — the list must include `dialout`.
- The device is off. Turn it on first; the USB connection needs the chip
  awake (the flasher resets it into the bootloader itself).
- Run `dmesg | tail -20` right after plugging in — you should see a
  `ttyACM0` line appear. If it appears and then vanishes, uninstall the
  braille-terminal service that hijacks serial devices:
  `sudo apt remove brltty` (harmless unless you use a braille display).

**Upload starts, then fails partway.**
Unplug, power the device off and on, plug back in, try again. If it keeps
failing, add the port explicitly:
`pio run -e xteink -t upload --upload-port /dev/ttyACM0`.

**Build fails mentioning missing headers (`BoardConfig.h`, `EInkDisplay.h`).**
The `freeink-sdk` submodule is missing or empty — Step 1's
`git submodule update --init`.

**`pio: command not found`.**
`pipx ensurepath`, then open a *new* terminal. Still nothing:
`~/.local/bin/pio --version` and add `~/.local/bin` to your PATH.

**The keyboard never shows up in the pairing list.**
Confirm it's actually in pairing mode (blinking light), and that it is a
BLE keyboard (see "What you need"). Move it next to the device and press
*Rescan*. Keyboards without a name in their advertisement are filtered out
by this build.

**It paired once but won't reconnect.**
Open pairing again and re-select it. If it's hopeless, pair it with the
keyboard fully reset (most have a "forget all hosts" combo).

**The screen looks smeared / ghost images.**
That's e-ink life with fast refreshes. `Ctrl+L` in the Writer (or any
full-refresh moment, like opening an app) cleans it. If it *never* cleans,
that's a bug — note it.

**Something crashed / rebooted / froze.**
Plug in USB, run `pio device monitor -b 115200`, reproduce, and save the
output — an ESP32 crash prints a "Guru Meditation" block that pinpoints
the bug.

---

## Getting back to safety

Whatever state the device ends up in, you have two exits:

1. **USB always works**: `pio run -e xteink -t upload` re-flashes InkPad
   from the computer over the ROM bootloader.
2. **CrossPoint's web installer** (<https://crosspointreader.com/>) flashes
   CrossPoint from a Chrome browser over the same USB port, no tools
   needed — from there, its SD update screen can flash InkPad back.

Nothing you do while testing — including a failed or interrupted SD-card
flash — changes the boot target unless the new image fully validated first,
so a bad `.bin` on the card just shows a "Flash failed" message and keeps
running InkPad.

---

*Honesty note: this firmware compiles clean and its logic has been audited
against the SDK's sources, but as of 2026-07-07 it has not yet run on
physical hardware. You are the first boot. The checklist above is ordered
so the most likely rough edges (fonts/layout sizing, refresh feel, BLE
pairing quirks) surface early and safely.*
