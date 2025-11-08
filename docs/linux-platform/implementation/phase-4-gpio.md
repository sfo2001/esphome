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
    - [ ] `bool line_requested_` - track if line is requested
    - [ ] **Interrupt support members**:
      - [ ] `std::unique_ptr<std::thread> interrupt_thread_` - event monitoring thread
      - [ ] `std::atomic<bool> interrupt_active_` - interrupt enabled flag
      - [ ] `void (*isr_callback_)(void *)` - ISR function pointer
      - [ ] `void *isr_arg_` - ISR argument
      - [ ] `gpio::InterruptType interrupt_type_` - edge detection mode

  - [ ] Method signatures:
    - [ ] `void setup() override`
    - [ ] `void pin_mode(gpio::Flags flags) override`
    - [ ] `bool digital_read() override`
    - [ ] `void digital_write(bool value) override`
    - [ ] `std::string dump_summary() override`
    - [ ] `void set_pin(uint8_t pin)`
    - [ ] `void set_inverted(bool inverted)`
    - [ ] `void set_flags(gpio::Flags flags)`
    - [ ] **Interrupt methods**:
      - [ ] `void attach_interrupt(void (*func)(void *), void *arg, gpio::InterruptType type) override`
      - [ ] `void detach_interrupt() override`
      - [ ] `ISRInternalGPIOPin to_isr() const override`
    - [ ] **Private helper methods**:
      - [ ] `bool open_chip_()` - open GPIO chip and get line handle
      - [ ] `void release_line_()` - release GPIO line if requested
      - [ ] `void interrupt_loop_()` - event monitoring thread function

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
    - [ ] Stop interrupt thread if active
    - [ ] Release line: `gpiod_line_release(line_)`
    - [ ] Close chip: `gpiod_chip_close(chip_)`

  - [ ] **Implement Interrupt Support** (~200 additional LOC):
    - [ ] **`attach_interrupt()` implementation**:
      - [ ] Validate line is in input mode
      - [ ] Store callback function and argument
      - [ ] Request edge events based on interrupt type:
        ```cpp
        switch (type) {
          case gpio::INTERRUPT_RISING_EDGE:
            ret = gpiod_line_request_rising_edge_events(line_, "esphome");
            break;
          case gpio::INTERRUPT_FALLING_EDGE:
            ret = gpiod_line_request_falling_edge_events(line_, "esphome");
            break;
          case gpio::INTERRUPT_ANY_EDGE:
            ret = gpiod_line_request_both_edges_events(line_, "esphome");
            break;
        }
        ```
      - [ ] Set `interrupt_active_ = true`
      - [ ] Launch interrupt monitoring thread: `interrupt_thread_ = std::make_unique<std::thread>([this]() { interrupt_loop_(); })`
      - [ ] Add error handling for edge request failure

    - [ ] **`interrupt_loop_()` implementation** (runs in dedicated thread):
      - [ ] Event dispatch loop:
        ```cpp
        void interrupt_loop_() {
          struct gpiod_line_event event;
          struct timespec timeout = {1, 0};  // 1 second timeout

          while (this->interrupt_active_) {
            // BLOCKS here until event or timeout (thread sleeps - 0% CPU)
            int ret = gpiod_line_event_wait(this->line_, &timeout);

            if (ret > 0) {  // Event occurred
              if (gpiod_line_event_read(this->line_, &event) == 0) {
                // Dispatch to ISR callback
                if (this->isr_callback_) {
                  this->isr_callback_(this->isr_arg_);
                }
              }
            } else if (ret < 0) {
              // Error occurred
              ESP_LOGE(TAG, "Event wait failed: %s", strerror(errno));
              break;
            }
            // ret == 0: timeout, check interrupt_active_ and continue
          }
        }
        ```
      - [ ] Handle event reading errors
      - [ ] Log debug info for event timestamps

    - [ ] **`detach_interrupt()` implementation**:
      - [ ] Set `interrupt_active_ = false`
      - [ ] Join interrupt thread (wait for exit)
      - [ ] Release line and re-request as input (without events)
      - [ ] Clear callback pointers

    - [ ] **`to_isr()` implementation**:
      - [ ] Return `ISRInternalGPIOPin` with necessary context
      - [ ] Note: May be simplified compared to ESP platforms

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

- [ ] **Create Basic GPIO test configuration** (`test_gpio_basic.yaml`)
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

- [ ] **Create Interrupt test configuration** (`test_gpio_interrupt.yaml`)
  ```yaml
  esphome:
    name: test_gpio_interrupt
    platform: linux

  linux:
    gpio_chip: gpiochip0

  logger:
    level: DEBUG

  # Test rising edge interrupt
  binary_sensor:
    - platform: gpio
      pin:
        number: 17
        mode:
          input: true
          pullup: true
      name: "Button Rising Edge"
      filters:
        - delayed_on: 10ms  # Debounce
      on_press:
        - logger.log: "INTERRUPT: Rising edge detected on GPIO 17"
        - lambda: |-
            ESP_LOGI("test", "Button press interrupt fired!");

  # Test falling edge interrupt
    - platform: gpio
      pin:
        number: 27
        mode:
          input: true
          pulldown: true
      name: "Button Falling Edge"
      filters:
        - delayed_off: 10ms  # Debounce
      on_release:
        - logger.log: "INTERRUPT: Falling edge detected on GPIO 27"

  # Rotary encoder test (uses both edges)
  sensor:
    - platform: rotary_encoder
      name: "Test Rotary Encoder"
      pin_a:
        number: 22
        mode:
          input: true
          pullup: true
      pin_b:
        number: 23
        mode:
          input: true
          pullup: true
      resolution: 1
      on_value:
        - logger.log:
            format: "Encoder position: %d"
            args: ['(int)x']
  ```

- [ ] **Compile and test**
  ```bash
  cd ~/esphome-dev
  source venv/bin/activate
  esphome compile test-configs/test_gpio_basic.yaml

  # Run (may need sudo if not in gpio group)
  ./.esphome/build/test_gpio_basic/test_gpio_basic
  ```

- [ ] **Verify Basic GPIO functionality**:
  - [ ] LED toggles every 2 seconds
  - [ ] Button press/release detected correctly
  - [ ] Pull-up resistor works (button reads high when not pressed)
  - [ ] Can control GPIO from API/MQTT if enabled

- [ ] **Verify Interrupt functionality**:
  - [ ] Rising edge interrupts fire on button press
  - [ ] Falling edge interrupts fire on button release
  - [ ] Both edge interrupts work (rotary encoder)
  - [ ] Multiple interrupts don't interfere with each other
  - [ ] Interrupt latency is reasonable (< 50ms for user input)
  - [ ] No interrupt events are lost
  - [ ] CPU usage is low when idle (interrupt thread sleeping)
  - [ ] `detach_interrupt()` properly stops monitoring

- [ ] **Test Interrupt edge cases**:
  - [ ] Rapid button bouncing handled correctly
  - [ ] Long-running ISR callbacks don't block other interrupts
  - [ ] Interrupt thread exits cleanly on shutdown
  - [ ] Reattaching interrupt works after detach

- [ ] **Test GPIO modes**:
  - [ ] Output mode (LED control)
  - [ ] Input mode with pull-up (button)
  - [ ] Input mode with pull-down
  - [ ] Inverted pins work correctly

- [ ] **Test error handling**:
  - [ ] Invalid pin number - should fail gracefully
  - [ ] Pin already in use - should report error
  - [ ] Missing gpio group - should report permission error
  - [ ] Interrupt on output pin - should fail gracefully
  - [ ] Event read errors logged properly

### Deliverables

- [ ] GPIO digital I/O works on Raspberry Pi 5
- [ ] Input with pull-up/pull-down works
- [ ] Output control works
- [ ] **GPIO interrupts work with edge detection** ⭐
- [ ] **Interrupt thread architecture implemented** ⭐
- [ ] **Rotary encoders work (validates interrupt quality)** ⭐
- [ ] Multiple GPIO pins can be used simultaneously
- [ ] Multiple interrupts can be active simultaneously
- [ ] Error handling is robust
- [ ] Performance validated (low CPU, low latency)

### Commit

```
feat(linux): add GPIO support with libgpiod including interrupts

- Implement LinuxGPIOPin using libgpiod chardev interface
- Support input/output modes with pull-up/pull-down
- Add configurable GPIO chip selection
- Map GPIO numbers to libgpiod line numbers
- **Implement GPIO interrupts using libgpiod edge events**
- **Add event monitoring thread for interrupt dispatch**
- **Support rising, falling, and both edge detection**
- Test with LED output, button input, and rotary encoder on Pi 5

GPIO digital I/O and interrupt handling verified with real hardware.
Interrupt latency measured at <20ms for user input scenarios.
```

---

## Implementation Notes

### Why Interrupt Support is Critical

Components that **require** GPIO interrupts and won't work without them:
- **Rotary Encoders** - Needs both-edge detection for quadrature signals
- **Pulse Counter** - Counts pulses via interrupts
- **Remote Receiver** - Decodes IR/RF signals with timing
- **Frequency Sensors** - Measures frequency via edge counting

### Interrupt Architecture Decision

**Chosen approach**: Dedicated thread per interrupt-enabled pin (or shared thread pool)

**Why not main loop polling?**
- ❌ Would add latency (loop cycle time dependent)
- ❌ Would miss rapid events
- ❌ Doesn't match ESPHome's interrupt semantics

**Why threads are OK here:**
- ✅ Thread sleeps (blocked on fd) until kernel wakes it → 0% CPU
- ✅ Matches ESP32 architecture (ISR task + queue)
- ✅ Proper isolation between pins
- ✅ Can use standard thread synchronization

### Performance Expectations

Based on kernel GPIO chardev architecture:
- **Interrupt latency**: 10-50 microseconds (kernel) + thread wakeup (1-10ms typical)
- **Total user callback latency**: < 20ms for most scenarios
- **CPU overhead**: ~0.1% per active interrupt (thread sleeping)
- **Event queue**: Kernel buffers events, no loss if callback is temporarily slow

This is **sufficient** for ESPHome use cases (human input, rotary encoders, door sensors).
Not suitable for: sub-millisecond timing, high-frequency PWM decoding (>10kHz).

### libgpiod Event Monitoring Flow

```
1. User calls attach_interrupt(callback, arg, INTERRUPT_RISING_EDGE)
        ↓
2. Request edge events: gpiod_line_request_rising_edge_events(line)
        ↓
3. Launch thread: interrupt_thread_ = std::thread([this]() { interrupt_loop_(); })
        ↓
4. Thread blocks on: gpiod_line_event_wait(line, timeout)
        |
        | (Thread SLEEPING here - 0% CPU usage)
        |
5. GPIO pin changes state → Hardware interrupt in kernel
        ↓
6. Kernel queues event in /dev/gpiochipX buffer
        ↓
7. Kernel wakes thread (file descriptor became readable)
        ↓
8. Thread reads event: gpiod_line_event_read(line, &event)
        ↓
9. Thread calls: isr_callback_(isr_arg_)
        ↓
10. Thread returns to step 4 (blocks again)
```

### Testing Strategy

1. **Basic interrupt**: Single button, verify rising/falling edges
2. **Multiple interrupts**: Two buttons on different pins simultaneously
3. **Rotary encoder**: Validates both-edge detection and timing
4. **Stress test**: Rapid button bouncing (debounce in ESPHome filters)
5. **Long callbacks**: Verify events aren't lost during slow ISR
6. **Cleanup**: Verify thread exits cleanly on detach/shutdown

### Reference Implementation

See `docs/linux-platform/wiringpi-analysis.md` Appendix C for conceptual implementation.

---

**Updated**: 2025-11-08
**Interrupt support added to Phase 4 plan**

---

