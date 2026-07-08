#pragma once

// Fixes doubled letters when typing quickly on a real keyboard (first K250
// hardware round: "tthhe" for "the").
//
// Root cause (SDK, BleKeyboardHost.cpp onReportIngest): after the standard
// keyboard decode, a "generic code" fallback runs whenever a report emitted no
// NEW key and the report map has a consumer page — true for every modern
// keyboard, because media keys live on that page. Rollover typing (pressing the
// next key before releasing the last) produces exactly such reports: the
// release frame still lists the held key, and the fallback re-emits it as a
// fresh press.
//
// Workaround without editing the submodule (CLAUDE.md rule 4): NimBLE keeps ONE
// notify callback per characteristic, so re-subscribing the HID input reports
// after each link-up swaps the SDK's handler for this filter. The filter
// edge-detects keys against its own shadow state and forwards a synthetic
// report carrying ONLY newly-pressed keys to the public
// BleHid.onReportIngest():
//   - each physical press reaches the SDK exactly once, so its decode emits
//     exactly one event and the generic fallback only ever sees zeros;
//   - a no-new-key frame forwards as all-zero — mods included, because a lone
//     modifier byte would be picked up as a "generic code" — which also clears
//     the SDK's held-key/auto-repeat state on release, exactly as before;
//   - sub-7-byte frames (consumer/media, page-turner remotes) pass through
//     untouched; the fallback is the right decoder for those.
// Reports that land between link-up and the next main-loop tick still take the
// unfiltered path; that window is milliseconds and precedes any typing.
// Upstream fix is one line (skip the fallback for keyboard-shaped reports when
// a keyboard page exists) — logged in docs/ROADMAP.md.

#include <BleKeyboardHost.h>
#include <BoardConfig.h>

#include <cstring>

#if FREEINK_CAP_BLE_HID_HOST
#include <NimBLEDevice.h>

namespace blekeyfilter {

inline uint8_t g_prev[6] = {0};

inline void onNotify(NimBLERemoteCharacteristic*, uint8_t* data, size_t len, bool) {
  if (!data || len == 0) return;
  const uint8_t* p = data;
  size_t n = len;
  if (n == 9) {  // leading report id — same normalization as the SDK
    p += 1;
    n -= 1;
  }
  const uint8_t* keys = nullptr;
  uint8_t mod = 0;
  if (n >= 8) {
    mod = p[0];
    keys = p + 2;
  } else if (n == 7) {
    mod = p[0];
    keys = p + 1;
  } else {
    BleHid.onReportIngest(data, len);  // not keyboard-shaped: SDK decodes as-is
    return;
  }

  uint8_t out[8] = {0};
  size_t outIdx = 2;
  for (int i = 0; i < 6; ++i) {
    const uint8_t k = keys[i];
    if (k == 0 || k == 0x01 /*ErrorRollOver*/) continue;
    bool wasDown = false;
    for (int j = 0; j < 6; ++j) {
      if (g_prev[j] == k) {
        wasDown = true;
        break;
      }
    }
    if (!wasDown && outIdx < sizeof(out)) out[outIdx++] = k;
  }
  memcpy(g_prev, keys, sizeof(g_prev));
  if (outIdx > 2) out[0] = mod;  // mods ride along only with an actual press
  BleHid.onReportIngest(out, sizeof(out));
}

// Re-subscribe the same input-report characteristics the SDK's setupHid()
// subscribed — mirroring its selection — with the filtering handler above.
inline void rehook() {
  memset(g_prev, 0, sizeof(g_prev));
  for (NimBLEClient* client : NimBLEDevice::getConnectedClients()) {
    NimBLERemoteService* hid = client->getService(NimBLEUUID(static_cast<uint16_t>(0x1812)));
    if (!hid) continue;
    for (NimBLERemoteCharacteristic* c : hid->getCharacteristics(false)) {  // cached, no re-discovery
      if (!c || !c->canNotify()) continue;
      const NimBLEUUID uuid = c->getUUID();
      if (uuid == NimBLEUUID(static_cast<uint16_t>(0x2A22))) {  // boot keyboard input fallback
        c->subscribe(true, onNotify);
        continue;
      }
      if (uuid != NimBLEUUID(static_cast<uint16_t>(0x2A4D))) continue;
      bool isInput = true;  // Report Reference descriptor byte[1]: 1 = Input
      if (NimBLERemoteDescriptor* ref = c->getDescriptor(NimBLEUUID(static_cast<uint16_t>(0x2908)))) {
        NimBLEAttValue v = ref->readValue();
        if (v.size() >= 2 && v[1] != 0x01) isInput = false;
      }
      if (isInput) c->subscribe(true, onNotify);
    }
  }
}

}  // namespace blekeyfilter

// Call once per main-loop tick, after BleHid.poll(); hooks every new link-up.
inline void bleKeyFilterPoll() {
  static bool wasConnected = false;
  const bool connected = BleHid.isConnected();
  if (connected == wasConnected) return;
  wasConnected = connected;
  if (connected) blekeyfilter::rehook();
}

#else

inline void bleKeyFilterPoll() {}

#endif  // FREEINK_CAP_BLE_HID_HOST
