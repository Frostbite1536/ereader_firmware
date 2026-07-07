# Tests

No on-device CI exists; the build gate is `pio run -e xteink`.

Planned (docs/ROADMAP.md): host-side unit tests for the Writer buffer
(insert/delete/cursor/viewport math) — the editing core is deliberately
Arduino-free logic on plain arrays so it can be extracted behind a header and
tested with the same approach as `freeink-sdk/libs/ui/FreeInkUI/test/host/`.

Until then, `prompts/bug-hunt-firmware.txt` describes the manual audit that
substitutes for a suite. On-device smoke checklist after flashing:
1. Boot to launcher, battery % plausible, model name matches the device.
   Press DOWN — the highlight must visibly move. Press LEFT/RIGHT — nothing
   may happen (a regression here launches the Writer unasked).
2. Writer: pair a keyboard, type a sentence — screen must NOT move until `.`
   or Enter. Esc redraws. Ctrl+S says "Saved"; file appears in /docs.
3. Long-document scroll: hold Enter/type until the text passes one full
   screen, keep going 2-3 more screens — the view must follow the caret and
   every line must render (this exercises the textArea scroll path that
   replaced a 16-line-capped draw). Arrow-key up to the top; the view must
   scroll back.
4. Power press mid-sentence → wake → text intact (autosave-on-sleep).
5. Flashcards: example deck flips and loops. With 12+ decks on the card, the
   deck list must scroll to the last one.
6. Swap to CrossPoint from /firmware, then swap back from CrossPoint's SD
   update screen. In the confirm dialog, the first DOWN press must land on
   "Cancel", and LEFT/RIGHT must do nothing.
