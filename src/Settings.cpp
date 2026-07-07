#include "Settings.h"

#include <Preferences.h>

Settings SETTINGS;

namespace {
constexpr const char* NS = "inkpad";
}

void Settings::load() {
  Preferences prefs;
  if (!prefs.begin(NS, /*readOnly=*/true)) return;  // first boot: keep defaults
  fontSize = prefs.getUChar("fontSize", fontSize);
  darkMode = prefs.getBool("darkMode", darkMode);
  autosaveOnRefresh = prefs.getBool("autoRefSave", autosaveOnRefresh);
  fullRefreshEvery = prefs.getUChar("fullEvery", fullRefreshEvery);
  lastApp = prefs.getUChar("lastApp", lastApp);
  String doc = prefs.getString("lastDoc", "");
  strlcpy(lastDoc, doc.c_str(), sizeof(lastDoc));
  prefs.end();
  if (fullRefreshEvery == 0) fullRefreshEvery = 15;
  if (fontSize > 3) fontSize = 1;
}

void Settings::save() const {
  Preferences prefs;
  if (!prefs.begin(NS, /*readOnly=*/false)) return;
  prefs.putUChar("fontSize", fontSize);
  prefs.putBool("darkMode", darkMode);
  prefs.putBool("autoRefSave", autosaveOnRefresh);
  prefs.putUChar("fullEvery", fullRefreshEvery);
  prefs.putUChar("lastApp", lastApp);
  prefs.putString("lastDoc", lastDoc);
  prefs.end();
}
