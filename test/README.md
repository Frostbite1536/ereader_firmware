# Tests

No on-device CI exists; the build gate is `pio run -e xteink`.

Planned (docs/ROADMAP.md): host-side unit tests for the Writer buffer
(insert/delete/cursor/viewport math) — the editing core is deliberately
Arduino-free logic on plain arrays so it can be extracted behind a header and
tested with the same approach as `freeink-sdk/libs/ui/FreeInkUI/test/host/`.

Until then, `prompts/bug-hunt-firmware.txt` describes the manual audit that
substitutes for a suite. On-device smoke checklist after flashing:
1. Boot to launcher — header reads "Cherith's InkPad", battery % plausible,
   model name matches the device, button legend along the bottom edge. Press
   DOWN — the highlight must visibly move. LEFT/RIGHT (the right two bottom
   buttons) must move it too — but ONLY move it; a press that *activates* a
   row is the old stray-dispatch regression that launched the Writer unasked.
   With dark mode on (Writer menu), the moving highlight must stay clearly
   visible.
2. Writer: pair a keyboard, type a sentence — screen must NOT move until `.`
   or Enter. Esc redraws. Ctrl+S says "Saved"; file appears in /docs.
3. Documents: Writer menu → New document, type a line, then Open document —
   both drafts listed; opening the first brings its text back. In landscape
   the 11-row menu must scroll down to "Exit to launcher".
4. Writer→Flashcards: type 2-3 `question|answer` lines, menu → Save folder
   (value flips to /decks, header path now /decks/...). Exit, open
   Flashcards — the file is a working deck (tagged "deck" in the Writer's
   picker too). Save folder again moves it back to /docs and it leaves the
   deck list.
5. Long-document scroll: hold Enter/type until the text passes one full
   screen, keep going 2-3 more screens — the view must follow the caret and
   every line must render (this exercises the textArea scroll path that
   replaced a 16-line-capped draw). Arrow-key up to the top; the view must
   scroll back.
6. Power press mid-sentence → wake → text intact (autosave-on-sleep).
7. Flashcards: example deck flips and loops. With 12+ decks on the card, the
   deck list must scroll to the last one.
8. Swap to CrossPoint from /firmware, then swap back from CrossPoint's SD
   update screen. In the confirm dialog, the first DOWN press must land on
   "Cancel", and LEFT/RIGHT may only move focus between the options —
   never activate one.
9. Writer menu → Screen rotation: device restarts into landscape with text
   intact (autosave), all screens laid out sane; toggle back the same way.
   If landscape comes up upside down, flip the orientation constant in
   main.cpp (LandscapeCounterClockwise → LandscapeClockwise).
