#include "util.h"
#include "esphome/core/defines.h"
#ifdef USE_NETWORK
#ifdef USE_WIFI
#include "esphome/components/wifi/wifi_component.h"
#endif

#ifdef USE_ETHERNET
#include "esphome/components/ethernet/ethernet_component.h"
#endif

#ifdef USE_OPENTHREAD
#include "esphome/components/openthread/openthread.h"
#endif

#ifdef USE_MODEM
#include "esphome/components/modem/modem_component.h"
#endif

#ifdef USE_HOST
#include <ifaddrs.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cstring>
#include "esphome/core/log.h"
#endif

namespace esphome {
namespace network {

// The order of the components is important: WiFi should come after any possible main interfaces (it may be used as
// an AP that use a previous interface for NAT).

#ifdef USE_HOST
static const char *const TAG = "network";

// Helper function to get primary IPv4 address on Linux
static std::string get_host_ip_address_() {
  struct ifaddrs *ifaddr = nullptr;
  struct ifaddrs *ifa = nullptr;

  if (getifaddrs(&ifaddr) == -1) {
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
      break;  // Found first valid address
    }
  }

  freeifaddrs(ifaddr);

  if (ip_address.empty()) {
    return "0.0.0.0";
  }

  return ip_address;
}

// Static storage for IP address string (for get_use_address() which returns const char*)
static std::string host_ip_address_cache_;
#endif  // USE_HOST

bool is_connected() {
#ifdef USE_ETHERNET
  if (ethernet::global_eth_component != nullptr && ethernet::global_eth_component->is_connected())
    return true;
#endif

#ifdef USE_MODEM
  if (modem::global_modem_component != nullptr)
    return modem::global_modem_component->is_connected();
#endif

#ifdef USE_WIFI
  if (wifi::global_wifi_component != nullptr)
    return wifi::global_wifi_component->is_connected();
#endif

#ifdef USE_OPENTHREAD
  if (openthread::global_openthread_component != nullptr)
    return openthread::global_openthread_component->is_connected();
#endif

#ifdef USE_HOST
  // Check if we have a valid IP address (not 0.0.0.0)
  std::string ip = get_host_ip_address_();
  return !ip.empty() && ip != "0.0.0.0";
#endif
  return false;
}

bool is_disabled() {
#ifdef USE_MODEM
  if (modem::global_modem_component != nullptr)
    return modem::global_modem_component->is_disabled();
#endif

#ifdef USE_WIFI
  if (wifi::global_wifi_component != nullptr)
    return wifi::global_wifi_component->is_disabled();
#endif
  return false;
}

network::IPAddresses get_ip_addresses() {
#ifdef USE_ETHERNET
  if (ethernet::global_eth_component != nullptr)
    return ethernet::global_eth_component->get_ip_addresses();
#endif

#ifdef USE_MODEM
  if (modem::global_modem_component != nullptr)
    return modem::global_modem_component->get_ip_addresses();
#endif

#ifdef USE_WIFI
  if (wifi::global_wifi_component != nullptr)
    return wifi::global_wifi_component->get_ip_addresses();
#endif
#ifdef USE_OPENTHREAD
  if (openthread::global_openthread_component != nullptr)
    return openthread::global_openthread_component->get_ip_addresses();
#endif

#ifdef USE_HOST
  // Return IP address for host platform
  IPAddresses addresses;
  std::string ip = get_host_ip_address_();
  if (!ip.empty() && ip != "0.0.0.0") {
    addresses[0] = IPAddress();
    addresses[0].from_string(ip);
  }
  return addresses;
#endif

  return {};
}

const char *get_use_address() {
  // Global component pointers are guaranteed to be set by component constructors when USE_* is defined
#ifdef USE_ETHERNET
  return ethernet::global_eth_component->get_use_address();
#endif

#ifdef USE_MODEM
  return modem::global_modem_component->get_use_address();
#endif

#ifdef USE_WIFI
  return wifi::global_wifi_component->get_use_address();
#endif

#ifdef USE_OPENTHREAD
  return openthread::global_openthread_component->get_use_address();
#endif

#ifdef USE_HOST
  // For host platform, return the detected IP address
  // Cache it to ensure the pointer remains valid
  host_ip_address_cache_ = get_host_ip_address_();
  return host_ip_address_cache_.c_str();
#endif

#if !defined(USE_ETHERNET) && !defined(USE_MODEM) && !defined(USE_WIFI) && !defined(USE_OPENTHREAD) && !defined(USE_HOST)
  // Fallback when no network component is defined
  return "";
#endif
}

}  // namespace network
}  // namespace esphome
#endif
