# Invariants

Rules that must hold in every commit. A change that violates one is wrong even
if it compiles and demos fine.

## Writer refresh contract

1. **A plain keystroke never refreshes the panel immediately.** Text lands in
   the buffer; the screen changes only on: Enter, Tab, `.` (fast refresh),
   Esc (forced fast refresh), Ctrl+L (full refresh / ghost clear), menu
   open/close — plus the typed-char budget: when `Settings.refreshEveryChars`
   is non-zero (menu "Refresh every", default 50; first hardware feedback),
   the Nth buffered character since the last refresh counts as a trigger.
   "Off" restores the strict contract. One exception rides *pauses*, not
   keystrokes: after ~2 s without input, a single idle catch-up refresh
   renders anything the contract left off-screen (and carries the autosave —
   see #4a). It fires at most once per pause and counts toward #2.
2. **Every Nth trigger refresh is promoted to FULL_REFRESH** (N =
   `Settings.fullRefreshEvery`, default 15) so ghosting and DC imbalance never
   accumulate unbounded. Do not remove the promotion.
3. **Backspace does not refresh immediately.** (Esc shows where you are on
   demand; the idle catch-up in #1 renders it after a pause.)

## Writer data safety

4. **No typed character may be lost by design.** The dirty flag is set on
   every buffer mutation and cleared only after a successful SD write. Sleep,
   app exit, and power-off paths must save-if-dirty *before* the display or
   SD powers down.

   4a. **Autosave never runs on the typing path.** It fires only on the idle
   catch-up (#1) — never from a trigger-key or budget refresh — so SD I/O
   (including a failed mount walk when no card is present) can only ever cost
   a pause, not a keystroke. The second K250 round's typing lag came from
   autosave-on-refresh saving mid-sentence; do not reattach it there.
5. **Saves are atomic enough:** write to `<name>.tmp`, then rename over the
   target. Never truncate the target before the new content is fully on card.
6. **Buffer-full is explicit.** At capacity the Writer rejects input and shows
   FULL in the status bar; it never silently drops or wraps.

## Flashing

7. **Validate before otadata.** The full image check (magic, segment walk,
   XOR checksum, SHA-256 trailer, size-fits-partition) must pass before any
   otadata write. A failed or interrupted *flash* leaves otadata untouched —
   the currently-running firmware must remain the boot target.
8. **Never use `Update.h` / `esp_ota_set_boot_partition` here** (X4 silicon
   rejects patched images in `esp_image_verify`; see HARDWARE.md).
9. **The partition table stays byte-identical to CrossPoint's.** Changing it
   breaks two-way swap and orphans installed devices.

## System

10. **Nothing under `freeink-sdk/` is ever edited** (submodule).
11. **No heap allocation in per-keystroke or per-render paths.** Fixed
    buffers; `String` only in cold paths (menus, file listing).
12. **BLE begin()/end() run at full CPU frequency** (NimBLE controller
    requirement).
