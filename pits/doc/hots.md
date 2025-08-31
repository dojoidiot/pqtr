# Photo Image Transfer System Hotspot Connection Script (hots.sh)

## Overview

`hots.sh` is a production-quality bash script that enables development machines to connect to Photo Image Transfer System IoT devices for testing purposes. It automatically detects and preserves the original network state, connects to PITS hotspots, and restores the original network configuration when testing is complete.

## Features

- **Automatic Network Detection**: Intelligently detects whether the dev machine is on WiFi, wired, or both
- **State Preservation**: Saves complete network state before switching to Photo Image Transfer System hotspot
- **Smart Restoration**: Restores exactly the network configuration that existed before testing
- **Comprehensive Testing**: Tests connectivity, HTTP access, and provides testing guidance
- **Production Ready**: Robust error handling, logging, and cleanup

## Requirements

- **NetworkManager**: Must be running and active
- **Required Packages**: `nmcli`, `ip` (usually available by default)
- **WiFi Capability**: Dev machine must have WiFi hardware
- **Photo Image Transfer System Device**: Target Photo Image Transfer System device must be running `apnt.sh` to create hotspot

## Usage

### Basic Commands

```bash
# Connect to Photo Image Transfer System hotspot for instance 00000001
./hots.sh connect 00000001

# Restore original network state
./hots.sh down

# Show current network status
./hots.sh status

# Test connectivity to PITS device
./hots.sh test 00000001

# Show help
./hots.sh help
```

### Command Details

#### `connect <inum>`
- **Purpose**: Connects to Photo Image Transfer System hotspot for specified instance
- **Parameters**: `inum` - Instance number (e.g., "00000001")
- **Actions**:
  - Saves current network state
  - Connects to Photo Image Transfer System hotspot
- Tests connectivity
- Provides testing guidance

#### `down`
- **Purpose**: Restores original network state
- **Actions**:
- Disconnects from Photo Image Transfer System hotspot
- Restores original WiFi/wired connections
- Tests internet connectivity
- Cleans up temporary files

#### `status`
- **Purpose**: Shows current network status
- **Output**: Active connections and device status

#### `test <inum>`
- **Purpose**: Tests connectivity to PITS device
- **Parameters**: `inum` - Instance number to test
- **Actions**: Performs connectivity tests without changing network state

## How It Works

### 1. Network State Detection
The script automatically detects the current network configuration:

```bash
# Detects WiFi connections (excluding PITS)
wifi_conns=$(nmcli -t -f NAME,DEVICE,TYPE connection show --active | grep "wlan" | grep -v "PITS" | cut -d: -f1 | head -1)

# Detects wired connections
wired_conns=$(nmcli -t -f NAME,DEVICE,TYPE connection show --active | grep "ethernet" | cut -d: -f1 | head -1)
```

### 2. State Preservation
Saves network state to `/tmp/pits/network_state.txt`:

```
# Active connections
nmcli connection show --active

# Connection types
WIFI:MyWiFiNetwork
WIRED:Ethernet-1
```

### 3. Hotspot Connection
- Scans for available networks
- Connects to Photo Image Transfer System hotspot (open network)
- Waits for IP assignment
- Tests connectivity

### 4. Network Restoration
- Disconnects from Photo Image Transfer System hotspot
- Restores original connections based on saved state
- Tests internet connectivity
- Cleans up temporary files

## Configuration

### Instance Configuration Files
The script reads from `var/{inum}.conf` files:

```ini
[instance]
hostname = "y9hnkl"
```

### SSID Generation
SSID is automatically generated as: `Photo Image Transfer System-{hostname}`

**Example**: `hostname = "y9hnkl"` → SSID: `Photo Image Transfer System-y9hnkl`

### Network Details
- **Gateway IP**: 192.168.4.1
- **Network**: 192.168.4.0/24
- **DHCP Range**: 192.168.4.10 - 192.168.4.100

## Testing Workflow

### 1. Prepare Photo Image Transfer System Device
```bash
# On Photo Image Transfer System device
./apnt.sh start y9hnkl
```

### 2. Connect from Dev Machine
```bash
# On dev machine
./hots.sh connect 00000001
```

### 3. Test Photo Image Transfer System Services
```bash
# Test FTP
ftp 192.168.4.1
# Username: y9hnkl, Password: y9hnkl

# Test HTTP
curl http://192.168.4.1

# Test other services as needed
```

### 4. Restore Network
```bash
# When testing complete
./hots.sh down
```

## Error Handling

### Prerequisites Check
- Verifies required packages are installed
- Ensures NetworkManager is running
- Exits with helpful error messages if requirements not met

### Connection Failures
- Checks if Photo Image Transfer System hotspot is visible
- Provides clear error messages
- Suggests troubleshooting steps

### Restoration Failures
- Gracefully handles missing state files
- Attempts fallback restoration methods
- Logs all actions for debugging

## Logging

### Log File Location
- **Path**: `/tmp/pits/hots.log`
- **Format**: Timestamped entries with color-coded levels

### Log Levels
- **INFO**: General information and status updates
- **WARN**: Non-critical issues and warnings
- **ERROR**: Critical errors that prevent operation
- **LOG**: Successful operations and confirmations

### Log Rotation
Logs are stored in `/tmp/pits/` and cleaned up automatically after successful restoration.

## Troubleshooting

### Common Issues

#### "NetworkManager is not running"
```bash
sudo systemctl start NetworkManager
sudo systemctl enable NetworkManager
```

#### "Hotspot not found"
- Ensure PITS device is running `apnt.sh`
- Check if WiFi is enabled on dev machine
- Verify SSID matches expected format

#### "Cannot restore network state"
- Check if state file exists: `ls -la /tmp/pits/network_state.txt`
- Verify NetworkManager is running
- Check for permission issues

#### "No WiFi IP address found"
- Wait longer for IP assignment
- Check if PITS device DHCP is working
- Verify WiFi connection is active

### Debug Mode
For troubleshooting, you can examine the state file:

```bash
# Check saved network state
cat /tmp/pits/network_state.txt

# Check logs
tail -f /tmp/pits/hots.log
```

## Security Considerations

### Network Isolation
- Photo Image Transfer System hotspot operates on isolated network (192.168.4.0/24)
- No direct internet access from Photo Image Transfer System network
- Dev machine temporarily loses internet during testing

### State File Security
- State files stored in `/tmp/pits/` (temporary directory)
- Files automatically cleaned up after restoration
- No sensitive information stored permanently

### Hotspot Security
- Photo Image Transfer System hotspots are configured as open networks
- No encryption for field testing scenarios
- Consider WPA2 for production deployments

## Integration with PITS System

### Dependencies
- **apnt.sh**: Creates WiFi hotspot on Photo Image Transfer System device
- **Instance Configs**: Provide hostname for SSID generation
- **pits.conf**: Contains network configuration details

### Workflow Integration
1. **Photo Image Transfer System Device**: Runs `apnt.sh` to create hotspot
2. **Dev Machine**: Uses `hots.sh` to connect and test
3. **Testing**: FTP, HTTP, and other service validation
4. **Cleanup**: `hots.sh down` restores original network

## Performance Characteristics

### Connection Time
- **Network Scan**: ~2 seconds
- **Connection**: ~3-5 seconds
- **IP Assignment**: ~3 seconds
- **Total Connect Time**: ~8-10 seconds

### Restoration Time
- **Disconnect**: ~1 second
- **Restore Connections**: ~3 seconds
- **Stabilization**: ~3 seconds
- **Total Restore Time**: ~7 seconds

### Resource Usage
- **Memory**: Minimal (bash script)
- **CPU**: Low (network operations only)
- **Disk**: Temporary files only
- **Network**: Brief interruption during switch

## Best Practices

### Development Workflow
1. **Save Work**: Ensure all work is saved before switching networks
2. **Plan Testing**: Have a clear testing plan to minimize network switching
3. **Batch Operations**: Group multiple tests together
4. **Quick Restoration**: Use `down` command immediately after testing

### Network Management
1. **Stable Connections**: Ensure original network is stable before testing
2. **Backup Plans**: Have alternative network access if restoration fails
3. **Monitoring**: Watch for network issues during testing
4. **Cleanup**: Always use `down` command to restore network

### Error Recovery
1. **Check Logs**: Review logs for error details
2. **Verify Prerequisites**: Ensure all requirements are met
3. **Manual Restoration**: Use NetworkManager GUI if script fails
4. **Support**: Document issues for team troubleshooting

## Future Enhancements

### Potential Improvements
- **Multiple Instance Support**: Connect to multiple PITS devices
- **Network Profiles**: Save/restore multiple network configurations
- **Automated Testing**: Built-in service testing capabilities
- **Configuration Management**: External configuration file support
- **Monitoring**: Real-time network status monitoring

### Compatibility
- **Linux Distributions**: Tested on Ubuntu/Debian, compatible with most Linux
- **NetworkManager Versions**: Compatible with NetworkManager 1.0+
- **Hardware**: Works with most WiFi adapters and network cards

## Support and Maintenance

### Code Quality
- **Bash Best Practices**: Follows shell scripting best practices
- **Error Handling**: Comprehensive error checking and recovery
- **Logging**: Detailed logging for debugging and monitoring
- **Documentation**: Inline comments and comprehensive documentation

### Maintenance
- **Regular Testing**: Test with different network configurations
- **Update Dependencies**: Keep NetworkManager and tools updated
- **Monitor Logs**: Review logs for patterns and issues
- **User Feedback**: Incorporate user experience improvements

---

*This script is part of the PITS (Picture Image Transfer System) project and is designed for development and testing workflows.*
