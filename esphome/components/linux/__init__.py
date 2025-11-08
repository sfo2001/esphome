import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import (
    KEY_CORE,
    KEY_FRAMEWORK_VERSION,
    KEY_TARGET_FRAMEWORK,
    KEY_TARGET_PLATFORM,
    PLATFORM_LINUX,
    ThreadModel,
)
from esphome.core import CORE

from .const import KEY_LINUX, linux_ns

# Force import gpio to register pin schema
from .gpio import linux_pin_to_code  # noqa

CODEOWNERS = ["@esphome/core"]
AUTO_LOAD = ["preferences"]
IS_TARGET_PLATFORM = True

CONF_PREFERENCES_PATH = "preferences_path"
CONF_GPIO_CHIP = "gpio_chip"
CONF_BINARY_PATH = "binary_path"


def set_core_data(config):
    CORE.data[KEY_LINUX] = {
        CONF_PREFERENCES_PATH: config[CONF_PREFERENCES_PATH],
        CONF_GPIO_CHIP: config[CONF_GPIO_CHIP],
    }
    if CONF_BINARY_PATH in config:
        CORE.data[KEY_LINUX][CONF_BINARY_PATH] = config[CONF_BINARY_PATH]
    CORE.data[KEY_CORE][KEY_TARGET_PLATFORM] = PLATFORM_LINUX
    CORE.data[KEY_CORE][KEY_TARGET_FRAMEWORK] = "native"
    CORE.data[KEY_CORE][KEY_FRAMEWORK_VERSION] = cv.Version(1, 0, 0)
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.Optional(
                CONF_PREFERENCES_PATH, default="/var/lib/esphome"
            ): cv.string,
            cv.Optional(CONF_GPIO_CHIP, default="gpiochip0"): cv.string,
            cv.Optional(CONF_BINARY_PATH): cv.string,
        }
    ),
    set_core_data,
)


async def to_code(config):
    cg.add_build_flag("-DUSE_LINUX")
    cg.add_build_flag("-std=gnu++20")
    cg.add_define("ESPHOME_BOARD", "linux")
    cg.add_define(ThreadModel.MULTI_ATOMICS)
    cg.add_platformio_option("platform", "platformio/native")
    cg.add_platformio_option("lib_ldf_mode", "off")
    cg.add_platformio_option("lib_compat_mode", "strict")

    # Add libgpiod library for GPIO support
    cg.add_build_flag("-lgpiod")

    # Add preferences path define
    cg.add_define("ESPHOME_PREFERENCES_PATH", config[CONF_PREFERENCES_PATH])

    # Add GPIO chip name define
    cg.add_define("ESPHOME_GPIO_CHIP", config[CONF_GPIO_CHIP])

    # Add binary path define (for OTA updates)
    if CONF_BINARY_PATH in config:
        cg.add_define("ESPHOME_BINARY_PATH", config[CONF_BINARY_PATH])

    # Setup preferences
    cg.add(linux_ns.setup_preferences())
