#pragma once

#ifdef USE_LINUX

#include "esphome/core/component.h"
#include "i2c_bus.h"

namespace esphome {
namespace i2c {

class LinuxI2CBus : public InternalI2CBus, public Component {
 public:
  void setup() override;
  void dump_config() override;
  ErrorCode write_readv(uint8_t address, const uint8_t *write_buffer, size_t write_count, uint8_t *read_buffer,
                        size_t read_count) override;
  float get_setup_priority() const override { return setup_priority::BUS; }

  void set_scan(bool scan) { this->scan_ = scan; }
  void set_bus_num(uint8_t bus_num) { this->bus_num_ = bus_num; }
  void set_frequency(uint32_t frequency) { this->frequency_ = frequency; }

  int get_port() const override { return this->bus_num_; }

 protected:
  int file_descriptor_{-1};
  uint8_t bus_num_{1};  // Default to /dev/i2c-1 (Raspberry Pi standard)
  uint32_t frequency_{100000};
};

}  // namespace i2c
}  // namespace esphome

#endif  // USE_LINUX
