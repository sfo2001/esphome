#ifdef USE_LINUX
#include "ota_backend_linux.h"
#include "ota_backend.h"

#include "esphome/core/application.h"
#include "esphome/core/defines.h"
#include "esphome/core/log.h"

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <limits.h>
#include <sys/stat.h>
#include <unistd.h>
#include <algorithm>
#include <vector>

namespace esphome {
namespace ota {

static const char *const TAG = "ota.linux";

std::unique_ptr<ota::OTABackend> make_ota_backend() { return make_unique<ota::LinuxOTABackend>(); }

LinuxOTABackend::LinuxOTABackend() {
  this->expected_md5_[0] = '\0';

  // Read /proc/self/exe to determine current binary path
  char path[PATH_MAX];
  ssize_t len = readlink("/proc/self/exe", path, sizeof(path) - 1);
  if (len != -1) {
    path[len] = '\0';
    this->binary_path_ = path;

    ESP_LOGD(TAG, "Detected binary path: %s", this->binary_path_.c_str());
  } else {
    ESP_LOGW(TAG, "Failed to read /proc/self/exe: %s", strerror(errno));
    // Fallback to a default path
    this->binary_path_ = "/tmp/esphome_device";
  }

#ifdef ESPHOME_BINARY_PATH
  // Allow override from configuration
  this->binary_path_ = ESPHOME_BINARY_PATH;
  ESP_LOGD(TAG, "Using configured binary path: %s", this->binary_path_.c_str());
#endif
}

std::string LinuxOTABackend::get_current_version_() {
  // Read symlink to determine current version
  // /usr/local/bin/mydevice -> mydevice.002
  // Parse and return version number (002)

  char link[PATH_MAX];
  ssize_t len = readlink(this->binary_path_.c_str(), link, sizeof(link) - 1);
  if (len == -1) {
    ESP_LOGD(TAG, "No symlink found, assuming first version");
    return "000";  // No symlink, first version
  }
  link[len] = '\0';

  // Parse version number from filename (e.g., "mydevice.002" -> "002")
  std::string target(link);

  // Handle both absolute and relative symlink targets
  // Extract just the filename
  size_t last_slash = target.rfind('/');
  if (last_slash != std::string::npos) {
    target = target.substr(last_slash + 1);
  }

  size_t dot_pos = target.rfind('.');
  if (dot_pos != std::string::npos) {
    std::string version_str = target.substr(dot_pos + 1);
    ESP_LOGD(TAG, "Current version: %s", version_str.c_str());
    return version_str;
  }

  ESP_LOGD(TAG, "Could not parse version from symlink target, assuming first version");
  return "000";
}

std::string LinuxOTABackend::get_next_version_path_() {
  std::string current_version = this->get_current_version_();
  int version_num = 0;

  // Try to parse the version number
  try {
    version_num = std::stoi(current_version);
  } catch (...) {
    ESP_LOGW(TAG, "Failed to parse version number, starting from 000");
    version_num = 0;
  }

  int next_version = version_num + 1;

  // Format as 3-digit version (001, 002, etc.)
  char version_buf[16];
  snprintf(version_buf, sizeof(version_buf), "%03d", next_version);

  // Get base path without symlink
  // If binary_path is /usr/local/bin/mydevice
  // Return /usr/local/bin/mydevice.003
  std::string next_path = this->binary_path_ + "." + version_buf;
  ESP_LOGI(TAG, "Next version path: %s", next_path.c_str());
  return next_path;
}

OTAResponseTypes LinuxOTABackend::begin(size_t image_size) {
  ESP_LOGI(TAG, "Starting OTA update (size: %zu bytes)", image_size);

  this->new_version_path_ = this->get_next_version_path_();

  ESP_LOGI(TAG, "New version will be written to: %s", this->new_version_path_.c_str());

  // Open file for writing
  this->file_stream_.open(this->new_version_path_, std::ios::binary | std::ios::trunc);
  if (!this->file_stream_) {
    ESP_LOGE(TAG, "Failed to open file for writing: %s (errno: %d, %s)", this->new_version_path_.c_str(), errno,
             strerror(errno));
    if (errno == EACCES || errno == EPERM) {
      return OTA_RESPONSE_ERROR_WRITING_FLASH;  // Permission denied
    }
    if (errno == ENOSPC) {
      return OTA_RESPONSE_ERROR_WRITING_FLASH;  // No space left
    }
    return OTA_RESPONSE_ERROR_UNKNOWN;
  }

  this->md5_.init();
  return OTA_RESPONSE_OK;
}

void LinuxOTABackend::set_update_md5(const char *md5) {
  strncpy(this->expected_md5_, md5, sizeof(this->expected_md5_) - 1);
  this->expected_md5_[sizeof(this->expected_md5_) - 1] = '\0';
  this->md5_set_ = true;
  ESP_LOGD(TAG, "Expected MD5: %s", this->expected_md5_);
}

OTAResponseTypes LinuxOTABackend::write(uint8_t *data, size_t len) {
  this->file_stream_.write(reinterpret_cast<char *>(data), len);
  this->md5_.add(data, len);

  if (!this->file_stream_) {
    ESP_LOGE(TAG, "Failed to write OTA data (errno: %d, %s)", errno, strerror(errno));
    return OTA_RESPONSE_ERROR_WRITING_FLASH;
  }
  return OTA_RESPONSE_OK;
}

OTAResponseTypes LinuxOTABackend::end() {
  ESP_LOGI(TAG, "Finalizing OTA update");

  this->file_stream_.close();

  // Verify MD5
  if (this->md5_set_) {
    this->md5_.calculate();
    if (!this->md5_.equals_hex(this->expected_md5_)) {
      ESP_LOGE(TAG, "MD5 mismatch! Aborting OTA.");
      unlink(this->new_version_path_.c_str());
      return OTA_RESPONSE_ERROR_MD5_MISMATCH;
    }
    ESP_LOGI(TAG, "MD5 verification successful");
  }

  // Make executable
  if (chmod(this->new_version_path_.c_str(), 0755) != 0) {
    ESP_LOGE(TAG, "Failed to chmod: %s", strerror(errno));
    unlink(this->new_version_path_.c_str());
    return OTA_RESPONSE_ERROR_UNKNOWN;
  }

  // Sync to ensure data written to disk
  sync();

  // Get base name for symlink (e.g., "mydevice.003" from full path)
  std::string new_version_basename = this->new_version_path_;
  size_t last_slash = new_version_basename.rfind('/');
  if (last_slash != std::string::npos) {
    new_version_basename = new_version_basename.substr(last_slash + 1);
  }

  // Atomic symlink update: ln -sf mydevice.003 mydevice
  std::string temp_link = this->binary_path_ + ".tmp";

  // Remove old temp link if it exists
  unlink(temp_link.c_str());

  // Create symlink to new version
  if (symlink(new_version_basename.c_str(), temp_link.c_str()) != 0) {
    ESP_LOGE(TAG, "Failed to create symlink: %s", strerror(errno));
    unlink(this->new_version_path_.c_str());
    return OTA_RESPONSE_ERROR_UNKNOWN;
  }

  // Atomic rename of symlink
  if (rename(temp_link.c_str(), this->binary_path_.c_str()) != 0) {
    ESP_LOGE(TAG, "Failed to update symlink: %s", strerror(errno));
    unlink(temp_link.c_str());
    unlink(this->new_version_path_.c_str());
    return OTA_RESPONSE_ERROR_UNKNOWN;
  }

  ESP_LOGI(TAG, "OTA update successful! Symlink updated to: %s", new_version_basename.c_str());

  // Cleanup old versions (keep last 2)
  this->cleanup_old_versions_(2);

  // Schedule exit so systemd restarts us with new version
  // Delay to allow OTA response to be sent
  ESP_LOGI(TAG, "Scheduling application restart...");
  App.schedule_exit();

  return OTA_RESPONSE_OK;
}

void LinuxOTABackend::abort() {
  ESP_LOGW(TAG, "Aborting OTA update");
  this->file_stream_.close();
  if (!this->new_version_path_.empty()) {
    unlink(this->new_version_path_.c_str());
  }
}

void LinuxOTABackend::cleanup_old_versions_(int keep_count) {
  // List all versioned binaries in directory
  // Sort by version number
  // Delete oldest ones, keeping last 'keep_count' versions

  if (keep_count <= 0) {
    return;
  }

  // Get directory from binary_path_
  std::string dir_path;
  std::string base_name;
  size_t last_slash = this->binary_path_.rfind('/');
  if (last_slash != std::string::npos) {
    dir_path = this->binary_path_.substr(0, last_slash);
    base_name = this->binary_path_.substr(last_slash + 1);
  } else {
    dir_path = ".";
    base_name = this->binary_path_;
  }

  // Open directory
  DIR *dir = opendir(dir_path.c_str());
  if (!dir) {
    ESP_LOGW(TAG, "Failed to open directory for cleanup: %s", strerror(errno));
    return;
  }

  // Find all versioned files matching pattern: base_name.\d{3}
  std::vector<std::pair<int, std::string>> versioned_files;
  struct dirent *entry;
  while ((entry = readdir(dir)) != nullptr) {
    std::string filename(entry->d_name);

    // Check if filename starts with base_name and has a dot
    if (filename.find(base_name + ".") == 0) {
      size_t dot_pos = filename.rfind('.');
      if (dot_pos != std::string::npos) {
        std::string version_str = filename.substr(dot_pos + 1);

        // Check if it's a 3-digit version number
        if (version_str.length() == 3) {
          try {
            int version = std::stoi(version_str);
            std::string full_path = dir_path + "/" + filename;
            versioned_files.push_back({version, full_path});
          } catch (...) {
            // Not a valid version number, skip
          }
        }
      }
    }
  }
  closedir(dir);

  // Sort by version number (ascending)
  std::sort(versioned_files.begin(), versioned_files.end(),
            [](const std::pair<int, std::string> &a, const std::pair<int, std::string> &b) { return a.first < b.first; });

  // Keep the newest 'keep_count' versions, delete the rest
  if (versioned_files.size() > static_cast<size_t>(keep_count)) {
    size_t delete_count = versioned_files.size() - keep_count;
    ESP_LOGI(TAG, "Cleaning up %zu old version(s)", delete_count);

    for (size_t i = 0; i < delete_count; i++) {
      const auto &file = versioned_files[i];
      ESP_LOGD(TAG, "Deleting old version: %s", file.second.c_str());
      if (unlink(file.second.c_str()) != 0) {
        ESP_LOGW(TAG, "Failed to delete %s: %s", file.second.c_str(), strerror(errno));
      }
    }
  }
}

}  // namespace ota
}  // namespace esphome

#endif  // USE_LINUX
