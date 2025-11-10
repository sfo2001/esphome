"""GPIO pin implementation for Linux platform."""
from esphome import pins
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_INPUT,
    CONF_INVERTED,
    CONF_MODE,
    CONF_NUMBER,
    CONF_OUTPUT,
    CONF_PULLDOWN,
    CONF_PULLUP,
)
from esphome.core import CORE

from .const import KEY_LINUX, linux_ns

LinuxGPIOPin = linux_ns.class_("LinuxGPIOPin", cg.InternalGPIOPin)


def validate_gpio_pin(value):
    """Validate GPIO pin number for Linux/Raspberry Pi."""
    if isinstance(value, str):
        # Allow GPIO prefix (e.g., "GPIO17" -> 17)
        if value.startswith("GPIO"):
            try:
                value = int(value[4:])
            except ValueError:
                raise cv.Invalid(f"Invalid GPIO pin number: {value}")
        else:
            raise cv.Invalid(
                f"Invalid pin specification: {value}. Use a number (e.g., 17) or GPIO prefix (e.g., GPIO17)"
            )

    value = cv.int_(value)

    # Raspberry Pi typically has GPIOs 0-27 available
    # Some models have more (e.g., Pi 5 has additional GPIOs on gpiochip4)
    # We allow 0-63 to support different models and future expansion
    if value < 0 or value > 63:
        raise cv.Invalid(f"Invalid GPIO pin number: {value}. Valid range is 0-63.")

    # Warn about commonly reserved pins (but don't block them)
    # GPIO 2, 3: I2C (SDA, SCL)
    # GPIO 14, 15: UART (TX, RX)
    # GPIO 7, 8, 9, 10, 11: SPI
    reserved_pins = {
        2: "I2C SDA",
        3: "I2C SCL",
        14: "UART TX",
        15: "UART RX",
        7: "SPI CE1",
        8: "SPI CE0",
        9: "SPI MISO",
        10: "SPI MOSI",
        11: "SPI SCLK",
    }

    if value in reserved_pins:
        # Just a warning, not an error - user might want to use these pins
        import logging

        _LOGGER = logging.getLogger(__name__)
        _LOGGER.warning(
            f"GPIO{value} is typically used for {reserved_pins[value]}. "
            "Make sure you're not using it for other purposes."
        )

    return value


def validate_mode(value):
    """Validate GPIO mode flags."""
    # Linux GPIO supports input, output, pullup, pulldown
    # Open-drain is not directly supported by libgpiod
    mode = value[CONF_MODE]
    is_input = mode[CONF_INPUT]
    is_output = mode[CONF_OUTPUT]
    is_pullup = mode[CONF_PULLUP]
    is_pulldown = mode[CONF_PULLDOWN]

    # Input and output cannot be both true
    if is_input and is_output:
        raise cv.Invalid("Pin cannot be both input and output")

    # Pull-up/pull-down only makes sense for inputs
    if (is_pullup or is_pulldown) and not is_input:
        raise cv.Invalid("Pull-up/pull-down resistors only work with input pins")

    # Cannot have both pull-up and pull-down
    if is_pullup and is_pulldown:
        raise cv.Invalid("Pin cannot have both pull-up and pull-down resistors")

    return value


LINUX_PIN_SCHEMA = cv.All(
    pins.gpio_base_schema(
        LinuxGPIOPin,
        validate_gpio_pin,
    ),
    validate_mode,
)


@pins.PIN_SCHEMA_REGISTRY.register("linux", LINUX_PIN_SCHEMA)
async def linux_pin_to_code(config):
    """Convert config to LinuxGPIOPin code."""
    var = cg.new_Pvariable(config[CONF_ID])
    num = config[CONF_NUMBER]
    cg.add(var.set_pin(num))

    # Only set if true to avoid bloating setup() function
    if config[CONF_INVERTED]:
        cg.add(var.set_inverted(True))

    # Set GPIO chip name from platform configuration
    if KEY_LINUX in CORE.data:
        from . import CONF_GPIO_CHIP

        gpio_chip = CORE.data[KEY_LINUX].get(CONF_GPIO_CHIP, "gpiochip0")
        cg.add(var.set_chip_name(gpio_chip))

    cg.add(var.set_flags(pins.gpio_flags_expr(config[CONF_MODE])))

    return var
