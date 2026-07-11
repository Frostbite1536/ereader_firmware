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
  // "autosave" (idle autosave) superseded "autoRefSave" (autosave-on-refresh);
  // fall back to the old key so a stored opt-out survives the upgrade.
  autosave = prefs.getBool("autosave", prefs.getBool("autoRefSave", autosave));
  fullRefreshEvery = prefs.getUChar("fullEvery", fullRefreshEvery);
  refreshEveryChars = prefs.getUChar("refEvChars", refreshEveryChars);
  lastApp = prefs.getUChar("lastApp", lastApp);
  landscape = prefs.getBool("landscape", landscape);
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
  prefs.putBool("autosave", autosave);  // key renamed with the semantics (was autoRefSave)
  prefs.putUChar("fullEvery", fullRefreshEvery);
  prefs.putUChar("refEvChars", refreshEveryChars);
  prefs.putUChar("lastApp", lastApp);
  prefs.putBool("landscape", landscape);
  prefs.putString("lastDoc", lastDoc);
  prefs.end();
}
