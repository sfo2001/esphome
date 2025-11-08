# Phase 8: OTA Updates Implementation

[← Back to README](../README.md) | [Roadmap](../ROADMAP.md)

**Status**: 📝 Architecture Designed - Ready to Implement

## Phase 8: OTA Updates Implementation

**Goal**: Enable over-the-air firmware updates for Linux platform
**Duration**: 2-3 days
**Compile**: AMD server (x86_64) and Raspberry Pi (ARM)
**Priority**: HIGH (enables remote updates without physical access)

### Architecture Overview

#### OTA Update Strategy: Versioned Binaries with Symlink

Unlike ESP platforms with dual flash partitions, Linux uses a versioned binary approach with atomic symlink switching.

**Directory Structure:**
```
/usr/local/bin/
  ├── mydevice -> mydevice.002 (symlink - points to current version)
  ├── mydevice.001 (previous version - kept for rollback)
  ├── mydevice.002 (currently running version)
  └── mydevice.003 (new version being written during OTA)
```

**Systemd Service** runs the symlink:
```ini
[Unit]
Description=ESPHome Device: MyDevice
After=network-online.target

[Service]
Type=simple
ExecStart=/usr/local/bin/mydevice
Restart=always
RestartSec=5s
User=esphome
SupplementaryGroups=i2c gpio spi
ReadWritePaths=/usr/local/bin

[Install]
WantedBy=multi-user.target
```

#### Update Process Flow

```
┌─────────────────────────────────────────────────────────────┐
│ 1. OTA Client connects to port 3232                         │
│    (Same protocol as ESP32/ESP8266)                         │
└─────────────────────────────────────────────────────────────┘
                          │
                          ▼
┌─────────────────────────────────────────────────────────────┐
│ 2. LinuxOTABackend::begin()                                 │
│    - Read symlink: mydevice -> mydevice.002                 │
│    - Parse version: .002                                    │
│    - Next version: .003                                     │
│    - Open file: /usr/local/bin/mydevice.003                 │
└─────────────────────────────────────────────────────────────┘
                          │
                          ▼
┌─────────────────────────────────────────────────────────────┐
│ 3. LinuxOTABackend::write() (called multiple times)         │
│    - Write binary data to mydevice.003                      │
│    - Update MD5 hash                                        │
└─────────────────────────────────────────────────────────────┘
                          │
                          ▼
┌─────────────────────────────────────────────────────────────┐
│ 4. LinuxOTABackend::end()                                   │
│    - Verify MD5 checksum                                    │
│    - chmod +x mydevice.003                                  │
│    - sync (ensure data written to disk)                     │
│    - ln -sf mydevice.003 mydevice (atomic switch)           │
│    - Optional: rm mydevice.001 (cleanup old versions)       │
│    - exit(0) - terminate current process                    │
└─────────────────────────────────────────────────────────────┘
                          │
                          ▼
┌─────────────────────────────────────────────────────────────┐
│ 5. Systemd automatically restarts service                   │
│    - Follows symlink to mydevice.003                        │
│    - New version starts running                             │
└─────────────────────────────────────────────────────────────┘
```

#### Advantages of This Approach

- ✅ **Atomic Updates**: Symlink change is atomic operation
- ✅ **Version History**: Keep last 2-3 versions for rollback
- ✅ **Simple Architecture**: Binary is the daemon (no wrapper needed)
- ✅ **Automatic Restart**: Systemd handles lifecycle
- ✅ **Manual Rollback**: `ln -sf mydevice.002 mydevice && systemctl restart`
- ✅ **Same Protocol**: Uses existing ESPHome OTA protocol (port 3232)
- ✅ **Debugging**: Know which version is running from filename

#### Comparison with ESP Platform

| Feature | ESP32 (Dual Partition) | Linux (Versioned Symlink) |
|---------|------------------------|---------------------------|
| Storage | Flash partitions | File system |
| Update Target | Inactive partition | New versioned file |
| Activation | Set boot partition + restart | Update symlink + restart |
| Rollback | Auto (boot old partition on fail) | Manual (change symlink) |
| Version History | 2 versions (current + update) | Configurable (keep N versions) |
| Disk Usage | Fixed (2× firmware size) | Grows (N× binary size) |

### Implementation Tasks

#### Core OTA Backend

- [ ] **Create `esphome/components/ota/ota_backend_linux.h`** (~100 LOC)

  ```cpp
  #pragma once
  #ifdef USE_LINUX
  #include "ota_backend.h"
  #include "esphome/components/md5/md5.h"
  #include <fstream>

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
    void cleanup_old_versions_(int keep_count = 2);

    std::string binary_path_;      // e.g., /usr/local/bin/mydevice
    std::string new_version_path_; // e.g., /usr/local/bin/mydevice.003
    std::ofstream file_stream_;
    md5::MD5Digest md5_;
    char expected_md5_[32];
    bool md5_set_{false};
  };

  }  // namespace ota
  }  // namespace esphome
  #endif
  ```

- [ ] **Create `esphome/components/ota/ota_backend_linux.cpp`** (~350 LOC)

  **Key Functions:**

  ```cpp
  LinuxOTABackend::LinuxOTABackend() {
    // Read /proc/self/exe to determine current binary path
    char path[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", path, sizeof(path) - 1);
    if (len != -1) {
      path[len] = '\0';
      this->binary_path_ = path;

      // If it's a symlink (e.g., mydevice -> mydevice.002),
      // resolve to the directory and symlink name
      // Store as base path for version management
    }
  }

  std::string LinuxOTABackend::get_current_version_() {
    // Read symlink to determine current version
    // /usr/local/bin/mydevice -> mydevice.002
    // Parse and return version number (002)

    char link[PATH_MAX];
    ssize_t len = readlink(this->binary_path_.c_str(), link, sizeof(link) - 1);
    if (len == -1) {
      return "000";  // No symlink, first version
    }
    link[len] = '\0';

    // Parse version number from filename (e.g., "mydevice.002" -> 2)
    std::string target(link);
    size_t dot_pos = target.rfind('.');
    if (dot_pos != std::string::npos) {
      std::string version_str = target.substr(dot_pos + 1);
      return version_str;
    }
    return "000";
  }

  std::string LinuxOTABackend::get_next_version_path_() {
    std::string current_version = this->get_current_version_();
    int version_num = std::stoi(current_version);
    int next_version = version_num + 1;

    // Format as 3-digit version (001, 002, etc.)
    char version_buf[16];
    snprintf(version_buf, sizeof(version_buf), "%03d", next_version);

    // Get base path without symlink
    // If binary_path is /usr/local/bin/mydevice
    // Return /usr/local/bin/mydevice.003
    return this->binary_path_ + "." + version_buf;
  }

  OTAResponseTypes LinuxOTABackend::begin(size_t image_size) {
    this->new_version_path_ = this->get_next_version_path_();

    ESP_LOGI(TAG, "Starting OTA update, new version: %s",
             this->new_version_path_.c_str());

    // Open file for writing
    this->file_stream_.open(this->new_version_path_,
                            std::ios::binary | std::ios::trunc);
    if (!this->file_stream_) {
      ESP_LOGE(TAG, "Failed to open file for writing: %s (errno: %d)",
               this->new_version_path_.c_str(), errno);
      if (errno == EACCES || errno == EPERM) {
        return OTA_RESPONSE_ERROR_WRITING_FLASH;  // Permission denied
      }
      return OTA_RESPONSE_ERROR_UNKNOWN;
    }

    this->md5_.init();
    return OTA_RESPONSE_OK;
  }

  OTAResponseTypes LinuxOTABackend::write(uint8_t *data, size_t len) {
    this->file_stream_.write(reinterpret_cast<char *>(data), len);
    this->md5_.add(data, len);

    if (!this->file_stream_) {
      ESP_LOGE(TAG, "Failed to write OTA data");
      return OTA_RESPONSE_ERROR_WRITING_FLASH;
    }
    return OTA_RESPONSE_OK;
  }

  OTAResponseTypes LinuxOTABackend::end() {
    this->file_stream_.close();

    // Verify MD5
    if (this->md5_set_) {
      this->md5_.calculate();
      if (!this->md5_.equals_hex(this->expected_md5_)) {
        ESP_LOGE(TAG, "MD5 mismatch! Aborting OTA.");
        unlink(this->new_version_path_.c_str());
        return OTA_RESPONSE_ERROR_MD5_MISMATCH;
      }
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

    ESP_LOGI(TAG, "OTA update successful! Symlink updated to: %s",
             new_version_basename.c_str());

    // Cleanup old versions (keep last 2)
    this->cleanup_old_versions_(2);

    // Schedule exit so systemd restarts us with new version
    // Delay to allow OTA response to be sent
    App.schedule_exit();

    return OTA_RESPONSE_OK;
  }

  void LinuxOTABackend::abort() {
    this->file_stream_.close();
    if (!this->new_version_path_.empty()) {
      unlink(this->new_version_path_.c_str());
    }
  }

  void LinuxOTABackend::cleanup_old_versions_(int keep_count) {
    // List all versioned binaries in directory
    // Sort by version number
    // Delete oldest ones, keeping last 'keep_count' versions
    // This prevents disk space from growing indefinitely

    // Implementation details:
    // - Get directory from binary_path_
    // - opendir() and readdir() to list files
    // - Filter files matching pattern: mydevice.\d{3}
    // - Sort by version number
    // - Keep newest keep_count, delete rest
  }
  ```

- [ ] **Update `esphome/components/ota/__init__.py`**

  Add Linux to platform file filtering:

  ```python
  FILTER_SOURCE_FILES = filter_source_files_from_platform(
      {
          "ota_backend_arduino_esp32.cpp": {PlatformFramework.ESP32_ARDUINO},
          "ota_backend_esp_idf.cpp": {PlatformFramework.ESP32_IDF},
          "ota_backend_arduino_esp8266.cpp": {PlatformFramework.ESP8266_ARDUINO},
          "ota_backend_arduino_rp2040.cpp": {PlatformFramework.RP2040_ARDUINO},
          "ota_backend_arduino_libretiny.cpp": {
              PlatformFramework.BK72XX_ARDUINO,
              PlatformFramework.RTL87XX_ARDUINO,
              PlatformFramework.LN882X_ARDUINO,
          },
          "ota_backend_linux.cpp": {PlatformFramework.LINUX_NATIVE},  # ADD THIS
      }
  )
  ```

- [ ] **Update `esphome/components/linux/__init__.py`**

  Add binary path configuration:

  ```python
  CONF_BINARY_PATH = "binary_path"

  CONFIG_SCHEMA = cv.All(
      cv.Schema(
          {
              cv.Optional(CONF_PREFERENCES_PATH, default="/var/lib/esphome"): cv.string,
              cv.Optional(CONF_GPIO_CHIP, default="gpiochip0"): cv.string,
              cv.Optional(CONF_BINARY_PATH): cv.string,  # Optional, auto-detect if not set
          }
      ),
      set_core_data,
  )

  async def to_code(config):
      # ... existing code ...

      # Add binary path for OTA backend
      if CONF_BINARY_PATH in config:
          cg.add_define("ESPHOME_BINARY_PATH", config[CONF_BINARY_PATH])
  ```

#### Deployment Helpers

- [ ] **Create `scripts/linux/generate-systemd-service.py`** (~150 LOC)

  Script to generate systemd service file from ESPHome configuration:

  ```python
  #!/usr/bin/env python3
  """Generate systemd service file for ESPHome Linux binary."""

  import argparse
  import sys

  TEMPLATE = """[Unit]
  Description=ESPHome Device: {device_name}
  After=network-online.target
  Wants=network-online.target

  [Service]
  Type=simple
  ExecStart={binary_path}
  Restart=always
  RestartSec=5s
  StartLimitBurst=5
  StartLimitIntervalSec=60s

  # User and groups
  User={user}
  Group={group}
  SupplementaryGroups={supplementary_groups}

  # Allow binary updates
  ReadWritePaths={binary_dir}

  # Security hardening
  NoNewPrivileges=true
  PrivateTmp=true

  # Logging
  StandardOutput=journal
  StandardError=journal
  SyslogIdentifier={device_name}

  [Install]
  WantedBy=multi-user.target
  """

  def generate_service(device_name, binary_path, user="esphome",
                       group="esphome", hardware_groups=None):
      if hardware_groups is None:
          hardware_groups = ["i2c", "gpio", "spi"]

      binary_dir = os.path.dirname(binary_path)

      return TEMPLATE.format(
          device_name=device_name,
          binary_path=binary_path,
          user=user,
          group=group,
          supplementary_groups=" ".join(hardware_groups),
          binary_dir=binary_dir,
      )
  ```

- [ ] **Create `scripts/linux/install-service.sh`** (~100 LOC)

  Installation script:

  ```bash
  #!/bin/bash
  # Install ESPHome Linux binary as systemd service

  set -e

  DEVICE_NAME="$1"
  BINARY_PATH="$2"

  if [ -z "$DEVICE_NAME" ] || [ -z "$BINARY_PATH" ]; then
      echo "Usage: $0 <device-name> <binary-path>"
      echo "Example: $0 mydevice /usr/local/bin/mydevice"
      exit 1
  fi

  # Create esphome user if doesn't exist
  if ! id esphome &>/dev/null; then
      sudo useradd -r -s /bin/false esphome
  fi

  # Add to hardware groups
  sudo usermod -aG i2c,gpio,spi esphome

  # Install binary
  sudo install -m 0755 "$BINARY_PATH" /usr/local/bin/

  # Create initial symlink (mydevice -> mydevice.001)
  BINARY_NAME=$(basename "$BINARY_PATH")
  sudo ln -sf "${BINARY_NAME}.001" "/usr/local/bin/${BINARY_NAME}"

  # Generate and install service file
  python3 generate-systemd-service.py "$DEVICE_NAME" "/usr/local/bin/${BINARY_NAME}" \
      | sudo tee "/etc/systemd/system/esphome-${DEVICE_NAME}.service"

  # Reload systemd and enable service
  sudo systemctl daemon-reload
  sudo systemctl enable "esphome-${DEVICE_NAME}.service"
  sudo systemctl start "esphome-${DEVICE_NAME}.service"

  echo "Service installed and started!"
  echo "Check status: sudo systemctl status esphome-${DEVICE_NAME}"
  echo "View logs: sudo journalctl -u esphome-${DEVICE_NAME} -f"
  ```

#### Testing

- [ ] **Create `tests/components/linux/test_ota.linux.yaml`**

  ```yaml
  esphome:
    name: test_ota_linux

  linux:
    preferences_path: /tmp/esphome-test
    binary_path: /tmp/esphome-test/test_ota_linux

  logger:
    level: VERBOSE

  # Enable OTA
  ota:
    - platform: esphome
      password: "test123"

  # Enable API for testing
  api:
    password: "test123"
  ```

- [ ] **Manual Testing Procedure**

  1. **Initial deployment**:
     ```bash
     # Compile test binary
     esphome compile tests/components/linux/test_ota.linux.yaml

     # Install as .001
     cp .esphome/build/test_ota_linux/test_ota_linux /tmp/test_ota_linux.001
     chmod +x /tmp/test_ota_linux.001
     ln -s test_ota_linux.001 /tmp/test_ota_linux

     # Run
     /tmp/test_ota_linux
     ```

  2. **OTA Update test**:
     ```bash
     # Make a small change to YAML (e.g., change log level)
     # Recompile
     esphome compile tests/components/linux/test_ota.linux.yaml

     # Upload via OTA
     esphome upload tests/components/linux/test_ota.linux.yaml
     ```

  3. **Verify update**:
     ```bash
     # Check symlink points to new version
     ls -la /tmp/test_ota_linux*
     # Should show: test_ota_linux -> test_ota_linux.002

     # Check both versions exist
     # test_ota_linux.001 (old)
     # test_ota_linux.002 (new, running)
     ```

  4. **Rollback test**:
     ```bash
     # Simulate failed update by reverting symlink
     ln -sf test_ota_linux.001 /tmp/test_ota_linux

     # Restart would now run old version
     ```

  5. **Cleanup test**:
     ```bash
     # Do several OTA updates
     # Verify old versions are deleted (keeping last 2)
     ls -la /tmp/test_ota_linux*
     # Should only see .002 and .003 after multiple updates
     ```

#### Security Considerations

- [ ] **File Permissions**

  ```yaml
  # In systemd service:
  ReadWritePaths=/usr/local/bin  # Allow writes to binary directory
  User=esphome                   # Run as non-root user
  ```

  Risks:
  - User `esphome` needs write access to `/usr/local/bin`
  - Mitigated by: NoNewPrivileges, PrivateTmp, limited SupplementaryGroups

- [ ] **MD5 Verification**

  Always verify MD5 before applying update (same as ESP platforms)

- [ ] **Atomic Operations**

  Use symlink rename for atomic switching (no partial updates)

- [ ] **Disk Space**

  Cleanup old versions to prevent disk exhaustion

#### Error Handling

- [ ] **Permission Denied**
  - Error: Cannot write to `/usr/local/bin/mydevice.003`
  - Solution: Check ReadWritePaths in systemd service, verify user permissions

- [ ] **MD5 Mismatch**
  - Error: Checksum verification failed
  - Solution: Abort update, delete partial file, log error

- [ ] **Symlink Creation Failed**
  - Error: Cannot create/update symlink
  - Solution: Cleanup temp files, return error, keep running on current version

- [ ] **Disk Full**
  - Error: Cannot write complete binary
  - Solution: Check available space before begin(), cleanup old versions proactively

### Deliverables

- [ ] **LinuxOTABackend compiles** on x86_64 and ARM
- [ ] **OTA protocol works** - can receive binary over network
- [ ] **Version management works** - increments version numbers correctly
- [ ] **Symlink switching works** - atomic updates
- [ ] **Systemd integration works** - automatic restart with new version
- [ ] **Cleanup works** - old versions deleted
- [ ] **Manual rollback works** - can revert to previous version
- [ ] **Error handling** - graceful failures, helpful error messages
- [ ] **Installation scripts** - easy deployment
- [ ] **Documentation** - user guide for OTA updates on Linux

### Commit

```
feat(linux): add OTA update support with versioned binaries

- Implement LinuxOTABackend for file-based updates
- Use versioned binaries with symlink switching (.001, .002, etc.)
- Atomic updates via symlink rename
- Keep last N versions for rollback capability
- Automatic cleanup of old versions
- Integration with systemd for auto-restart
- Add installation scripts and systemd service templates
- Test on x86_64 and Raspberry Pi

OTA updates now work on Linux platform with version management.
```

### Future Enhancements (Optional)

- [ ] **Automatic Rollback**: Watchdog detects unhealthy new version, auto-reverts
- [ ] **Differential Updates**: Only transmit changed sections (save bandwidth)
- [ ] **Compression**: Support compressed binary uploads
- [ ] **A/B Testing**: Run new version in parallel, switch if healthy
- [ ] **Update Scheduling**: Schedule updates for specific times
- [ ] **Multi-device Updates**: Update multiple Pi devices in sequence

---

