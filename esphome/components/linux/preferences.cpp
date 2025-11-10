#ifdef USE_LINUX

#include <filesystem>
#include <fstream>
#include <cstdlib>
#include <sys/stat.h>
#include "preferences.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"

namespace esphome {
namespace esphome_linux {
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
  std::error_code ec;
  fs::create_directories(this->filename_, ec);
  if (ec) {
    ESP_LOGE(TAG, "Failed to create preferences directory '%s': %s", this->filename_.c_str(), ec.message().c_str());
    ESP_LOGE(TAG, "You may need to create the directory manually or run with appropriate permissions");
    this->setup_complete_ = true;
    return;
  }

  // Append device name to create unique preferences file
  this->filename_.append("/");
  this->filename_.append(App.get_name());
  this->filename_.append(".prefs");

  // Try to load existing preferences file
  std::ifstream file(this->filename_, std::ios::binary);
  if (file.is_open()) {
    while (file.good() && !file.eof()) {
      uint32_t key;
      uint8_t len;

      file.read(reinterpret_cast<char *>(&key), sizeof(key));
      if (file.gcount() != sizeof(key))
        break;

      file.read(reinterpret_cast<char *>(&len), sizeof(len));
      if (file.gcount() != sizeof(len))
        break;

      std::vector<uint8_t> data(len);
      file.read(reinterpret_cast<char *>(data.data()), len);
      if (file.gcount() != len)
        break;

      this->data[key] = data;
    }
    file.close();
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

  std::ofstream file(this->filename_, std::ios::binary | std::ios::trunc);
  if (!file.is_open()) {
    ESP_LOGE(TAG, "Failed to open preferences file '%s' for writing", this->filename_.c_str());
    return false;
  }

  for (const auto &entry : this->data) {
    file.write(reinterpret_cast<const char *>(&entry.first), sizeof(uint32_t));
    uint8_t len = entry.second.size();
    file.write(reinterpret_cast<const char *>(&len), sizeof(len));
    file.write(reinterpret_cast<const char *>(entry.second.data()), entry.second.size());
  }

  file.close();

  if (!file.good()) {
    ESP_LOGE(TAG, "Error writing preferences to '%s'", this->filename_.c_str());
    return false;
  }

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

bool LinuxPreferenceBackend::load(uint8_t *data, size_t len) { return linux_preferences->load(this->key_, data, len); }

LinuxPreferences *linux_preferences;

}  // namespace esphome_linux

ESPPreferences *global_preferences;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace esphome

#endif  // USE_LINUX
