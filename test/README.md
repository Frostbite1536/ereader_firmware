# Tests

No on-device CI exists; the build gate is `pio run -e xteink`.

Planned (docs/ROADMAP.md): host-side unit tests for the Writer buffer
(insert/delete/cursor/viewport math) — the editing core is deliberately
Arduino-free logic on plain arrays so it can be extracted behind a header and
tested with the same approach as `freeink-sdk/libs/ui/FreeInkUI/test/host/`.

Until then, `prompts/bug-hunt-firmware.txt` describes the manual audit that
substitutes for a suite. On-device smoke checklist after flashing:
1. Boot to launcher, battery % plausible, model name matches the device.
2. Writer: pair a keyboard, type a sentence — screen must NOT move until `.`
   or Enter. Esc redraws. Ctrl+S says "Saved"; file appears in /docs.
3. Power press mid-sentence → wake → text intact (autosave-on-sleep).
4. Flashcards: example deck flips and loops.
5. Swap to CrossPoint from /firmware, then swap back from CrossPoint's SD
   update screen.
