#pragma once

#ifdef USE_LINUX

#include "esphome/core/preferences.h"
#include <map>

namespace esphome {
namespace esphome_linux {

/// @brief Backend for individual preference storage on Linux
///
/// Each preference object (created by make_preference()) gets its own backend
/// instance that stores a 32-bit key identifying the preference. Save/load
/// operations delegate to the global LinuxPreferences singleton.
class LinuxPreferenceBackend : public ESPPreferenceBackend {
 public:
  explicit LinuxPreferenceBackend(uint32_t key) { this->key_ = key; }

  bool save(const uint8_t *data, size_t len) override;
  bool load(uint8_t *data, size_t len) override;

 protected:
  uint32_t key_{};
};

/// @brief Linux implementation of ESPHome preferences system
///
/// Stores preferences in a binary file on the filesystem instead of flash/EEPROM.
/// The file format is a simple sequence of [key:4][len:1][data:len] blocks.
///
/// File Location: /var/lib/esphome/<device-name>.prefs (configurable)
///
/// The entire preferences file is loaded into memory at startup and written
/// atomically on sync() calls. This approach is suitable for Linux systems
/// with abundant RAM and fast filesystem access.
///
/// Thread Safety: Not thread-safe. Should only be accessed from the main loop.
class LinuxPreferences : public ESPPreferences {
 public:
  bool sync() override;
  bool reset() override;

  ESPPreferenceObject make_preference(size_t length, uint32_t type, bool in_flash) override;
  ESPPreferenceObject make_preference(size_t length, uint32_t type) override {
    return make_preference(length, type, false);
  }

  /// @brief Save a preference value to in-memory storage
  /// @param key 32-bit hash identifying the preference
  /// @param data Binary data to save
  /// @param len Length of data (max 255 bytes due to file format)
  /// @return true if saved successfully, false if len > 255
  bool save(uint32_t key, const uint8_t *data, size_t len) {
    if (len > 255)
      return false;
    this->setup_();
    std::vector vec(data, data + len);
    this->data[key] = vec;
    return true;
  }

  /// @brief Load a preference value from in-memory storage
  /// @param key 32-bit hash identifying the preference
  /// @param data Buffer to load data into
  /// @param len Expected length of data
  /// @return true if loaded successfully, false if not found or size mismatch
  bool load(uint32_t key, uint8_t *data, size_t len) {
    if (len > 255)
      return false;
    this->setup_();
    auto it = this->data.find(key);
    if (it == this->data.end())
      return false;
    const auto &vec = it->second;
    if (vec.size() != len)
      return false;
    memcpy(data, vec.data(), len);
    return true;
  }

 protected:
  /// @brief Initialize preferences system (load from disk)
  void setup_();
  /// @brief Whether setup has been completed
  bool setup_complete_{};
  /// @brief Full path to preferences file
  std::string filename_{};
  /// @brief In-memory storage: key -> binary data mapping
  std::map<uint32_t, std::vector<uint8_t>> data{};
};

void setup_preferences();
extern LinuxPreferences *linux_preferences;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace esphome_linux
}  // namespace esphome

#endif  // USE_LINUX
