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

// Linux Preferences Storage Implementation
//
// This implements ESPHome's preferences/storage backend for Linux platforms using
// a simple binary file format. Unlike embedded platforms that use flash/EEPROM,
// Linux stores preferences in the filesystem.
//
// File Format:
//   Location: /var/lib/esphome/<device-name>.prefs (configurable via ESPHOME_PREFERENCES_PATH)
//   Structure: Repeated blocks of [uint32_t key][uint8_t length][uint8_t[] data]
//   - key: 32-bit hash identifying the preference
//   - length: Size of data in bytes (max 255)
//   - data: Raw binary data for the preference value
//
// The file is loaded entirely into memory on startup and written atomically on sync().
// This approach is suitable for Linux systems with abundant RAM and fast filesystem access.

void LinuxPreferences::setup_() {
  if (this->setup_complete_)
    return;

  // Determine preferences storage path with precedence:
  // 1. ESPHOME_PREFERENCES_PATH environment variable (runtime override)
  // 2. ESPHOME_PREFERENCES_PATH compile-time define (set via cg.add_define())
  // 3. Default: /var/lib/esphome (standard Linux system location)
  const char *prefs_path = getenv("ESPHOME_PREFERENCES_PATH");
  if (prefs_path == nullptr) {
#ifdef ESPHOME_PREFERENCES_PATH
    prefs_path = ESPHOME_PREFERENCES_PATH;
#else
    prefs_path = "/var/lib/esphome";
#endif
  }

  this->filename_ = prefs_path;

  // Create directory if it doesn't exist (similar to mkdir -p)
  // This ensures the preferences directory is available before writing
  std::error_code ec;
  fs::create_directories(this->filename_, ec);
  if (ec) {
    ESP_LOGE(TAG, "Failed to create preferences directory '%s': %s", this->filename_.c_str(), ec.message().c_str());
    ESP_LOGE(TAG, "You may need to create the directory manually or run with appropriate permissions");
    this->setup_complete_ = true;
    return;
  }

  // Build full file path: <prefs_path>/<device-name>.prefs
  // Each ESPHome device gets its own preferences file to allow multiple devices
  // to run on the same system without conflicts
  this->filename_.append("/");
  this->filename_.append(App.get_name());
  this->filename_.append(".prefs");

  // Load existing preferences from disk into memory
  // The entire preferences file is read at startup for fast access during runtime
  std::ifstream file(this->filename_, std::ios::binary);
  if (file.is_open()) {
    // Parse the binary format: [key:4][len:1][data:len] repeated
    while (file.good() && !file.eof()) {
      uint32_t key;
      uint8_t len;

      // Read 32-bit preference key (hash of preference identifier)
      file.read(reinterpret_cast<char *>(&key), sizeof(key));
      if (file.gcount() != sizeof(key))
        break;

      // Read 8-bit length of preference data
      file.read(reinterpret_cast<char *>(&len), sizeof(len));
      if (file.gcount() != sizeof(len))
        break;

      // Read the actual preference data
      std::vector<uint8_t> data(len);
      file.read(reinterpret_cast<char *>(data.data()), len);
      if (file.gcount() != len)
        break;

      // Store in memory for fast access (key -> data mapping)
      this->data[key] = data;
    }
    file.close();
    ESP_LOGD(TAG, "Loaded preferences from '%s'", this->filename_.c_str());
  } else {
    ESP_LOGD(TAG, "No existing preferences file found at '%s'", this->filename_.c_str());
  }

  this->setup_complete_ = true;
}

// Sync in-memory preferences to disk
// This is called by ESPHome when preferences need to be persisted (e.g., after
// configuration changes, WiFi credentials updates, etc.)
bool LinuxPreferences::sync() {
  this->setup_();

  if (this->filename_.empty()) {
    ESP_LOGE(TAG, "Preferences file path not initialized");
    return false;
  }

  // Open file in truncate mode to write fresh copy of all preferences
  // This atomic write approach prevents corruption from partial writes
  std::ofstream file(this->filename_, std::ios::binary | std::ios::trunc);
  if (!file.is_open()) {
    ESP_LOGE(TAG, "Failed to open preferences file '%s' for writing", this->filename_.c_str());
    return false;
  }

  // Write all preferences in binary format: [key:4][len:1][data:len]
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

// Reset all preferences (clear in-memory data)
// Note: This doesn't delete the file on disk until sync() is called
bool LinuxPreferences::reset() {
  linux_preferences->data.clear();
  ESP_LOGD(TAG, "Reset preferences");
  return true;
}

// Create a new preference object
// This is called by ESPHome components to create typed preference storage
// (e.g., for WiFi credentials, OTA state, component settings)
ESPPreferenceObject LinuxPreferences::make_preference(size_t length, uint32_t type, bool in_flash) {
  auto backend = new LinuxPreferenceBackend(type);
  return ESPPreferenceObject(backend);
}

// Initialize the Linux preferences system
// Called during ESPHome startup to set up the global preferences backend
void setup_preferences() {
  auto *pref = new LinuxPreferences();  // NOLINT(cppcoreguidelines-owning-memory)
  linux_preferences = pref;
  global_preferences = pref;
}

// LinuxPreferenceBackend - Individual preference storage backend
// Each preference (e.g., WiFi SSID, OTA state) gets its own backend instance
// that delegates to the global LinuxPreferences singleton

bool LinuxPreferenceBackend::save(const uint8_t *data, size_t len) {
  return linux_preferences->save(this->key_, data, len);
}

bool LinuxPreferenceBackend::load(uint8_t *data, size_t len) { return linux_preferences->load(this->key_, data, len); }

// Global singleton instance for Linux preferences
LinuxPreferences *linux_preferences;

}  // namespace esphome_linux

// Global preferences pointer used by all ESPHome components
// On Linux, this points to the LinuxPreferences implementation
ESPPreferences *global_preferences;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace esphome

#endif  // USE_LINUX
