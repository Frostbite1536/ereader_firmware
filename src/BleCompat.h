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

#include <BleKeyboardHost.h>
#include <BoardConfig.h>

#if FREEINK_CAP_BLE_HID_HOST
#include <NimBLEDevice.h>

namespace blecompat {

inline NimBLEClient* g_client = nullptr;

// Mirror of the SDK's private ClientCB (BleKeyboardHost.cpp), routed through the
// same public BleKeyboardHost hooks, with ONE behavioral change:
// onConnParamsUpdateRequest accepts the peer's request instead of rejecting it.
// The SDK rejects to protect page-turner remotes that misbehave after a
// negotiation, but real keyboards sit on the other side of that trade: they
// request their preferred interval/latency right after encryption (Logitech
// boards do this within seconds), and some drop the link when refused — which
// reads as "pairs, then instantly disconnects". Every phone/PC host accepts
// these requests; the Writer hosts keyboards, so it should too. (ROADMAP.md
// lists the upstream ask: an SDK knob for this.)
class CompatClientCB : public NimBLEClientCallbacks {
  void onDisconnect(NimBLEClient*, int) override { BleHid.onLinkDown(); }
  void onPassKeyEntry(NimBLEConnInfo& connInfo) override {
    NimBLEDevice::injectPassKey(connInfo, NimBLEDevice::getSecurityPasskey());
  }
  uint32_t onPassKeyDisplay(NimBLEConnInfo&) override {
    const uint32_t passkey = NimBLEDevice::getSecurityPasskey();
    BleHid.onPairingPasskey(passkey);
    return passkey;
  }
  void onConfirmPasskey(NimBLEConnInfo& connInfo, uint32_t) override {
    NimBLEDevice::injectConfirmPasskey(connInfo, true);
  }
  bool onConnParamsUpdateRequest(NimBLEClient*, const ble_gap_upd_params*) override { return true; }
};

inline CompatClientCB g_clientCb;

}  // namespace blecompat
#endif

inline void applyBleKeyboardCompat() {
#if FREEINK_CAP_BLE_HID_HOST
  NimBLEDevice::setSecurityAuth(/*bonding=*/true, /*mitm=*/FREEINK_BLE_HID_REQUIRE_MITM, /*sc=*/true);
  NimBLEDevice::setSecurityInitKey(BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID);
  NimBLEDevice::setSecurityRespKey(BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID);

  // Swap the SDK's client callbacks for the conn-param-accepting mirror above.
  // begin() created exactly one (not yet connected) client; NimBLE hands it back
  // here. Swapping pre-connection means the mirror is live for the whole pairing
  // sequence, not just after link-up.
  blecompat::g_client = NimBLEDevice::getDisconnectedClient();
  if (blecompat::g_client) blecompat::g_client->setClientCallbacks(&blecompat::g_clientCb, false);
#endif
}

// Abort an in-flight connect attempt (the SDK's auto-reconnect to an old bonded
// keyboard can hold its single connection task for the full 8 s connect timeout).
// Non-blocking; the connection task unwinds on its own. Safe to call when idle.
inline void bleCancelConnectAttempt() {
#if FREEINK_CAP_BLE_HID_HOST
  if (blecompat::g_client) blecompat::g_client->cancelConnect();
#endif
}
