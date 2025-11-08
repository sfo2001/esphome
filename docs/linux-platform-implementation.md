# Linux Platform Implementation Roadmap

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

## Phase 5: Integration Testing (REQUIRES HARDWARE)

**Goal**: Verify I2C + GPIO work together, test on all available Pi models
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

## Phase 6: Documentation & Polish

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

- [ ] Phase 1: Platform Foundation (No Hardware) - 0%
- [ ] Phase 2: Preferences/Storage (No Hardware) - 0%
- [ ] Phase 3: I2C Implementation (Pi 5) - 0%
- [ ] Phase 4: GPIO Implementation (Pi 5) - 0%
- [ ] Phase 5: Integration Testing (All Pi models) - 0%
- [ ] Phase 6: Documentation & Polish - 0%

### Quick Reference

**Current Phase**: Not started
**Blocking Issues**: None
**Next Action**: Start Phase 1 - create platform foundation

---

## Notes & Discoveries

### Implementation Notes

_Use this section to document any insights, gotchas, or important decisions made during implementation._

-

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
