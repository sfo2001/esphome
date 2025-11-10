# Current Active Work

[← Back to README](README.md) | [Roadmap](ROADMAP.md)

**Last Updated**: 2025-11-08

## What's Being Worked On

This document provides quick links to the currently active implementation phases.

---

## ✅ Recently Completed Phases

### Phase 8: OTA Updates Implementation
**Status**: ✅ **COMPLETE** - Code merged and functional

**What Was Built**:
- ✅ LinuxOTABackend class with versioned binary support
- ✅ Atomic symlink switching for updates
- ✅ MD5 verification before applying
- ✅ Automatic version cleanup (keeps last 2)
- ✅ Systemd service templates and installation scripts
- ✅ Exception-safe parsing (compatible with `-fno-exceptions`)

**What's Pending**:
- ⏳ Hardware testing on Raspberry Pi (requires network)

**Merged**: PR #11
**Commit**: `3b986ff5e` - feat(linux): add OTA update support with versioned binaries

[📄 View Details →](implementation/phase-8-ota-updates.md)

---

### Phase 9: Network Support
**Status**: ✅ **COMPLETE** - Network layer functional

**What Was Built**:
- ✅ LinuxTCPServer class for bind/listen/accept operations
- ✅ LinuxTCPClient class for TCP connections
- ✅ LinuxUDPSocket class for future mDNS support
- ✅ Network utilities (IP detection using getifaddrs)
- ✅ Integration with existing ESPHome network abstraction
- ✅ IPv4/IPv6 dual-stack support with POSIX sockets

**Architecture Discovery**:
- ESPHome already had excellent platform abstractions
- Socket abstraction layer automatically uses BSD sockets on Linux
- OTA and API components work on Linux without modification
- Only needed to implement Linux-specific network utilities

**What's Pending**:
- ⏳ Hardware testing on Raspberry Pi

**Merged**: PR #12
**Commit**: `69e88c7f3` - feat(linux): implement Phase 9 network support for OTA and API

[📄 View Details →](implementation/phase-9-network-support.md)

---

## 🔧 Active Phases - Hardware Testing Needed

These phases have code complete but require Raspberry Pi hardware for validation:

### Phase 3: I2C Implementation
**Status**: 🔧 Code Complete - Awaiting Hardware Testing

**What's Done**:
- ✅ LinuxI2CBus implementation using i2c-dev
- ✅ Configuration schema
- ✅ Test configurations created

**What's Needed**:
- ⏳ Test on Raspberry Pi with real I2C devices
- ⏳ Validate I2C scan functionality
- ⏳ Test with multiple sensors (ADS1115, BME280)

[📄 View Details →](implementation/phase-3-i2c.md)

---

### Phase 4: GPIO Implementation
**Status**: 🔧 Code Complete - Awaiting Hardware Testing

**What's Done**:
- ✅ LinuxGPIOPin implementation using libgpiod
- ✅ Input/output modes, pull-up/pull-down support
- ✅ Test configurations created

**What's Needed**:
- ⏳ Test LED output control
- ⏳ Test button input with pull-up/pull-down
- ⏳ Validate on Raspberry Pi hardware

[📄 View Details →](implementation/phase-4-gpio.md)

---

### Phase 5: SPI Implementation
**Status**: 🔧 Code Complete - Awaiting Hardware Testing

**What's Done**:
- ✅ LinuxSPIBus implementation using spidev
- ✅ Full-duplex and half-duplex support
- ✅ Test configurations created

**What's Needed**:
- ⏳ Test with SPI display (ST7789, ILI9341)
- ⏳ Test with SPI sensors
- ⏳ Validate on Raspberry Pi hardware

[📄 View Details →](implementation/phase-5-spi.md)

---

## 📝 Proposed Enhancement - Optional UX Improvement

### Phase 9.5: Linux Deployment Strategies
**Status**: 📝 Design Complete - Optional Enhancement

**Purpose**: Streamline initial deployment to Linux systems via SSH

**Proposed Approaches**:
1. **`prepare-linux` command** - Generate deployment package (quick win)
2. **SSH deploy integration** - `esphome run --device ssh://pi@raspberrypi` (long-term)

**Current State**: Manual deployment works but requires multiple steps

**Note**: This is an optional UX improvement. Core functionality (OTA) is already working.

[📄 View Details →](implementation/phase-9.5-linux-deployment.md)

---

## Next Actions

### Priority 1: Hardware Testing (Primary Focus)

**Network + OTA are complete!** Now we need to validate everything on actual hardware.

1. **Setup Raspberry Pi 5 or Pi 3**
   - Install ESPHome development environment
   - Enable I2C, GPIO, SPI interfaces
   - Add user to hardware groups
   - Configure network connectivity

2. **Test Network + OTA Integration**
   - Deploy test binary to Raspberry Pi
   - Verify network utilities detect IP address
   - Test OTA update over network
   - Validate version management and symlink switching
   - Test systemd service restart mechanism

3. **Test Phase 3 (I2C)**
   - Connect I2C sensor (BME280, ADS1115)
   - Run test configuration
   - Validate I2C scan and communication

4. **Test Phase 4 (GPIO)**
   - Connect LED to GPIO 27
   - Connect button to GPIO 17
   - Validate input/output, pull-up/pull-down

5. **Test Phase 5 (SPI)**
   - Connect SPI display or sensor
   - Validate SPI communication
   - Test multiple SPI devices

6. **Integration Testing (Phase 6)**
   - Compile configuration with I2C + GPIO + SPI + OTA + API
   - Test all components working simultaneously
   - Validate performance and stability
   - Test OTA updates with all components active
   - Test API connection from Home Assistant

### Priority 2: Optional UX Improvements

If time permits and after hardware validation:

1. **Implement Phase 9.5.1** (`prepare-linux` command)
   - Generate deployment package
   - Self-contained installer script
   - Better than manual deployment

2. **Consider Phase 9.5.2** (SSH deploy integration)
   - Integrate with `esphome run --device ssh://...`
   - Automatic binary deployment via SSH
   - Matches ESP32 UX perfectly

---

## Status Dashboard

| Component | Code | Compilation | Network | Hardware Test | Integration |
|-----------|------|-------------|---------|---------------|-------------|
| Platform Foundation | ✅ | ✅ | ✅ | ✅ | ✅ |
| Preferences | ✅ | ✅ | ✅ | ✅ | ✅ |
| Network Support | ✅ | ✅ | ✅ | ⏳ | ⏳ |
| OTA Updates | ✅ | ✅ | ✅ | ⏳ | ⏳ |
| I2C | ✅ | ✅ | N/A | ⏳ | ⏳ |
| GPIO | ✅ | ✅ | N/A | ⏳ | ⏳ |
| SPI | ✅ | ✅ | N/A | ⏳ | ⏳ |

**Legend**:
- ✅ Complete
- ⏳ Pending
- N/A - Not applicable
- ❌ Blocked

---

## Resolved Blockers ✅

### ~~Phase 9 (Network): CRITICAL blocker~~
- **Was Blocking**: OTA and API functionality
- **Resolution**: ✅ **RESOLVED** - Network layer implemented and functional
  - Network utilities implemented using getifaddrs()
  - OTA and API can now receive connections over network
  - Remote management now possible

---

## Current Blockers

### Hardware Access Required
- **Blocker**: Phases 3-5 and OTA/Network require Raspberry Pi for testing
- **Impact**: Cannot validate hardware implementations
- **Resolution**: Obtain Raspberry Pi 5 or Pi 3 for testing
- **Note**: All code is complete and compiles successfully

---

## Key Implementation Files

### OTA Backend (Phase 8)
- `esphome/components/ota/ota_backend_linux.h`
- `esphome/components/ota/ota_backend_linux.cpp`
- `esphome/components/ota/__init__.py` (backend registration)

### Network Layer (Phase 9)
- `esphome/components/network/linux_network.h`
- `esphome/components/network/linux_network.cpp`
- `esphome/components/network/util.cpp` (network utilities integration)

### Test Configurations
- `tests/components/linux/test_ota.linux.yaml`
- `tests/components/linux/test_network.linux.yaml`

---

## Quick Links

- **Setup Raspberry Pi** → [reference/commands.md#raspberry-pi-setup](reference/commands.md)
- **Troubleshooting** → [reference/troubleshooting.md](reference/troubleshooting.md)
- **Implementation Notes** → [notes/implementation-notes.md](notes/implementation-notes.md)
- **All Phases** → [ROADMAP.md](ROADMAP.md)

---

**👉 Start Here**: Focus on **Priority 1: Hardware Testing** to validate all completed implementations on Raspberry Pi.
