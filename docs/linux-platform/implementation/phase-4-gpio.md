# Phase 4: GPIO Implementation

[← Previous Phase](phase-3-i2c.md) | [← Back to README](../README.md) | [→ Next Phase](phase-5-spi.md) | [Roadmap](../ROADMAP.md)

**Status**: 🔧 Code Complete - Hardware Testing Pending

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

