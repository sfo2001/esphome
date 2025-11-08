# Phase 5: SPI Implementation

[← Previous Phase](phase-4-gpio.md) | [← Back to README](../README.md) | [→ Next Phase](pending/phase-6-integration-testing.md) | [Roadmap](../ROADMAP.md)

**Status**: 🔧 Code Complete - Hardware Testing Pending

## Phase 5: SPI Implementation (REQUIRES HARDWARE)

**Goal**: Real SPI hardware support for Raspberry Pi
**Duration**: 2-3 days
**Compile**: Raspberry Pi 5 (ARM native)
**Priority**: HIGH (commonly used for displays and sensors)

This phase is split into two sub-phases:
- **Phase 5a**: Implementation (can be done on development server)
- **Phase 5b**: Hardware Testing (requires Raspberry Pi)

### Phase 5a: SPI Implementation

#### Pre-requisites

##### Hardware Setup on Raspberry Pi 5 (for later testing)

- [ ] **Install required packages**
  ```bash
  sudo apt-get update
  sudo apt-get install -y spi-tools
  ```

- [ ] **Enable SPI interface**
  ```bash
  sudo raspi-config
  # Interface Options -> SPI -> Enable
  # Or add to /boot/config.txt: dtparam=spi=on
  sudo reboot
  ```

- [ ] **Verify SPI bus available**
  ```bash
  ls -l /dev/spidev*
  # Should show /dev/spidev0.0 and /dev/spidev0.1
  ```

- [ ] **Add user to spi group**
  ```bash
  sudo usermod -aG spi $USER
  # Log out and back in for group to take effect
  ```

#### Implementation Tasks

- [ ] **Create `esphome/components/spi/spi_bus_linux.h`** (~120 LOC)
  - [ ] `LinuxSPIBus` class extending `SPIBus`
  - [ ] Member variables:
    - [ ] `int file_descriptor_` - fd for `/dev/spidevX.Y`
    - [ ] `uint8_t bus_num_` - SPI bus number (default: 0)
    - [ ] `uint8_t device_num_` - SPI device number (default: 0)
    - [ ] `uint32_t mode_` - SPI mode (0-3)
    - [ ] `uint8_t bits_per_word_` - bits per word (default: 8)
    - [ ] `uint32_t max_speed_` - maximum speed in Hz
  - [ ] Method signatures:
    - [ ] `void setup()`
    - [ ] `void dump_config()`
    - [ ] `SPIDelegate *get_delegate(...) override`
    - [ ] `bool is_hw() override { return true; }`
    - [ ] Setters for bus_num, device_num

- [ ] **Create `esphome/components/spi/spi_bus_linux.cpp`** (~400 LOC)
  - [ ] Include required headers:
    ```cpp
    #include <linux/spi/spidev.h>
    #include <sys/ioctl.h>
    #include <fcntl.h>
    #include <unistd.h>
    #include <cstring>
    #include <cerrno>
    ```

  - [ ] **Create `LinuxSPIDelegate` class** (extends `SPIDelegate`):
    - [ ] Implements actual SPI transfer operations
    - [ ] Uses `SPI_IOC_MESSAGE` ioctl for transactions
    - [ ] Supports full-duplex and half-duplex transfers

  - [ ] **Implement `LinuxSPIBus::setup()`**:
    - [ ] Open `/dev/spidev{bus_num}.{device_num}`
    - [ ] Store file descriptor
    - [ ] Configure SPI mode using `SPI_IOC_WR_MODE`
    - [ ] Configure bits per word using `SPI_IOC_WR_BITS_PER_WORD`
    - [ ] Configure max speed using `SPI_IOC_WR_MAX_SPEED_HZ`
    - [ ] Handle errors (file not found, permission denied)
    - [ ] Log successful initialization

  - [ ] **Implement `LinuxSPIDelegate::transfer()`**:
    - [ ] Use `SPI_IOC_MESSAGE` ioctl with `spi_ioc_transfer` structure
    - [ ] Support single byte transfer
    - [ ] Handle CS control if needed
    - [ ] Map Linux errno to appropriate error codes
    - [ ] Add detailed error logging

  - [ ] **Implement `LinuxSPIDelegate::transfer(buffer)`**:
    - [ ] Support buffer transfer (full-duplex)
    - [ ] Support separate tx/rx buffers
    - [ ] Handle transaction setup

  - [ ] **Implement `dump_config()`**:
    - [ ] Log bus number, device number, mode, speed

  - [ ] **Add destructor**:
    - [ ] Close file descriptor if open

- [ ] **Update `esphome/components/spi/__init__.py`**
  - [ ] Import `LinuxSPIBus`:
    ```python
    from esphome.const import PLATFORM_LINUX
    # In appropriate location
    LinuxSPIBus = spi_ns.class_("LinuxSPIBus", SPIBus, cg.Component)
    ```

  - [ ] Add Linux-specific configuration schema:
    ```python
    CONF_BUS_NUM = "bus_num"
    CONF_DEVICE_NUM = "device_num"

    # Add to CONFIG_SCHEMA based on platform
    if CORE.is_linux:
        # bus_num and device_num instead of pin configuration
        cv.Optional(CONF_BUS_NUM, default=0): cv.int_range(min=0, max=10)
        cv.Optional(CONF_DEVICE_NUM, default=0): cv.int_range(min=0, max=10)
    ```

  - [ ] Update `FILTER_SOURCE_FILES`:
    ```python
    FILTER_SOURCE_FILES = filter_source_files_from_platform(
        {
            # ... existing entries ...
            "spi_bus_linux.cpp": {PlatformFramework.LINUX_NATIVE},
        }
    )
    ```

  - [ ] Update `to_code()` function:
    - [ ] Detect Linux platform and use LinuxSPIBus
    - [ ] Configure bus_num and device_num

- [ ] **Create `tests/test_build_components/common/spi/linux.yaml`**
  ```yaml
  spi:
    id: spi_bus
    bus_num: 0      # /dev/spidev0.0 on Raspberry Pi
    device_num: 0
    # Note: clk_pin, miso_pin, mosi_pin are ignored on Linux
    # SPI pins are fixed by hardware (GPIO 10, 9, 11 on Pi)
  ```

- [ ] **Create `tests/components/linux/test_spi.linux.yaml`**
  ```yaml
  esphome:
    name: test_spi_basic

  linux:

  logger:
    level: VERBOSE

  spi:
    bus_num: 0
    device_num: 0

  # Test with a simple SPI device (if available)
  # For now, just test that SPI bus initializes
  ```

### Phase 5b: Hardware Testing (REQUIRES RASPBERRY PI)

#### Testing Prerequisites

- [ ] **Verify SPI hardware access**
  ```bash
  # Check SPI device files exist
  ls -l /dev/spidev0.*

  # Test basic SPI communication (if device connected)
  # Note: This requires an actual SPI device
  ```

- [ ] **Connect test SPI device** (choose one):
  - [ ] SPI-based display (e.g., ST7789, ILI9341)
  - [ ] SPI sensor (e.g., MAX31855 thermocouple, BME280 in SPI mode)
  - [ ] SPI OLED display
  - [ ] Note SPI pins on Raspberry Pi:
    - GPIO 10: MOSI (Master Out, Slave In)
    - GPIO 9: MISO (Master In, Slave Out)
    - GPIO 11: SCLK (Clock)
    - GPIO 8: CE0 (Chip Select 0) - /dev/spidev0.0
    - GPIO 7: CE1 (Chip Select 1) - /dev/spidev0.1

#### Testing Tasks

- [ ] **Create test configuration for SPI device**
  ```yaml
  esphome:
    name: test_spi_display
    platform: linux

  linux:

  logger:
    level: VERBOSE

  spi:
    bus_num: 0
    device_num: 0

  # Example: ST7789 display
  display:
    - platform: st7789v
      model: TTGO TDisplay 135x240
      cs_pin: 8  # Hardware CS on Pi (GPIO 8)
      dc_pin: 25  # Data/Command pin
      reset_pin: 27  # Reset pin
      lambda: |-
        it.print(0, 0, id(font), "Hello from Linux!");

  font:
    - file: "fonts/Arial.ttf"
      id: font
      size: 20
  ```

- [ ] **Compile on Raspberry Pi**
  ```bash
  cd ~/esphome-dev
  source venv/bin/activate
  esphome compile test-configs/test_spi_display.yaml
  ```

- [ ] **Run with SPI device connected**
  ```bash
  # May need sudo if not in spi group
  sudo ./.esphome/build/test_spi_display/test_spi_display

  # Or if in spi group:
  ./.esphome/build/test_spi_display/test_spi_display
  ```

- [ ] **Verify SPI functionality**:
  - [ ] SPI device initializes successfully
  - [ ] Display/sensor shows output
  - [ ] Multiple transfers work correctly
  - [ ] Different SPI modes work (if testable)
  - [ ] No I/O errors in logs

- [ ] **Test with second SPI device** (if available):
  - [ ] Use /dev/spidev0.1 (device_num: 1)
  - [ ] Verify multiple devices can coexist

- [ ] **Test error handling**:
  - [ ] Wrong bus/device number - should report errors gracefully
  - [ ] Remove spi group membership - should report permission error
  - [ ] Disconnect SPI device - should handle communication errors

### Deliverables

#### Phase 5a (Implementation)
- [ ] SPI bus implementation compiles on x86_64
- [ ] Code follows ESPHome coding standards
- [ ] Configuration schema defined
- [ ] Test configurations created

#### Phase 5b (Hardware Testing)
- [ ] SPI communication works on Raspberry Pi 5
- [ ] At least one SPI device component works (display or sensor)
- [ ] Error handling is robust
- [ ] Can run without sudo (spi group membership)

### Commit

```
feat(linux): add SPI hardware support (untested)

- Implement LinuxSPIBus using spidev kernel interface
- Support configurable bus and device numbers (default: /dev/spidev0.0)
- Add LinuxSPIDelegate for SPI transactions via SPI_IOC_MESSAGE ioctl
- Support full-duplex and half-duplex transfers
- Map Linux errno to ESPHome error codes
- Add comprehensive error logging
- Add test configurations for SPI

Note: Implementation complete but requires Raspberry Pi hardware for testing.
SPI communication not yet verified with real devices.
```

---

