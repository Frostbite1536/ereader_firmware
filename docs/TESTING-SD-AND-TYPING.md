# SD card + typing speed retest — beginner's guide

This is a follow-up to `TESTING-LINUX.md` for the two problems from your
last report: **the device never reads the SD card** (saving and opening
fail, Flashcards finds no decks) and **everything got slow** after the
"refresh every 25/50/100 characters" update.

Good news: reading the code found one bug behind both. This guide walks
you through updating, then checking the SD card, then checking the typing
feel. Same setup as before (the Mint computer with `pio`, the USB-C data
cable). Total time: about 20 minutes.

> **What changed, in plain words:** the screen and the SD card share most
> of their wires inside the device, and the screen's driver was the one
> setting those wires up — but it never connected the one wire the chip
> *listens* to the SD card on. The device was talking to the card and
> deaf to every answer, which is why no fix so far (and no card, and no
> re-format) ever helped. The firmware now connects that wire itself at
> boot.
>
> Second change: autosave used to write to the SD card *while you were
> typing* (every time the screen refreshed) — with the card unreadable,
> each of those writes turned into a long stall, which is the sluggishness
> you felt. Autosave now waits until your hands have been still for about
> 2 seconds, so typing never waits on the card. That same 2-second pause
> now also catches the screen up, so text you typed (and backspaces, and
> cursor moves) appear after a short pause even before you hit the
> refresh count.

---

## Part A — Update and re-flash (≈10 min)

Open a terminal (`Ctrl+Alt+T`) and paste, one line at a time:

```sh
cd ~/ereader_firmware
git fetch
git checkout feedback-fixes-and-features
git pull
git submodule update --init
pio run -e xteink
```

Wait for `[SUCCESS]`, plug in the e-reader (powered on), and flash:

```sh
pio run -e xteink -t upload
```

(Any trouble with these steps is covered in `TESTING-LINUX.md`, Steps 3–4
and Troubleshooting.)

---

## Part B — SD card checks (the main event)

Put the SD card in **before** powering on (a card inserted while an app is
open isn't noticed until you re-enter the app from the launcher).

1. **Save:** Launcher → Writer, type a sentence, press **Ctrl+S**. The
   bottom bar must say **"Saved"** — not "Save FAILED". This single check
   is the whole bug: if it says Saved, the card works.
2. **The `*` marker:** type a few more characters — a `*` appears next to
   the filename (unsaved changes). Stop typing for 2 seconds — the screen
   refreshes by itself and the `*` disappears. That's the new autosave
   doing its job.
3. **Open:** BACK → menu → **Open document** — your draft should be
   listed. Open it; the text comes back.
4. **Decks:** in the Writer, type two lines like
   `capital of France?|Paris` (one question, a `|`, one answer per line),
   then menu → **Save folder** (it flips to `/decks`). Exit to the
   launcher, open **Flashcards** — the file should appear as a deck and
   the cards should flip.
5. **On the computer:** afterwards, put the card in the computer — you
   should see `/docs` with your drafts and `/decks` with the deck file.

If **any** of these fail, skip to Part D and grab the log line — it now
tells us exactly what the card said.

---

## Part C — Typing feel

1. In the Writer menu, set **Refresh every** to **25** (the most demanding
   setting), then Resume and type a few fast sentences. The screen updates
   every ~25 characters as before, but typing between refreshes should
   feel immediate — no multi-second freezes.
2. **Backspace and arrows:** delete a few characters, move the cursor with
   the arrows, then rest your hands. Within ~2 seconds the screen catches
   up and shows the edit. (Before, only Esc would show it.)
3. **Autosave menu row:** it's now called just **"Autosave"** (was
   "Autosave on refresh") because of the behavior change above. Leave it On.
4. **Card-out behavior (optional):** with no SD card inserted, type a few
   words and pause — about 2 seconds later the bar should show
   **"Save FAILED"** once, and typing should stay smooth regardless.
5. **Power-cut test, updated:** type a few words, **wait 2 seconds** (that
   pause is when the save happens), then cut power however you can. On
   reboot the words must still be there. Words typed in the final 2
   seconds before a sudden power cut are the one thing autosave can't
   promise — sleep via the power button still saves everything first.

---

## Part D — If the SD card still fails: capture the log

The firmware prints one line that names the exact failure. With the device
plugged in:

```sh
pio device monitor -b 115200
```

Then press the reset/power so the device boots while the monitor is
running (or just enter the Writer — it retries the card on entry). Look
for a line starting with `[SD]`:

- `[SD] SD card detected` — the card mounted; whatever failed is
  elsewhere. Note what on-screen action still fails.
- `[SD] SD card not detected (err=0x.. data=0x.. ...)` — copy the **whole
  line** and send it back; the `err=` code names the precise step the
  card refused.

Exit the monitor with `Ctrl+C`. Two cheap things also worth one try each:
a different SD card, and re-formatting the card as FAT32 on the computer.

---

## FAQ

**Why didn't the last update fix the card, when it was supposed to?**
The last update guessed the card was being talked to *too fast* and tried
slower and slower speeds. Reading the display driver's code showed the
real problem: the listening wire was never connected, so no speed could
ever work. This update connects it.

**Did my old cards/files get damaged by the bug?**
No. The device could never write (or read) anything, so the card is
exactly as the computer left it.

**Does "Autosave" still protect me if the battery dies mid-sentence?**
Yes, up to the last pause in your typing — it saves ~2 seconds after your
hands stop, plus every time you sleep the device or leave the Writer. The
only exposure is text typed in the final couple of seconds before a
sudden power loss.

**Typing feels fine but the screen "flashes" every 25 characters — is
that the bug?**
No, that's the refresh setting you chose doing its job (e-ink can't
repaint silently). Set "Refresh every" to 50, 100, or Off if the flash
annoys you — text always appears on the next pause either way.
