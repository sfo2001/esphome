# Phase 9: Network Support Implementation

[← Back to README](../README.md) | [Roadmap](../ROADMAP.md)

**Status**: ⏳ Pending - Design Phase

## Phase 9: Network Support Implementation

**Goal**: Enable network connectivity for OTA updates and API communication
**Duration**: 3-5 days
**Compile**: AMD server (x86_64) and Raspberry Pi (ARM)
**Priority**: HIGH (required for OTA and API functionality)

### Overview

The Linux platform currently has OTA and API components that compile successfully but cannot function without network support. This phase implements the network layer using native Linux sockets and network interfaces.

**Current Status**:
- ✅ OTA backend implemented (phase-8-ota-updates.md)
- ✅ API component compiles
- ❌ Network layer missing (this phase)

**Dependencies**:
- OTA requires network for receiving updates
- API requires network for native ESPHome protocol
- Both use TCP sockets on specific ports

### Architecture Overview

#### Network Stack Comparison

| Component | ESP32/ESP8266 | Linux Platform |
|-----------|---------------|----------------|
| **WiFi/Ethernet** | WiFi library | Network interface (already configured by OS) |
| **TCP Stack** | lwIP embedded | Linux kernel TCP/IP stack |
| **Socket API** | Arduino WiFiClient | POSIX sockets (socket, bind, listen, accept) |
| **Network Manager** | WiFi.begin() | Use existing network (no connection management) |
| **IP Address** | DHCP via WiFi library | Read from system (getifaddrs) |
| **DNS** | Built-in DNS client | System resolver (getaddrinfo) |

#### Key Design Principles

1. **No Network Management**: Linux platform assumes network is already configured by the OS (NetworkManager, systemd-networkd, etc.)
2. **Interface Agnostic**: Works with any network interface (eth0, wlan0, etc.)
3. **POSIX Sockets**: Use standard socket(), bind(), listen(), accept() APIs
4. **IPv4 + IPv6**: Support both protocols where applicable
5. **Non-blocking I/O**: Use select()/poll() for asynchronous operations

#### Component Architecture

```
┌──────────────────────────────────────────────────────────┐
│ ESPHome Application Layer                                │
├──────────────────────────────────────────────────────────┤
│ OTA Component          │ API Component                   │
│ (port 3232)            │ (port 6053)                     │
└────────────┬───────────┴──────────────┬──────────────────┘
             │                          │
             ▼                          ▼
┌──────────────────────────────────────────────────────────┐
│ Network Abstraction Layer (this phase)                   │
├──────────────────────────────────────────────────────────┤
│ • LinuxTCPServer (bind, listen, accept)                  │
│ • LinuxTCPClient (connect, read, write)                  │
│ • LinuxUDPSocket (sendto, recvfrom)                      │
│ • Network utilities (get IP, check connectivity)         │
└────────────┬─────────────────────────────────────────────┘
             │
             ▼
┌──────────────────────────────────────────────────────────┐
│ Linux Kernel Network Stack                               │
│ (POSIX sockets: socket, bind, listen, accept, etc.)     │
└──────────────────────────────────────────────────────────┘
```

### Implementation Tasks

#### Core Network Components

- [ ] **Create `esphome/components/network/linux_network.h`** (~150 LOC)

  ```cpp
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

    ssize_t send_to(const uint8_t *buf, size_t len,
                    const std::string &host, uint16_t port);
    ssize_t recv_from(uint8_t *buf, size_t len,
                      std::string &from_host, uint16_t &from_port);

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
  ```

- [ ] **Create `esphome/components/network/linux_network.cpp`** (~400 LOC)

  **Key Implementation Details:**

  ```cpp
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

  // LinuxTCPServer implementation
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
    if (setsockopt(this->server_fd_, SOL_SOCKET, SO_REUSEADDR,
                   &opt, sizeof(opt)) < 0) {
      ESP_LOGW(TAG, "Failed to set SO_REUSEADDR: %s", strerror(errno));
    }

    // Disable IPv6-only mode (allow IPv4-mapped addresses)
    int ipv6only = 0;
    if (setsockopt(this->server_fd_, IPPROTO_IPV6, IPV6_V6ONLY,
                   &ipv6only, sizeof(ipv6only)) < 0) {
      ESP_LOGW(TAG, "Failed to disable IPV6_V6ONLY: %s", strerror(errno));
    }

    // Bind to all interfaces, specified port
    memset(&this->addr_, 0, sizeof(this->addr_));
    this->addr_.sin6_family = AF_INET6;
    this->addr_.sin6_addr = in6addr_any;  // :: (all interfaces)
    this->addr_.sin6_port = htons(this->port_);

    if (bind(this->server_fd_, (struct sockaddr *)&this->addr_,
             sizeof(this->addr_)) < 0) {
      ESP_LOGE(TAG, "Failed to bind to port %d: %s",
               this->port_, strerror(errno));
      close(this->server_fd_);
      this->server_fd_ = -1;
      return false;
    }

    // Listen for connections (backlog: 5)
    if (listen(this->server_fd_, 5) < 0) {
      ESP_LOGE(TAG, "Failed to listen on port %d: %s",
               this->port_, strerror(errno));
      close(this->server_fd_);
      this->server_fd_ = -1;
      return false;
    }

    // Set non-blocking
    int flags = fcntl(this->server_fd_, F_GETFL, 0);
    fcntl(this->server_fd_, F_SETFL, flags | O_NONBLOCK);

    ESP_LOGI(TAG, "TCP server listening on port %d", this->port_);
    return true;
  }

  void LinuxTCPServer::stop() {
    if (this->server_fd_ >= 0) {
      close(this->server_fd_);
      this->server_fd_ = -1;
    }
  }

  int LinuxTCPServer::accept_connection() {
    if (this->server_fd_ < 0) {
      return -1;
    }

    struct sockaddr_in6 client_addr;
    socklen_t addr_len = sizeof(client_addr);

    int client_fd = accept(this->server_fd_,
                          (struct sockaddr *)&client_addr,
                          &addr_len);

    if (client_fd < 0) {
      if (errno != EAGAIN && errno != EWOULDBLOCK) {
        ESP_LOGW(TAG, "accept() failed: %s", strerror(errno));
      }
      return -1;
    }

    // Log connection (IPv4-mapped or IPv6)
    char addr_str[INET6_ADDRSTRLEN];
    inet_ntop(AF_INET6, &client_addr.sin6_addr,
              addr_str, sizeof(addr_str));
    ESP_LOGD(TAG, "Accepted connection from [%s]:%d",
             addr_str, ntohs(client_addr.sin6_port));

    return client_fd;
  }

  // LinuxNetwork utilities implementation
  void LinuxNetwork::setup() {
    this->ip_address_ = this->get_ip_address();
    this->hostname_ = this->get_hostname();

    ESP_LOGI(TAG, "Network initialized");
    ESP_LOGI(TAG, "  Hostname: %s", this->hostname_.c_str());
    ESP_LOGI(TAG, "  IP Address: %s", this->ip_address_.c_str());
  }

  std::string LinuxNetwork::get_ip_address() {
    // Use getifaddrs() to find first non-loopback IPv4 address
    struct ifaddrs *ifaddr, *ifa;

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
        struct sockaddr_in *addr = (struct sockaddr_in *)ifa->ifa_addr;

        // Skip loopback
        if (strcmp(ifa->ifa_name, "lo") == 0) {
          continue;
        }

        char addr_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &addr->sin_addr, addr_str, sizeof(addr_str));

        // Skip if 0.0.0.0 or link-local (169.254.x.x)
        if (strcmp(addr_str, "0.0.0.0") == 0 ||
            strncmp(addr_str, "169.254.", 8) == 0) {
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

  bool LinuxNetwork::is_connected() {
    // Check if we have a valid IP address (not 0.0.0.0)
    std::string ip = this->get_ip_address();
    return !ip.empty() && ip != "0.0.0.0";
  }

  std::string LinuxNetwork::get_hostname() {
    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) == 0) {
      return std::string(hostname);
    }
    return "unknown";
  }

  }  // namespace network
  }  // namespace esphome

  #endif  // USE_LINUX
  ```

#### Integration with OTA and API

- [ ] **Update `esphome/components/ota/ota_component.h`**

  Add Linux socket integration:

  ```cpp
  #ifdef USE_LINUX
  #include "esphome/components/network/linux_network.h"

  protected:
    network::LinuxTCPServer *ota_server_{nullptr};
    network::LinuxTCPClient *ota_client_{nullptr};
  #endif
  ```

- [ ] **Update `esphome/components/ota/ota_component.cpp`**

  Implement Linux-specific OTA server loop:

  ```cpp
  void OTAComponent::setup() {
  #ifdef USE_LINUX
    this->ota_server_ = new network::LinuxTCPServer(this->port_);
    if (!this->ota_server_->begin()) {
      ESP_LOGE(TAG, "Failed to start OTA server on port %d", this->port_);
      this->mark_failed();
      return;
    }
  #endif
  }

  void OTAComponent::loop() {
  #ifdef USE_LINUX
    // Check for new connections
    if (this->ota_client_ == nullptr || !this->ota_client_->connected()) {
      int client_fd = this->ota_server_->accept_connection();
      if (client_fd >= 0) {
        this->ota_client_ = new network::LinuxTCPClient(client_fd);
        this->ota_client_->set_non_blocking(false);
        // Start OTA session
        this->handle_ota_connection_();
      }
    }
  #endif
  }
  ```

- [ ] **Similar updates for API component** (`esphome/components/api/`)

  API uses same pattern but on different port (6053)

#### Configuration Schema

- [ ] **Update `esphome/components/linux/__init__.py`**

  Add network configuration options:

  ```python
  CONF_NETWORK_INTERFACE = "network_interface"

  CONFIG_SCHEMA = cv.All(
      cv.Schema(
          {
              cv.Optional(CONF_PREFERENCES_PATH, default="/var/lib/esphome"): cv.string,
              cv.Optional(CONF_GPIO_CHIP, default="gpiochip0"): cv.string,
              cv.Optional(CONF_BINARY_PATH): cv.string,
              cv.Optional(CONF_NETWORK_INTERFACE): cv.string,  # Optional: auto-detect if not set
          }
      ),
      set_core_data,
  )
  ```

#### Testing

- [ ] **Create `tests/components/linux/test_network.linux.yaml`**

  ```yaml
  esphome:
    name: test_network_linux

  linux:
    preferences_path: /tmp/esphome-test

  logger:
    level: VERBOSE

  # Network component (implicit, auto-initialized)

  # Test OTA with network
  ota:
    - platform: esphome
      password: "test123"

  # Test API with network
  api:
    password: "test123"
  ```

- [ ] **Manual Testing Procedure**

  1. **Compile and run**:
     ```bash
     esphome compile tests/components/linux/test_network.linux.yaml
     .esphome/build/test_network_linux/test_network_linux
     ```

  2. **Verify network detection**:
     ```bash
     # Check logs show:
     # [network.linux] Network initialized
     # [network.linux]   Hostname: retropi
     # [network.linux]   IP Address: 192.168.1.100
     # [ota] OTA server listening on port 3232
     # [api] API server listening on port 6053
     ```

  3. **Test OTA connection**:
     ```bash
     # From development machine:
     esphome upload tests/components/linux/test_network.linux.yaml --device 192.168.1.100
     ```

  4. **Test API connection**:
     ```bash
     # From Home Assistant or esphome CLI:
     esphome logs tests/components/linux/test_network.linux.yaml --device 192.168.1.100
     ```

  5. **Test network resilience**:
     ```bash
     # Disconnect/reconnect network
     # Verify service recovers gracefully
     ```

### Implementation Phases

#### Phase 9.1: Core Socket Implementation (2 days)

- [ ] Implement LinuxTCPServer class
- [ ] Implement LinuxTCPClient class
- [ ] Implement network utilities (IP detection, hostname)
- [ ] Basic compilation and unit tests

#### Phase 9.2: OTA Integration (1 day)

- [ ] Update OTA component for Linux sockets
- [ ] Test OTA upload over network
- [ ] Verify version switching works with network updates

#### Phase 9.3: API Integration (1 day)

- [ ] Update API component for Linux sockets
- [ ] Test API connection from Home Assistant
- [ ] Test log streaming over API

#### Phase 9.4: Testing & Polish (1 day)

- [ ] Integration testing with all components
- [ ] Error handling and edge cases
- [ ] Performance testing (concurrent connections)
- [ ] Documentation updates

### Security Considerations

- [ ] **Firewall Configuration**

  Update systemd service documentation:
  ```bash
  # Allow OTA port
  sudo ufw allow 3232/tcp comment "ESPHome OTA"

  # Allow API port
  sudo ufw allow 6053/tcp comment "ESPHome API"
  ```

- [ ] **Password Protection**

  OTA and API already support password authentication

- [ ] **Network Interfaces**

  Bind to all interfaces (::) but document how to restrict:
  ```cpp
  // Future: bind to specific interface
  // inet_pton(AF_INET6, "fd00::1", &addr.sin6_addr);
  ```

### Error Handling

- [ ] **Port Already in Use**
  - Error: Cannot bind to port 3232
  - Solution: Check for conflicting services, change port in config

- [ ] **Network Unreachable**
  - Error: No network interfaces found
  - Solution: Wait for network-online.target, check systemd dependencies

- [ ] **Connection Timeout**
  - Error: Client connection dropped during OTA
  - Solution: Resume support (future), retry mechanism

- [ ] **DNS Resolution Failed**
  - Error: Cannot resolve hostname
  - Solution: Use IP address directly, check /etc/resolv.conf

### Deliverables

- [ ] **Network layer compiles** on x86_64 and ARM
- [ ] **TCP server works** - can accept connections on specified port
- [ ] **IP detection works** - correctly identifies primary IP address
- [ ] **OTA works over network** - can upload new binary via network
- [ ] **API works over network** - Home Assistant can connect
- [ ] **IPv4 and IPv6 supported** - works on both protocols
- [ ] **Error handling** - graceful failures with helpful messages
- [ ] **Documentation** - setup guide for firewall and network configuration

### Commit Strategy

```
feat(linux): add network support for OTA and API

Phase 9.1: Core socket implementation
- Implement LinuxTCPServer for listening on ports
- Implement LinuxTCPClient for connections
- Add network utilities (IP detection, hostname)
- Use POSIX sockets (socket, bind, listen, accept)

Phase 9.2: OTA integration
- Update OTA component to use Linux sockets
- Enable OTA uploads over network
- Test version switching with network updates

Phase 9.3: API integration
- Update API component to use Linux sockets
- Enable API connections from Home Assistant
- Support log streaming over API

Phase 9.4: Testing and polish
- Integration testing across components
- Error handling and edge cases
- Performance validation
- Documentation updates

Network support now functional on Linux platform.
OTA updates and API communication work over TCP.
```

### Future Enhancements (Optional)

- [ ] **mDNS Support**: Service discovery (mydevice.local)
- [ ] **UDP Support**: For faster protocols
- [ ] **TLS/SSL**: Encrypted OTA and API (using OpenSSL)
- [ ] **IPv6-only Mode**: For modern networks
- [ ] **Network Health Monitoring**: Automatic reconnection
- [ ] **Bandwidth Throttling**: Limit OTA transfer speed
- [ ] **Connection Pooling**: Reuse sockets for API
- [ ] **WebSocket Support**: For dashboard integration

### Known Limitations

1. **No WiFi Management**: Assumes network is already configured by OS
2. **No Connection State Tracking**: Doesn't detect network down/up events
3. **Single Client**: OTA/API serve one client at a time
4. **No TLS**: Plain TCP connections (passwords still required)
5. **IPv4 Preferred**: Falls back to IPv6 but prioritizes IPv4 for logging

---

**Next Steps**: Begin Phase 9.1 implementation with LinuxTCPServer class

**Blocked By**: None - can start immediately

**Blocks**: Full OTA and API functionality (currently non-functional without network)
