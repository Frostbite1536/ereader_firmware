#pragma once

#include <Arduino.h>
#include <BoardConfig.h>
#include <SDCardManager.h>

#include <cctype>
#include <cstring>

// Mount-on-demand with the boot-time folder layout. SdMan.begin() used to run
// exactly once at boot, so a card inserted after power-on (or one that failed
// the first probe) left SD dead until the next reboot — the first-hardware-test
// "no folders created, no decks found" report. Apps call this whenever they
// actually need the card. Cheap when already mounted; failed retries are
// throttled so the Writer's autosave path never stalls typing on a missing
// card. On a fresh mount the probe walks down from the profile's default SPI
// clock (40 MHz on the shared display bus) — the slower rungs only engage when
// the default fails, and BoardConfig::ACTIVE.sd.spiHz is the SDK's documented
// board-override knob for exactly this.
inline bool ensureSdMounted() {
  if (SdMan.ready()) return true;

  static uint32_t lastAttemptMs = 0;
  const uint32_t now = millis();
  if (lastAttemptMs != 0 && now - lastAttemptMs < 4000) return false;
  lastAttemptMs = now;

  static const uint32_t kSpiHz[] = {0 /* profile default */, 20000000, 10000000};
  for (const uint32_t hz : kSpiHz) {
    BoardConfig::ACTIVE.sd.spiHz = hz;
    if (SdMan.begin()) break;
  }
  if (!SdMan.ready()) return false;

  SdMan.ensureDirectoryExists("/docs");
  SdMan.ensureDirectoryExists("/decks");
  SdMan.ensureDirectoryExists("/firmware");
  return true;
}

// Case-insensitive extension check — deck/bin files copied from a Windows PC
// can arrive as ".TXT"/".BIN" and must still count.
inline bool hasExtension(const String& name, const char* ext) {
  const int nameLen = static_cast<int>(name.length());
  const int extLen = static_cast<int>(strlen(ext));
  if (nameLen < extLen) return false;
  for (int i = 0; i < extLen; i++) {
    if (tolower(static_cast<unsigned char>(name[nameLen - extLen + i])) !=
        tolower(static_cast<unsigned char>(ext[i]))) {
      return false;
    }
  }
  return true;
}
