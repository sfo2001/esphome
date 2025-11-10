#ifdef USE_LINUX
#include "linux_network.h"
#include "esphome/core/log.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <netdb.h>
#include <cerrno>
#include <cstring>

namespace esphome {
namespace network {

static const char *const TAG = "network.linux";

// ============================================================================
// LinuxNetwork utilities implementation
// ============================================================================

void LinuxNetwork::setup() {
  this->hostname_ = this->get_hostname();
  this->ip_address_ = this->get_ip_address();

  ESP_LOGI(TAG, "Network initialized");
  ESP_LOGI(TAG, "  Hostname: %s", this->hostname_.c_str());
  ESP_LOGI(TAG, "  IP Address: %s", this->ip_address_.c_str());
}

void LinuxNetwork::loop() {
  // Nothing to do in loop for now
  // In future, could add periodic network health checks
}

void LinuxNetwork::dump_config() {
  ESP_LOGCONFIG(TAG, "Linux Network:");
  ESP_LOGCONFIG(TAG, "  Hostname: %s", this->hostname_.c_str());
  ESP_LOGCONFIG(TAG, "  IP Address: %s", this->ip_address_.c_str());
}

std::string LinuxNetwork::get_ip_address() {
  // Use getifaddrs() to find first non-loopback IPv4 address
  struct ifaddrs *ifaddr = nullptr;
  struct ifaddrs *ifa = nullptr;

  if (getifaddrs(&ifaddr) == -1) {
    ESP_LOGE(TAG, "getifaddrs() failed: %s", strerror(errno));
    return "0.0.0.0";
  }

  std::string ip_address;

  for (ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
    if (ifa->ifa_addr == nullptr) {
      continue;
    }

    // Look for IPv4 addresses
    if (ifa->ifa_addr->sa_family == AF_INET) {
      struct sockaddr_in *addr = (struct sockaddr_in *) ifa->ifa_addr;

      // Skip loopback
      if (strcmp(ifa->ifa_name, "lo") == 0) {
        continue;
      }

      char addr_str[INET_ADDRSTRLEN];
      inet_ntop(AF_INET, &addr->sin_addr, addr_str, sizeof(addr_str));

      // Skip if 0.0.0.0 or link-local (169.254.x.x)
      if (strcmp(addr_str, "0.0.0.0") == 0 || strncmp(addr_str, "169.254.", 8) == 0) {
        continue;
      }

      ip_address = addr_str;
      ESP_LOGD(TAG, "Found IP address %s on interface %s", addr_str, ifa->ifa_name);
      break;  // Found first valid address
    }
  }

  freeifaddrs(ifaddr);

  if (ip_address.empty()) {
    ESP_LOGW(TAG, "No valid IP address found, returning 0.0.0.0");
    return "0.0.0.0";
  }

  return ip_address;
}

bool LinuxNetwork::is_connected() {
  // Check if we have a valid IP address (not 0.0.0.0)
  std::string ip = this->get_ip_address();
  return !ip.empty() && ip != "0.0.0.0";
}

std::string LinuxNetwork::get_hostname() {
  char hostname[256];
  if (gethostname(hostname, sizeof(hostname)) == 0) {
    hostname[sizeof(hostname) - 1] = '\0';  // Ensure null-terminated
    return std::string(hostname);
  }
  ESP_LOGW(TAG, "Failed to get hostname: %s", strerror(errno));
  return "unknown";
}

}  // namespace network
}  // namespace esphome

#endif  // USE_LINUX
