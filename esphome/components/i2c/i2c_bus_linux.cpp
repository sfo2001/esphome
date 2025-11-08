#ifdef USE_LINUX

#include "i2c_bus_linux.h"
#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include <sys/ioctl.h>
#include <unistd.h>

namespace esphome {
namespace i2c {

static const char *const TAG = "i2c.linux";

void LinuxI2CBus::setup() {
  // Construct the device path
  char device_path[32];
  snprintf(device_path, sizeof(device_path), "/dev/i2c-%d", this->bus_num_);

  // Open the I2C device
  this->file_descriptor_ = open(device_path, O_RDWR);
  if (this->file_descriptor_ < 0) {
    ESP_LOGE(TAG, "Failed to open I2C bus %d (%s): %s", this->bus_num_, device_path, strerror(errno));
    ESP_LOGE(TAG, "Make sure:");
    ESP_LOGE(TAG, "  1. I2C is enabled (check /boot/config.txt or use raspi-config)");
    ESP_LOGE(TAG, "  2. Device file exists: %s", device_path);
    ESP_LOGE(TAG, "  3. User has permission (add user to 'i2c' group)");
    this->mark_failed();
    return;
  }

  ESP_LOGCONFIG(TAG, "I2C bus %d opened successfully on %s", this->bus_num_, device_path);

  if (this->scan_) {
    ESP_LOGV(TAG, "Scanning I2C bus for active devices");
    this->i2c_scan_();
  }
}

void LinuxI2CBus::dump_config() {
  ESP_LOGCONFIG(TAG, "I2C Bus:");
  ESP_LOGCONFIG(TAG, "  Bus Number: %d (/dev/i2c-%d)", this->bus_num_, this->bus_num_);
  ESP_LOGCONFIG(TAG, "  Frequency: %u Hz", this->frequency_);

  if (this->file_descriptor_ < 0) {
    ESP_LOGE(TAG, "  Status: FAILED - could not open I2C device");
  } else {
    ESP_LOGCONFIG(TAG, "  Status: OK");
  }

  if (this->scan_) {
    ESP_LOGI(TAG, "Results from I2C bus scan:");
    if (this->scan_results_.empty()) {
      ESP_LOGI(TAG, "  Found no I2C devices!");
    } else {
      for (const auto &result : this->scan_results_) {
        if (result.second) {
          ESP_LOGI(TAG, "  Found I2C device at address 0x%02X", result.first);
        }
      }
    }
  }
}

ErrorCode LinuxI2CBus::write_readv(uint8_t address, const uint8_t *write_buffer, size_t write_count,
                                    uint8_t *read_buffer, size_t read_count) {
  if (this->file_descriptor_ < 0) {
    ESP_LOGE(TAG, "I2C bus not initialized");
    return ERROR_NOT_INITIALIZED;
  }

  // Combined write-read transaction using I2C_RDWR ioctl
  // This performs an atomic write-read operation (write followed by read with repeated start)

  struct i2c_msg messages[2];
  int num_messages = 0;

  // Prepare write message if we have data to write
  if (write_count > 0 && write_buffer != nullptr) {
    messages[num_messages].addr = address;
    messages[num_messages].flags = 0;  // Write operation
    messages[num_messages].len = write_count;
    // i2c_msg expects non-const buffer pointer, but won't modify it for write operations
    messages[num_messages].buf = const_cast<uint8_t *>(write_buffer);
    num_messages++;
  }

  // Prepare read message if we have data to read
  if (read_count > 0 && read_buffer != nullptr) {
    messages[num_messages].addr = address;
    messages[num_messages].flags = I2C_M_RD;  // Read operation
    messages[num_messages].len = read_count;
    messages[num_messages].buf = read_buffer;
    num_messages++;
  }

  if (num_messages == 0) {
    // No operation requested
    return ERROR_OK;
  }

  // Perform the I2C transaction
  struct i2c_rdwr_ioctl_data ioctl_data;
  ioctl_data.msgs = messages;
  ioctl_data.nmsgs = num_messages;

  int result = ioctl(this->file_descriptor_, I2C_RDWR, &ioctl_data);
  if (result < 0) {
    // Map Linux errno to ESPHome I2C error codes
    ErrorCode error_code = ERROR_UNKNOWN;

    switch (errno) {
      case EREMOTEIO:  // Remote I/O error (device not responding)
      case ENXIO:      // No such device or address
        error_code = ERROR_NOT_ACKNOWLEDGED;
        ESP_LOGW(TAG, "I2C device at address 0x%02X not acknowledged (errno: %d - %s)", address, errno,
                 strerror(errno));
        break;

      case ETIMEDOUT:  // Timeout
      case EAGAIN:     // Resource temporarily unavailable
        error_code = ERROR_TIMEOUT;
        ESP_LOGW(TAG, "I2C timeout communicating with device at address 0x%02X (errno: %d - %s)", address, errno,
                 strerror(errno));
        break;

      case EINVAL:  // Invalid argument
        error_code = ERROR_INVALID_ARGUMENT;
        ESP_LOGE(TAG, "Invalid I2C transaction parameters for device at address 0x%02X (errno: %d - %s)", address,
                 errno, strerror(errno));
        break;

      case EMSGSIZE:  // Message too long
        error_code = ERROR_TOO_LARGE;
        ESP_LOGE(TAG, "I2C message too large for device at address 0x%02X (errno: %d - %s)", address, errno,
                 strerror(errno));
        break;

      default:
        error_code = ERROR_UNKNOWN;
        ESP_LOGE(TAG, "I2C error communicating with device at address 0x%02X (errno: %d - %s)", address, errno,
                 strerror(errno));
        break;
    }

    return error_code;
  }

  ESP_LOGVV(TAG, "I2C transaction successful: wrote %zu bytes, read %zu bytes from device 0x%02X", write_count,
            read_count, address);

  return ERROR_OK;
}

}  // namespace i2c
}  // namespace esphome

#endif  // USE_LINUX
