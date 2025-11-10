# Troubleshooting Guide

[← Back to README](../README.md)

This document provides solutions to common issues encountered during Linux platform development.

## Compilation Issues

### Error: `platform not found`

**Problem**: ESPHome doesn't recognize the Linux platform.

**Solution**:
```bash
# Ensure you're on the feature/linux-platform branch
git checkout feature/linux-platform
git pull

# Reinstall ESPHome in development mode
pip install -e .
```

### Error: `libgpiod not found`

**Problem**: GPIO compilation fails due to missing libgpiod.

**Solution**:
```bash
# Install libgpiod development package
sudo apt-get install -y libgpiod-dev
```

## Runtime Issues

### Error: Permission denied on `/dev/i2c-1`

**Problem**: User doesn't have permission to access I2C bus.

**Solution**:
```bash
# Add user to i2c group
sudo usermod -aG i2c $USER

# Log out and back in for changes to take effect
# Or run: newgrp i2c
```

### Error: Permission denied on `/dev/gpiochip0`

**Problem**: User doesn't have permission to access GPIO.

**Solution**:
```bash
# Add user to gpio group
sudo usermod -aG gpio $USER

# Log out and back in
# Or run: newgrp gpio
```

### Error: `/dev/i2c-1` does not exist

**Problem**: I2C interface not enabled on Raspberry Pi.

**Solution**:
```bash
# Enable I2C via raspi-config
sudo raspi-config
# Navigate to: Interface Options -> I2C -> Enable

# Or manually add to /boot/config.txt
echo "dtparam=i2c_arm=on" | sudo tee -a /boot/config.txt
sudo reboot
```

### Error: GPIO chip not found

**Problem**: libgpiod can't find the GPIO chip.

**Solution**:
```bash
# List available GPIO chips
gpiodetect

# If gpiochip0 not found, try gpiochip4 (Raspberry Pi 5)
# Update config:
linux:
  gpio_chip: gpiochip4
```

## Hardware Detection Issues

### I2C device not detected

**Problem**: `i2cdetect` doesn't show expected device.

**Debugging steps**:
```bash
# Check I2C bus exists
ls -l /dev/i2c-*

# Scan I2C bus
i2cdetect -y 1

# Check physical connections
# - SDA connected to GPIO 2 (Pin 3)
# - SCL connected to GPIO 3 (Pin 5)
# - GND connected
# - VCC connected (3.3V or 5V depending on device)

# Check pull-up resistors
# Most I2C devices have built-in pull-ups
# If not, add 4.7kΩ resistors to SDA and SCL
```

### SPI device not working

**Problem**: SPI communication fails.

**Debugging steps**:
```bash
# Check SPI is enabled
ls -l /dev/spidev*

# Should show /dev/spidev0.0 and /dev/spidev0.1

# If not, enable SPI
sudo raspi-config
# Interface Options -> SPI -> Enable

# Check wiring
# - MOSI: GPIO 10 (Pin 19)
# - MISO: GPIO 9 (Pin 21)
# - SCLK: GPIO 11 (Pin 23)
# - CE0: GPIO 8 (Pin 24) for /dev/spidev0.0
# - CE1: GPIO 7 (Pin 26) for /dev/spidev0.1
```

## Performance Issues

### High CPU usage

**Problem**: ESPHome process using excessive CPU.

**Debugging**:
```bash
# Check which function is consuming CPU
perf top -p $(pgrep esphome_binary)

# Common causes:
# - Tight loop without delays
# - Excessive I2C polling
# - Missing yield() calls
```

### Memory leak

**Problem**: Memory usage grows over time.

**Debugging**:
```bash
# Run with valgrind
valgrind --leak-check=full ./your_binary

# Monitor memory usage
watch -n 1 'ps aux | grep your_binary'
```

## Build Issues

### Error: `std::filesystem` not found

**Problem**: C++20 filesystem support missing.

**Solution**:
Ensure using C++20:
```python
# In esphome/components/linux/__init__.py
cg.add_build_flag("-std=gnu++20")
```

### Error: undefined reference to `sync`

**Problem**: Missing sync function.

**Solution**:
```cpp
// Add include in file
#include <unistd.h>
```

## Raspberry Pi Specific Issues

### Raspberry Pi 5 vs older models

**Issue**: GPIO chip naming differs.

**Solution**:
- **Pi 1-4**: Uses `gpiochip0`
- **Pi 5**: Uses `gpiochip0` and `gpiochip4`

Check with `gpiodetect` and configure accordingly.

### Raspberry Pi 1 compatibility

**Issue**: Very slow compilation on Pi 1.

**Recommendation**:
- Compile on development server (x86_64)
- Cross-compile for ARM
- Or use Pi 3/5 for development

## Getting Help

If your issue isn't listed here:

1. **Check logs**: Look for error messages in the console output
2. **Enable verbose logging**: Set `logger: level: VERBOSE` in config
3. **Check permissions**: Ensure user is in correct groups (i2c, gpio, spi)
4. **Hardware connections**: Double-check wiring with multimeter
5. **Consult ESPHome Discord**: #development channel
6. **GitHub Issues**: https://github.com/esphome/esphome/issues

## Useful Debug Commands

```bash
# System information
uname -a
cat /proc/cpuinfo | grep Model

# Hardware interfaces
ls -l /dev/i2c-* /dev/gpiochip* /dev/spidev*

# User groups
groups

# I2C tools
i2cdetect -y 1
i2cget -y 1 0x48 0x00

# GPIO tools
gpiodetect
gpioinfo gpiochip0
gpioget gpiochip0 17
gpioset gpiochip0 17=1

# SPI test (if spi-tools installed)
# Note: This requires spidev_test utility

# Check running processes
ps aux | grep esphome

# Monitor system resources
htop
```
