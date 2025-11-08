#pragma once

#ifdef USE_LINUX

#include "esphome/core/component.h"
#include "spi.h"

namespace esphome {
namespace spi {

class LinuxSPIDelegate;

class LinuxSPIBus : public SPIBus, public Component {
 public:
  void setup() override;
  void dump_config() override;
  ~LinuxSPIBus();

  SPIDelegate *get_delegate(uint32_t data_rate, SPIBitOrder bit_order, SPIMode mode, GPIOPin *cs_pin,
                            bool release_device, bool write_only) override;
  bool is_hw() override { return true; }

  float get_setup_priority() const override { return setup_priority::BUS; }

  void set_bus_num(uint8_t bus_num) { this->bus_num_ = bus_num; }
  void set_device_num(uint8_t device_num) { this->device_num_ = device_num; }

 protected:
  int file_descriptor_{-1};
  uint8_t bus_num_{0};      // Default to /dev/spidev0.0
  uint8_t device_num_{0};   // Default to /dev/spidev0.0
  uint8_t bits_per_word_{8};

  friend class LinuxSPIDelegate;
};

class LinuxSPIDelegate : public SPIDelegate {
 public:
  LinuxSPIDelegate(LinuxSPIBus *parent, uint32_t data_rate, SPIBitOrder bit_order, SPIMode mode, GPIOPin *cs_pin);
  ~LinuxSPIDelegate() override;

  uint8_t transfer(uint8_t data) override;
  void transfer(uint8_t *ptr, size_t length) override;
  void transfer(const uint8_t *txbuf, uint8_t *rxbuf, size_t length) override;

  void begin_transaction() override;
  void end_transaction() override;

 protected:
  LinuxSPIBus *parent_{nullptr};
  int fd_{-1};
  uint32_t speed_{1000000};
  uint8_t spi_mode_{0};
  bool transaction_active_{false};
};

}  // namespace spi
}  // namespace esphome

#endif  // USE_LINUX
