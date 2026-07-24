# Keyboard pairing retest — beginner's guide

This is a short follow-up to `TESTING-LINUX.md` for one specific problem:
**the Logitech K250 showed the "Type 123456" prompt but never finished
pairing.** The firmware now pairs the same modern way phones and laptops
do, and this guide walks you through updating, retrying, and — if it still
fails — capturing the one log line that tells us exactly what went wrong.

You need the same setup as before (the Mint computer with `pio` installed,
the USB-C data cable). Total time: about 15 minutes.

> **What changed, in plain words:** Bluetooth keyboards can pair in an old
> style and a modern style. The firmware only spoke the old style; your
> K250 (like most keyboards made in the last decade) really only gets
> tested against the modern style. The firmware now offers the modern
> style first. It also remembers keyboards in a way that survives the
> keyboard changing its Bluetooth address, which Logitech keyboards do
> every 15 minutes or so.

## Pairing a SECOND (new) keyboard — read this if the first one already works

Three more fixes landed for the "I bought a different keyboard and it won't
connect" case. Update with Part A (note the branch name above changed),
then know what to expect:

1. **Picking a keyboard from the list now always answers.** Before, while
   the device was quietly busy trying to reach the *old* keyboard in the
   background, pressing CONFIRM on the new one could silently do nothing —
   it looked frozen. Now the screen says **"Connecting to \<name\>..."**
   the moment you pick, and the device finishes the background attempt,
   cancels it, and connects to your pick by itself (this can take a few
   seconds — that's normal, leave it be).
2. **Keyboards float to the top of the list.** Gadgets that aren't
   keyboards (TVs, earbuds) can no longer crowd your keyboard off the
   8-row screen. Rows tagged **HID** are keyboard-like devices — yours
   should be near the top.
3. **Keyboards that ask for special connection settings are accommodated.**
   Some keyboards (several Logitech models) pair fine and then hang up
   within a second or two because the old firmware refused a technical
   request they make right after pairing. It is now accepted, the same way
   phones and laptops do.

Your old keyboard stays paired — both will connect automatically, whichever
is switched on. And the same advice as Part B still applies to the NEW
keyboard: make it forget any half-finished pairing attempts (hold its
Bluetooth/pairing button until the light blinks fast) before trying again.

---

## Part A — Update and re-flash (≈10 min)

Open a terminal (`Ctrl+Alt+T`) and paste, one line at a time:

```sh
cd ~/ereader_firmware
git fetch
git checkout claude/logitech-keyboard-connection-npc2ij
git pull
git submodule update --init
pio run -e xteink
```

Wait for `[SUCCESS]`. Then plug in the e-reader (powered on), and flash:

```sh
pio run -e xteink -t upload
```

The device reboots into the launcher when it's done. (Any problem with
these steps — cable, ports, permissions — is covered in
`TESTING-LINUX.md`, Steps 3–4 and Troubleshooting.)

---

## Part B — Make the keyboard forget the failed attempts

This step matters: **the earlier failed tries can leave a half-finished
pairing stored inside the keyboard**, which then sabotages the next try.

On the K250:

1. Check the power switch on the back/underside is **on**.
2. **Hold the Bluetooth button for about 3 seconds** until its light
   starts **blinking fast**. Fast blinking = "I forgot the old attempt,
   I'm ready to pair fresh."

(Other keyboards: check the manual for "pairing mode" — it's almost always
holding a Bluetooth key or an `Fn`+key combo until a light blinks.)

---

## Part C — Pair it

1. Launcher → **Writer** → press **BACK** → **Keyboard pairing**.
2. Wait for the scan (~5 seconds). The K250 should appear in the list —
   select it with UP/DOWN and press **CONFIRM**.
3. If the popup asks for a code: **type `123456` on the K250's number row,
   then press Enter on the K250.** Nothing needs to be pressed on the
   e-reader itself during this — the buttons on the device do nothing for
   this step, and that's normal.
4. Success = you land back in the document and the bottom bar shows
   **"Keyboard connected"**. Type a sentence and a `.` to see it appear.

Two details worth knowing:

- The code is **always 123456** on this firmware. If the screen ever shows
  a *different* number, that's important — write down exactly what it said
  and where.
- If it fails, the screen shows a reason ("Pairing failed", "Connect
  timeout", "Not a HID device", "No HID input report"). **Write down the
  exact wording** — each one points at a different stage.

### After it pairs — three quick extra checks

These verify the "keeps working later" half of the fix:

1. **Sleep/wake:** press the power button (sleep), press it again (wake),
   open the Writer — the keyboard should reconnect by itself within a few
   seconds (watch the status bar).
2. **The 20-minute test:** leave the device and keyboard alone for 20+
   minutes (past the keyboard's address-change interval), then wake and
   type. It should reconnect without re-pairing. This exact case was
   broken before the fix.
3. **Reboot:** power the device fully off and on — the keyboard should
   reconnect without visiting the pairing screen.

---

## Part D — If it still won't pair: capture the log

There's a special build that narrates every Bluetooth step over USB. This
is the single most useful thing you can send back.

1. Flash the debug build and start the monitor:

   ```sh
   pio run -e xteink-bledbg -t upload
   pio device monitor -b 115200
   ```

2. With the monitor running (keep the device plugged in — Bluetooth works
   fine on USB), repeat Parts B and C exactly.
3. When it fails, select everything in the terminal and copy it into a
   text file. The lines starting with `[BleHid]` and `[BLE adv]` are the
   payload — especially any line like:

   ```
   [BleHid] security failed: XX:XX:XX:XX:XX:XX err=1030
   ```

   That `err=` number names the precise failing step.
4. Send the file back, along with the exact wording the e-reader's screen
   showed. Exit the monitor with `Ctrl+C`.

When you're done debugging, flash the normal build back:

```sh
pio run -e xteink -t upload
```

### Nuclear option (only if pairing keeps failing in *changing* ways)

A full chip erase wipes every stored pairing and setting on the device
(your documents are safe — they live on the SD card, which this doesn't
touch). The device is blank until the second command finishes:

```sh
pio run -e xteink -t erase
pio run -e xteink -t upload
```

Then also make the keyboard forget (Part B) and pair from scratch.

---

## FAQ

**Why won't my old Bluetooth 3.0 keyboard connect?**
It never can, on any firmware. Bluetooth has two incompatible flavors:
"Classic" (what a 3.0 keyboard speaks) and "Low Energy" (BLE). The radio
chip in the Xteink only has BLE — same as most fitness trackers and
e-readers. A keyboard needs to say Bluetooth 4.0/5.x/"Smart"/BLE on the
box. The pairing screen now says this when it finds nothing.

**The screen said "Type 123456" — I typed it and pressed Enter, but it
still failed. Did I do it wrong?**
No — that part was exactly right, and the fact that you *got* the code
prompt means scanning and connecting both work. The failure was in the
step right after, which is what this update changes.

**Do I press anything on the e-reader while entering the code?**
No. Once the code is on screen, everything happens on the keyboard:
type the six digits, press Enter. The e-reader notices by itself.

**It shows two different numbers on different attempts?**
It shouldn't — the code is fixed at 123456. If you genuinely saw another
number, note it exactly (and where it appeared); that's a bug report gold
nugget.
