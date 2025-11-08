# Paperman Server Systemd Service

This directory contains a systemd service unit for running paperman-server as a system service.

## Files

- `paperman-server.service` - Systemd service unit file
- `install-service.sh` - Installation script
- `SYSTEMD-README.md` - This file

## Installation

1. **Edit the service file** to configure your repository path:
   ```bash
   nano paperman-server.service
   ```

   Change this line to point to your papers directory:
   ```
   ExecStart=/home/sglass/files/max/git/paperman-server /home/sglass/papers
   ```

2. **Install the service** (requires root):
   ```bash
   sudo ./install-service.sh
   ```

## Usage

### Start the service
```bash
sudo systemctl start paperman-server
```

### Stop the service
```bash
sudo systemctl stop paperman-server
```

### Check status
```bash
sudo systemctl status paperman-server
```

### Enable autostart on boot
```bash
sudo systemctl enable paperman-server
```

### Disable autostart
```bash
sudo systemctl disable paperman-server
```

### View logs
```bash
# Follow logs in real-time
sudo journalctl -u paperman-server -f

# View last 50 lines
sudo journalctl -u paperman-server -n 50

# View logs since boot
sudo journalctl -u paperman-server -b
```

### Restart after configuration changes
```bash
sudo systemctl restart paperman-server
```

## Configuration

The service runs with these settings:

- **User**: sglass
- **Group**: sglass
- **Working Directory**: /home/sglass/files/max/git
- **Default Port**: 8080
- **Repository**: /home/sglass/papers (change in ExecStart line)
- **Auto-restart**: On failure, with 5 second delay
- **Private temp**: Yes (isolated /tmp)

## Modifying the Service

If you need to change the configuration:

1. Edit the service file:
   ```bash
   sudo nano /etc/systemd/system/paperman-server.service
   ```

2. Reload systemd:
   ```bash
   sudo systemctl daemon-reload
   ```

3. Restart the service:
   ```bash
   sudo systemctl restart paperman-server
   ```

## Advanced Options

### Change port
Edit the service file and modify ExecStart:
```
ExecStart=/home/sglass/files/max/git/paperman-server -p 9000 /home/sglass/papers
```

### Multiple repositories
```
ExecStart=/home/sglass/files/max/git/paperman-server /home/sglass/papers /home/sglass/archive
```

## Troubleshooting

### Service won't start
```bash
# Check detailed status
sudo systemctl status paperman-server

# Check logs for errors
sudo journalctl -u paperman-server -n 100
```

### Permission issues
- Ensure the `paperman-server` binary is executable: `chmod +x paperman-server`
- Ensure the `paperman` binary is in the same directory
- Verify the repository path exists and is readable by the service user

### Port already in use
If port 8080 is already in use, add `-p <port>` to ExecStart to use a different port.

## Uninstallation

```bash
# Stop and disable the service
sudo systemctl stop paperman-server
sudo systemctl disable paperman-server

# Remove the service file
sudo rm /etc/systemd/system/paperman-server.service

# Reload systemd
sudo systemctl daemon-reload
```

## Security Notes

The service runs with:
- `NoNewPrivileges=true` - Prevents privilege escalation
- `PrivateTmp=yes` - Isolated temporary directory
- Runs as non-root user (sglass)

For additional security, consider:
- Using a dedicated service account
- Adding firewall rules to restrict access
- Enabling HTTPS with a reverse proxy (nginx/apache)
