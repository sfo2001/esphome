# Useful Commands Reference

[← Back to README](../README.md)

This document contains useful commands for developing and testing the Linux platform.

## Useful Commands Reference

### Development Workflow

```bash
# === ON AMD SERVER (Phases 1-2) ===

# Edit code
cd ~/devel/esphome
git checkout feature/linux-platform

# Test compilation (x86_64)
esphome compile test-config.yaml

# Run linters
script/lint-python
script/lint-cpp

# Commit changes
git add .
git commit -m "feat(linux): ..."
git push origin feature/linux-platform


# === ON RASPBERRY PI 5 (Phases 3-6) ===

# One-time setup
mkdir ~/esphome-dev && cd ~/esphome-dev
git clone <fork-url> .
git checkout feature/linux-platform
python3 -m venv venv
source venv/bin/activate
pip install -e .

# Development cycle
git pull
esphome compile test-i2c.yaml
sudo ./test-i2c  # or without sudo if in groups

# Check I2C devices
i2cdetect -y 1

# Check GPIO chips
gpiodetect
gpioinfo gpiochip0

# Monitor logs
journalctl -f  # if running as systemd service
```

### Debugging Commands

```bash
# Check user groups
groups $USER

# Add to groups (requires logout)
sudo usermod -aG i2c,gpio $USER

# Check I2C bus
ls -l /dev/i2c-*
i2cdetect -y 1

# Check GPIO
ls -l /dev/gpiochip*
gpioinfo gpiochip0

# Check permissions
ls -l /dev/i2c-1
ls -l /dev/gpiochip0

# Monitor system resources
htop
# or
top

# Check for memory leaks
valgrind --leak-check=full ./your-esphome-binary
```

---

