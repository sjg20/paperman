#!/bin/bash
# Installation script for paperman-server systemd service

set -e

SERVICE_NAME="paperman-server.service"
SERVICE_FILE="$(dirname "$0")/${SERVICE_NAME}"
SYSTEMD_DIR="/etc/systemd/system"

# Check if running as root
if [ "$EUID" -ne 0 ]; then
    echo "This script must be run as root (use sudo)"
    exit 1
fi

# Check if service file exists
if [ ! -f "$SERVICE_FILE" ]; then
    echo "Error: Service file not found: $SERVICE_FILE"
    exit 1
fi

echo "Installing paperman-server systemd service..."

# Copy service file to systemd directory
echo "  Copying service file to $SYSTEMD_DIR/"
cp "$SERVICE_FILE" "$SYSTEMD_DIR/"

# Reload systemd daemon
echo "  Reloading systemd daemon..."
systemctl daemon-reload

# Enable the service
echo "  Enabling service to start on boot..."
systemctl enable "$SERVICE_NAME"

echo ""
echo "Installation complete!"
echo ""
echo "You can now:"
echo "  Start the service:   sudo systemctl start $SERVICE_NAME"
echo "  Stop the service:    sudo systemctl stop $SERVICE_NAME"
echo "  Check status:        sudo systemctl status $SERVICE_NAME"
echo "  View logs:           sudo journalctl -u $SERVICE_NAME -f"
echo "  Disable autostart:   sudo systemctl disable $SERVICE_NAME"
echo ""
echo "Note: Edit $SYSTEMD_DIR/$SERVICE_NAME to change the repository path"
echo "      Default repository: /home/sglass/papers"
