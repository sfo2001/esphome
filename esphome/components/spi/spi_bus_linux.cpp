#ifdef USE_LINUX

#include "spi_bus_linux.h"
#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <linux/spi/spidev.h>
#include <sys/ioctl.h>
#include <unistd.h>

namespace esphome {
namespace spi {

static const char *const TAG = "spi.linux";

void LinuxSPIBus::setup() {
  // Construct the device path
  char device_path[32];
  snprintf(device_path, sizeof(device_path), "/dev/spidev%d.%d", this->bus_num_, this->device_num_);

  // Open the SPI device
  this->file_descriptor_ = open(device_path, O_RDWR);
  if (this->file_descriptor_ < 0) {
    ESP_LOGE(TAG, "Failed to open SPI bus %d.%d (%s): %s", this->bus_num_, this->device_num_, device_path,
             strerror(errno));
    ESP_LOGE(TAG, "Make sure:");
    ESP_LOGE(TAG, "  1. SPI is enabled (check /boot/config.txt or use raspi-config)");
    ESP_LOGE(TAG, "  2. Device file exists: %s", device_path);
    ESP_LOGE(TAG, "  3. User has permission (add user to 'spi' group)");
    this->mark_failed();
    return;
  }

  // Configure bits per word (8 bits is standard)
  if (ioctl(this->file_descriptor_, SPI_IOC_WR_BITS_PER_WORD, &this->bits_per_word_) < 0) {
    ESP_LOGE(TAG, "Failed to set SPI bits per word: %s", strerror(errno));
    this->mark_failed();
    return;
  }

  ESP_LOGCONFIG(TAG, "SPI bus %d.%d opened successfully on %s", this->bus_num_, this->device_num_, device_path);
}

LinuxSPIBus::~LinuxSPIBus() {
  if (this->file_descriptor_ >= 0) {
    close(this->file_descriptor_);
    this->file_descriptor_ = -1;
  }
}

void LinuxSPIBus::dump_config() {
  ESP_LOGCONFIG(TAG, "SPI Bus:");
  ESP_LOGCONFIG(TAG, "  Bus Number: %d.%d (/dev/spidev%d.%d)", this->bus_num_, this->device_num_, this->bus_num_,
                this->device_num_);
  ESP_LOGCONFIG(TAG, "  Bits per word: %d", this->bits_per_word_);

  if (this->file_descriptor_ < 0) {
    ESP_LOGE(TAG, "  Status: FAILED - could not open SPI device");
  } else {
    ESP_LOGCONFIG(TAG, "  Status: OK");
  }
}

SPIDelegate *LinuxSPIBus::get_delegate(uint32_t data_rate, SPIBitOrder bit_order, SPIMode mode, GPIOPin *cs_pin,
                                       bool release_device, bool write_only) {
  return new LinuxSPIDelegate(this, data_rate, bit_order, mode, cs_pin);
}

// ============= LinuxSPIDelegate Implementation =============

LinuxSPIDelegate::LinuxSPIDelegate(LinuxSPIBus *parent, uint32_t data_rate, SPIBitOrder bit_order, SPIMode mode,
                                   GPIOPin *cs_pin)
    : SPIDelegate(data_rate, bit_order, mode, cs_pin), parent_(parent) {
  this->fd_ = parent->file_descriptor_;
  this->speed_ = data_rate;
  this->spi_mode_ = static_cast<uint8_t>(mode);

  if (this->fd_ < 0) {
    ESP_LOGE(TAG, "Cannot create SPI delegate: parent bus not initialized");
    return;
  }

  // Configure SPI mode
  if (ioctl(this->fd_, SPI_IOC_WR_MODE, &this->spi_mode_) < 0) {
    ESP_LOGE(TAG, "Failed to set SPI mode %d: %s", this->spi_mode_, strerror(errno));
  }

  // Configure max speed
  if (ioctl(this->fd_, SPI_IOC_WR_MAX_SPEED_HZ, &this->speed_) < 0) {
    ESP_LOGE(TAG, "Failed to set SPI speed %u Hz: %s", this->speed_, strerror(errno));
  }

  // Check bit order - warn if not MSB first (which is standard for SPI)
  if (bit_order != BIT_ORDER_MSB_FIRST) {
    // Linux SPI driver handles LSB_FIRST via mode flags (SPI_LSB_FIRST)
    uint8_t lsb_first = (bit_order == BIT_ORDER_LSB_FIRST) ? 1 : 0;
    if (ioctl(this->fd_, SPI_IOC_WR_LSB_FIRST, &lsb_first) < 0) {
      ESP_LOGW(TAG, "Failed to set LSB first mode: %s", strerror(errno));
    }
  }

  ESP_LOGV(TAG, "SPI delegate created: mode=%d, speed=%u Hz", this->spi_mode_, this->speed_);
}

LinuxSPIDelegate::~LinuxSPIDelegate() {
  // File descriptor is managed by parent bus, don't close here
}

void LinuxSPIDelegate::begin_transaction() {
  this->transaction_active_ = true;
  // Call parent to handle CS pin
  SPIDelegate::begin_transaction();
}

void LinuxSPIDelegate::end_transaction() {
  // Call parent to handle CS pin
  SPIDelegate::end_transaction();
  this->transaction_active_ = false;
}

uint8_t LinuxSPIDelegate::transfer(uint8_t data) {
  if (this->fd_ < 0) {
    ESP_LOGE(TAG, "SPI transfer failed: device not initialized");
    return 0;
  }

  uint8_t rx_data = 0;
  struct spi_ioc_transfer transfer_config;
  memset(&transfer_config, 0, sizeof(transfer_config));

  transfer_config.tx_buf = reinterpret_cast<uint64_t>(&data);
  transfer_config.rx_buf = reinterpret_cast<uint64_t>(&rx_data);
  transfer_config.len = 1;
  transfer_config.speed_hz = this->speed_;
  transfer_config.bits_per_word = 8;
  transfer_config.cs_change = 0;  // Don't change CS after transfer

  int result = ioctl(this->fd_, SPI_IOC_MESSAGE(1), &transfer_config);
  if (result < 0) {
    ESP_LOGE(TAG, "SPI transfer failed: %s", strerror(errno));
    return 0;
  }

  ESP_LOGVV(TAG, "SPI transfer: TX=0x%02X, RX=0x%02X", data, rx_data);
  return rx_data;
}

void LinuxSPIDelegate::transfer(uint8_t *ptr, size_t length) {
  // Full-duplex transfer: same buffer for TX and RX
  this->transfer(ptr, ptr, length);
}

void LinuxSPIDelegate::transfer(const uint8_t *txbuf, uint8_t *rxbuf, size_t length) {
  if (this->fd_ < 0) {
    ESP_LOGE(TAG, "SPI transfer failed: device not initialized");
    return;
  }

  if (length == 0) {
    return;
  }

  struct spi_ioc_transfer transfer_config;
  memset(&transfer_config, 0, sizeof(transfer_config));

  transfer_config.tx_buf = reinterpret_cast<uint64_t>(txbuf);
  transfer_config.rx_buf = reinterpret_cast<uint64_t>(rxbuf);
  transfer_config.len = length;
  transfer_config.speed_hz = this->speed_;
  transfer_config.bits_per_word = 8;
  transfer_config.cs_change = 0;  // Don't change CS after transfer

  int result = ioctl(this->fd_, SPI_IOC_MESSAGE(1), &transfer_config);
  if (result < 0) {
    ESP_LOGE(TAG, "SPI buffer transfer failed (length=%zu): %s", length, strerror(errno));
    return;
  }

  ESP_LOGVV(TAG, "SPI buffer transfer successful: %zu bytes", length);
}

}  // namespace spi
}  // namespace esphome

#endif  // USE_LINUX
