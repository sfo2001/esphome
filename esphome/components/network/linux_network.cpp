#ifdef USE_LINUX
#include "linux_network.h"
#include "esphome/core/log.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <netdb.h>
#include <cerrno>
#include <cstring>

namespace esphome {
namespace network {

static const char *const TAG = "network.linux";

// ============================================================================
// LinuxTCPServer implementation
// ============================================================================

LinuxTCPServer::LinuxTCPServer(uint16_t port) : port_(port) {}

LinuxTCPServer::~LinuxTCPServer() { this->stop(); }

bool LinuxTCPServer::begin() {
  // Create IPv6 socket (supports IPv4 via IPv4-mapped addresses)
  this->server_fd_ = socket(AF_INET6, SOCK_STREAM, 0);
  if (this->server_fd_ < 0) {
    ESP_LOGE(TAG, "Failed to create socket: %s", strerror(errno));
    return false;
  }

  // Set socket options: reuse address
  int opt = 1;
  if (setsockopt(this->server_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
    ESP_LOGW(TAG, "Failed to set SO_REUSEADDR: %s", strerror(errno));
  }

  // Disable IPv6-only mode (allow IPv4-mapped addresses)
  int ipv6only = 0;
  if (setsockopt(this->server_fd_, IPPROTO_IPV6, IPV6_V6ONLY, &ipv6only, sizeof(ipv6only)) < 0) {
    ESP_LOGW(TAG, "Failed to disable IPV6_V6ONLY: %s", strerror(errno));
  }

  // Bind to all interfaces, specified port
  memset(&this->addr_, 0, sizeof(this->addr_));
  this->addr_.sin6_family = AF_INET6;
  this->addr_.sin6_addr = in6addr_any;  // :: (all interfaces)
  this->addr_.sin6_port = htons(this->port_);

  if (bind(this->server_fd_, (struct sockaddr *) &this->addr_, sizeof(this->addr_)) < 0) {
    ESP_LOGE(TAG, "Failed to bind to port %d: %s", this->port_, strerror(errno));
    if (errno == EADDRINUSE) {
      ESP_LOGE(TAG, "Port %d is already in use. Check for conflicting services.", this->port_);
    } else if (errno == EACCES) {
      ESP_LOGE(TAG, "Permission denied to bind to port %d. Try using a port >= 1024 or run with appropriate privileges.",
               this->port_);
    }
    ::close(this->server_fd_);
    this->server_fd_ = -1;
    return false;
  }

  // Listen for connections (backlog: 5)
  if (listen(this->server_fd_, 5) < 0) {
    ESP_LOGE(TAG, "Failed to listen on port %d: %s", this->port_, strerror(errno));
    ::close(this->server_fd_);
    this->server_fd_ = -1;
    return false;
  }

  // Set non-blocking
  int flags = fcntl(this->server_fd_, F_GETFL, 0);
  if (flags < 0) {
    ESP_LOGW(TAG, "Failed to get socket flags: %s", strerror(errno));
  } else {
    if (fcntl(this->server_fd_, F_SETFL, flags | O_NONBLOCK) < 0) {
      ESP_LOGW(TAG, "Failed to set non-blocking mode: %s", strerror(errno));
    }
  }

  ESP_LOGI(TAG, "TCP server listening on port %d", this->port_);
  return true;
}

void LinuxTCPServer::stop() {
  if (this->server_fd_ >= 0) {
    ::close(this->server_fd_);
    this->server_fd_ = -1;
  }
}

int LinuxTCPServer::accept_connection() {
  if (this->server_fd_ < 0) {
    return -1;
  }

  struct sockaddr_in6 client_addr;
  socklen_t addr_len = sizeof(client_addr);

  int client_fd = accept(this->server_fd_, (struct sockaddr *) &client_addr, &addr_len);

  if (client_fd < 0) {
    if (errno != EAGAIN && errno != EWOULDBLOCK) {
      ESP_LOGW(TAG, "accept() failed: %s", strerror(errno));
    }
    return -1;
  }

  // Log connection (IPv4-mapped or IPv6)
  char addr_str[INET6_ADDRSTRLEN];
  inet_ntop(AF_INET6, &client_addr.sin6_addr, addr_str, sizeof(addr_str));
  ESP_LOGD(TAG, "Accepted connection from [%s]:%d", addr_str, ntohs(client_addr.sin6_port));

  return client_fd;
}

// ============================================================================
// LinuxTCPClient implementation
// ============================================================================

LinuxTCPClient::LinuxTCPClient(int fd) : fd_(fd), connected_(fd >= 0) {}

LinuxTCPClient::~LinuxTCPClient() { this->close(); }

bool LinuxTCPClient::connect(const std::string &host, uint16_t port) {
  // Create socket
  this->fd_ = socket(AF_INET, SOCK_STREAM, 0);
  if (this->fd_ < 0) {
    ESP_LOGE(TAG, "Failed to create client socket: %s", strerror(errno));
    return false;
  }

  // Resolve hostname
  struct addrinfo hints;
  struct addrinfo *result = nullptr;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;      // Allow IPv4 or IPv6
  hints.ai_socktype = SOCK_STREAM;  // TCP socket

  char port_str[16];
  snprintf(port_str, sizeof(port_str), "%d", port);

  int ret = getaddrinfo(host.c_str(), port_str, &hints, &result);
  if (ret != 0) {
    ESP_LOGE(TAG, "Failed to resolve hostname %s: %s", host.c_str(), gai_strerror(ret));
    ::close(this->fd_);
    this->fd_ = -1;
    return false;
  }

  // Try to connect using the first address
  bool connected = false;
  for (struct addrinfo *rp = result; rp != nullptr; rp = rp->ai_next) {
    if (::connect(this->fd_, rp->ai_addr, rp->ai_addrlen) == 0) {
      connected = true;
      break;
    }
  }

  freeaddrinfo(result);

  if (!connected) {
    ESP_LOGE(TAG, "Failed to connect to %s:%d: %s", host.c_str(), port, strerror(errno));
    ::close(this->fd_);
    this->fd_ = -1;
    return false;
  }

  this->connected_ = true;
  ESP_LOGD(TAG, "Connected to %s:%d", host.c_str(), port);
  return true;
}

void LinuxTCPClient::close() {
  if (this->fd_ >= 0) {
    ::close(this->fd_);
    this->fd_ = -1;
    this->connected_ = false;
  }
}

ssize_t LinuxTCPClient::read(uint8_t *buf, size_t len) {
  if (this->fd_ < 0) {
    return -1;
  }

  ssize_t result = ::read(this->fd_, buf, len);
  if (result < 0) {
    if (errno != EAGAIN && errno != EWOULDBLOCK) {
      ESP_LOGW(TAG, "read() failed: %s", strerror(errno));
      this->close();
    }
    return -1;
  }

  if (result == 0) {
    // Connection closed by peer
    ESP_LOGD(TAG, "Connection closed by peer");
    this->close();
    return 0;
  }

  return result;
}

ssize_t LinuxTCPClient::write(const uint8_t *buf, size_t len) {
  if (this->fd_ < 0) {
    return -1;
  }

  ssize_t result = ::write(this->fd_, buf, len);
  if (result < 0) {
    if (errno != EAGAIN && errno != EWOULDBLOCK) {
      ESP_LOGW(TAG, "write() failed: %s", strerror(errno));
      this->close();
    }
    return -1;
  }

  return result;
}

bool LinuxTCPClient::set_non_blocking(bool non_blocking) {
  if (this->fd_ < 0) {
    return false;
  }

  int flags = fcntl(this->fd_, F_GETFL, 0);
  if (flags < 0) {
    ESP_LOGE(TAG, "Failed to get socket flags: %s", strerror(errno));
    return false;
  }

  if (non_blocking) {
    flags |= O_NONBLOCK;
  } else {
    flags &= ~O_NONBLOCK;
  }

  if (fcntl(this->fd_, F_SETFL, flags) < 0) {
    ESP_LOGE(TAG, "Failed to set socket flags: %s", strerror(errno));
    return false;
  }

  return true;
}

// ============================================================================
// LinuxUDPSocket implementation
// ============================================================================

LinuxUDPSocket::LinuxUDPSocket() {}

LinuxUDPSocket::~LinuxUDPSocket() { this->stop(); }

bool LinuxUDPSocket::begin(uint16_t port) {
  this->port_ = port;

  // Create UDP socket
  this->fd_ = socket(AF_INET, SOCK_DGRAM, 0);
  if (this->fd_ < 0) {
    ESP_LOGE(TAG, "Failed to create UDP socket: %s", strerror(errno));
    return false;
  }

  // Bind to port
  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = INADDR_ANY;
  addr.sin_port = htons(port);

  if (bind(this->fd_, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
    ESP_LOGE(TAG, "Failed to bind UDP socket to port %d: %s", port, strerror(errno));
    ::close(this->fd_);
    this->fd_ = -1;
    return false;
  }

  ESP_LOGD(TAG, "UDP socket listening on port %d", port);
  return true;
}

void LinuxUDPSocket::stop() {
  if (this->fd_ >= 0) {
    ::close(this->fd_);
    this->fd_ = -1;
  }
}

ssize_t LinuxUDPSocket::send_to(const uint8_t *buf, size_t len, const std::string &host, uint16_t port) {
  if (this->fd_ < 0) {
    return -1;
  }

  // Resolve hostname
  struct addrinfo hints;
  struct addrinfo *result = nullptr;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_DGRAM;

  char port_str[16];
  snprintf(port_str, sizeof(port_str), "%d", port);

  int ret = getaddrinfo(host.c_str(), port_str, &hints, &result);
  if (ret != 0) {
    ESP_LOGE(TAG, "Failed to resolve hostname %s: %s", host.c_str(), gai_strerror(ret));
    return -1;
  }

  ssize_t sent = sendto(this->fd_, buf, len, 0, result->ai_addr, result->ai_addrlen);
  freeaddrinfo(result);

  if (sent < 0) {
    ESP_LOGW(TAG, "sendto() failed: %s", strerror(errno));
    return -1;
  }

  return sent;
}

ssize_t LinuxUDPSocket::recv_from(uint8_t *buf, size_t len, std::string &from_host, uint16_t &from_port) {
  if (this->fd_ < 0) {
    return -1;
  }

  struct sockaddr_in from_addr;
  socklen_t from_len = sizeof(from_addr);

  ssize_t received = recvfrom(this->fd_, buf, len, 0, (struct sockaddr *) &from_addr, &from_len);

  if (received < 0) {
    if (errno != EAGAIN && errno != EWOULDBLOCK) {
      ESP_LOGW(TAG, "recvfrom() failed: %s", strerror(errno));
    }
    return -1;
  }

  // Convert address to string
  char addr_str[INET_ADDRSTRLEN];
  inet_ntop(AF_INET, &from_addr.sin_addr, addr_str, sizeof(addr_str));
  from_host = addr_str;
  from_port = ntohs(from_addr.sin_port);

  return received;
}

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
