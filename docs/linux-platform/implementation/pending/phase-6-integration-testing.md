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

### Your Specific Use Case: PiCorePlayer Integration

#### Understanding PiCorePlayer Architecture

piCorePlayer is built on **Tiny Core Linux**, which:
- **Runs entirely in RAM** - The system boots from a read-only SquashFS image
- **Does not use traditional package managers** - No apt, yum, or pacman
- **Uses TCZ extensions** - SquashFS packages that are loop-mounted at boot
- **Requires backup for persistence** - Changes must be saved with `pcp bu` command
- **Does not use systemd** - Uses traditional init scripts via `/opt/bootlocal.sh`

#### ESPHome Integration Approaches

There are **three main approaches** to integrate ESPHome with piCorePlayer:

##### **Approach 1: Create a TCZ Extension (Recommended)**

This is the cleanest method for deploying binaries on piCorePlayer.

**Advantages:**
- Clean integration with piCorePlayer's extension system
- Proper dependency management
- Easy to distribute and install
- Follows Tiny Core Linux best practices

**Process:**
1. **Compile ESPHome binary** on a development system (or cross-compile for ARM)
   ```bash
   esphome compile picore_display_controller.yaml
   ```

2. **Identify library dependencies** using `ldd`:
   ```bash
   ldd .esphome/build/picore_display_controller/picore_display_controller
   ```

3. **Create TCZ directory structure**:
   ```bash
   mkdir -p tcz/usr/local/bin
   mkdir -p tcz/usr/local/etc/esphome
   cp .esphome/build/picore_display_controller/picore_display_controller tcz/usr/local/bin/
   cp picore_display_controller.yaml tcz/usr/local/etc/esphome/
   ```

4. **Create the TCZ extension** using `mksquashfs`:
   ```bash
   mksquashfs tcz esphome-picore.tcz -b 4k -no-xattrs
   ```

5. **Create dependency file** (esphome-picore.tcz.dep):
   ```
   pigpio.tcz
   ```
   Note: piCorePlayer uses **pigpio** for GPIO, not libgpiod or lgpio

6. **Create info file** (esphome-picore.tcz.info):
   ```
   Title:          esphome-picore.tcz
   Description:    ESPHome for piCorePlayer
   Version:        2024.x.x
   Author:         Your Name
   Original-site:  https://esphome.io
   Copying-policy: GPL
   Size:           XXXkB
   Extension_by:   Your Name
   Comments:       ESPHome compiled for piCorePlayer
   Change-log:     Initial release
   Current:        2024.x.x
   ```

7. **Install on piCorePlayer**:
   ```bash
   # Copy to piCorePlayer
   scp esphome-picore.tcz* tc@picoreplayer.local:/mnt/mmcblk0p2/tce/optional/

   # SSH to piCorePlayer
   ssh tc@picoreplayer.local

   # Add to onboot list
   echo "esphome-picore.tcz" >> /mnt/mmcblk0p2/tce/onboot.lst

   # Load immediately (or reboot)
   tce-load -i esphome-picore.tcz
   ```

8. **Create startup script** in `/opt/bootlocal.sh`:
   ```bash
   # Start ESPHome service
   /usr/local/bin/picore_display_controller 2>&1 >> /var/log/esphome.log &
   ```

9. **Backup changes**:
   ```bash
   pcp bu
   ```

**Reference Examples:**
- [Snapcast TCZ Extension](https://github.com/m-kloeckner/snapcast-tcz)
- [RoonBridge Extension](https://github.com/aposcic/pcp-roonbridge-extension)

##### **Approach 2: Use /opt Directory (Simpler, for Testing)**

For quick testing or if you don't need distribution:

1. **Compile binary** and copy to piCorePlayer:
   ```bash
   esphome compile picore_display_controller.yaml
   scp -r .esphome/build/picore_display_controller tc@picoreplayer.local:/opt/esphome/
   ```

2. **Add to bootlocal.sh**:
   ```bash
   ssh tc@picoreplayer.local
   sudo vi /opt/bootlocal.sh

   # Add before #pCPstart line:
   /opt/esphome/picore_display_controller 2>&1 >> /opt/esphome/esphome.log &
   ```

3. **Backup**:
   ```bash
   pcp bu
   ```

**Note:** The `/opt` directory is included in piCorePlayer's backup system by default.

##### **Approach 3: Run from Persistent Partition**

Store the binary directly on the SD card's persistent partition:

```bash
# Copy to persistent storage
scp -r .esphome/build/picore_display_controller tc@picoreplayer.local:/mnt/mmcblk0p2/esphome/

# Add to bootlocal.sh
/mnt/mmcblk0p2/esphome/picore_display_controller 2>&1 >> /var/log/esphome.log &

# Backup
pcp bu
```

#### GPIO Library Considerations

**Critical:** ESPHome's Linux platform needs to be compatible with piCorePlayer's GPIO libraries.

- **piCorePlayer uses pigpio.tcz**, not libgpiod or lgpio
- **Raspberry Pi 5 note**: sysfs GPIO is deprecated; Pi 5 uses the new GPIO character device interface
- **piCorePlayer 9.x** includes `raspi-utils` package with `pinctrl` for hardware-level GPIO manipulation

**Action Items:**
- [ ] Verify ESPHome Linux GPIO implementation works with pigpio
- [ ] Test GPIO compatibility on different Pi models (especially Pi 5)
- [ ] Consider using `/dev/gpiochipX` character device interface for maximum compatibility
- [ ] Document any GPIO library dependencies in TCZ extension

#### Integration Testing Tasks

- [ ] **Create test configuration**:
  ```yaml
  esphome:
    name: picore_display_controller
    platform: linux
    comment: "I2C display controller for PiCorePlayer"

  linux:
    gpio_chip: gpiochip0
    preferences_path: /opt/esphome/preferences  # Use /opt for persistence

  api:
    password: !secret api_password

  logger:
    level: INFO
    logs:
      component: DEBUG  # Debug logs for troubleshooting

  i2c:
    bus_num: 1
    frequency: 100kHz
    scan: true

  # Your Atmel-based smart displays
  # (Add specific component once identified - may need custom component)
  # Example placeholder:
  # display:
  #   - platform: [your_display_component]
  #     i2c_id: i2c_bus
  #     address: 0x3C
  #     # ... display config ...

  # Integration with PiCorePlayer (if needed)
  # - Read playback status via D-Bus or HTTP API
  # - Display track info on I2C displays
  # - Control buttons via GPIO
  ```

- [ ] **Test Approach 2 first (simplest)**:
  - [ ] Compile ESPHome binary on development system
  - [ ] Copy to piCorePlayer's `/opt/esphome/`
  - [ ] Create startup script in `/opt/bootlocal.sh`
  - [ ] Test manual startup
  - [ ] Reboot and verify auto-start
  - [ ] Monitor logs in `/opt/esphome/esphome.log`

- [ ] **Test with actual I2C displays**:
  - [ ] Identify exact display component/protocol
  - [ ] Verify I2C bus is accessible (run `i2cdetect -y 1`)
  - [ ] Test display initialization
  - [ ] Test display updates
  - [ ] Verify no conflicts with PiCorePlayer

- [ ] **Performance testing**:
  - [ ] Measure CPU usage: `top` or `htop`
  - [ ] Monitor memory usage
  - [ ] Verify no audio glitches from Squeezelite playback
  - [ ] Test I2C display update frequency
  - [ ] Run for extended period (>1 hour) to check stability

- [ ] **Create TCZ extension** (after testing):
  - [ ] Package binary and config files
  - [ ] Create dependency list (.tcz.dep)
  - [ ] Create info file (.tcz.info)
  - [ ] Test installation from TCZ
  - [ ] Validate with submitqc tool (if submitting to repository)

#### Dependency Management

**Required Dependencies:**
- **pigpio.tcz** - GPIO library (if using GPIO)
- **i2c-tools.tcz** - I2C utilities (for debugging)
- **Python runtime** - Only if using Python-based components (unlikely for compiled binary)

**To check installed extensions:**
```bash
cat /mnt/mmcblk0p2/tce/onboot.lst
```

**To install missing extensions:**
1. Use piCorePlayer web interface: Main Page → Extensions → Available
2. Or via command line: `tce-load -wi pigpio.tcz`

#### PiCorePlayer-Specific Considerations

- [ ] **Audio priority**: Ensure ESPHome doesn't interfere with Squeezelite audio
- [ ] **Resource usage**: piCorePlayer runs on constrained systems; monitor CPU/RAM
- [ ] **Backup strategy**: Always test `pcp bu` and restore after reboot
- [ ] **Update compatibility**: Document which piCorePlayer versions are tested
- [ ] **Squeezelite integration**: Test both when Squeezelite is playing and idle

#### Troubleshooting

**Common Issues:**

1. **Changes disappear after reboot**
   - Solution: Run `pcp bu` after making changes

2. **Binary fails to start**
   - Check dependencies with `ldd /opt/esphome/picore_display_controller`
   - Verify executable permissions: `chmod +x /opt/esphome/picore_display_controller`
   - Check logs: `tail -f /opt/esphome/esphome.log`

3. **GPIO access denied**
   - Ensure user `tc` has GPIO permissions
   - Load pigpio.tcz extension
   - Consider running as root (add `sudo` to bootlocal.sh)

4. **I2C devices not detected**
   - Load i2c-tools.tcz: `tce-load -wi i2c-tools.tcz`
   - Check kernel modules: `lsmod | grep i2c`
   - Verify wiring and device addresses: `i2cdetect -y 1`

5. **Service doesn't start at boot**
   - Verify `/opt/bootlocal.sh` is executable
   - Check log: `cat /var/log/pcp_boot.log`
   - Ensure backup was performed: `ls -lh /mnt/mmcblk0p2/tce/mydata.tgz`

#### Documentation Needs

- [ ] Create installation guide for end users
- [ ] Document TCZ extension creation process
- [ ] Provide troubleshooting steps
- [ ] Document compatibility matrix (piCorePlayer versions, Pi models)
- [ ] Create example configurations
- [ ] Document PiCorePlayer-specific limitations

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

