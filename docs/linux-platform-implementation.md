# Linux Platform Implementation Roadmap

> **⚠️ DEPRECATION NOTICE**
>
> This file has been reorganized for better navigation. Please use the new structure:
>
> - **📋 Start Here**: [linux-platform/README.md](linux-platform/README.md)
> - **🗺️ Overview**: [linux-platform/ROADMAP.md](linux-platform/ROADMAP.md)
> - **🚀 Current Work**: [linux-platform/CURRENT.md](linux-platform/CURRENT.md)
> - **📚 Implementation Details**: [linux-platform/implementation/](linux-platform/implementation/)
>
> This file is kept for reference but is no longer actively maintained.

---

**Project**: Add Linux (Raspberry Pi) as ESPHome target platform
**Branch**: `feature/linux-platform`
**Base**: `dev` branch
**Target**: Production-ready Linux platform with I2C and GPIO support

## Use Case

Run ESPHome on Raspberry Pi to control I2C-connected smart displays (Atmel-based) integrated with PiCorePlayer Squeezebox system.

## Available Hardware

- **Primary**: Raspberry Pi 5 (ARMv8, 64-bit)
- **Secondary**: Raspberry Pi 3 (ARMv7, 32-bit)
- **Legacy**: Raspberry Pi 1 (2011 model)

## Development Strategy

### Compilation Approach

#### Phases 1-2: Development Server (x86_64)
- **Where**: AMD development server
- **Target**: x86_64 native binary
- **Why**: Fast iteration, no hardware dependencies
- **Testing**: Basic platform structure, preferences

```bash
# On AMD server
esphome config test-config.yaml
esphome compile test-config.yaml
```

#### Phases 3-6: Raspberry Pi 5 (ARM native)
- **Where**: Raspberry Pi 5
- **Target**: ARM native compilation
- **Why**: Real hardware required, guaranteed compatibility
- **Testing**: I2C, GPIO, full integration

**Development Workflow**:
```bash
# 1. Edit on AMD server
cd ~/devel/esphome
git checkout feature/linux-platform
# ... make changes ...
git commit -m "feat(linux): implement I2C support"
git push origin feature/linux-platform

# 2. Sync to Pi 5
ssh pi5
cd ~/esphome-dev
git pull

# 3. Compile and test natively
source venv/bin/activate
esphome compile test-i2c.yaml
sudo ./test-i2c  # Run with hardware access
```

---

## Phase 1: Platform Foundation (No Hardware)

**Goal**: Basic platform registration and compilation
**Duration**: 1-2 days
**Compile**: AMD server (x86_64)

### Tasks

#### Core Platform Files

- [ ] **Create `esphome/const.py` additions**
  - [ ] Add `PLATFORM_LINUX = "linux"`
  - [ ] Add to `Platform` enum
  - [ ] Add `PlatformFramework.LINUX_NATIVE = (Platform.LINUX, Framework.NATIVE)`

- [ ] **Create `esphome/core/__init__.py` additions**
  - [ ] Add `is_linux` property to `EsphomeCore` class
  ```python
  @property
  def is_linux(self) -> bool:
      return self.target_platform == PLATFORM_LINUX
  ```

- [ ] **Create `esphome/components/linux/` directory**

- [ ] **Create `esphome/components/linux/__init__.py`** (~200 LOC)
  - [ ] Set `IS_TARGET_PLATFORM = True`
  - [ ] Implement `set_core_data(config)` function
  - [ ] Define `CONFIG_SCHEMA` with:
    - [ ] `preferences_path` (default: `/var/lib/esphome`)
    - [ ] `gpio_chip` (default: `gpiochip0`)
  - [ ] Implement `async def to_code(config)`
    - [ ] Add `cg.add_platformio_option("platform", "platformio/native")`
    - [ ] Add `cg.add_build_flag("-DUSE_LINUX")`
    - [ ] Add `cg.add_define(ThreadModel.MULTI_ATOMICS)`
  - [ ] Set `AUTO_LOAD = ["preferences"]`

- [ ] **Create `esphome/components/linux/const.py`** (~30 LOC)
  - [ ] Define `KEY_LINUX = "linux"`
  - [ ] Platform-specific constants

- [ ] **Create `esphome/components/linux/core.cpp`** (~200 LOC)
  - [ ] Copy structure from `esphome/components/host/core.cpp`
  - [ ] Implement HAL functions:
    - [ ] `uint32_t millis()` - using `clock_gettime(CLOCK_MONOTONIC)`
    - [ ] `uint32_t micros()` - microsecond precision
    - [ ] `void delay(uint32_t ms)` - using `nanosleep()`
    - [ ] `void delayMicroseconds(uint32_t us)`
    - [ ] `void arch_restart()` - using `exit(0)`
    - [ ] `void arch_init()` - empty for Linux
    - [ ] `void arch_feed_wdt()` - no-op (no watchdog)
    - [ ] `uint32_t arch_get_cpu_cycle_count()` - stub
    - [ ] `uint32_t arch_get_cpu_freq_hz()` - stub
    - [ ] `uint8_t progmem_read_byte(const uint8_t *addr)` - direct read

- [ ] **Create `esphome/components/linux/helpers.cpp`** (~50 LOC)
  - [ ] Copy from `esphome/components/host/helpers.cpp`
  - [ ] Utility functions

#### Testing Infrastructure

- [ ] **Create `tests/components/linux/` directory**

- [ ] **Create `tests/components/linux/test.linux.yaml`**
  ```yaml
  esphome:
    name: test_linux_basic
    platform: linux

  linux:
    preferences_path: /tmp/esphome-test

  logger:
    level: VERBOSE
  ```

- [ ] **Create `tests/test_build_components/build_components_base.linux.yaml`**
  ```yaml
  esphome:
    name: componenttestlinux
    friendly_name: $component_name
    platform: linux

  linux:
    preferences_path: /tmp/esphome-ci

  logger:
    level: VERY_VERBOSE

  packages:
    component_under_test: !include
      file: $component_test_file
  ```

#### Validation

- [ ] **Compile test on AMD server**
  ```bash
  ./script/test_build_components -c linux -e config
  ```

- [ ] **Verify generated platformio.ini** contains:
  - [ ] `platform = platformio/native`
  - [ ] `build_flags = -DUSE_LINUX`

- [ ] **Test basic execution** (x86_64 binary)
  ```bash
  esphome compile tests/components/linux/test.linux.yaml
  ./tests/components/linux/.esphome/build/test_linux_basic/test_linux_basic
  ```
  - [ ] Program starts without errors
  - [ ] Logger outputs appear
  - [ ] Can Ctrl+C to exit cleanly

### Deliverables

- [ ] Platform compiles without errors on x86_64
- [ ] Basic program runs with setup() and loop()
- [ ] No hardware I/O yet (that's Phase 3+)

### Commit

```
feat(linux): add basic Linux platform support

- Add PLATFORM_LINUX to const.py and platform detection
- Implement core HAL using POSIX APIs
- Add basic platform component structure
- Create initial test configuration

Platform compiles and runs basic programs without I/O.
```

---

## Phase 2: Preferences/Storage (No Hardware)

**Goal**: Persistent configuration storage
**Duration**: 0.5-1 day
**Compile**: AMD server (x86_64)

### Tasks

- [ ] **Create `esphome/components/linux/preferences.h`** (~100 LOC)
  - [ ] `LinuxPreferences` class extending `ESPPreferences`
  - [ ] `LinuxPreferenceBackend` class extending `ESPPreferenceBackend`
  - [ ] File path management for preferences storage

- [ ] **Create `esphome/components/linux/preferences.cpp`** (~200 LOC)
  - [ ] Copy structure from `esphome/components/host/preferences.cpp`
  - [ ] Implement file-based storage:
    - [ ] `bool sync()` - write to file
    - [ ] `bool load()` - read from file
    - [ ] JSON serialization/deserialization
  - [ ] Use configured `preferences_path` from YAML
  - [ ] Create directory if it doesn't exist
  - [ ] Handle file permissions errors gracefully

- [ ] **Update `esphome/components/linux/__init__.py`**
  - [ ] Ensure `AUTO_LOAD = ["preferences"]` is set
  - [ ] Add preferences configuration options if needed

#### Testing

- [ ] **Create test configuration with preferences**
  ```yaml
  esphome:
    name: test_preferences
    platform: linux

  linux:
    preferences_path: /tmp/esphome-prefs-test

  logger:

  # Component that uses preferences (e.g., OTA, API)
  api:
  ```

- [ ] **Test preferences operations**
  - [ ] Compile and run
  - [ ] Verify preferences file created at specified path
  - [ ] Modify preferences in code
  - [ ] Restart program
  - [ ] Verify preferences persisted
  - [ ] Test with non-existent directory (should auto-create)
  - [ ] Test with read-only filesystem (should fail gracefully)

### Deliverables

- [ ] Preferences save to filesystem
- [ ] Preferences load on restart
- [ ] Directory auto-creation works
- [ ] Error handling for permission issues

### Commit

```
feat(linux): add preferences/storage support

- Implement file-based preferences system
- Support configurable storage path
- Auto-create directories as needed
- Add error handling for permission issues

Preferences persist across program restarts.
```

---

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

## Phase 4: GPIO Implementation (REQUIRES HARDWARE)

**Goal**: Real GPIO control using libgpiod
**Duration**: 2-3 days
**Compile**: Raspberry Pi 5 (ARM native)

### Pre-requisites

#### Install libgpiod on Pi 5

- [ ] **Install development packages**
  ```bash
  sudo apt-get install -y libgpiod-dev libgpiod2 gpiod
  ```

- [ ] **Verify libgpiod works**
  ```bash
  gpiodetect
  # Should show gpiochip0, gpiochip4 (Pi 5 specific)

  gpioinfo gpiochip0
  # Shows all GPIO lines
  ```

- [ ] **Add user to gpio group**
  ```bash
  sudo usermod -aG gpio $USER
  # Log out and back in
  ```

- [ ] **Test GPIO access**
  ```bash
  # Set GPIO 17 high (LED test)
  gpioset gpiochip0 17=1

  # Get GPIO 17 state
  gpioget gpiochip0 17
  ```

#### Update Build System

- [ ] **Update `esphome/components/linux/__init__.py`**
  - [ ] Add libgpiod to build dependencies:
    ```python
    async def to_code(config):
        # ... existing code ...
        cg.add_platformio_option("lib_deps", ["libgpiod"])
        # Or add build flag:
        cg.add_build_flag("-lgpiod")
    ```

### Implementation Tasks

- [ ] **Create `esphome/components/linux/gpio.h`** (~120 LOC)
  - [ ] Include libgpiod:
    ```cpp
    #include <gpiod.h>
    ```

  - [ ] `LinuxGPIOPin` class extending `InternalGPIOPin`
  - [ ] Member variables:
    - [ ] `struct gpiod_chip *chip_` - GPIO chip handle
    - [ ] `struct gpiod_line *line_` - GPIO line handle
    - [ ] `uint8_t pin_` - GPIO number
    - [ ] `bool inverted_` - inversion flag
    - [ ] `gpio::Flags flags_` - pin configuration
    - [ ] `std::string chip_name_` - chip name (e.g., "gpiochip0")

  - [ ] Method signatures:
    - [ ] `void setup() override`
    - [ ] `void pin_mode(gpio::Flags flags) override`
    - [ ] `bool digital_read() override`
    - [ ] `void digital_write(bool value) override`
    - [ ] `std::string dump_summary() override`
    - [ ] `void set_pin(uint8_t pin)`
    - [ ] `void set_inverted(bool inverted)`
    - [ ] `void set_flags(gpio::Flags flags)`

- [ ] **Create `esphome/components/linux/gpio.cpp`** (~400 LOC)
  - [ ] **Implement `setup()`**:
    - [ ] Open GPIO chip: `gpiod_chip_open_by_name(chip_name_.c_str())`
    - [ ] Get line: `gpiod_chip_get_line(chip_, pin_)`
    - [ ] Check if line is available (not already in use)
    - [ ] Call `pin_mode()` to configure based on flags

  - [ ] **Implement `pin_mode()`**:
    - [ ] Release line if already requested
    - [ ] Determine direction (input/output) from flags
    - [ ] Request line with appropriate flags:
      - [ ] Output: `gpiod_line_request_output(line_, "esphome", default_value)`
      - [ ] Input: `gpiod_line_request_input(line_, "esphome")`
      - [ ] Input with pull-up: `gpiod_line_request_input_flags(line_, "esphome", GPIOD_LINE_REQUEST_FLAG_BIAS_PULL_UP)`
      - [ ] Input with pull-down: `gpiod_line_request_input_flags(line_, "esphome", GPIOD_LINE_REQUEST_FLAG_BIAS_PULL_DOWN)`

  - [ ] **Implement `digital_read()`**:
    - [ ] Read value: `gpiod_line_get_value(line_)`
    - [ ] Apply inversion if needed
    - [ ] Return boolean

  - [ ] **Implement `digital_write()`**:
    - [ ] Apply inversion if needed
    - [ ] Set value: `gpiod_line_set_value(line_, value)`

  - [ ] **Implement `dump_summary()`**:
    - [ ] Return string like "GPIO17 (gpiochip0)"

  - [ ] **Add destructor**:
    - [ ] Release line: `gpiod_line_release(line_)`
    - [ ] Close chip: `gpiod_chip_close(chip_)`

- [ ] **Create `esphome/components/linux/gpio.py`** (~150 LOC)
  - [ ] Register pin schema:
    ```python
    @pins.PIN_SCHEMA_REGISTRY.register("linux", LinuxGPIOPin)
    async def linux_pin_to_code(config):
        var = cg.new_Pvariable(config[CONF_ID])
        num = config[CONF_NUMBER]
        cg.add(var.set_pin(num))
        cg.add(var.set_inverted(config[CONF_INVERTED]))
        cg.add(var.set_flags(pins.gpio_flags_expr(config[CONF_MODE])))
        return var
    ```

  - [ ] Define valid pin ranges for different Pi models
  - [ ] Add validation for reserved pins (e.g., I2C pins)

- [ ] **Update `esphome/components/linux/__init__.py`**
  - [ ] Import GPIO schema
  - [ ] Add GPIO chip configuration:
    ```python
    CONFIG_SCHEMA = cv.Schema({
        # ... existing config ...
        cv.Optional("gpio_chip", default="gpiochip0"): cv.string,
    })
    ```

- [ ] **Create `tests/test_build_components/common/gpio/linux.yaml`**
  ```yaml
  # No special GPIO bus configuration needed for Linux
  # Pins are specified directly in component configs
  ```

#### Testing

- [ ] **Hardware setup for GPIO testing**:
  - [ ] Connect LED to GPIO 27 with resistor (output test)
  - [ ] Connect button to GPIO 17 with pull-up (input test)

- [ ] **Create GPIO test configuration**
  ```yaml
  esphome:
    name: test_gpio_basic
    platform: linux

  linux:
    gpio_chip: gpiochip0

  logger:
    level: VERBOSE

  binary_sensor:
    - platform: gpio
      pin:
        number: 17
        mode:
          input: true
          pullup: true
      name: "Test Button"
      on_press:
        - logger.log: "Button pressed!"
      on_release:
        - logger.log: "Button released!"

  switch:
    - platform: gpio
      pin: 27
      name: "Test LED"
      id: test_led

  # Auto-toggle LED for testing
  interval:
    - interval: 2s
      then:
        - switch.toggle: test_led
  ```

- [ ] **Compile and test**
  ```bash
  cd ~/esphome-dev
  source venv/bin/activate
  esphome compile test-configs/test_gpio_basic.yaml

  # Run (may need sudo if not in gpio group)
  ./.esphome/build/test_gpio_basic/test_gpio_basic
  ```

- [ ] **Verify GPIO functionality**:
  - [ ] LED toggles every 2 seconds
  - [ ] Button press/release detected correctly
  - [ ] Pull-up resistor works (button reads high when not pressed)
  - [ ] Can control GPIO from API/MQTT if enabled

- [ ] **Test GPIO modes**:
  - [ ] Output mode (LED control)
  - [ ] Input mode with pull-up (button)
  - [ ] Input mode with pull-down
  - [ ] Inverted pins work correctly

- [ ] **Test error handling**:
  - [ ] Invalid pin number - should fail gracefully
  - [ ] Pin already in use - should report error
  - [ ] Missing gpio group - should report permission error

### Deliverables

- [ ] GPIO digital I/O works on Raspberry Pi 5
- [ ] Input with pull-up/pull-down works
- [ ] Output control works
- [ ] Multiple GPIO pins can be used simultaneously
- [ ] Error handling is robust

### Commit

```
feat(linux): add GPIO support with libgpiod

- Implement LinuxGPIOPin using libgpiod chardev interface
- Support input/output modes with pull-up/pull-down
- Add configurable GPIO chip selection
- Map GPIO numbers to libgpiod line numbers
- Test with LED output and button input on Pi 5

GPIO digital I/O verified with real hardware.
```

---

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

## Phase 6: Integration Testing (REQUIRES HARDWARE)

**Goal**: Verify I2C + GPIO + SPI work together, test on all available Pi models
**Duration**: 1-2 days
**Compile**: Raspberry Pi 5, Pi 3, Pi 1

### Multi-Platform Testing

#### Test on Raspberry Pi 5

- [ ] **Create comprehensive test configuration**
  ```yaml
  esphome:
    name: pi5_integration_test
    platform: linux

  linux:
    gpio_chip: gpiochip0  # Pi 5 has gpiochip0 and gpiochip4
    preferences_path: /var/lib/esphome

  # Enable API for Home Assistant integration
  api:
    password: !secret api_password

  # Enable MQTT (if needed)
  mqtt:
    broker: 192.168.1.100
    username: !secret mqtt_user
    password: !secret mqtt_pass

  logger:
    level: DEBUG

  # I2C devices
  i2c:
    bus_num: 1
    frequency: 100kHz
    scan: true

  spi:
    bus_num: 0
    device_num: 0

  sensor:
    - platform: ads1115
      address: 0x48
      id: adc
      A0:
        name: "Analog Input A0"

    - platform: bme280
      address: 0x76
      temperature:
        name: "Temperature"
      humidity:
        name: "Humidity"
      pressure:
        name: "Pressure"

  # GPIO devices
  binary_sensor:
    - platform: gpio
      pin:
        number: 17
        mode:
          input: true
          pullup: true
      name: "Control Button"
      on_press:
        - switch.toggle: status_led

  switch:
    - platform: gpio
      pin: 27
      name: "Status LED"
      id: status_led

  # Your actual I2C smart displays (when identified)
  # Add configuration here
  ```

- [ ] **Compile and test on Pi 5**
  ```bash
  esphome compile pi5_integration_test.yaml
  sudo ./.esphome/build/pi5_integration_test/pi5_integration_test
  ```

- [ ] **Verification checklist**:
  - [ ] I2C scan detects all devices
  - [ ] All sensors report valid data
  - [ ] GPIO button works
  - [ ] GPIO LED responds to button and API commands
  - [ ] No conflicts between I2C and GPIO
  - [ ] Program runs stably for >1 hour
  - [ ] Memory usage is stable (no leaks)

#### Test on Raspberry Pi 3 (ARMv7, 32-bit)

- [ ] **Transfer code to Pi 3**
  ```bash
  # On Pi 3
  cd ~/esphome-dev
  git pull
  source venv/bin/activate
  ```

- [ ] **Compile natively on Pi 3**
  ```bash
  esphome compile pi5_integration_test.yaml
  ```

- [ ] **Note any Pi 3-specific issues**:
  - [ ] Compilation time (slower CPU)
  - [ ] Any 32-bit vs 64-bit differences
  - [ ] GPIO chip naming differences

- [ ] **Run same integration test**
  - [ ] All features work as on Pi 5
  - [ ] Performance is acceptable

#### Test on Raspberry Pi 1 (2011, Legacy)

- [ ] **Check compatibility**
  - [ ] Transfer code to Pi 1
  - [ ] Attempt compilation (may be very slow)
  - [ ] Document if compilation fails or is impractical

- [ ] **Decision**:
  - [ ] Determine minimum supported Pi model
  - [ ] Document limitations if any

### Your Specific Use Case

- [ ] **PiCorePlayer Integration Test**

  Create configuration for actual project:
  ```yaml
  esphome:
    name: picore_display_controller
    platform: linux
    comment: "I2C display controller for PiCorePlayer"

  linux:
    gpio_chip: gpiochip0
    preferences_path: /var/lib/esphome

  api:
    password: !secret api_password

  logger:
    level: INFO

  i2c:
    bus_num: 1
    frequency: 100kHz

  # Your Atmel-based smart displays
  # (Add specific component once identified - may need custom component)
  # Example placeholder:
  # display:
  #   - platform: [your_display_component]
  #     i2c_id: i2c_bus
  #     address: 0x3C
  #     # ... display config ...

  # Integration with PiCorePlayer (if needed)
  # - Read playback status
  # - Display track info on I2C displays
  # - Control buttons via GPIO
  ```

- [ ] **Test with actual I2C displays**:
  - [ ] Identify exact display component/protocol
  - [ ] Create or adapt ESPHome component
  - [ ] Test display initialization
  - [ ] Test display updates
  - [ ] Test with PiCorePlayer running

- [ ] **Performance testing**:
  - [ ] Measure CPU usage with both ESPHome and PiCorePlayer running
  - [ ] Verify no audio glitches from Squeezebox
  - [ ] Test I2C display update frequency

### Component Compatibility Testing

- [ ] **Add `test.linux.yaml` to existing I2C components**:

  For each supported component in `tests/components/`:
  - [ ] `ads1115/test.linux.yaml`
  - [ ] `bme280/test.linux.yaml`
  - [ ] `ssd1306/test.linux.yaml` (if using I2C OLED)
  - [ ] Any other I2C sensors you plan to use

- [ ] **Test component grouping**:
  ```bash
  # Test multiple components together
  ./script/test_build_components -c ads1115,bme280,ssd1306 -t linux -e config
  ```

### Deliverables

- [ ] Complete system works on Pi 5 (I2C + GPIO + API)
- [ ] Tested on Pi 3 with compatibility notes
- [ ] Tested on Pi 1 or documented as unsupported
- [ ] Your specific PiCorePlayer use case works
- [ ] Multiple I2C devices work simultaneously
- [ ] No interference between I2C and GPIO
- [ ] Component tests pass for major I2C components

### Commit

```
test(linux): add comprehensive integration tests

- Test I2C + GPIO together on Pi 5
- Verify compatibility on Pi 3 (ARMv7)
- Document Pi 1 limitations
- Test with multiple I2C devices simultaneously
- Add component tests for I2C sensors on Linux
- Verify PiCorePlayer integration (display controller use case)

Full platform tested on real hardware across Pi models.
```

---

## Phase 7: Documentation & Polish

**Goal**: Production-ready platform with complete documentation
**Duration**: 1-2 days
**Compile**: N/A (documentation)

### Documentation Tasks

#### Platform Documentation (esphome-docs repository)

- [ ] **Create platform documentation page**

  File: `components/linux.rst` (in esphome-docs repo)

  Sections:
  - [ ] Overview and use cases
  - [ ] Supported hardware (Raspberry Pi models)
  - [ ] System requirements
  - [ ] Installation instructions
  - [ ] Permissions setup (i2c, gpio groups)
  - [ ] Configuration reference
  - [ ] GPIO pin reference
  - [ ] I2C bus configuration
  - [ ] Limitations and known issues
  - [ ] Troubleshooting guide

- [ ] **Create setup guide**

  File: `guides/linux-raspberry-pi-setup.rst`

  Sections:
  - [ ] Raspberry Pi OS installation
  - [ ] Enabling I2C and GPIO
  - [ ] User permissions setup
  - [ ] ESPHome installation
  - [ ] First project walkthrough
  - [ ] Integration with Home Assistant
  - [ ] Systemd service setup (optional)

- [ ] **Update I2C documentation**

  File: `components/i2c.rst`

  - [ ] Add Linux-specific configuration
  - [ ] Document `bus_num` parameter
  - [ ] Add Raspberry Pi example

- [ ] **Update GPIO documentation**

  File: `components/gpio.rst` or platform-specific page

  - [ ] Add Linux GPIO pin reference
  - [ ] Document `gpio_chip` parameter
  - [ ] Add pinout diagrams for Pi models
  - [ ] Document pull-up/pull-down support

#### Code Documentation

- [ ] **Add inline comments to C++ code**:
  - [ ] `i2c_bus_linux.cpp` - explain i2c-dev usage
  - [ ] `gpio_linux.cpp` - explain libgpiod API
  - [ ] `preferences.cpp` - explain file storage format

- [ ] **Add Python docstrings**:
  - [ ] `esphome/components/linux/__init__.py`
  - [ ] `esphome/components/linux/gpio.py`

- [ ] **Update CLAUDE.md** (this repository):
  - [ ] Add Linux platform to platform list
  - [ ] Document Linux-specific development workflow
  - [ ] Add native compilation instructions
  - [ ] Update testing instructions

#### Example Configurations

- [ ] **Create example configurations**

  Directory: `examples/linux/` (in esphome-docs or this repo)

  Examples to create:
  - [ ] `basic-gpio.yaml` - Simple LED and button
  - [ ] `i2c-sensors.yaml` - Multiple I2C sensors
  - [ ] `home-automation.yaml` - Complete home automation node
  - [ ] `display-controller.yaml` - I2C display use case
  - [ ] `picore-player-integration.yaml` - Your specific use case

### Polish Tasks

#### Error Handling

- [ ] **Improve error messages**:
  - [ ] Permission denied → suggest adding user to groups
  - [ ] I2C bus not found → suggest enabling I2C interface
  - [ ] GPIO chip not found → suggest checking chip name
  - [ ] Device not found → suggest running i2cdetect

- [ ] **Add runtime validation**:
  - [ ] Check user is in i2c group (warning if not)
  - [ ] Check user is in gpio group (warning if not)
  - [ ] Validate GPIO chip exists before opening
  - [ ] Validate I2C bus exists before opening

#### Performance Optimization

- [ ] **Profile I2C performance**:
  - [ ] Measure transaction latency
  - [ ] Compare to ESP32 baseline
  - [ ] Optimize if needed

- [ ] **Profile GPIO performance**:
  - [ ] Measure read/write latency
  - [ ] Test interrupt performance (if implemented)

- [ ] **Memory profiling**:
  - [ ] Check for memory leaks (valgrind)
  - [ ] Measure baseline memory usage
  - [ ] Compare to ESP32 (should be higher but stable)

#### CI/CD Integration (Optional)

- [ ] **Add CI tests if feasible**:
  - [ ] Compilation tests (can run on CI x86_64)
  - [ ] Consider QEMU for basic functionality tests
  - [ ] Document that I2C/GPIO tests require real hardware

- [ ] **Update CI configuration**:
  - [ ] `.github/workflows/ci.yml`
  - [ ] Add Linux platform to build matrix
  - [ ] Skip hardware tests in CI (or use QEMU)

### Deliverables

- [ ] Complete user documentation
- [ ] Example configurations for common use cases
- [ ] Inline code documentation
- [ ] Error messages guide users to solutions
- [ ] Performance is acceptable and stable
- [ ] CI integration (or documentation why it's not feasible)

### Commits

```
docs(linux): add comprehensive platform documentation

- Create Linux platform documentation page
- Add Raspberry Pi setup guide
- Document I2C and GPIO configuration
- Add troubleshooting guide
- Create example configurations

Complete documentation for production use.
```

```
feat(linux): improve error handling and user experience

- Add helpful error messages with solutions
- Validate user permissions and provide guidance
- Check hardware availability before opening
- Improve logging for debugging

Better user experience with clear error guidance.
```

---

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

## Pull Request Checklist

When ready to submit PR to `dev` branch:

- [ ] All phases completed and tested
- [ ] Code follows ESPHome style guide (ruff, clang-format)
- [ ] All tests pass:
  ```bash
  # Python linting
  script/lint-python

  # C++ linting (if applicable)
  script/lint-cpp

  # Component tests
  ./script/test_build_components -c linux -e config
  ```

- [ ] Documentation complete (or separate PR to esphome-docs)
- [ ] CLAUDE.md updated with Linux platform info
- [ ] Commits are clean and atomic
- [ ] PR description filled out with:
  - [ ] Summary of changes
  - [ ] Tested hardware (Pi models)
  - [ ] Breaking changes (if any)
  - [ ] Example configuration
  - [ ] Screenshots/logs of working system

- [ ] CODEOWNERS entry added:
  ```
  # Linux platform
  /esphome/components/linux/ @yourusername
  /esphome/components/i2c/i2c_bus_linux.* @yourusername
  ```

---

## Progress Tracking

### Overall Progress

- [x] Phase 1: Platform Foundation (No Hardware) - 100%
- [x] Phase 2: Preferences/Storage (No Hardware) - 100%
- [x] Phase 3: I2C Implementation (Pi 5) - 100% (Implementation Complete - Testing Required)
- [x] Phase 4: GPIO Implementation (Pi 5) - 100% (Implementation Complete - Testing Required)
- [x] Phase 5: SPI Implementation (Pi 5) - 100% (Implementation Complete - Testing Required)
  - [x] Phase 5a: Implementation - 100%
  - [ ] Phase 5b: Hardware Testing - 0%
- [ ] Phase 6: Integration Testing (All Pi models) - 0%
- [ ] Phase 7: Documentation & Polish - 0%
- [ ] Phase 8: OTA Updates Implementation - 0% (Architecture Documented)

### Quick Reference

**Current Phase**: Phase 8 (OTA Updates) - Architecture design complete
**Blocking Issues**: Phases 3-5b testing requires Raspberry Pi hardware
**Next Action**: Implement LinuxOTABackend or continue with hardware testing of Phases 3-5

---

## Notes & Discoveries

### Implementation Notes

_Use this section to document any insights, gotchas, or important decisions made during implementation._

#### Phase 1: Platform Foundation (Completed)

**Date**: 2025-11-08
**Status**: ✅ Complete - Compilation tested on x86_64

**Files Created**:
- `esphome/const.py` - Added `PLATFORM_LINUX`, `Platform.LINUX` enum, `PlatformFramework.LINUX_NATIVE`
- `esphome/core/__init__.py` - Added `is_linux` property (line 758-759)
- `esphome/components/linux/__init__.py` - Platform registration and configuration
- `esphome/components/linux/const.py` - Platform constants and namespace definitions
- `esphome/components/linux/core.cpp` - HAL implementation using POSIX APIs
- `esphome/components/linux/helpers.cpp` - Random number generation, mutex, MAC address utilities
- `tests/components/linux/test.linux.yaml` - Basic platform test
- `tests/test_build_components/build_components_base.linux.yaml` - CI test base configuration

**Implementation Highlights**:
- All HAL functions implemented using POSIX/Linux APIs (`clock_gettime`, `nanosleep`, etc.)
- Platform uses native compilation via PlatformIO (`platformio/native`)
- Thread model: `MULTI_ATOMICS` for proper multi-threading support
- Proper use of `CORE.data` for state management (follows best practices)

**Testing Status**:
- ✅ Configuration validation passes
- ✅ Compilation successful on x86_64
- ✅ Code follows ESPHome coding standards

#### Phase 2: Preferences/Storage (Completed)

**Date**: 2025-11-08
**Status**: ✅ Complete - Compilation tested on x86_64

**Files Created**:
- `esphome/components/linux/preferences.h` - Preferences class definitions
- `esphome/components/linux/preferences.cpp` - File-based storage implementation
- `tests/components/linux/test_preferences.linux.yaml` - Preferences test with API component

**Implementation Highlights**:
- File-based persistence using `std::fstream` (C++ streams)
- Directory auto-creation with error handling
- Configurable preferences path (default: `/var/lib/esphome`)
- Binary format: key (uint32) + length (uint8) + data
- Proper RAII with C++ iostreams

**Testing Status**:
- ✅ Configuration validation passes
- ✅ Compilation successful
- ⏳ Runtime testing pending (requires Linux environment)

**Code Quality Notes**:
- Refactored from C-style FILE* to C++ std::fstream for better RAII
- Removed VLA (variable-length array) - replaced with std::vector

#### Phase 3: I2C Implementation (Completed - Untested)

**Date**: 2025-11-08
**Status**: ✅ Complete (Implementation) - ⏳ Hardware Testing Pending

**Files Created/Modified**:
- Created `esphome/components/i2c/i2c_bus_linux.h` - Header file for Linux I2C bus implementation
- Created `esphome/components/i2c/i2c_bus_linux.cpp` - Implementation using Linux i2c-dev kernel interface
- Modified `esphome/components/i2c/__init__.py` - Added Linux support to I2C component
- Created `tests/test_build_components/common/i2c/linux.yaml` - Common I2C test configuration
- Created `tests/components/linux/test_i2c.linux.yaml` - Linux-specific I2C test

**Implementation Details**:
1. **Linux I2C Bus Class**: `LinuxI2CBus` extends `InternalI2CBus` and `Component`
   - Uses `/dev/i2c-X` device files (default: `/dev/i2c-1` for Raspberry Pi)
   - Implements `write_readv()` using `I2C_RDWR` ioctl for atomic write-read transactions
   - Maps Linux errno codes to ESPHome `ErrorCode` enum for consistent error reporting

2. **Configuration Schema**:
   - Added `bus_num` parameter for Linux platform (default: 1)
   - Linux doesn't use `sda`/`scl` pins (uses kernel device files instead)
   - Maintains compatibility with existing platforms

3. **Error Handling**:
   - Comprehensive error mapping: EREMOTEIO/ENXIO → ERROR_NOT_ACKNOWLEDGED
   - Timeout handling: ETIMEDOUT/EAGAIN → ERROR_TIMEOUT
   - Detailed logging with helpful messages for common issues (permissions, device not found)

4. **Testing**:
   - Configuration validation tests can run on x86_64 development server
   - Hardware testing requires Raspberry Pi with I2C enabled
   - Test configuration uses bus_num=1 (standard Raspberry Pi I2C bus)

**Key Decisions**:
- Used `I2C_RDWR` ioctl for combined write-read operations to ensure atomicity
- Default bus number is 1 (`/dev/i2c-1`) as this is the standard on Raspberry Pi
- Error messages include actionable suggestions (enable I2C, check permissions, add to i2c group)

**Known Limitations**:
- Implementation is untested on real hardware (requires Raspberry Pi)
- Frequency setting is stored but not enforced (Linux kernel manages bus speed)
- No bus recovery mechanism (unlike Arduino implementation)

**Next Steps for Testing**:
1. Enable I2C on Raspberry Pi (`raspi-config`)
2. Add user to `i2c` group
3. Compile test configuration on Raspberry Pi
4. Run with real I2C device connected
5. Verify I2C scan detects devices
6. Test actual sensor communication

#### Phase 4: GPIO Implementation (Completed - Untested)

**Date**: 2025-11-08
**Status**: ✅ Complete (Implementation) - ⏳ Hardware Testing Pending

**Files Created/Modified**:
- Created `esphome/components/linux/gpio.h` - Header for Linux GPIO pin class using libgpiod
- Created `esphome/components/linux/gpio.cpp` - Implementation of GPIO operations
- Created `esphome/components/linux/gpio.py` - Python pin schema and validation
- Modified `esphome/components/linux/__init__.py` - Added GPIO support and libgpiod library
- Created `tests/components/linux/test_gpio.linux.yaml` - Comprehensive GPIO test configuration
- Created `tests/test_build_components/common/gpio/linux.yaml` - Common GPIO test file

**Implementation Details**:
1. **Linux GPIO Pin Class**: `LinuxGPIOPin` extends `InternalGPIOPin`
   - Uses libgpiod chardev interface for GPIO control
   - Supports configurable GPIO chip (default: gpiochip0)
   - Implements digital_read() and digital_write() operations
   - Supports pull-up/pull-down resistors for input pins
   - Proper RAII with destructor to release GPIO lines

2. **Pin Modes Supported**:
   - Output mode: Set GPIO as output with default state
   - Input mode: Read GPIO state
   - Input with pull-up: Internal pull-up resistor enabled
   - Input with pull-down: Internal pull-down resistor enabled
   - Inverted pins: Logical inversion supported for both input and output

3. **GPIO Pin Validation**:
   - Validates pin numbers (0-63 range)
   - Warns about commonly reserved pins (I2C, SPI, UART)
   - Supports "GPIO17" or 17 format for pin numbers
   - Validates mode combinations (can't be both input and output)

4. **Configuration**:
   - GPIO chip name configurable in platform config (default: "gpiochip0")
   - Pins specified directly in component configurations
   - Supports standard ESPHome pin schema (number, mode, inverted)

5. **Error Handling**:
   - Comprehensive error messages with actionable suggestions
   - Permission denied → suggests adding user to gpio group
   - Chip not found → suggests using gpiodetect command
   - Pin already in use → reports the consumer name
   - Detailed logging for all GPIO operations

6. **Testing Configuration**:
   - Tests GPIO output (switch component)
   - Tests GPIO input with pull-up and pull-down
   - Tests inverted pins
   - Includes interval-based toggling for testing

**Key Decisions**:
- Used libgpiod chardev interface (modern GPIO access method for Linux)
- GPIO chip name configurable per platform (supports Raspberry Pi 5's multiple chips)
- Pin validation allows 0-63 range (supports different Pi models and future expansion)
- Warnings for reserved pins but doesn't block their use (user choice)
- Interrupts not yet implemented (marked with warning in code)

**Known Limitations**:
- Implementation is untested on real hardware (requires Raspberry Pi)
- GPIO interrupts not implemented (detach_interrupt() and to_isr() are stubs)
- Frequency/speed control not exposed (kernel manages this)
- Open-drain mode not directly supported by libgpiod (not implemented)
- No bus recovery mechanism
- All GPIO operations require Raspberry Pi with libgpiod installed

**Compilation Testing**:
- ⏳ Configuration validation: Pending (requires build test)
- ⏳ Compilation test: Pending (requires Linux platform with libgpiod-dev)
- ⏳ Code style check: Pending (ruff, clang-format)

**Next Steps for Hardware Testing**:
1. Install libgpiod on Raspberry Pi: `sudo apt-get install libgpiod-dev gpiod`
2. Add user to gpio group: `sudo usermod -aG gpio $USER`
3. Verify GPIO chips available: `gpiodetect` (should show gpiochip0)
4. Compile test configuration on Raspberry Pi
5. Connect test LED to GPIO 17 (output test)
6. Connect test button to GPIO 22 (input test with pull-up)
7. Run test and verify:
   - LED toggles every 2 seconds
   - Button presses are detected
   - Pull-up/pull-down resistors work correctly
   - No permission errors
8. Test multiple simultaneous GPIO pins
9. Test error handling (invalid pin, pin in use, etc.)
10. Verify no conflicts with I2C pins (if I2C also in use)

**Integration with Phase 3**:
- GPIO and I2C should work together without conflicts
- Integration testing will verify both can be used simultaneously
- Same permissions model (user groups) for both I2C and GPIO

#### Phase 5: SPI Implementation (Completed - Untested)

**Date**: 2025-11-08
**Status**: ✅ Complete (Implementation) - ⏳ Hardware Testing Pending

**Files Created/Modified**:
- Created `esphome/components/spi/spi_bus_linux.h` - Header file for Linux SPI bus implementation
- Created `esphome/components/spi/spi_bus_linux.cpp` - Implementation using Linux spidev kernel interface
- Modified `esphome/components/spi/__init__.py` - Added Linux support to SPI component
- Created `tests/test_build_components/common/spi/linux.yaml` - Common SPI test configuration
- Created `tests/components/linux/test_spi.linux.yaml` - Linux-specific SPI test

**Implementation Details**:
1. **Linux SPI Bus Class**: `LinuxSPIBus` extends `SPIBus` and `Component`
   - Uses `/dev/spidevX.Y` device files (default: `/dev/spidev0.0` for Raspberry Pi)
   - Configures SPI mode, bits per word, and speed using ioctl
   - Returns `LinuxSPIDelegate` instances for device communication

2. **Linux SPI Delegate**: `LinuxSPIDelegate` extends `SPIDelegate`
   - Implements actual SPI transfer operations using `SPI_IOC_MESSAGE` ioctl
   - Supports single byte and buffer transfers (full-duplex)
   - Handles separate TX/RX buffers
   - Manages CS (chip select) via parent SPIDelegate class

3. **Configuration Schema**:
   - Added `bus_num` parameter for Linux platform (default: 0)
   - Added `device_num` parameter for Linux platform (default: 0)
   - Linux doesn't use `clk_pin`, `miso_pin`, `mosi_pin` (uses kernel device files instead)
   - SPI pins on Raspberry Pi are hardware-fixed:
     - GPIO 10: MOSI, GPIO 9: MISO, GPIO 11: SCLK
     - GPIO 8: CE0 (/dev/spidev0.0), GPIO 7: CE1 (/dev/spidev0.1)

4. **Error Handling**:
   - Comprehensive error logging with helpful messages for common issues (permissions, device not found)
   - Maps Linux errno codes to appropriate errors
   - Provides actionable suggestions (enable SPI, check permissions, add to spi group)

5. **Testing**:
   - Python syntax validation passes
   - Test configurations created
   - Full compilation and hardware testing requires Raspberry Pi

**Key Decisions**:
- Used `SPI_IOC_MESSAGE` ioctl with `spi_ioc_transfer` structure for all transfers
- Default bus is 0, device is 0 (`/dev/spidev0.0`) as this is standard on Raspberry Pi
- SPI mode, speed, and bit order configured per delegate (device-specific)
- CS (chip select) handling delegated to parent SPIDelegate class (uses GPIO)
- Bits per word defaults to 8 (standard for SPI)

**Known Limitations**:
- Implementation is untested on real hardware (requires Raspberry Pi)
- SPI speed is requested but actual speed may differ based on hardware capabilities
- No SPI bus arbitration between multiple processes
- CS pin handling relies on GPIO implementation
- All SPI operations require Raspberry Pi with SPI enabled

**Next Steps for Hardware Testing**:
1. Enable SPI on Raspberry Pi (`raspi-config`)
2. Add user to `spi` group
3. Compile test configuration on Raspberry Pi
4. Run with real SPI device connected (display or sensor)
5. Verify SPI communication works
6. Test with multiple SPI devices if available
7. Test error handling (wrong bus/device number, permissions, etc.)

**Integration with Phases 3-4**:
- SPI should work alongside I2C and GPIO without conflicts
- Same permissions model (user groups) for all hardware interfaces
- Integration testing (Phase 6) will verify all three work together

---

### Code Quality Audit (2025-11-08)

**Auditor**: Claude (AI Assistant)
**Scope**: Phases 1-3 implementation review
**Status**: ✅ PASS with fixes applied

#### Issues Found and Resolved

1. **Critical: Variable Length Array (VLA)**
   - **File**: `preferences.cpp:58`
   - **Issue**: `uint8_t data[len]` - VLA is not standard C++20
   - **Fix**: Replaced with `std::vector<uint8_t> data(len)`
   - **Status**: ✅ Fixed

2. **Critical: Missing I2C Destructor**
   - **Files**: `i2c_bus_linux.h`, `i2c_bus_linux.cpp`
   - **Issue**: File descriptor not closed on object destruction
   - **Fix**: Added destructor to close `file_descriptor_`
   - **Status**: ✅ Fixed

3. **Critical: Platform Specification in YAML**
   - **File**: `test_i2c.linux.yaml`
   - **Issue**: `platform: linux` in esphome section (incorrect for target platforms)
   - **Fix**: Removed - platform detected from `linux:` component section
   - **Explanation**: ESPHome detects target platform from component presence, not from `platform:` key
   - **Status**: ✅ Fixed

4. **Critical: Namespace Inconsistency**
   - **Files**: `preferences.h`, `preferences.cpp`
   - **Issue**: C++ used `linux_platform` namespace, Python defined `linux` namespace
   - **Fix**: Changed C++ to use `linux` namespace for consistency
   - **Status**: ✅ Fixed

5. **Enhancement: C-Style File Operations**
   - **File**: `preferences.cpp`
   - **Issue**: Used C-style `FILE*` instead of C++ streams
   - **Fix**: Refactored to use `std::ifstream` and `std::ofstream`
   - **Benefits**: Better RAII, exception safety, more idiomatic C++
   - **Status**: ✅ Fixed

6. **Verification: Member Initialization**
   - **Files**: All header files
   - **Status**: ✅ Already correct - all members use in-class initializers

#### Additional Fixes Applied

- **Logger Component**: Added Linux platform detection (already present in merged code)
- **I2C Component**: Added `FILTER_SOURCE_FILES` entry for `i2c_bus_linux.cpp` (already present)

#### Coding Standards Compliance

**✅ Pass Areas**:
- Naming conventions (classes, functions, members, constants)
- Namespace usage
- Header guards (`#pragma once`)
- Conditional compilation (`#ifdef USE_LINUX`)
- Member access prefixed with `this->`
- Error handling with helpful messages
- Modern C++ usage (C++20 features where appropriate)

**Audit Conclusion**: All critical issues resolved. Code is ready for PR submission to `feature/linux-platform`.

### Performance Metrics

_Track performance measurements here._

- I2C transaction latency:
- GPIO read/write latency:
- Memory usage (baseline):
- CPU usage (idle):
- CPU usage (active with I2C polling):

### Hardware-Specific Quirks

_Document any hardware-specific issues discovered._

**Raspberry Pi 5**:
-

**Raspberry Pi 3**:
-

**Raspberry Pi 1**:
-

---

## Useful Commands Reference

### Development Workflow

```bash
# === ON AMD SERVER (Phases 1-2) ===

# Edit code
cd ~/devel/esphome
git checkout feature/linux-platform

# Test compilation (x86_64)
esphome compile test-config.yaml

# Run linters
script/lint-python
script/lint-cpp

# Commit changes
git add .
git commit -m "feat(linux): ..."
git push origin feature/linux-platform


# === ON RASPBERRY PI 5 (Phases 3-6) ===

# One-time setup
mkdir ~/esphome-dev && cd ~/esphome-dev
git clone <fork-url> .
git checkout feature/linux-platform
python3 -m venv venv
source venv/bin/activate
pip install -e .

# Development cycle
git pull
esphome compile test-i2c.yaml
sudo ./test-i2c  # or without sudo if in groups

# Check I2C devices
i2cdetect -y 1

# Check GPIO chips
gpiodetect
gpioinfo gpiochip0

# Monitor logs
journalctl -f  # if running as systemd service
```

### Debugging Commands

```bash
# Check user groups
groups $USER

# Add to groups (requires logout)
sudo usermod -aG i2c,gpio $USER

# Check I2C bus
ls -l /dev/i2c-*
i2cdetect -y 1

# Check GPIO
ls -l /dev/gpiochip*
gpioinfo gpiochip0

# Check permissions
ls -l /dev/i2c-1
ls -l /dev/gpiochip0

# Monitor system resources
htop
# or
top

# Check for memory leaks
valgrind --leak-check=full ./your-esphome-binary
```

---

## Success Criteria - Final Checklist

### Minimum Viable Product (MVP)

- [ ] **Platform compiles and runs** on Raspberry Pi 5
- [ ] **I2C communication works** with at least 2 different I2C devices
- [ ] **GPIO digital I/O works** for both input and output
- [ ] **Preferences persist** across program restarts
- [ ] **API integration works** with Home Assistant
- [ ] **Your PiCorePlayer use case works**: I2C displays controlled from Pi running ESPHome
- [ ] **Tested on Pi 3** with compatibility notes
- [ ] **Documentation exists** for setup and usage

### Production Ready

- [ ] All MVP criteria met
- [ ] Comprehensive error handling with helpful messages
- [ ] Performance is acceptable (no lag in I2C display updates)
- [ ] Memory usage is stable (no leaks)
- [ ] Complete user documentation
- [ ] Example configurations available
- [ ] Tests added for major I2C components
- [ ] Code passes all linters
- [ ] Ready for PR review

---

**Last Updated**: [Date of last edit]
**Status**: Not Started
**Current Phase**: Phase 0 (Planning Complete)
