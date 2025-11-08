#include "gpio.h"

#ifdef USE_LINUX

#include "esphome/core/log.h"
#include <cerrno>
#include <cstring>

namespace esphome {
namespace esphome_linux {

static const char *const TAG = "linux.gpio";

LinuxGPIOPin::~LinuxGPIOPin() {
  this->release_line_();
  if (this->chip_ != nullptr) {
    gpiod_chip_close(this->chip_);
    this->chip_ = nullptr;
  }
}

bool LinuxGPIOPin::open_chip_() {
  if (this->chip_ != nullptr) {
    return true;  // Already open
  }

  this->chip_ = gpiod_chip_open_by_name(this->chip_name_.c_str());
  if (this->chip_ == nullptr) {
    ESP_LOGE(TAG, "Failed to open GPIO chip '%s': %s", this->chip_name_.c_str(), strerror(errno));
    if (errno == ENOENT) {
      ESP_LOGE(TAG, "Chip not found. Available chips can be listed with 'gpiodetect' command");
    } else if (errno == EACCES) {
      ESP_LOGE(TAG, "Permission denied. Add your user to the 'gpio' group: sudo usermod -aG gpio $USER");
    }
    return false;
  }

  // Get the GPIO line
  this->line_ = gpiod_chip_get_line(this->chip_, this->pin_);
  if (this->line_ == nullptr) {
    ESP_LOGE(TAG, "Failed to get GPIO line %u from chip '%s': %s", this->pin_, this->chip_name_.c_str(),
             strerror(errno));
    gpiod_chip_close(this->chip_);
    this->chip_ = nullptr;
    return false;
  }

  return true;
}

void LinuxGPIOPin::release_line_() {
  if (this->line_requested_ && this->line_ != nullptr) {
    gpiod_line_release(this->line_);
    this->line_requested_ = false;
  }
}

void LinuxGPIOPin::setup() {
  if (!this->open_chip_()) {
    this->mark_failed();
    return;
  }

  // Check if line is available
  if (gpiod_line_is_used(this->line_)) {
    ESP_LOGW(TAG, "GPIO%u is already in use by '%s'", this->pin_, gpiod_line_consumer(this->line_));
  }

  // Apply the initial pin mode
  this->pin_mode(this->flags_);
}

void LinuxGPIOPin::pin_mode(gpio::Flags flags) {
  // Release line if already requested
  this->release_line_();

  this->flags_ = flags;

  if (this->line_ == nullptr) {
    ESP_LOGE(TAG, "Cannot set pin mode: GPIO line not initialized");
    return;
  }

  const char *consumer = "esphome";
  int ret = 0;

  if (flags & gpio::FLAG_OUTPUT) {
    // Output mode
    int default_value = this->inverted_ ? 1 : 0;
    ret = gpiod_line_request_output(this->line_, consumer, default_value);
    if (ret < 0) {
      ESP_LOGE(TAG, "Failed to request GPIO%u as output: %s", this->pin_, strerror(errno));
      return;
    }
    ESP_LOGV(TAG, "GPIO%u configured as OUTPUT (default: %d)", this->pin_, default_value);
  } else if (flags & gpio::FLAG_INPUT) {
    // Input mode - determine request flags
    int request_flags = 0;

    if (flags & gpio::FLAG_PULLUP) {
      request_flags |= GPIOD_LINE_REQUEST_FLAG_BIAS_PULL_UP;
      ESP_LOGV(TAG, "GPIO%u configured as INPUT with PULL-UP", this->pin_);
    } else if (flags & gpio::FLAG_PULLDOWN) {
      request_flags |= GPIOD_LINE_REQUEST_FLAG_BIAS_PULL_DOWN;
      ESP_LOGV(TAG, "GPIO%u configured as INPUT with PULL-DOWN", this->pin_);
    } else {
      ESP_LOGV(TAG, "GPIO%u configured as INPUT (no pull resistor)", this->pin_);
    }

    if (request_flags != 0) {
      ret = gpiod_line_request_input_flags(this->line_, consumer, request_flags);
    } else {
      ret = gpiod_line_request_input(this->line_, consumer);
    }

    if (ret < 0) {
      ESP_LOGE(TAG, "Failed to request GPIO%u as input: %s", this->pin_, strerror(errno));
      return;
    }
  }

  this->line_requested_ = true;
}

bool LinuxGPIOPin::digital_read() {
  if (this->line_ == nullptr || !this->line_requested_) {
    ESP_LOGE(TAG, "Cannot read GPIO%u: line not requested", this->pin_);
    return false;
  }

  int value = gpiod_line_get_value(this->line_);
  if (value < 0) {
    ESP_LOGE(TAG, "Failed to read GPIO%u: %s", this->pin_, strerror(errno));
    return false;
  }

  // Apply inversion if needed
  bool result = value != 0;
  if (this->inverted_) {
    result = !result;
  }

  return result;
}

void LinuxGPIOPin::digital_write(bool value) {
  if (this->line_ == nullptr || !this->line_requested_) {
    ESP_LOGE(TAG, "Cannot write GPIO%u: line not requested", this->pin_);
    return;
  }

  // Apply inversion if needed
  if (this->inverted_) {
    value = !value;
  }

  int ret = gpiod_line_set_value(this->line_, value ? 1 : 0);
  if (ret < 0) {
    ESP_LOGE(TAG, "Failed to write GPIO%u: %s", this->pin_, strerror(errno));
  }
}

std::string LinuxGPIOPin::dump_summary() {
  char buffer[64];
  snprintf(buffer, sizeof(buffer), "GPIO%u (%s)", this->pin_, this->chip_name_.c_str());
  return std::string(buffer);
}

void LinuxGPIOPin::detach_interrupt() {
  // Interrupts not yet implemented for Linux platform
  ESP_LOGW(TAG, "GPIO interrupts not yet implemented on Linux platform");
}

ISRInternalGPIOPin LinuxGPIOPin::to_isr() const {
  // Interrupts not yet implemented for Linux platform
  return ISRInternalGPIOPin(nullptr);
}

}  // namespace esphome_linux
}  // namespace esphome

#endif  // USE_LINUX
