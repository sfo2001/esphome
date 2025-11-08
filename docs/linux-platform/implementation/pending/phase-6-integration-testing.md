# Phase 6: Integration Testing

[← Previous Phase](../phase-5-spi.md) | [← Back to README](../../README.md) | [→ Next Phase](phase-7-documentation.md) | [Roadmap](../../ROADMAP.md)

**Status**: ⏳ Pending - Not Started

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

