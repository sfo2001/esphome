#pragma once

#ifdef USE_LINUX

#include "esphome/core/hal.h"
#include <gpiod.h>
#include <string>

namespace esphome {
namespace esphome_linux {

/// @brief GPIO pin implementation for Linux using libgpiod
class LinuxGPIOPin : public InternalGPIOPin {
 public:
  void set_pin(uint8_t pin) { this->pin_ = pin; }
  void set_inverted(bool inverted) { this->inverted_ = inverted; }
  void set_flags(gpio::Flags flags) { this->flags_ = flags; }
  void set_chip_name(const std::string &chip_name) { this->chip_name_ = chip_name; }

  void setup() override;
  void pin_mode(gpio::Flags flags) override;
  bool digital_read() override;
  void digital_write(bool value) override;
  std::string dump_summary() const override;
  void detach_interrupt() const override;
  ISRInternalGPIOPin to_isr() const override;

  uint8_t get_pin() const { return this->pin_; }
  gpio::Flags get_flags() const override { return this->flags_; }
  bool is_inverted() const override { return this->inverted_; }

  ~LinuxGPIOPin();

 protected:
  void attach_interrupt(void (*func)(void *), void *arg, gpio::InterruptType type) const override;

  /// Open the GPIO chip and get the line handle
  bool open_chip_();
  /// Release the GPIO line if it's currently requested
  void release_line_();

  struct gpiod_chip *chip_{nullptr};
  struct gpiod_line *line_{nullptr};
  uint8_t pin_{0};
  bool inverted_{false};
  gpio::Flags flags_{gpio::FLAG_NONE};
  std::string chip_name_{"gpiochip0"};
  bool line_requested_{false};
};

}  // namespace esphome_linux
}  // namespace esphome

#endif  // USE_LINUX
