#pragma once

#include <Arduino.h>

// NVS-backed settings. Load once at boot; save() after any change (writes are
// cheap and rare — menu actions only, never per-keystroke).
struct Settings {
  // Writer
  uint8_t fontSize = 1;             // 0=S 1=M 2=L 3=XL (DisplayTarget slots 3-6)
  bool darkMode = false;            // white-on-black via InvertedDrawTarget
  bool autosaveOnRefresh = true;    // save on every trigger-key refresh
  uint8_t fullRefreshEvery = 15;    // promote every Nth fast refresh to FULL
  char lastDoc[64] = {0};           // resume target, e.g. "/docs/draft-003.txt"

  // System
  uint8_t lastApp = 0;              // AppId to resume into after wake
  bool landscape = false;           // UI orientation; baked in at boot (toggle restarts)

  void load();
  void save() const;
};

extern Settings SETTINGS;
