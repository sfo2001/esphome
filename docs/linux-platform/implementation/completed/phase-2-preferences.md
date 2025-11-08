# Phase 2: Preferences/Storage

[← Previous Phase](phase-1-platform-foundation.md) | [← Back to README](../../README.md) | [→ Next Phase](../phase-3-i2c.md) | [Roadmap](../../ROADMAP.md)

**Status**: ✅ Complete

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

