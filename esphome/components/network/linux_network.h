#pragma once
#ifdef USE_LINUX

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include <string>
#include <netinet/in.h>

namespace esphome {
namespace network {

/// TCP Server for Linux platform (OTA, API)
class LinuxTCPServer {
 public:
  LinuxTCPServer(uint16_t port);
  ~LinuxTCPServer();

  bool begin();  // Create socket, bind, listen
  void stop();   // Close server socket

  int accept_connection();  // Accept new client, return fd (-1 if none)
  bool is_listening() const { return this->server_fd_ >= 0; }

  uint16_t get_port() const { return this->port_; }

 protected:
  uint16_t port_{0};
  int server_fd_{-1};
  struct sockaddr_in6 addr_{};  // IPv6 address (supports IPv4 via IPv4-mapped)
};

/// TCP Client for Linux platform
class LinuxTCPClient {
 public:
  LinuxTCPClient(int fd = -1);  // Can wrap existing fd from accept()
  ~LinuxTCPClient();

  bool connect(const std::string &host, uint16_t port);
  void close();

  ssize_t read(uint8_t *buf, size_t len);
  ssize_t write(const uint8_t *buf, size_t len);

  bool connected() const { return this->fd_ >= 0; }
  int get_fd() const { return this->fd_; }

  // Set socket non-blocking
  bool set_non_blocking(bool non_blocking);

 protected:
  int fd_{-1};
  bool connected_{false};
};

/// UDP Socket for Linux platform (future: mDNS)
class LinuxUDPSocket {
 public:
  LinuxUDPSocket();
  ~LinuxUDPSocket();

  bool begin(uint16_t port);
  void stop();

  ssize_t send_to(const uint8_t *buf, size_t len, const std::string &host, uint16_t port);
  ssize_t recv_from(uint8_t *buf, size_t len, std::string &from_host, uint16_t &from_port);

 protected:
  int fd_{-1};
  uint16_t port_{0};
};

/// Network utilities
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
