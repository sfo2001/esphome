# Phase 3: I2C Implementation

[← Previous Phase](completed/phase-2-preferences.md) | [← Back to README](../README.md) | [→ Next Phase](phase-4-gpio.md) | [Roadmap](../ROADMAP.md)

**Status**: 🔧 Code Complete - Hardware Testing Pending

## Phase 3: I2C Implementation (REQUIRES HARDWARE)

**Goal**: Real I2C hardware support for Raspberry Pi
**Duration**: 2-3 days
**Compile**: Raspberry Pi 5 (ARM native)
**Priority**: HIGHEST (required for your use case)

### Pre-requisites

#### Hardware Setup on Raspberry Pi 5

- [ ] **Install required packages**
  ```bash
  sudo apt-get update
  sudo apt-get install -y i2c-tools libi2c-dev
  ```

- [ ] **Enable I2C interface**
  ```bash
  sudo raspi-config
  # Interface Options -> I2C -> Enable
  # Or add to /boot/config.txt: dtparam=i2c_arm=on
  sudo reboot
  ```

- [ ] **Verify I2C bus available**
  ```bash
  ls -l /dev/i2c-*
  # Should show /dev/i2c-1
  ```

- [ ] **Add user to i2c group**
  ```bash
  sudo usermod -aG i2c $USER
  # Log out and back in for group to take effect
  ```

- [ ] **Connect I2C test device**
  - [ ] Device connected to GPIO 2 (SDA) and GPIO 3 (SCL)
  - [ ] Power and ground connected
  - [ ] Test with i2cdetect:
    ```bash
    i2cdetect -y 1
    # Should show device address (e.g., 0x48 for ADS1115)
    ```

#### Development Environment on Pi 5

- [ ] **Clone repository**
  ```bash
  mkdir ~/esphome-dev
  cd ~/esphome-dev
  git clone <your-fork-url> .
  git checkout feature/linux-platform
  ```

- [ ] **Setup Python environment**
  ```bash
  python3 -m venv venv
  source venv/bin/activate
  pip install --upgrade pip
  pip install -e .
  pip install -r requirements_dev.txt
  ```

- [ ] **Verify ESPHome runs**
  ```bash
  esphome version
  ```

### Implementation Tasks

- [ ] **Create `esphome/components/i2c/i2c_bus_linux.h`** (~80 LOC)
  - [ ] `LinuxI2CBus` class extending `InternalI2CBus`
  - [ ] Member variables:
    - [ ] `int file_descriptor_` - fd for `/dev/i2c-X`
    - [ ] `uint8_t bus_num_` - I2C bus number (default: 1)
    - [ ] `uint32_t frequency_` - bus frequency
  - [ ] Method signatures:
    - [ ] `void setup() override`
    - [ ] `void dump_config() override`
    - [ ] `ErrorCode write_readv(...) override`
    - [ ] `int get_port() const override`
    - [ ] `void set_bus_num(uint8_t bus_num)`
    - [ ] `void set_frequency(uint32_t frequency)`

- [ ] **Create `esphome/components/i2c/i2c_bus_linux.cpp`** (~300 LOC)
  - [ ] Include required headers:
    ```cpp
    #include <linux/i2c-dev.h>
    #include <sys/ioctl.h>
    #include <fcntl.h>
    #include <unistd.h>
    #include <cstring>
    #include <cerrno>
    ```

  - [ ] **Implement `setup()`**:
    - [ ] Open `/dev/i2c-{bus_num}`
    - [ ] Store file descriptor
    - [ ] Handle errors (file not found, permission denied)
    - [ ] Log successful initialization

  - [ ] **Implement `write_readv()`**:
    - [ ] Use `I2C_RDWR` ioctl with `i2c_msg` structures
    - [ ] Create message array for combined write-read transaction
    - [ ] Map Linux errno to `i2c::ErrorCode`
    - [ ] Add detailed error logging

  - [ ] **Implement `dump_config()`**:
    - [ ] Log bus number, frequency, status

  - [ ] **Add destructor**:
    - [ ] Close file descriptor if open

- [ ] **Update `esphome/components/i2c/__init__.py`**
  - [ ] Import `LinuxI2CBus`:
    ```python
    from esphome.components.i2c.i2c_bus_linux import LinuxI2CBus
    ```

  - [ ] Update `_bus_declare_type()` function:
    ```python
    def _bus_declare_type(value):
        if CORE.using_arduino:
            return cv.declare_id(ArduinoI2CBus)(value)
        if CORE.using_esp_idf:
            return cv.declare_id(IDFI2CBus)(value)
        if CORE.using_zephyr:
            return cv.declare_id(ZephyrI2CBus)(value)
        if CORE.is_linux:  # ADD THIS
            return cv.declare_id(LinuxI2CBus)(value)
        raise NotImplementedError
    ```

  - [ ] Update `FILTER_SOURCE_FILES`:
    ```python
    FILTER_SOURCE_FILES = filter_source_files_from_platform(
        {
            # ... existing entries ...
            "i2c_bus_linux.cpp": {PlatformFramework.LINUX_NATIVE},
        }
    )
    ```

  - [ ] Update `CONFIG_SCHEMA` for Linux:
    - [ ] Add `bus_num` configuration option
    - [ ] Keep existing `sda`/`scl` for GPIO reference (documentation only on Linux)

- [ ] **Create `tests/test_build_components/common/i2c/linux.yaml`**
  ```yaml
  packages:
    i2c: !include ../../../packages/test_common_i2c.yaml

  i2c:
    id: i2c_bus
    bus_num: 1  # /dev/i2c-1 on Raspberry Pi
    frequency: 100kHz
    scan: true
  ```

#### Testing

- [ ] **Create test configuration for I2C sensor**
  ```yaml
  esphome:
    name: test_i2c_ads1115
    platform: linux

  linux:

  logger:
    level: VERBOSE

  i2c:
    bus_num: 1
    scan: true
    frequency: 100kHz

  sensor:
    - platform: ads1115
      address: 0x48
      continuous_mode: false
      id: ads_sensor

      # Test all channels
      A0:
        name: "ADS1115 Channel A0"
        update_interval: 5s
      A1:
        name: "ADS1115 Channel A1"
        update_interval: 5s
  ```

- [ ] **Compile on Pi 5**
  ```bash
  cd ~/esphome-dev
  source venv/bin/activate
  esphome compile test-configs/test_i2c_ads1115.yaml
  ```

- [ ] **Run with I2C device connected**
  ```bash
  # May need sudo if not in i2c group
  sudo ./.esphome/build/test_i2c_ads1115/test_i2c_ads1115

  # Or if in i2c group:
  ./.esphome/build/test_i2c_ads1115/test_i2c_ads1115
  ```

- [ ] **Verify I2C functionality**:
  - [ ] I2C scan detects device at correct address
  - [ ] Sensor initialization succeeds
  - [ ] Sensor readings are valid
  - [ ] Multiple reads work correctly
  - [ ] No I/O errors in logs

- [ ] **Test with second I2C device** (if available):
  - [ ] BME280, SSD1306, or other I2C component
  - [ ] Verify multiple devices on same bus work

- [ ] **Test error handling**:
  - [ ] Disconnect I2C device - should report errors gracefully
  - [ ] Wrong address - should report device not found
  - [ ] Remove i2c group membership - should report permission error

### Deliverables

- [ ] I2C communication works on Raspberry Pi 5
- [ ] I2C scan detects connected devices
- [ ] At least one I2C sensor component works (ADS1115 or BME280)
- [ ] Error handling is robust
- [ ] Can run without sudo (i2c group membership)

### Commit

```
feat(linux): add I2C hardware support

- Implement LinuxI2CBus using i2c-dev kernel interface
- Support configurable bus number (default: /dev/i2c-1)
- Add combined write-read transactions via I2C_RDWR ioctl
- Map Linux errno to ESPHome I2C error codes
- Add comprehensive error logging
- Test with ADS1115 sensor on Raspberry Pi 5

I2C communication verified with real hardware.
```

---

