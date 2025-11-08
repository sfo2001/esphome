#!/usr/bin/env python3
"""Generate systemd service file for ESPHome Linux binary."""

import argparse
import os
import sys


TEMPLATE = """[Unit]
Description=ESPHome Device: {device_name}
After=network-online.target
Wants=network-online.target

[Service]
Type=simple
ExecStart={binary_path}
Restart=always
RestartSec=5s
StartLimitBurst=5
StartLimitIntervalSec=60s

# User and groups
User={user}
Group={group}
SupplementaryGroups={supplementary_groups}

# Allow binary updates
ReadWritePaths={binary_dir}

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


def generate_service(
    device_name, binary_path, user="esphome", group="esphome", hardware_groups=None
):
    """Generate systemd service file content.

    Args:
        device_name: Name of the ESPHome device
        binary_path: Full path to the binary symlink
        user: User to run the service as
        group: Group to run the service as
        hardware_groups: List of supplementary groups for hardware access

    Returns:
        Generated systemd service file content
    """
    if hardware_groups is None:
        hardware_groups = ["i2c", "gpio", "spi"]

    binary_dir = os.path.dirname(binary_path)

    return TEMPLATE.format(
        device_name=device_name,
        binary_path=binary_path,
        user=user,
        group=group,
        supplementary_groups=" ".join(hardware_groups),
        binary_dir=binary_dir,
    )


def main():
    """Main entry point for the script."""
    parser = argparse.ArgumentParser(
        description="Generate systemd service file for ESPHome Linux binary"
    )
    parser.add_argument("device_name", help="Name of the ESPHome device")
    parser.add_argument(
        "binary_path",
        help="Full path to the binary (will be used as symlink)",
    )
    parser.add_argument("--user", default="esphome", help="User to run service as")
    parser.add_argument("--group", default="esphome", help="Group to run service as")
    parser.add_argument(
        "--hardware-groups",
        default="i2c,gpio,spi",
        help="Comma-separated list of hardware groups",
    )
    parser.add_argument(
        "--output",
        "-o",
        help="Output file path (default: print to stdout)",
    )

    args = parser.parse_args()

    hardware_groups = [g.strip() for g in args.hardware_groups.split(",") if g.strip()]

    service_content = generate_service(
        args.device_name,
        args.binary_path,
        args.user,
        args.group,
        hardware_groups,
    )

    if args.output:
        with open(args.output, "w") as f:
            f.write(service_content)
        print(f"Service file written to: {args.output}", file=sys.stderr)
    else:
        print(service_content)

    return 0


if __name__ == "__main__":
    sys.exit(main())
