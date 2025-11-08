# Phase 1: Platform Foundation

[← Back to README](../../README.md) | [→ Next Phase](phase-2-preferences.md) | [Roadmap](../../ROADMAP.md)

**Status**: ✅ Complete

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

