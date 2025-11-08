# Phase 9.5: Linux Deployment Strategies

[← Back to Phase 9](phase-9-network-support.md) | [README](../README.md) | [Roadmap](../ROADMAP.md)

**Status**: 📝 Design Phase - Implementation Proposals
**Priority**: MEDIUM (improves user experience)
**Duration**: 2-3 days

## Overview

Unlike embedded platforms (ESP32, ESP8266, RP2040) which require USB serial flashing for initial deployment, Linux platforms need a different approach. This phase documents deployment strategies for ESPHome on Linux, from initial installation to OTA updates.

## Context: Why Linux is Different

### Embedded Platform Workflow (ESP32)

```bash
# First deployment - REQUIRES physical USB connection
esphome run mydevice.yaml --device /dev/ttyUSB0

# Subsequent updates - OTA over network
esphome run mydevice.yaml --device 192.168.1.100
```

**Why USB first?**
- ESP32 has no operating system
- Factory ESP32 has ROM bootloader accepting serial uploads
- Writes directly to flash memory
- After first flash, OTA becomes available

### Linux Platform Reality

**Key Differences:**
- ✅ Has full operating system (Linux)
- ✅ Network accessible via SSH by default
- ✅ Binary is a regular executable, not firmware
- ✅ No "flashing" - just file copy + service installation
- ❌ No USB serial interface for flashing

**Opportunity:**
Linux deployment can be **more streamlined** than embedded platforms because network access is available from the start!

---

## Current State (Phase 9 - Manual Deployment)

After Phase 9 implementation, the workflow requires multiple manual steps:

### Step 1: Compile Locally

```bash
esphome compile mydevice.yaml
# Generates: .esphome/build/mydevice/mydevice
```

### Step 2: Copy Binary to Target

```bash
scp .esphome/build/mydevice/mydevice user@raspberrypi:/tmp/
```

### Step 3: SSH into Target and Install

```bash
ssh user@raspberrypi

# On the remote system:
sudo mv /tmp/mydevice /usr/local/bin/
sudo chmod +x /usr/local/bin/mydevice

# Create systemd service
sudo nano /etc/systemd/system/esphome-mydevice.service

# Add service configuration (see below)
# Enable and start
sudo systemctl daemon-reload
sudo systemctl enable esphome-mydevice
sudo systemctl start esphome-mydevice
```

### Step 4: Subsequent Updates (OTA)

```bash
# After initial setup - works like ESP32!
esphome run mydevice.yaml --device 192.168.1.100
```

### Systemd Service Template

```ini
[Unit]
Description=ESPHome mydevice
After=network-online.target
Wants=network-online.target

[Service]
Type=simple
ExecStart=/usr/local/bin/mydevice
Restart=always
RestartSec=5
User=pi
WorkingDirectory=/home/pi

[Install]
WantedBy=multi-user.target
```

### Problems with Current Approach

1. ❌ **Too many manual steps** - User must SSH in and run commands
2. ❌ **Breaks ESPHome UX pattern** - Not a single `esphome run` command
3. ❌ **Error-prone** - Easy to make typos in systemd service
4. ❌ **No log streaming** - Can't see output immediately
5. ❌ **Inconsistent experience** - Different from ESP32 workflow

---

## Proposal 1: SSH-Based `esphome run` (Recommended)

**Goal**: Match the ESP32 UX - one command for initial deployment.

### User Workflow

```bash
# First deployment - SSH-based (like USB serial for ESP32)
esphome run mydevice.yaml --device ssh://pi@raspberrypi

# Subsequent updates - OTA (like ESP32)
esphome run mydevice.yaml --device 192.168.1.100
# or
esphome run mydevice.yaml --device mydevice.local
```

### How It Works

When `--device ssh://user@host` is detected for Linux platform:

1. **Compile locally** (same as now)
2. **Copy binary via SCP**
   ```bash
   scp .esphome/build/mydevice/mydevice pi@raspberrypi:/tmp/
   ```

3. **Generate systemd service** from template
   ```python
   service_content = generate_systemd_service(
       name=config['esphome']['name'],
       binary_path=f"/usr/local/bin/{name}",
       user=ssh_user,
       description=f"ESPHome {name}"
   )
   ```

4. **Install via SSH** (single command chain)
   ```bash
   ssh pi@raspberrypi << 'EOF'
     # Install binary
     sudo install -m 755 /tmp/mydevice /usr/local/bin/mydevice

     # Install service
     echo "$SERVICE_CONTENT" | sudo tee /etc/systemd/system/esphome-mydevice.service

     # Enable and start
     sudo systemctl daemon-reload
     sudo systemctl enable esphome-mydevice
     sudo systemctl restart esphome-mydevice

     # Wait for service to start
     sleep 2
   EOF
   ```

5. **Stream logs via SSH** (like serial monitor)
   ```bash
   ssh pi@raspberrypi 'journalctl -u esphome-mydevice -f'
   ```
   User sees output immediately, can Ctrl+C to disconnect

6. **Detect OTA available** - Parse logs for IP address and port
   ```
   [INFO] Network initialized
   [INFO]   IP Address: 192.168.1.100
   [INFO] OTA server listening on port 3232
   ```

   Display message:
   ```
   ✅ Deployment successful!
   📡 OTA now available at: 192.168.1.100:3232

   Next time, use: esphome run mydevice.yaml --device 192.168.1.100
   ```

### Implementation Location

**File**: `esphome/upload.py` or new `esphome/platform_deploy/linux.py`

```python
async def upload_program(config, target, file):
    """Main upload dispatcher"""

    if CORE.is_linux:
        # Detect SSH target
        if target.startswith('ssh://'):
            return await deploy_linux_ssh(config, target, file)
        else:
            # Standard OTA upload
            return await upload_ota(config, target, file)

    # ... existing ESP32/ESP8266/RP2040 logic


async def deploy_linux_ssh(config, ssh_target, binary_path):
    """Deploy ESPHome binary to Linux target via SSH"""

    # Parse SSH target: ssh://user@host or ssh://user@host:port
    user, host, port = parse_ssh_target(ssh_target)
    device_name = config['esphome']['name']

    logger.info(f"Deploying {device_name} to {host} via SSH...")

    # Step 1: Copy binary
    logger.info("Copying binary...")
    await run_command([
        'scp', '-P', str(port),
        binary_path,
        f'{user}@{host}:/tmp/{device_name}'
    ])

    # Step 2: Generate systemd service
    service_content = generate_systemd_service(config, user)

    # Step 3: Install via SSH
    logger.info("Installing service...")
    install_script = f"""
        set -e
        sudo install -m 755 /tmp/{device_name} /usr/local/bin/{device_name}
        echo '{service_content}' | sudo tee /etc/systemd/system/esphome-{device_name}.service > /dev/null
        sudo systemctl daemon-reload
        sudo systemctl enable esphome-{device_name}
        sudo systemctl restart esphome-{device_name}
        sleep 2
        echo "✅ Service started"
    """

    await run_ssh_command(user, host, port, install_script)

    # Step 4: Stream logs
    logger.info(f"Streaming logs from {device_name}...")
    logger.info("Press Ctrl+C to stop")

    try:
        await stream_ssh_logs(user, host, port, f"esphome-{device_name}")
    except KeyboardInterrupt:
        logger.info("\nLog streaming stopped")

    # Parse IP from logs and display OTA info
    display_ota_info(device_name, detected_ip="192.168.1.100")


def generate_systemd_service(config, user):
    """Generate systemd service file content"""
    name = config['esphome']['name']

    return f"""[Unit]
Description=ESPHome {name}
After=network-online.target
Wants=network-online.target

[Service]
Type=simple
ExecStart=/usr/local/bin/{name}
Restart=always
RestartSec=5
User={user}
WorkingDirectory=/home/{user}

[Install]
WantedBy=multi-user.target
"""


async def stream_ssh_logs(user, host, port, service_name):
    """Stream systemd logs via SSH"""
    cmd = [
        'ssh', '-p', str(port),
        f'{user}@{host}',
        f'journalctl -u {service_name} -f --no-pager'
    ]

    # Stream output to console
    process = await asyncio.create_subprocess_exec(
        *cmd,
        stdout=asyncio.subprocess.PIPE,
        stderr=asyncio.subprocess.PIPE
    )

    while True:
        line = await process.stdout.readline()
        if not line:
            break
        print(line.decode().rstrip())
```

### Configuration Extensions

**Optional**: Add SSH configuration to YAML:

```yaml
linux:
  ssh_deploy:
    user: pi
    default_path: /usr/local/bin
    service_user: pi
```

But this is **not required** - can infer from `--device ssh://user@host`.

### Advantages

1. ✅ **Consistent UX** - Works exactly like ESP32 serial flash
2. ✅ **One command** - No manual SSH steps
3. ✅ **Log streaming** - See output immediately
4. ✅ **Error detection** - Know if deployment failed
5. ✅ **Smooth transition to OTA** - Auto-detects IP and suggests OTA command
6. ✅ **Standard ESPHome workflow** - Familiar to all users

### SSH Authentication

Uses standard SSH authentication:
- SSH keys (recommended) - No password prompt
- Password - User prompted if no key available
- SSH config - Honors `~/.ssh/config` settings

```bash
# Setup SSH key (one-time)
ssh-copy-id pi@raspberrypi

# Then ESPHome works without password
esphome run mydevice.yaml --device ssh://pi@raspberrypi
```

---

## Proposal 2: `prepare-linux` Command (Simpler Alternative)

**Goal**: Generate a deployment package that can be easily installed.

### User Workflow

```bash
# Step 1: Generate deployment package
esphome prepare-linux mydevice.yaml --output /tmp/mydevice-deploy

# Creates:
# /tmp/mydevice-deploy/
#   ├── mydevice              (binary)
#   ├── install.sh            (self-extracting installer)
#   └── mydevice.service      (systemd service)

# Step 2: Deploy via SSH (one line)
scp -r /tmp/mydevice-deploy pi@raspberrypi:/tmp/
ssh pi@raspberrypi 'sudo /tmp/mydevice-deploy/install.sh'

# OR: Deploy with pipe (no files on disk)
ssh pi@raspberrypi 'bash -s' < /tmp/mydevice-deploy/install.sh

# Step 3: Subsequent updates - OTA
esphome run mydevice.yaml --device 192.168.1.100
```

### Generated `install.sh`

```bash
#!/bin/bash
# Auto-generated by ESPHome for device: mydevice
# Generated: 2025-11-08 20:30:00

set -e

DEVICE_NAME="mydevice"
INSTALL_DIR="/usr/local/bin"
SERVICE_FILE="/etc/systemd/system/esphome-${DEVICE_NAME}.service"

echo "🚀 Installing ESPHome device: $DEVICE_NAME"

# Check if running as root or with sudo
if [ "$EUID" -ne 0 ]; then
    echo "❌ This script must be run with sudo"
    exit 1
fi

# Get the script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Install binary
echo "📦 Installing binary to $INSTALL_DIR..."
install -m 755 "$SCRIPT_DIR/$DEVICE_NAME" "$INSTALL_DIR/$DEVICE_NAME"

# Install systemd service
echo "⚙️  Installing systemd service..."
install -m 644 "$SCRIPT_DIR/${DEVICE_NAME}.service" "$SERVICE_FILE"

# Reload systemd and enable service
echo "🔄 Enabling service..."
systemctl daemon-reload
systemctl enable "esphome-${DEVICE_NAME}"
systemctl restart "esphome-${DEVICE_NAME}"

# Wait for service to start
sleep 2

# Check status
if systemctl is-active --quiet "esphome-${DEVICE_NAME}"; then
    echo "✅ $DEVICE_NAME installed and running!"

    # Try to detect IP address
    IP_ADDR=$(hostname -I | awk '{print $1}')
    if [ -n "$IP_ADDR" ]; then
        echo "📡 OTA updates available at: ${IP_ADDR}:3232"
        echo ""
        echo "Next time, use: esphome run mydevice.yaml --device ${IP_ADDR}"
    fi

    echo ""
    echo "View logs with: journalctl -u esphome-${DEVICE_NAME} -f"
else
    echo "❌ Service failed to start. Check logs with:"
    echo "   journalctl -u esphome-${DEVICE_NAME} -n 50"
    exit 1
fi
```

### Implementation

**File**: `esphome/commands/prepare_linux.py`

```python
import os
import shutil
from pathlib import Path

def prepare_linux_deployment(config, output_dir):
    """Generate Linux deployment package"""

    output_path = Path(output_dir)
    output_path.mkdir(parents=True, exist_ok=True)

    device_name = config['esphome']['name']
    binary_src = get_binary_path(config)

    # Copy binary
    shutil.copy(binary_src, output_path / device_name)

    # Generate systemd service
    service_content = generate_systemd_service(config)
    (output_path / f"{device_name}.service").write_text(service_content)

    # Generate installer script
    install_script = generate_install_script(config)
    install_path = output_path / "install.sh"
    install_path.write_text(install_script)
    install_path.chmod(0o755)

    # Generate README
    readme = generate_deployment_readme(config)
    (output_path / "README.txt").write_text(readme)

    print(f"✅ Deployment package created: {output_path}")
    print(f"\nTo deploy:")
    print(f"  scp -r {output_path} pi@raspberrypi:/tmp/")
    print(f"  ssh pi@raspberrypi 'sudo /tmp/{output_path.name}/install.sh'")
```

### Advantages

1. ✅ **Simpler to implement** - No SSH logic in ESPHome
2. ✅ **Portable** - Can share deployment package
3. ✅ **Offline-friendly** - Copy to USB stick, install anywhere
4. ✅ **Transparent** - User can inspect install.sh before running
5. ✅ **Flexible** - Can customize installation location

### Disadvantages

1. ❌ **Still requires manual steps** - User runs scp/ssh commands
2. ❌ **No log streaming** - Must SSH in to see logs
3. ❌ **Different from ESP32 UX** - Not a single command

---

## Comparison Matrix

| Feature | Manual (Current) | SSH Deploy (Proposal 1) | Prepare-Linux (Proposal 2) |
|---------|------------------|-------------------------|---------------------------|
| **Single command** | ❌ No | ✅ Yes | ⚠️ Two commands (scp + ssh) |
| **Matches ESP32 UX** | ❌ No | ✅ Yes | ❌ No |
| **Log streaming** | ❌ Manual | ✅ Automatic | ❌ Manual |
| **Implementation complexity** | N/A | 🔴 Medium (300 LOC) | 🟢 Low (150 LOC) |
| **User learning curve** | 🔴 High | 🟢 Low | 🟡 Medium |
| **Error detection** | ❌ Manual | ✅ Automatic | ⚠️ Via script exit code |
| **Offline deployment** | ✅ Yes | ❌ No | ✅ Yes |
| **Customization** | ✅ Full | ⚠️ Limited | ✅ Full (edit script) |
| **SSH key setup** | Required | Required | Required |
| **Works without network** | ❌ No (needs SSH) | ❌ No (needs SSH) | ✅ Yes (USB stick) |

---

## Recommendation: Implement Both

### Phase 9.5.1: Add `prepare-linux` Command (Quick Win)

Implement Proposal 2 first because:
- ✅ **Easy to implement** (~2-3 hours)
- ✅ **Immediate value** - Better than manual steps
- ✅ **No risk** - Doesn't change existing behavior
- ✅ **Good documentation artifact** - Shows what's needed

**Timeline**: 1 day

### Phase 9.5.2: Add SSH Deploy to `esphome run` (Long-term)

Implement Proposal 1 after because:
- ✅ **Better UX** - Matches ESP32 workflow
- ✅ **Long-term solution** - This is what users expect
- ⚠️ **More complex** - Needs careful testing
- ⚠️ **Requires SSH library integration**

**Timeline**: 2-3 days

### Migration Path

```bash
# Today (Phase 9): Manual deployment
scp binary pi@raspberrypi:/tmp/
ssh pi@raspberrypi
# ... manual service setup

# Phase 9.5.1: prepare-linux command (near-term)
esphome prepare-linux mydevice.yaml
scp -r deploy/ pi@raspberrypi:/tmp/
ssh pi@raspberrypi 'sudo /tmp/deploy/install.sh'

# Phase 9.5.2: SSH deploy (long-term)
esphome run mydevice.yaml --device ssh://pi@raspberrypi

# Always: OTA after initial deployment
esphome run mydevice.yaml --device 192.168.1.100
```

---

## Implementation Checklist

### Phase 9.5.1: `prepare-linux` Command

- [ ] Create `esphome/commands/prepare_linux.py`
- [ ] Implement `generate_install_script()` function
- [ ] Implement `generate_systemd_service()` function (reusable)
- [ ] Implement `generate_deployment_readme()` function
- [ ] Add CLI command: `esphome prepare-linux <config> --output <dir>`
- [ ] Test on Raspberry Pi
- [ ] Document in user guide

**Files to modify:**
- New: `esphome/commands/prepare_linux.py` (~150 LOC)
- Modify: `esphome/__main__.py` (add CLI command)
- New: `docs/linux-deployment-guide.md`

### Phase 9.5.2: SSH Deploy Integration

- [ ] Create `esphome/platform_deploy/linux.py`
- [ ] Implement `deploy_linux_ssh()` function
- [ ] Implement `stream_ssh_logs()` function
- [ ] Integrate with `esphome upload.py`
- [ ] Add SSH target parsing (`ssh://user@host:port`)
- [ ] Test with various SSH configurations
- [ ] Test error handling (connection failures, sudo failures)
- [ ] Document in user guide

**Files to modify:**
- New: `esphome/platform_deploy/linux.py` (~300 LOC)
- Modify: `esphome/upload.py` (add Linux dispatch)
- Modify: `esphome/__main__.py` (pass --device to upload)
- Update: `docs/linux-deployment-guide.md`

### Dependencies

**For SSH deployment**, need to handle:
- SSH key authentication (via system SSH)
- Password authentication (prompt user)
- SSH config file (`~/.ssh/config`)
- Known hosts verification
- Port forwarding conflicts
- Sudo password prompts (should fail gracefully)

**Recommendation**: Use subprocess to call system `ssh` and `scp` commands rather than Python SSH libraries (paramiko, fabric). This:
- ✅ Respects user's SSH config
- ✅ Uses user's SSH keys automatically
- ✅ Handles known_hosts properly
- ✅ Simpler implementation
- ❌ Requires SSH client installed (but should be on any system deploying to Linux)

---

## Testing Plan

### Manual Testing

**Phase 9.5.1: prepare-linux**
1. Generate package: `esphome prepare-linux test.yaml`
2. Verify files created (binary, service, install.sh)
3. SCP to Raspberry Pi
4. Run install.sh with sudo
5. Verify service starts
6. Verify OTA works afterward

**Phase 9.5.2: SSH deploy**
1. Test with SSH key: `esphome run test.yaml --device ssh://pi@raspberrypi`
2. Test with password authentication
3. Test with custom port: `--device ssh://pi@raspberrypi:2222`
4. Test error cases (wrong host, wrong user, no sudo)
5. Verify log streaming works
6. Verify Ctrl+C stops streaming but leaves service running
7. Verify OTA works afterward

### Automated Testing

```python
# tests/test_linux_deployment.py

def test_generate_systemd_service():
    """Test systemd service generation"""
    config = {'esphome': {'name': 'test_device'}}
    service = generate_systemd_service(config, user='pi')

    assert 'ExecStart=/usr/local/bin/test_device' in service
    assert 'User=pi' in service
    assert 'WantedBy=multi-user.target' in service


def test_generate_install_script():
    """Test install script generation"""
    config = {'esphome': {'name': 'test_device'}}
    script = generate_install_script(config)

    assert '#!/bin/bash' in script
    assert 'DEVICE_NAME="test_device"' in script
    assert 'systemctl enable' in script


def test_parse_ssh_target():
    """Test SSH target parsing"""
    user, host, port = parse_ssh_target('ssh://pi@raspberrypi')
    assert user == 'pi'
    assert host == 'raspberrypi'
    assert port == 22

    user, host, port = parse_ssh_target('ssh://user@192.168.1.100:2222')
    assert user == 'user'
    assert host == '192.168.1.100'
    assert port == 2222
```

---

## Documentation Updates

### User Documentation (esphome-docs repo)

Create `docs/guides/linux-deployment.rst`:

```rst
Linux Platform Deployment
==========================

ESPHome can run on Linux systems like Raspberry Pi. This guide covers deployment
methods from initial installation to OTA updates.

Quick Start
-----------

**First Deployment (SSH):**

.. code-block:: bash

    esphome run mydevice.yaml --device ssh://pi@raspberrypi

**Subsequent Updates (OTA):**

.. code-block:: bash

    esphome run mydevice.yaml --device 192.168.1.100

Deployment Methods
------------------

Method 1: SSH Deploy (Recommended)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

One command deployment, just like ESP32 USB flashing:

.. code-block:: bash

    # Setup SSH key (one-time)
    ssh-copy-id pi@raspberrypi

    # Deploy
    esphome run mydevice.yaml --device ssh://pi@raspberrypi

[... full documentation ...]
```

---

## Future Enhancements

### Phase 9.5.3: Advanced Features (Optional)

1. **Docker deployment**
   ```bash
   esphome run mydevice.yaml --device docker://mycontainer
   ```

2. **Ansible playbook generation**
   ```bash
   esphome prepare-ansible mydevice.yaml --output playbook.yml
   ```

3. **Multi-host deployment**
   ```bash
   esphome run mydevice.yaml --device ssh://pi@pi1,ssh://pi@pi2,ssh://pi@pi3
   ```

4. **Systemd service customization**
   ```yaml
   linux:
     systemd:
       restart_sec: 10
       environment:
         MY_VAR: value
       after:
         - custom.service
   ```

---

## Security Considerations

### SSH Key Management

**Best Practice**: Use SSH keys, not passwords
```bash
# Generate key if needed
ssh-keygen -t ed25519 -C "esphome@mydevice"

# Copy to target
ssh-copy-id -i ~/.ssh/id_ed25519 pi@raspberrypi
```

### Sudo Requirements

The installation script needs sudo for:
- Installing binary to `/usr/local/bin/`
- Installing systemd service to `/etc/systemd/system/`
- Running `systemctl daemon-reload` and `systemctl enable`

**Options**:
1. **Prompt for sudo password** (current approach)
2. **Configure passwordless sudo** for install script:
   ```bash
   # /etc/sudoers.d/esphome
   pi ALL=(ALL) NOPASSWD: /bin/systemctl daemon-reload
   pi ALL=(ALL) NOPASSWD: /bin/systemctl enable esphome-*
   pi ALL=(ALL) NOPASSWD: /bin/systemctl restart esphome-*
   pi ALL=(ALL) NOPASSWD: /usr/bin/install * /usr/local/bin/*
   ```

3. **User-space installation** (no sudo):
   - Install to `~/.local/bin/` instead of `/usr/local/bin/`
   - Use systemd user services (`systemctl --user`)
   - Limitation: Won't auto-start on boot by default

### Binary Verification

**Future enhancement**: Sign binaries and verify signatures before installation
```bash
# Generate signature during compilation
openssl dgst -sha256 -sign private.key -out mydevice.sig mydevice

# Verify before installation
openssl dgst -sha256 -verify public.key -signature mydevice.sig mydevice
```

---

## Deliverables

### Phase 9.5.1 (1 day)
- [ ] `prepare-linux` command implemented
- [ ] Generated installer tested on Raspberry Pi
- [ ] Documentation written

### Phase 9.5.2 (2-3 days)
- [ ] SSH deploy implemented in `esphome run`
- [ ] Log streaming works
- [ ] Error handling robust
- [ ] Documentation updated
- [ ] Unit tests written

---

## Success Criteria

### Phase 9.5.1
- ✅ User can generate deployment package in one command
- ✅ Generated installer works on Raspberry Pi
- ✅ Systemd service starts correctly
- ✅ OTA works after installation

### Phase 9.5.2
- ✅ User can deploy with single `esphome run` command
- ✅ Works exactly like ESP32 USB flash (UX-wise)
- ✅ Logs stream to console
- ✅ Error messages are actionable
- ✅ OTA works after SSH deployment
- ✅ No manual SSH steps required

---

**Next Steps**: Begin Phase 9.5.1 implementation with `prepare-linux` command
