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
│  COMPLETED - NEEDS NETWORK SUPPORT                          │
├─────────────────────────────────────────────────────────────┤
│  Phase 8: OTA Updates Implementation                        │
└─────────────────────────────────────────────────────────────┘
                          ↓
┌─────────────────────────────────────────────────────────────┐
│  PENDING - CRITICAL BLOCKER                                 │
├─────────────────────────────────────────────────────────────┤
│  Phase 9: Network Support (TCP/IP Stack)                    │
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

## ✅ Phase 8: OTA Updates (COMPLETED - NEEDS NETWORK)

**Goal**: Over-the-air firmware updates
**Duration**: 2-3 days
**Hardware**: AMD server + Raspberry Pi
**Status**: ✅ Implementation complete, requires Phase 9 (network) to function

### What Was Built
- LinuxOTABackend class with versioned binary support
- Atomic symlink switching for updates
- MD5 verification before applying
- Automatic version cleanup (keeps last 2)
- Systemd service templates and installation scripts

### Key Files
- `esphome/components/ota/ota_backend_linux.h`
- `esphome/components/ota/ota_backend_linux.cpp`
- `scripts/linux/generate-systemd-service.py`
- `scripts/linux/install-service.sh`

### What's Pending
- ⏳ **Network support required** (Phase 9) for OTA to function
- ⏳ Hardware testing on Raspberry Pi after network implementation

### Deliverables
- ✅ OTA backend compiles successfully
- ✅ Version management and symlink logic implemented
- ✅ MD5 verification working
- ✅ Systemd integration scripts created
- ❌ **Cannot test without network layer**

[Details →](implementation/phase-8-ota-updates.md)

---

## ⏳ Phase 9: Network Support (PENDING - CRITICAL BLOCKER)

**Goal**: Enable network connectivity for OTA and API
**Duration**: 3-5 days
**Hardware**: AMD server + Raspberry Pi
**Status**: ⏳ Not started, blocks OTA and API functionality

### Architecture
- **POSIX sockets**: Use standard Linux socket API
- **No WiFi management**: Assume network configured by OS
- **TCP Server/Client**: For OTA (port 3232) and API (port 6053)
- **IPv4 + IPv6 support**: Work with both protocols
- **Non-blocking I/O**: Asynchronous operations

### What Needs to Be Built
- LinuxTCPServer class (bind, listen, accept)
- LinuxTCPClient class (connect, read, write)
- LinuxUDPSocket class (for future mDNS)
- Network utilities (IP detection, hostname)
- Integration with OTA and API components

### Key Design Points
- Assumes network already configured (NetworkManager, systemd-networkd)
- Uses kernel TCP/IP stack (no custom implementation)
- Interface agnostic (works with eth0, wlan0, any interface)
- Bind to all interfaces (IPv6 :: with IPv4-mapped support)

### Blocked Features
- ❌ OTA uploads (requires TCP on port 3232)
- ❌ API connections (requires TCP on port 6053)
- ❌ Home Assistant integration (requires API)
- ❌ Remote logging (requires API)

[Details →](implementation/phase-9-network-support.md)

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
Phase 8 (OTA) ──→ Phase 9 (Network) ──→ CRITICAL for OTA/API functionality
    ↓                                      (blocks remote management)
Phase 6 (Integration Testing) requires all phases 3-5 + 9
```

## Timeline Estimate

| Phase | Status | Estimated Remaining Time |
|-------|--------|--------------------------|
| 1-2   | ✅ Complete | 0 days |
| 3-5   | 🔧 Testing | 1-2 days (hardware testing) |
| 8     | ✅ Complete | 0 days (needs Phase 9 to function) |
| 9     | ⏳ Pending | 3-5 days (CRITICAL - blocks OTA/API) |
| 6     | ⏳ Pending | 1-2 days (after Phase 9) |
| 7     | ⏳ Pending | 1-2 days |
| **Total** | | **6-12 days remaining** |

*Note: Timeline assumes Raspberry Pi hardware is available for testing*

## Blocking Issues

### Current Blockers
- **Phase 9 (Network)**: CRITICAL blocker for OTA and API functionality
  - OTA backend complete but cannot receive updates without network
  - API component compiles but cannot accept connections
  - Remote management impossible without network layer

- **Hardware Access**: Phases 3-5 require Raspberry Pi for testing
  - Can proceed in parallel with Phase 9 implementation

### Resolution
- **Priority 1**: Implement Phase 9 (Network Support) - enables remote management
- **Priority 2**: Hardware testing of Phases 3-5 (I2C, GPIO, SPI)
- **Priority 3**: Integration testing (Phase 6) after Phases 3-5 and 9 complete

## Next Actions

1. **Option A: Network Implementation Path** (Recommended - Unblocks OTA/API)
   - Implement Phase 9 (Network Support)
   - Start with LinuxTCPServer and LinuxTCPClient classes
   - Integrate with OTA component for remote updates
   - Integrate with API component for Home Assistant
   - Test on x86_64 and Raspberry Pi

2. **Option B: Hardware Testing Path** (Can run in parallel)
   - Setup Raspberry Pi 5 with ESPHome environment
   - Test Phase 3 (I2C) with real sensors
   - Test Phase 4 (GPIO) with LED/button
   - Test Phase 5 (SPI) with display
   - Verify all hardware components work

3. **Option C: Parallel Path** (Recommended if multiple developers)
   - **Developer 1**: Network implementation (Phase 9) - CRITICAL
   - **Developer 2**: Hardware testing (Phases 3-5)
   - **Converge**: Integration testing (Phase 6) with network + hardware
   - **Final**: Documentation and polish (Phase 7)

---

**See [CURRENT.md](CURRENT.md) for what's actively being worked on**
