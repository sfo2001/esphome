#ifdef USE_LINUX

#include <filesystem>
#include <fstream>
#include <cstdlib>
#include <sys/stat.h>
#include "preferences.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"

namespace esphome {
namespace linux_platform {
namespace fs = std::filesystem;

static const char *const TAG = "linux.preferences";

void LinuxPreferences::setup_() {
  if (this->setup_complete_)
    return;

  // Use configured preferences path or fall back to /var/lib/esphome
  const char *prefs_path = getenv("ESPHOME_PREFERENCES_PATH");
  if (prefs_path == nullptr) {
#ifdef ESPHOME_PREFERENCES_PATH
    prefs_path = ESPHOME_PREFERENCES_PATH;
#else
    prefs_path = "/var/lib/esphome";
#endif
  }

  this->filename_ = prefs_path;

  // Create directory if it doesn't exist
  try {
    fs::create_directories(this->filename_);
  } catch (const std::exception &e) {
    ESP_LOGE(TAG, "Failed to create preferences directory '%s': %s", this->filename_.c_str(), e.what());
    ESP_LOGE(TAG, "You may need to create the directory manually or run with appropriate permissions");
    this->setup_complete_ = true;
    return;
  }

  // Append device name to create unique preferences file
  this->filename_.append("/");
  this->filename_.append(App.get_name());
  this->filename_.append(".prefs");

  // Try to load existing preferences file
  FILE *fp = fopen(this->filename_.c_str(), "rb");
  if (fp != nullptr) {
    while (!feof(fp)) {
      uint32_t key;
      uint8_t len;
      if (fread(&key, sizeof(key), 1, fp) != 1)
        break;
      if (fread(&len, sizeof(len), 1, fp) != 1)
        break;
      uint8_t data[len];
      if (fread(data, sizeof(uint8_t), len, fp) != len)
        break;
      std::vector vec(data, data + len);
      this->data[key] = vec;
    }
    fclose(fp);
    ESP_LOGD(TAG, "Loaded preferences from '%s'", this->filename_.c_str());
  } else {
    ESP_LOGD(TAG, "No existing preferences file found at '%s'", this->filename_.c_str());
  }

  this->setup_complete_ = true;
}

bool LinuxPreferences::sync() {
  this->setup_();

  if (this->filename_.empty()) {
    ESP_LOGE(TAG, "Preferences file path not initialized");
    return false;
  }

  FILE *fp = fopen(this->filename_.c_str(), "wb");
  if (fp == nullptr) {
    ESP_LOGE(TAG, "Failed to open preferences file '%s' for writing", this->filename_.c_str());
    return false;
  }

  std::map<uint32_t, std::vector<uint8_t>>::iterator it;
  for (it = this->data.begin(); it != this->data.end(); ++it) {
    fwrite(&it->first, sizeof(uint32_t), 1, fp);
    uint8_t len = it->second.size();
    fwrite(&len, sizeof(len), 1, fp);
    fwrite(it->second.data(), sizeof(uint8_t), it->second.size(), fp);
  }

  fclose(fp);
  ESP_LOGD(TAG, "Synced preferences to '%s'", this->filename_.c_str());
  return true;
}

bool LinuxPreferences::reset() {
  linux_preferences->data.clear();
  ESP_LOGD(TAG, "Reset preferences");
  return true;
}

ESPPreferenceObject LinuxPreferences::make_preference(size_t length, uint32_t type, bool in_flash) {
  auto backend = new LinuxPreferenceBackend(type);
  return ESPPreferenceObject(backend);
}

void setup_preferences() {
  auto *pref = new LinuxPreferences();  // NOLINT(cppcoreguidelines-owning-memory)
  linux_preferences = pref;
  global_preferences = pref;
}

bool LinuxPreferenceBackend::save(const uint8_t *data, size_t len) {
  return linux_preferences->save(this->key_, data, len);
}

bool LinuxPreferenceBackend::load(uint8_t *data, size_t len) {
  return linux_preferences->load(this->key_, data, len);
}

LinuxPreferences *linux_preferences;

}  // namespace linux_platform

ESPPreferences *global_preferences;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace esphome

#endif  // USE_LINUX
