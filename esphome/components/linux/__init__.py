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
    # Use platform-linux_arm for ARM compilation support (Raspberry Pi, etc.)
    cg.add_platformio_option("platform", "https://github.com/sfo2001/platform-linux_arm.git")
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


def upload_program(config, args, host):
    """Platform-specific upload handler for Linux.

    Handles SSH-based deployment for initial installation.
    Falls back to OTA for regular updates.

    Args:
        config: ESPHome configuration dictionary
        args: Command-line arguments
        host: Target device (ssh://user@host or IP address)

    Returns:
        True if SSH deployment was handled, False to fall back to OTA
    """
    # Only handle ssh:// targets, let OTA handle IP addresses
    if not host.startswith("ssh://"):
        return False

    from esphome import const
    from esphome.log import color, AnsiFore
    import logging
    import subprocess
    import time
    from pathlib import Path

    _LOGGER = logging.getLogger(__name__)

    # Parse SSH target
    ssh_info = _parse_ssh_target(host)
    if ssh_info is None:
        _LOGGER.error("Invalid SSH target format. Use: ssh://user@host[:port]")
        return True  # True means we handled it (even if error)

    user, hostname, port = ssh_info
    device_name = config[const.CONF_ESPHOME][const.CONF_NAME]

    _LOGGER.info(
        "Deploying %s to %s@%s via SSH...",
        device_name,
        user,
        hostname,
    )

    # Get binary path
    from esphome.core import CORE

    binary_path = Path(CORE.relative_build_path(device_name)) / device_name
    if not binary_path.exists():
        _LOGGER.error("Binary not found at %s. Run 'esphome compile' first.", binary_path)
        return True

    # Step 1: Copy binary via SCP
    _LOGGER.info("Copying binary to target...")
    scp_cmd = [
        "scp",
        "-P",
        str(port),
        str(binary_path),
        f"{user}@{hostname}:/tmp/{device_name}",
    ]

    try:
        result = subprocess.run(scp_cmd, check=True, capture_output=True, text=True)
        _LOGGER.info("  ✓ Binary copied")
    except subprocess.CalledProcessError as e:
        _LOGGER.error("Failed to copy binary via SCP:")
        _LOGGER.error("  %s", e.stderr)
        _LOGGER.error(
            "Ensure SSH access is configured (ssh-copy-id %s@%s)",
            user,
            hostname,
        )
        return True
    except FileNotFoundError:
        _LOGGER.error("scp command not found. Please install OpenSSH client.")
        return True

    # Step 2: Generate systemd service content
    service_content = _generate_systemd_service_content(device_name)

    # Step 3: Create and run installer script via SSH
    _LOGGER.info("Installing service on target...")

    install_script = f"""
set -e

DEVICE_NAME="{device_name}"
INSTALL_DIR="/usr/local/bin"
SERVICE_NAME="esphome-${{DEVICE_NAME}}"
SERVICE_FILE="/etc/systemd/system/${{SERVICE_NAME}}.service"
USER="esphome"
GROUP="esphome"

# Create user if doesn't exist
if ! id "$USER" &>/dev/null; then
    useradd -r -s /bin/false "$USER"
fi

# Add user to hardware groups (if they exist)
HARDWARE_GROUPS=()
for group in i2c gpio spi dialout; do
    if getent group "$group" >/dev/null 2>&1; then
        HARDWARE_GROUPS+=("$group")
    fi
done

if [ ${{#HARDWARE_GROUPS[@]}} -gt 0 ]; then
    usermod -aG "$(IFS=,; echo "${{HARDWARE_GROUPS[*]}}")" "$USER"
fi

# Install binary as versioned (*.001 for initial, OTA creates *.002, etc.)
BINARY_NAME="{device_name}"
INSTALLED_BINARY="${{INSTALL_DIR}}/${{BINARY_NAME}}.001"
SYMLINK_PATH="${{INSTALL_DIR}}/${{BINARY_NAME}}"

# Install binary
install -m 0755 "/tmp/${{DEVICE_NAME}}" "${{INSTALLED_BINARY}}"

# Create symlink
ln -sf "${{BINARY_NAME}}.001" "${{SYMLINK_PATH}}"

# Install service file
cat > "${{SERVICE_FILE}}" << 'SERVICEEOF'
{service_content}
SERVICEEOF

# Set permissions
chmod 644 "${{SERVICE_FILE}}"

# Reload systemd
systemctl daemon-reload

# Enable service
systemctl enable "${{SERVICE_NAME}}"

# Restart service (will start if not running)
systemctl restart "${{SERVICE_NAME}}"

# Wait for service to start
sleep 2

# Check if service started successfully
if systemctl is-active --quiet "${{SERVICE_NAME}}"; then
    echo "✓ Service started successfully"
else
    echo "✗ Service failed to start"
    systemctl status "${{SERVICE_NAME}}" --no-pager || true
    exit 1
fi
"""

    ssh_cmd = [
        "ssh",
        "-p",
        str(port),
        f"{user}@{hostname}",
        f"sudo bash -c {subprocess.list2cmdline([install_script])}",
    ]

    try:
        result = subprocess.run(ssh_cmd, check=True, capture_output=True, text=True)
        print(result.stdout)
        _LOGGER.info("  ✓ Service installed and started")
    except subprocess.CalledProcessError as e:
        _LOGGER.error("Failed to install service:")
        _LOGGER.error("  %s", e.stderr)
        _LOGGER.error(
            "Ensure the user has sudo privileges on the target system"
        )
        return True

    # Step 4: Stream logs
    print()
    print(color(AnsiFore.BOLD_GREEN, "✅ Deployment successful!"))
    print()
    print(color(AnsiFore.BOLD_WHITE, "Streaming logs (Ctrl+C to stop):"))
    print()

    # Stream journalctl logs
    log_cmd = [
        "ssh",
        "-p",
        str(port),
        f"{user}@{hostname}",
        f"journalctl -u esphome-{device_name} -f --no-pager",
    ]

    try:
        # Run journalctl in foreground, allow Ctrl+C to stop
        subprocess.run(log_cmd)
    except KeyboardInterrupt:
        print()
        print()
        _LOGGER.info("Log streaming stopped")

    # Print next steps
    print()
    print(color(AnsiFore.BOLD_WHITE, "Next steps:"))
    print()
    print("  • Service is running in the background")
    print(f"  • View logs:    ssh {user}@{hostname} 'journalctl -u esphome-{device_name} -f'")
    print(f"  • Check status: ssh {user}@{hostname} 'systemctl status esphome-{device_name}'")
    print()
    print("  • For updates, use OTA:")
    print(f"    esphome run <config>.yaml --device {hostname}")
    print()

    return True


def _parse_ssh_target(target):
    """Parse SSH target string.

    Args:
        target: SSH target in format ssh://user@host[:port]

    Returns:
        Tuple of (user, host, port) or None if invalid
    """
    import re

    # Match ssh://user@host or ssh://user@host:port
    pattern = r"^ssh://([^@]+)@([^:]+)(?::(\d+))?$"
    match = re.match(pattern, target)

    if not match:
        return None

    user = match.group(1)
    host = match.group(2)
    port = int(match.group(3)) if match.group(3) else 22

    return (user, host, port)


def _generate_systemd_service_content(device_name):
    """Generate systemd service file content.

    Args:
        device_name: Name of the ESPHome device

    Returns:
        Systemd service file content as string
    """
    return f"""[Unit]
Description=ESPHome Device: {device_name}
After=network-online.target
Wants=network-online.target

[Service]
Type=simple
ExecStart=/usr/local/bin/{device_name}
Restart=always
RestartSec=5s
StartLimitBurst=5
StartLimitIntervalSec=60s

# User and groups
User=esphome
Group=esphome
SupplementaryGroups=i2c gpio spi

# Allow binary updates (for OTA)
ReadWritePaths=/usr/local/bin

# Security hardening
NoNewPrivileges=true
PrivateTmp=true

# Logging
StandardOutput=journal
StandardError=journal
SyslogIdentifier={device_name}

[Install]
WantedBy=multi-user.target
"""
