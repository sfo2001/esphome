# Linux Platform Implementation

**Project**: Add Linux (Raspberry Pi) as ESPHome target platform
**Branch**: `feature/linux-platform`
**Base**: `dev` branch
**Target**: Production-ready Linux platform with I2C, GPIO, SPI, and OTA support

## Use Case

Run ESPHome on Raspberry Pi to control I2C-connected smart displays (Atmel-based) integrated with PiCorePlayer Squeezebox system.

## Available Hardware

- **Primary**: Raspberry Pi 5 (ARMv8, 64-bit)
- **Secondary**: Raspberry Pi 3 (ARMv7, 32-bit)
- **Legacy**: Raspberry Pi 1 (2011 model)

## Current Status

### Progress Overview

| Phase | Status | Progress | Location |
|-------|--------|----------|----------|
| **1. Platform Foundation** | ✅ Complete | 100% | [completed/](implementation/completed/phase-1-platform-foundation.md) |
| **2. Preferences/Storage** | ✅ Complete | 100% | [completed/](implementation/completed/phase-2-preferences.md) |
| **3. I2C Implementation** | 🔧 Code Complete | Testing Pending | [active](implementation/phase-3-i2c.md) |
| **4. GPIO Implementation** | 🔧 Code Complete | Testing Pending | [active](implementation/phase-4-gpio.md) |
| **5. SPI Implementation** | 🔧 Code Complete | Testing Pending | [active](implementation/phase-5-spi.md) |
| **6. Integration Testing** | ⏳ Pending | 0% | [pending/](implementation/pending/phase-6-integration-testing.md) |
| **7. Documentation & Polish** | ⏳ Pending | 0% | [pending/](implementation/pending/phase-7-documentation.md) |
| **8. OTA Updates** | 📝 Designed | Ready to Implement | [active](implementation/phase-8-ota-updates.md) |

### What's Next?

**Active Work**: Phases 3, 4, 5, and 8
- **Hardware Testing Needed**: Phases 3-5 need Raspberry Pi for testing I2C, GPIO, and SPI
- **Ready to Implement**: Phase 8 (OTA) architecture is documented, implementation can begin
- **Blocking**: Raspberry Pi hardware access required for integration testing

**Quick Start** → See [CURRENT.md](CURRENT.md) for what's actively being worked on

## Quick Navigation

### 📋 Planning & Roadmap
- **[ROADMAP.md](ROADMAP.md)** - Overview of all 8 phases with brief descriptions
- **[CURRENT.md](CURRENT.md)** - Current active work (Phases 3-5, 8)

### 🚀 Implementation Details
- **[implementation/](implementation/)** - Detailed phase documentation
  - **[completed/](implementation/completed/)** - Finished phases (1-2)
  - **[pending/](implementation/pending/)** - Not yet started (6-7)
  - **Active phases** (3-5, 8) - Root level of implementation/

### 📚 Reference Materials
- **[reference/commands.md](reference/commands.md)** - Useful development commands
- **[reference/troubleshooting.md](reference/troubleshooting.md)** - Common issues and solutions

### 📝 Notes & Discoveries
- **[notes/implementation-notes.md](notes/implementation-notes.md)** - Insights, gotchas, decisions
- **[notes/code-review.md](notes/code-review.md)** - Code quality audit results

## Development Strategy

### Compilation Approach

**Phases 1-2** (No Hardware):
- **Where**: AMD development server (x86_64)
- **Why**: Fast iteration, no hardware dependencies
- **Testing**: Basic platform structure, preferences

**Phases 3-8** (Requires Hardware):
- **Where**: Raspberry Pi 5 (ARM native compilation)
- **Why**: Real hardware required, guaranteed compatibility
- **Testing**: I2C, GPIO, SPI, OTA, full integration

## Architecture Highlights

### Platform Components

1. **Core HAL** (`components/linux/core.cpp`)
   - POSIX-based timing (clock_gettime, nanosleep)
   - Thread model: MULTI_ATOMICS
   - Native compilation via PlatformIO

2. **I2C Bus** (`components/i2c/i2c_bus_linux.cpp`)
   - Uses `/dev/i2c-X` kernel interface
   - I2C_RDWR ioctl for atomic transactions
   - Configurable bus number

3. **GPIO** (`components/linux/gpio.cpp`)
   - libgpiod chardev interface (modern GPIO access)
   - Supports input/output, pull-up/pull-down
   - Configurable GPIO chip (gpiochip0)

4. **SPI Bus** (`components/spi/spi_bus_linux.cpp`)
   - Uses `/dev/spidevX.Y` kernel interface
   - Full-duplex and half-duplex transfers
   - Configurable bus and device numbers

5. **Preferences** (`components/linux/preferences.cpp`)
   - File-based persistence
   - Configurable storage path
   - Binary format with C++ streams

6. **OTA Updates** (Designed, not yet implemented)
   - Versioned binaries with symlink switching
   - Atomic updates, version history
   - Systemd-managed lifecycle

## Quick Start

### Initial Setup (Development Server)

```bash
# Clone and checkout branch
git clone <your-fork-url>
cd esphome
git checkout feature/linux-platform

# Setup Python environment
python3 -m venv venv
source venv/bin/activate
pip install -e .

# Test compilation
esphome compile tests/components/linux/test.linux.yaml
```

### Hardware Testing (Raspberry Pi)

```bash
# On Raspberry Pi
# 1. Install dependencies
sudo apt-get install -y i2c-tools libi2c-dev libgpiod-dev gpiod

# 2. Enable hardware interfaces
sudo raspi-config
# Enable I2C, GPIO, SPI

# 3. Add user to groups
sudo usermod -aG i2c,gpio,spi $USER
# Log out and back in

# 4. Clone and setup
git clone <your-fork-url>
cd esphome
git checkout feature/linux-platform
python3 -m venv venv
source venv/bin/activate
pip install -e .

# 5. Test I2C
esphome compile tests/components/linux/test_i2c.linux.yaml
.esphome/build/test_i2c/test_i2c

# 6. Check I2C devices
i2cdetect -y 1
```

## Success Criteria

### Minimum Viable Product (MVP)

- [x] Platform compiles and runs on x86_64
- [x] Platform compiles and runs on ARM
- [x] Preferences persist across restarts
- [ ] I2C communication works with real hardware
- [ ] GPIO digital I/O works with real hardware
- [ ] SPI communication works with real hardware
- [ ] OTA updates work
- [ ] API integration works with Home Assistant
- [ ] PiCorePlayer use case validated

### Production Ready

- [ ] All MVP criteria met
- [ ] Comprehensive error handling
- [ ] Performance validated (no lag in I2C display updates)
- [ ] Memory stable (no leaks)
- [ ] Complete user documentation
- [ ] Example configurations
- [ ] Tests added for major components
- [ ] Code passes all linters

## Contributing

See [ROADMAP.md](ROADMAP.md) for detailed implementation plans for each phase.

When working on a phase:
1. Read the phase documentation in `implementation/`
2. Follow the task checklist
3. Update progress in this README
4. Document discoveries in `notes/`

## Resources

- **Original Implementation Plan**: `docs/linux-platform-implementation.md` (deprecated, kept for reference)
- **Code Review**: `notes/code-review.md`
- **ESPHome Docs**: https://esphome.io/
- **ESPHome Contributing Guide**: https://esphome.io/guides/contributing.html

---

**Last Updated**: 2025-11-08
**Current Focus**: Hardware testing of I2C/GPIO/SPI + OTA implementation design
