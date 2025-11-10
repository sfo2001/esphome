#pragma once
#ifdef USE_LINUX
#include "ota_backend.h"

#include "esphome/components/md5/md5.h"
#include "esphome/core/defines.h"
#include "esphome/core/macros.h"

#include <fstream>
#include <string>

namespace esphome {
namespace ota {

class LinuxOTABackend : public OTABackend {
 public:
  LinuxOTABackend();
  OTAResponseTypes begin(size_t image_size) override;
  void set_update_md5(const char *md5) override;
  OTAResponseTypes write(uint8_t *data, size_t len) override;
  OTAResponseTypes end() override;
  void abort() override;
  bool supports_compression() override { return false; }

  void set_binary_path(const std::string &path) { this->binary_path_ = path; }

 protected:
  std::string get_current_version_();
  std::string get_next_version_path_();
  void cleanup_old_versions_(int keep_count);

  std::string binary_path_;      // e.g., /usr/local/bin/mydevice
  std::string new_version_path_;  // e.g., /usr/local/bin/mydevice.003
  std::ofstream file_stream_;
  md5::MD5Digest md5_;
  char expected_md5_[33];  // 32 hex chars + null terminator
  bool md5_set_{false};
};

}  // namespace ota
}  // namespace esphome

#endif  // USE_LINUX
