# Linux Platform Implementation Roadmap

This document provides a high-level overview of all implementation phases. For detailed task lists and implementation guides, see the individual phase documents in `implementation/`.

## Phase Overview

```
┌─────────────────────────────────────────────────────────────┐
│  COMPLETED                                                  │
├─────────────────────────────────────────────────────────────┤
│  Phase 1: Platform Foundation                               │
│  Phase 2: Preferences/Storage                               │
└─────────────────────────────────────────────────────────────┘
                          ↓
┌─────────────────────────────────────────────────────────────┐
│  ACTIVE - CODE COMPLETE, TESTING PENDING                    │
├─────────────────────────────────────────────────────────────┤
│  Phase 3: I2C Implementation                                │
│  Phase 4: GPIO Implementation                               │
│  Phase 5: SPI Implementation                                │
└─────────────────────────────────────────────────────────────┘
                          ↓
┌─────────────────────────────────────────────────────────────┐
│  ACTIVE - ARCHITECTURE DESIGNED                             │
├─────────────────────────────────────────────────────────────┤
│  Phase 8: OTA Updates Implementation                        │
└─────────────────────────────────────────────────────────────┘
                          ↓
┌─────────────────────────────────────────────────────────────┐
│  PENDING                                                    │
├─────────────────────────────────────────────────────────────┤
│  Phase 6: Integration Testing                               │
│  Phase 7: Documentation & Polish                            │
└─────────────────────────────────────────────────────────────┘
```

---

## ✅ Phase 1: Platform Foundation (COMPLETED)

**Goal**: Basic platform registration and compilation
**Duration**: 1-2 days
**Hardware**: None (x86_64 development server)
**Status**: ✅ 100% Complete

### What Was Built
- Platform registration in ESPHome (PLATFORM_LINUX)
- Core HAL implementation using POSIX APIs
- Basic setup() and loop() program execution
- PlatformIO native platform integration

### Key Files
- `esphome/components/linux/__init__.py` - Platform registration
- `esphome/components/linux/core.cpp` - HAL implementation
- `esphome/const.py` - Platform constants

### Deliverables
- ✅ Platform compiles on x86_64
- ✅ Basic program runs with setup() and loop()
- ✅ Test configuration created

[Details →](implementation/completed/phase-1-platform-foundation.md)

---

## ✅ Phase 2: Preferences/Storage (COMPLETED)

**Goal**: Persistent configuration storage
**Duration**: 0.5-1 day
**Hardware**: None (x86_64 development server)
**Status**: ✅ 100% Complete

### What Was Built
- File-based preferences system
- Configurable storage path
- Directory auto-creation
- Binary persistence format

### Key Files
- `esphome/components/linux/preferences.h` - Preferences classes
- `esphome/components/linux/preferences.cpp` - File-based storage

### Deliverables
- ✅ Preferences save to filesystem
- ✅ Preferences load on restart
- ✅ Directory auto-creation works
- ✅ Error handling for permission issues

[Details →](implementation/completed/phase-2-preferences.md)

---

## 🔧 Phase 3: I2C Implementation (CODE COMPLETE)

**Goal**: Real I2C hardware support
**Duration**: 2-3 days
**Hardware**: Raspberry Pi 5 required
**Status**: 🔧 Code complete, hardware testing pending

### What Was Built
- LinuxI2CBus using i2c-dev kernel interface
- Configurable bus number (default: /dev/i2c-1)
- Combined write-read transactions via I2C_RDWR ioctl
- Error mapping from Linux errno to ESPHome codes

### Key Files
- `esphome/components/i2c/i2c_bus_linux.h`
- `esphome/components/i2c/i2c_bus_linux.cpp`

### What's Pending
- ⏳ Hardware testing on Raspberry Pi
- ⏳ I2C scan validation
- ⏳ Testing with real I2C sensors (ADS1115, BME280, etc.)

[Details →](implementation/phase-3-i2c.md)

---

## 🔧 Phase 4: GPIO Implementation (CODE COMPLETE)

**Goal**: Real GPIO control using libgpiod
**Duration**: 2-3 days
**Hardware**: Raspberry Pi 5 required
**Status**: 🔧 Code complete, hardware testing pending

### What Was Built
- LinuxGPIOPin using libgpiod chardev interface
- Support for input/output modes
- Pull-up/pull-down resistors
- Configurable GPIO chip (gpiochip0)

### Key Files
- `esphome/components/linux/gpio.h`
- `esphome/components/linux/gpio.cpp`
- `esphome/components/linux/gpio.py` - Pin schema

### What's Pending
- ⏳ Hardware testing on Raspberry Pi
- ⏳ LED output testing
- ⏳ Button input testing with pull-up/pull-down

[Details →](implementation/phase-4-gpio.md)

---

## 🔧 Phase 5: SPI Implementation (CODE COMPLETE)

**Goal**: Real SPI hardware support
**Duration**: 2-3 days
**Hardware**: Raspberry Pi 5 required
**Status**: 🔧 Code complete, hardware testing pending

### What Was Built
- LinuxSPIBus using spidev kernel interface
- Configurable bus and device numbers
- Full-duplex and half-duplex transfers
- SPI mode, speed, and bit configuration

### Key Files
- `esphome/components/spi/spi_bus_linux.h`
- `esphome/components/spi/spi_bus_linux.cpp`

### What's Pending
- ⏳ Hardware testing on Raspberry Pi
- ⏳ Testing with SPI displays (ST7789, ILI9341)
- ⏳ Testing with SPI sensors

[Details →](implementation/phase-5-spi.md)

---

## 📝 Phase 8: OTA Updates (ARCHITECTURE DESIGNED)

**Goal**: Over-the-air firmware updates
**Duration**: 2-3 days
**Hardware**: AMD server + Raspberry Pi
**Status**: 📝 Architecture documented, ready to implement

### Architecture
- **Versioned binaries**: mydevice.001, mydevice.002, etc.
- **Symlink switching**: Atomic updates via symlink rename
- **Systemd integration**: Auto-restart with new version
- **Version cleanup**: Keep last N versions for rollback

### What Needs to Be Built
- LinuxOTABackend class
- Version management logic
- Symlink update mechanism
- Systemd service templates
- Installation scripts

### Key Design Points
- Same OTA protocol as ESP platforms (port 3232)
- MD5 verification before applying
- Manual rollback capability
- No dual-partition complexity

[Details →](implementation/phase-8-ota-updates.md)

---

## ⏳ Phase 6: Integration Testing (PENDING)

**Goal**: Verify all components work together
**Duration**: 1-2 days
**Hardware**: Raspberry Pi 5, Pi 3, Pi 1
**Status**: ⏳ Not started

### Planned Testing
- I2C + GPIO + SPI simultaneously
- Multiple I2C devices on same bus
- Performance validation
- Memory leak detection
- PiCorePlayer integration (actual use case)

### Target Platforms
- Raspberry Pi 5 (ARMv8, 64-bit)
- Raspberry Pi 3 (ARMv7, 32-bit)
- Raspberry Pi 1 (compatibility check)

### Success Criteria
- All components work without conflicts
- Stable operation for 1+ hours
- Acceptable performance
- No memory leaks

[Details →](implementation/pending/phase-6-integration-testing.md)

---

## ⏳ Phase 7: Documentation & Polish (PENDING)

**Goal**: Production-ready with complete documentation
**Duration**: 1-2 days
**Hardware**: N/A
**Status**: ⏳ Not started

### Planned Work
- User documentation (esphome-docs repository)
- Setup guide for Raspberry Pi
- Example configurations
- Error message improvements
- Performance optimization
- CI/CD integration

### Documentation Scope
- Platform overview page
- Raspberry Pi setup guide
- I2C, GPIO, SPI configuration examples
- OTA setup guide
- Troubleshooting guide

[Details →](implementation/pending/phase-7-documentation.md)

---

## Dependencies

```
Phase 1 (Foundation)
    ↓
Phase 2 (Preferences)
    ↓
Phase 3 (I2C) ─┐
Phase 4 (GPIO) ┼─→ Phase 6 (Integration) → Phase 7 (Documentation)
Phase 5 (SPI) ─┘
    ↓
Phase 8 (OTA) ──→ (Can be done in parallel with Phase 6/7)
```

## Timeline Estimate

| Phase | Status | Estimated Remaining Time |
|-------|--------|--------------------------|
| 1-2   | ✅ Complete | 0 days |
| 3-5   | 🔧 Testing | 1-2 days (hardware testing) |
| 8     | 📝 Designed | 2-3 days (implementation) |
| 6     | ⏳ Pending | 1-2 days |
| 7     | ⏳ Pending | 1-2 days |
| **Total** | | **5-9 days remaining** |

*Note: Timeline assumes Raspberry Pi hardware is available for testing*

## Blocking Issues

### Current Blockers
- **Hardware Access**: Phases 3-6 require Raspberry Pi for testing
- **None for Phase 8**: OTA implementation can proceed independently

### Resolution
- Phase 8 can be implemented and tested on x86_64 first
- Hardware testing can be batched (test phases 3-5 together)
- Integration testing (Phase 6) requires all previous phases working

## Next Actions

1. **Option A: Hardware Testing Path**
   - Setup Raspberry Pi 5 with ESPHome environment
   - Test Phase 3 (I2C) with real sensors
   - Test Phase 4 (GPIO) with LED/button
   - Test Phase 5 (SPI) with display
   - Proceed to Phase 6 integration testing

2. **Option B: OTA Implementation Path**
   - Implement LinuxOTABackend
   - Create systemd service templates
   - Test OTA on x86_64 development server
   - Test OTA on Raspberry Pi after hardware validation

3. **Option C: Parallel Path** (Recommended if multiple developers)
   - One person: Hardware testing (Phases 3-5)
   - Another person: OTA implementation (Phase 8)
   - Converge for integration testing (Phase 6)

---

**See [CURRENT.md](CURRENT.md) for what's actively being worked on**
