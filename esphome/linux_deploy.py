"""Linux platform deployment utilities.

This module provides deployment functionality for Linux platforms including:
- prepare-linux command: Generate self-contained deployment packages
- SSH-based deployment: Direct deployment via esphome run --device ssh://...
"""

from __future__ import annotations

import logging
import re
import subprocess
from datetime import datetime
from pathlib import Path
from typing import TYPE_CHECKING

from esphome.const import (
    CONF_ESPHOME,
    CONF_NAME,
    PLATFORM_LINUX,
)
from esphome.core import CORE
from esphome.log import color, AnsiFore

if TYPE_CHECKING:
    from esphome.config import ConfigType
    from esphome.__main__ import ArgsProtocol

_LOGGER = logging.getLogger(__name__)


def command_prepare_linux(args: ArgsProtocol, config: ConfigType) -> int | None:
    """Generate Linux deployment package with installer script and systemd service.

    This command creates a self-contained deployment directory containing:
    - The compiled binary
    - A self-contained install.sh script
    - Systemd service file template
    - README with deployment instructions

    Args:
        args: Command-line arguments including output_dir
        config: ESPHome configuration dictionary

    Returns:
        Exit code (0 for success, 1 for error)
    """
    import shutil

    if CORE.target_platform != PLATFORM_LINUX:
        _LOGGER.error(
            "prepare-linux is only supported for Linux platform. "
            "Current platform: %s. Set 'esphome: platform: linux' in your YAML.",
            CORE.target_platform
        )
        return 1

    # Import here to avoid circular dependencies
    from esphome.__main__ import write_cpp, compile_program

    # Compile the binary first
    _LOGGER.info("Compiling binary...")
    exit_code = write_cpp(config)
    if exit_code != 0:
        return exit_code
    exit_code = compile_program(args, config)
    if exit_code != 0:
        return exit_code
    _LOGGER.info("Successfully compiled program.")

    # Get binary path
    device_name = config[CONF_ESPHOME][CONF_NAME]
    build_dir = Path(CORE.relative_build_path(device_name))
    binary_src = build_dir / device_name

    if not binary_src.exists():
        _LOGGER.error("Binary not found at %s", binary_src)
        return 1

    # Create output directory
    output_dir = Path(args.output_dir) if hasattr(args, 'output_dir') and args.output_dir else Path(f"{device_name}-deploy")
    output_dir.mkdir(parents=True, exist_ok=True)

    _LOGGER.info("Creating deployment package in %s", output_dir.absolute())

    # Copy binary
    binary_dest = output_dir / device_name
    shutil.copy2(binary_src, binary_dest)
    binary_dest.chmod(0o755)
    _LOGGER.info("  ✓ Copied binary")

    # Generate systemd service file
    service_content = _generate_systemd_service(device_name)
    service_file = output_dir / f"{device_name}.service"
    service_file.write_text(service_content)
    _LOGGER.info("  ✓ Generated systemd service")

    # Generate installer script
    install_script = _generate_installer_script(device_name)
    install_file = output_dir / "install.sh"
    install_file.write_text(install_script)
    install_file.chmod(0o755)
    _LOGGER.info("  ✓ Generated installer script")

    # Generate README
    readme_content = _generate_deployment_readme(device_name)
    readme_file = output_dir / "README.txt"
    readme_file.write_text(readme_content)
    _LOGGER.info("  ✓ Generated README")

    # Print success message with instructions
    _print_deployment_instructions(output_dir, device_name, args.configuration[0])

    return 0


def upload_program_ssh(config: ConfigType, args, host: str) -> bool:
    """Platform-specific upload handler for Linux via SSH.

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

    # Parse SSH target
    ssh_info = _parse_ssh_target(host)
    if ssh_info is None:
        _LOGGER.error("Invalid SSH target format. Use: ssh://user@host[:port]")
        return True  # True means we handled it (even if error)

    user, hostname, port = ssh_info
    device_name = config[CONF_ESPHOME][CONF_NAME]

    _LOGGER.info(
        "Deploying %s to %s@%s via SSH...",
        device_name,
        user,
        hostname,
    )

    # Test SSH connectivity first
    if not _test_ssh_connection(user, hostname, port):
        return True

    # Get binary path
    binary_path = Path(CORE.relative_build_path(device_name)) / device_name
    if not binary_path.exists():
        _LOGGER.error("Binary not found at %s. Run 'esphome compile' first.", binary_path)
        return True

    # Step 1: Copy binary via SCP
    if not _copy_binary_scp(binary_path, user, hostname, port, device_name):
        return True

    # Step 2: Install and start service
    service_content = _generate_systemd_service(device_name)
    if not _install_service_ssh(user, hostname, port, device_name, service_content):
        return True

    # Step 3: Stream logs
    _stream_logs_ssh(user, hostname, port, device_name)

    return True


def _test_ssh_connection(user: str, hostname: str, port: int) -> bool:
    """Test SSH connectivity before attempting deployment.

    Args:
        user: SSH username
        hostname: Target hostname or IP
        port: SSH port

    Returns:
        True if SSH connection successful, False otherwise
    """
    _LOGGER.info("Testing SSH connection...")

    test_cmd = [
        "ssh",
        "-p", str(port),
        "-o", "ConnectTimeout=5",
        "-o", "BatchMode=yes",  # Don't prompt for password
        f"{user}@{hostname}",
        "echo 'SSH OK'"
    ]

    try:
        result = subprocess.run(
            test_cmd,
            check=True,
            capture_output=True,
            text=True,
            timeout=10
        )
        _LOGGER.info("  ✓ SSH connection successful")
        return True
    except subprocess.TimeoutExpired:
        _LOGGER.error("SSH connection timeout. Check network and firewall settings.")
        _LOGGER.error("  Troubleshooting:")
        _LOGGER.error("    - Verify network connectivity: ping %s", hostname)
        _LOGGER.error("    - Check SSH service is running on target")
        _LOGGER.error("    - Verify firewall allows SSH (port %d)", port)
        return False
    except subprocess.CalledProcessError as e:
        _LOGGER.error("SSH authentication failed.")
        _LOGGER.error("  %s", e.stderr.strip() if e.stderr else "No error details")
        _LOGGER.error("  Troubleshooting:")
        _LOGGER.error("    - Set up SSH key authentication: ssh-copy-id -p %d %s@%s", port, user, hostname)
        _LOGGER.error("    - Or use password authentication (may require -o BatchMode=no)")
        _LOGGER.error("    - Verify username is correct: %s", user)
        return False
    except FileNotFoundError:
        _LOGGER.error("ssh command not found. Please install OpenSSH client.")
        _LOGGER.error("  Ubuntu/Debian: sudo apt install openssh-client")
        _LOGGER.error("  Fedora/RHEL:   sudo dnf install openssh-clients")
        return False


def _copy_binary_scp(binary_path: Path, user: str, hostname: str, port: int, device_name: str) -> bool:
    """Copy binary to target via SCP.

    Args:
        binary_path: Local path to binary
        user: SSH username
        hostname: Target hostname
        port: SSH port
        device_name: Device name

    Returns:
        True if successful, False otherwise
    """
    _LOGGER.info("Copying binary to target...")

    scp_cmd = [
        "scp",
        "-P", str(port),
        str(binary_path),
        f"{user}@{hostname}:/tmp/{device_name}",
    ]

    try:
        subprocess.run(scp_cmd, check=True, capture_output=True, text=True)
        _LOGGER.info("  ✓ Binary copied")
        return True
    except subprocess.CalledProcessError as e:
        _LOGGER.error("Failed to copy binary via SCP:")
        _LOGGER.error("  %s", e.stderr)
        return False
    except FileNotFoundError:
        _LOGGER.error("scp command not found. Please install OpenSSH client.")
        return False


def _install_service_ssh(user: str, hostname: str, port: int, device_name: str, service_content: str) -> bool:
    """Install and start systemd service via SSH.

    Args:
        user: SSH username
        hostname: Target hostname
        port: SSH port
        device_name: Device name
        service_content: Systemd service file content

    Returns:
        True if successful, False otherwise
    """
    _LOGGER.info("Installing service on target...")

    install_script = _generate_ssh_install_script(device_name, service_content)

    ssh_cmd = [
        "ssh",
        "-p", str(port),
        f"{user}@{hostname}",
        "sudo bash -s"
    ]

    try:
        result = subprocess.run(
            ssh_cmd,
            input=install_script,
            text=True,
            check=True,
            capture_output=True
        )
        print(result.stdout)
        _LOGGER.info("  ✓ Service installed and started")
        return True
    except subprocess.CalledProcessError as e:
        _LOGGER.error("Failed to install service:")
        _LOGGER.error("  %s", e.stderr)
        _LOGGER.error("  Ensure the user has sudo privileges on the target system")
        return False


def _stream_logs_ssh(user: str, hostname: str, port: int, device_name: str):
    """Stream logs from target via SSH.

    Args:
        user: SSH username
        hostname: Target hostname
        port: SSH port
        device_name: Device name
    """
    print()
    print(color(AnsiFore.BOLD_GREEN, "✅ Deployment successful!"))
    print()
    print(color(AnsiFore.BOLD_WHITE, "Streaming logs (Ctrl+C to stop):"))
    print()

    log_cmd = [
        "ssh",
        "-p", str(port),
        f"{user}@{hostname}",
        f"journalctl -u esphome-{device_name} -f --no-pager",
    ]

    try:
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
    print(f"  • View logs:    ssh -p {port} {user}@{hostname} 'journalctl -u esphome-{device_name} -f'")
    print(f"  • Check status: ssh -p {port} {user}@{hostname} 'systemctl status esphome-{device_name}'")
    print()
    print("  • For updates, use OTA:")
    print(f"    esphome run <config>.yaml --device {hostname}")
    print()


def _parse_ssh_target(target: str) -> tuple[str, str, int] | None:
    """Parse SSH target string.

    Args:
        target: SSH target in format ssh://user@host[:port]

    Returns:
        Tuple of (user, host, port) or None if invalid
    """
    # Match ssh://user@host or ssh://user@host:port
    pattern = r"^ssh://([^@]+)@([^:]+)(?::(\d+))?$"
    match = re.match(pattern, target)

    if not match:
        return None

    user = match.group(1)
    host = match.group(2)
    port = int(match.group(3)) if match.group(3) else 22

    return (user, host, port)


def _print_deployment_instructions(output_dir: Path, device_name: str, config_file: str):
    """Print deployment instructions after package creation.

    Args:
        output_dir: Path to deployment package directory
        device_name: Device name
        config_file: Configuration file name
    """
    print()
    print(color(AnsiFore.BOLD_GREEN, "✅ Deployment package created successfully!"))
    print()
    print(f"Package location: {color(AnsiFore.BOLD_CYAN, str(output_dir.absolute()))}")
    print()
    print(color(AnsiFore.BOLD_WHITE, "To deploy to Raspberry Pi:"))
    print()
    print("  1. Copy package to target:")
    scp_cmd = f"scp -r {output_dir.name} pi@raspberrypi:/tmp/"
    print(f"     {color(AnsiFore.CYAN, scp_cmd)}")
    print()
    print("  2. Install on target:")
    ssh_cmd = f'ssh pi@raspberrypi "sudo /tmp/{output_dir.name}/install.sh"'
    print(f"     {color(AnsiFore.CYAN, ssh_cmd)}")
    print()
    print("  3. Subsequent updates via OTA:")
    ota_cmd = f"esphome run {config_file} --device <IP_ADDRESS>"
    print(f"     {color(AnsiFore.CYAN, ota_cmd)}")
    print()
    print(f"For detailed instructions, see: {output_dir}/README.txt")
    print()


def _generate_systemd_service(device_name: str) -> str:
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


def _generate_installer_script(device_name: str) -> str:
    """Generate self-contained installer script.

    Args:
        device_name: Name of the ESPHome device

    Returns:
        Bash installer script content
    """
    return f"""#!/bin/bash
# Auto-generated ESPHome installer script
# Device: {device_name}
# Generated: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}

set -e

# Colors for output
RED='\\033[0;31m'
GREEN='\\033[0;32m'
YELLOW='\\033[1;33m'
CYAN='\\033[0;36m'
NC='\\033[0m' # No Color

DEVICE_NAME="{device_name}"
INSTALL_DIR="/usr/local/bin"
SERVICE_NAME="esphome-${{DEVICE_NAME}}"
SERVICE_FILE="/etc/systemd/system/${{SERVICE_NAME}}.service"
USER="esphome"
GROUP="esphome"

# Get the script directory
SCRIPT_DIR="$(cd "$(dirname "${{BASH_SOURCE[0]}}")" && pwd)"

print_info() {{
    echo -e "${{GREEN}}[INFO]${{NC}} $1"
}}

print_warn() {{
    echo -e "${{YELLOW}}[WARN]${{NC}} $1"
}}

print_error() {{
    echo -e "${{RED}}[ERROR]${{NC}} $1"
}}

# Check if running with sufficient privileges
if [ "$EUID" -ne 0 ]; then
    print_error "This script must be run with sudo"
    echo "Usage: sudo $0"
    exit 1
fi

echo ""
print_info "Installing ESPHome device: ${{CYAN}}${{DEVICE_NAME}}${{NC}}"
echo ""

# Create user if doesn't exist
if ! id "$USER" &>/dev/null; then
    print_info "Creating user: $USER"
    useradd -r -s /bin/false "$USER"
else
    print_info "User $USER already exists"
fi

# Add user to hardware groups (if they exist)
HARDWARE_GROUPS=()
for group in i2c gpio spi dialout; do
    if getent group "$group" >/dev/null 2>&1; then
        HARDWARE_GROUPS+=("$group")
    fi
done

if [ ${{#HARDWARE_GROUPS[@]}} -gt 0 ]; then
    print_info "Adding user to hardware groups: ${{HARDWARE_GROUPS[*]}}"
    usermod -aG "$(IFS=,; echo "${{HARDWARE_GROUPS[*]}}")" "$USER"
else
    print_warn "No hardware groups found (i2c, gpio, spi, dialout)"
fi

# Install binary
BINARY_NAME=$(basename "${{SCRIPT_DIR}}/${{DEVICE_NAME}}")
INSTALLED_BINARY="${{INSTALL_DIR}}/${{BINARY_NAME}}.001"
SYMLINK_PATH="${{INSTALL_DIR}}/${{BINARY_NAME}}"

print_info "Installing binary as version .001"
install -m 0755 "${{SCRIPT_DIR}}/${{DEVICE_NAME}}" "${{INSTALLED_BINARY}}"

# Create initial symlink (mydevice -> mydevice.001)
print_info "Creating symlink: ${{SYMLINK_PATH}} -> ${{BINARY_NAME}}.001"
ln -sf "${{BINARY_NAME}}.001" "${{SYMLINK_PATH}}"

# Install systemd service
print_info "Installing systemd service"
install -m 644 "${{SCRIPT_DIR}}/${{DEVICE_NAME}}.service" "${{SERVICE_FILE}}"

# Reload systemd
print_info "Reloading systemd daemon"
systemctl daemon-reload

# Enable service
print_info "Enabling service: ${{SERVICE_NAME}}"
systemctl enable "${{SERVICE_NAME}}"

# Start service
print_info "Starting service: ${{SERVICE_NAME}}"
systemctl start "${{SERVICE_NAME}}"

# Wait for service to start
sleep 2

# Check service status
echo ""
if systemctl is-active --quiet "${{SERVICE_NAME}}"; then
    print_info "Service started successfully!"

    # Try to detect IP address
    IP_ADDR=$(hostname -I | awk '{{print $1}}')
    if [ -n "$IP_ADDR" ]; then
        echo ""
        print_info "📡 OTA updates available at: ${{CYAN}}${{IP_ADDR}}:3232${{NC}}"
        echo ""
        print_info "Next time, use OTA:"
        echo "  esphome run {device_name}.yaml --device ${{IP_ADDR}}"
    fi
else
    print_error "Service failed to start. Check logs with:"
    echo "  journalctl -u ${{SERVICE_NAME}} -n 50"
    exit 1
fi

echo ""
print_info "Installation complete!"
echo ""
echo "Useful commands:"
echo "  Check status:   systemctl status ${{SERVICE_NAME}}"
echo "  View logs:      journalctl -u ${{SERVICE_NAME}} -f"
echo "  Restart:        systemctl restart ${{SERVICE_NAME}}"
echo "  Stop:           systemctl stop ${{SERVICE_NAME}}"
echo ""
"""


def _generate_ssh_install_script(device_name: str, service_content: str) -> str:
    """Generate installer script for SSH deployment.

    Args:
        device_name: Name of the ESPHome device
        service_content: Systemd service file content

    Returns:
        Bash script for SSH-based installation
    """
    return f"""
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


def _generate_deployment_readme(device_name: str) -> str:
    """Generate README with deployment instructions.

    Args:
        device_name: Name of the ESPHome device

    Returns:
        README content
    """
    return f"""ESPHome Linux Deployment Package
=================================

Device: {device_name}
Generated: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}

Contents
--------
- {device_name}          # Compiled binary
- {device_name}.service  # Systemd service file
- install.sh            # Self-contained installer script
- README.txt            # This file

Quick Start
-----------

1. Copy this directory to your Raspberry Pi:

   scp -r {device_name}-deploy pi@raspberrypi:/tmp/

2. SSH into your Raspberry Pi and run the installer:

   ssh pi@raspberrypi
   sudo /tmp/{device_name}-deploy/install.sh

3. The service will start automatically. Check status:

   sudo systemctl status esphome-{device_name}

4. View logs:

   sudo journalctl -u esphome-{device_name} -f

5. For subsequent updates, use OTA:

   esphome run {device_name}.yaml --device <IP_ADDRESS>

Installation Details
--------------------

The installer script will:
- Create 'esphome' user if it doesn't exist
- Add user to hardware groups (i2c, gpio, spi, dialout)
- Install binary to /usr/local/bin/{device_name}.001
- Create symlink /usr/local/bin/{device_name} -> {device_name}.001
- Install systemd service to /etc/systemd/system/esphome-{device_name}.service
- Enable and start the service

OTA Updates
-----------

After initial installation, the device supports Over-The-Air (OTA) updates.
The OTA server listens on port 3232.

To update:
1. Make changes to your YAML configuration
2. Run: esphome run {device_name}.yaml --device <IP_ADDRESS>

The new binary will be installed as {device_name}.002, the symlink will be
updated, and the service will restart automatically.

Troubleshooting
---------------

Service won't start:
  sudo journalctl -u esphome-{device_name} -n 50

Permission issues:
  - Ensure user is in hardware groups: i2c, gpio, spi
  - Check: groups esphome

Network issues:
  - Check IP address: hostname -I
  - Test connectivity: ping <IP_ADDRESS>

OTA not working:
  - Check firewall: sudo ufw status
  - Allow port 3232: sudo ufw allow 3232/tcp

Uninstall
---------

To remove the service:

  sudo systemctl stop esphome-{device_name}
  sudo systemctl disable esphome-{device_name}
  sudo rm /etc/systemd/system/esphome-{device_name}.service
  sudo rm /usr/local/bin/{device_name}*
  sudo systemctl daemon-reload

Support
-------

For more information, see:
https://esphome.io/

"""
