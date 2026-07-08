#pragma once

// App-layer BLE pairing policy, applied AFTER BleHid.begin() — begin() sets
// the SDK's own defaults, and NimBLE's setters write live ble_hs_cfg globals,
// so the last writer before a pairing attempt wins. This is the sanctioned way
// to retune pairing without editing the SDK submodule (CLAUDE.md rule 4; the
// upstream ask is logged in docs/ROADMAP.md).
//
// Why override: the SDK targets BLE page-turner remotes and pairs at the
// lowest common denominator — legacy-only pairing (sc=false), ENC-only key
// exchange. Real keyboards are the Writer's whole point, and they pair the
// way phones do:
//   - LE Secure Connections. Every phone/PC OS has requested SC since ~2016,
//     so a modern keyboard's legacy-pairing path is untested at best (first
//     hardware round: a 2025 Logitech K250 reached the passkey prompt, then
//     pairing failed). sc=true still falls back to legacy pairing for pre-SC
//     peripherals, so page-turner remotes keep working.
//   - Identity key (IRK) exchange. Keyboards advertise with a resolvable
//     private address that rotates (~15 min); without the peer's IRK the bond
//     pins to a stale address and reconnects after the first rotation fail.
// MITM stays off (SDK default): displayless remotes keep Just Works, and a
// keyboard that demands MITM raises it from its own side of the negotiation.

#include <BoardConfig.h>

#if FREEINK_CAP_BLE_HID_HOST
#include <NimBLEDevice.h>
#endif

inline void applyBleKeyboardCompat() {
#if FREEINK_CAP_BLE_HID_HOST
  NimBLEDevice::setSecurityAuth(/*bonding=*/true, /*mitm=*/FREEINK_BLE_HID_REQUIRE_MITM, /*sc=*/true);
  NimBLEDevice::setSecurityInitKey(BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID);
  NimBLEDevice::setSecurityRespKey(BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID);
#endif
}
