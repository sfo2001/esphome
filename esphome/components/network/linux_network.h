#pragma once
#ifdef USE_LINUX

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include <string>

namespace esphome {
namespace network {

/// Network utilities for Linux platform
class LinuxNetwork : public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;

  /// Get primary IPv4 address (for logging/display)
  std::string get_ip_address();

  /// Check if network is available
  bool is_connected();

  /// Get hostname
  std::string get_hostname();

 protected:
  std::string ip_address_{};
  std::string hostname_{};
};

}  // namespace network
}  // namespace esphome

#endif  // USE_LINUX
