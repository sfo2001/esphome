#!/bin/bash
# Install ESPHome Linux binary as systemd service

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Function to print colored messages
print_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

print_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Check if running with sufficient privileges
check_privileges() {
    if [ "$EUID" -ne 0 ] && ! sudo -n true 2>/dev/null; then
        print_error "This script requires sudo privileges. Please run with sudo or ensure passwordless sudo is configured."
        exit 1
    fi
}

# Parse arguments
usage() {
    echo "Usage: $0 <device-name> <binary-path> [options]"
    echo ""
    echo "Arguments:"
    echo "  device-name       Name of the ESPHome device"
    echo "  binary-path       Path to the compiled binary"
    echo ""
    echo "Options:"
    echo "  --install-dir DIR    Directory to install binary (default: /usr/local/bin)"
    echo "  --user USER          User to run service as (default: esphome)"
    echo "  --group GROUP        Group to run service as (default: esphome)"
    echo "  --no-start           Don't start the service after installation"
    echo "  --help               Show this help message"
    echo ""
    echo "Example:"
    echo "  $0 mydevice /path/to/mydevice"
    echo "  $0 mydevice /path/to/mydevice --install-dir /opt/esphome --user pi"
    exit 1
}

# Default values
INSTALL_DIR="/usr/local/bin"
USER="esphome"
GROUP="esphome"
START_SERVICE=true

# Parse arguments
DEVICE_NAME=""
BINARY_PATH=""

while [[ $# -gt 0 ]]; do
    case $1 in
        --install-dir)
            INSTALL_DIR="$2"
            shift 2
            ;;
        --user)
            USER="$2"
            shift 2
            ;;
        --group)
            GROUP="$2"
            shift 2
            ;;
        --no-start)
            START_SERVICE=false
            shift
            ;;
        --help)
            usage
            ;;
        *)
            if [ -z "$DEVICE_NAME" ]; then
                DEVICE_NAME="$1"
            elif [ -z "$BINARY_PATH" ]; then
                BINARY_PATH="$1"
            else
                print_error "Unknown argument: $1"
                usage
            fi
            shift
            ;;
    esac
done

# Validate required arguments
if [ -z "$DEVICE_NAME" ] || [ -z "$BINARY_PATH" ]; then
    print_error "Missing required arguments"
    usage
fi

# Validate binary exists
if [ ! -f "$BINARY_PATH" ]; then
    print_error "Binary not found: $BINARY_PATH"
    exit 1
fi

check_privileges

print_info "Installing ESPHome device: $DEVICE_NAME"
print_info "Binary: $BINARY_PATH"
print_info "Install directory: $INSTALL_DIR"
print_info "User: $USER"
print_info "Group: $GROUP"

# Create user if doesn't exist
if ! id "$USER" &>/dev/null; then
    print_info "Creating user: $USER"
    sudo useradd -r -s /bin/false "$USER"
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

if [ ${#HARDWARE_GROUPS[@]} -gt 0 ]; then
    print_info "Adding user to hardware groups: ${HARDWARE_GROUPS[*]}"
    sudo usermod -aG "$(IFS=,; echo "${HARDWARE_GROUPS[*]}")" "$USER"
else
    print_warn "No hardware groups found (i2c, gpio, spi, dialout)"
fi

# Create install directory if it doesn't exist
if [ ! -d "$INSTALL_DIR" ]; then
    print_info "Creating install directory: $INSTALL_DIR"
    sudo mkdir -p "$INSTALL_DIR"
fi

# Install binary
BINARY_NAME=$(basename "$BINARY_PATH")
INSTALLED_BINARY="$INSTALL_DIR/$BINARY_NAME.001"
SYMLINK_PATH="$INSTALL_DIR/$BINARY_NAME"

print_info "Installing binary as version .001"
sudo install -m 0755 "$BINARY_PATH" "$INSTALLED_BINARY"

# Create initial symlink (mydevice -> mydevice.001)
print_info "Creating symlink: $SYMLINK_PATH -> $BINARY_NAME.001"
sudo ln -sf "$BINARY_NAME.001" "$SYMLINK_PATH"

# Generate service file
SERVICE_NAME="esphome-$DEVICE_NAME"
SERVICE_PATH="/etc/systemd/system/$SERVICE_NAME.service"

print_info "Generating systemd service file"
python3 "$SCRIPT_DIR/generate-systemd-service.py" \
    "$DEVICE_NAME" \
    "$SYMLINK_PATH" \
    --user "$USER" \
    --group "$GROUP" \
    --hardware-groups "$(IFS=,; echo "${HARDWARE_GROUPS[*]}")" \
    | sudo tee "$SERVICE_PATH" >/dev/null

# Set correct permissions on service file
sudo chmod 644 "$SERVICE_PATH"

# Reload systemd
print_info "Reloading systemd daemon"
sudo systemctl daemon-reload

# Enable service
print_info "Enabling service: $SERVICE_NAME"
sudo systemctl enable "$SERVICE_NAME"

# Start service if requested
if [ "$START_SERVICE" = true ]; then
    print_info "Starting service: $SERVICE_NAME"
    sudo systemctl start "$SERVICE_NAME"

    # Wait a moment for service to start
    sleep 2

    # Check service status
    if sudo systemctl is-active --quiet "$SERVICE_NAME"; then
        print_info "Service started successfully!"
    else
        print_warn "Service failed to start. Check logs with:"
        print_warn "  sudo journalctl -u $SERVICE_NAME -n 50"
    fi
fi

echo ""
print_info "Installation complete!"
echo ""
echo "Useful commands:"
echo "  Check status:   sudo systemctl status $SERVICE_NAME"
echo "  View logs:      sudo journalctl -u $SERVICE_NAME -f"
echo "  Restart:        sudo systemctl restart $SERVICE_NAME"
echo "  Stop:           sudo systemctl stop $SERVICE_NAME"
echo "  Start:          sudo systemctl start $SERVICE_NAME"
echo ""
echo "OTA updates will automatically create new versions (e.g., $BINARY_NAME.002)"
echo "The service will restart automatically after OTA updates."
