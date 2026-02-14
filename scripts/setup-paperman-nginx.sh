#!/bin/bash
#
# Automated setup script for Paperman Search Server with nginx
# This script sets up nginx reverse proxy with HTTPS and systemd service
#
# Usage: sudo scripts/setup-paperman-nginx.sh   (from project root)
#    or: sudo ./setup-paperman-nginx.sh          (from scripts/)
#

set -e  # Exit on error

# Resolve project root from the script's location
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJ_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Helper functions
info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

error() {
    echo -e "${RED}[ERROR]${NC} $1"
    exit 1
}

check_root() {
    if [ "$EUID" -ne 0 ]; then
        error "Please run as root (use sudo)"
    fi
}

check_prerequisites() {
    info "Checking prerequisites..."

    local missing=""

    command -v nginx >/dev/null 2>&1 || missing="$missing nginx"
    command -v openssl >/dev/null 2>&1 || missing="$missing openssl"
    command -v systemctl >/dev/null 2>&1 || missing="$missing systemd"
    command -v curl >/dev/null 2>&1 || missing="$missing curl"

    if [ -n "$missing" ]; then
        warn "Missing packages:$missing"
        read -p "Install missing packages? (y/n) " -n 1 -r
        echo
        if [[ $REPLY =~ ^[Yy]$ ]]; then
            apt update
            apt install -y $missing
        else
            error "Required packages not installed"
        fi
    fi

    info "All prerequisites satisfied"
}

check_files() {
    info "Checking required files in $PROJ_DIR ..."

    [ -f "$PROJ_DIR/paperman" ] || error "paperman binary not found in $PROJ_DIR"
    [ -f "$PROJ_DIR/paperman-server" ] || error "paperman-server binary not found in $PROJ_DIR"
    [ -f "$SCRIPT_DIR/nginx-paperman.conf" ] || error "nginx-paperman.conf not found in $SCRIPT_DIR"
    [ -f "$SCRIPT_DIR/paperman-server.service" ] || error "paperman-server.service not found in $SCRIPT_DIR"

    # Make binaries executable
    chmod +x "$PROJ_DIR/paperman" "$PROJ_DIR/paperman-server"

    info "All required files present"
}

stop_apache() {
    if systemctl is-active --quiet apache2; then
        info "Apache is running. Stopping Apache..."
        systemctl stop apache2
        systemctl disable apache2
        info "Apache stopped and disabled"
    fi
}

generate_ssl_cert() {
    info "Generating SSL certificate..."

    mkdir -p /etc/nginx/ssl

    if [ -f "/etc/nginx/ssl/paperman.crt" ]; then
        warn "SSL certificate already exists"
        read -p "Regenerate? (y/n) " -n 1 -r
        echo
        [[ ! $REPLY =~ ^[Yy]$ ]] && return
    fi

    HOSTNAME=$(hostname -f 2>/dev/null || hostname)

    openssl req -x509 -nodes -days 365 -newkey rsa:2048 \
        -keyout /etc/nginx/ssl/paperman.key \
        -out /etc/nginx/ssl/paperman.crt \
        -subj "/CN=$HOSTNAME" \
        2>/dev/null

    chmod 600 /etc/nginx/ssl/paperman.key
    chmod 644 /etc/nginx/ssl/paperman.crt

    info "SSL certificate generated for $HOSTNAME"
}

install_nginx_config() {
    info "Installing nginx configuration..."

    cp "$SCRIPT_DIR/nginx-paperman.conf" /etc/nginx/sites-available/paperman

    # Update hostname in config
    HOSTNAME=$(hostname -f 2>/dev/null || hostname)
    sed -i "s/your-server.example.com/$HOSTNAME/g" /etc/nginx/sites-available/paperman

    # Remove default site
    rm -f /etc/nginx/sites-enabled/default

    # Enable paperman site
    ln -sf /etc/nginx/sites-available/paperman /etc/nginx/sites-enabled/

    # Test configuration
    info "Testing nginx configuration..."
    if nginx -t 2>/dev/null; then
        info "Nginx configuration valid"
    else
        error "Nginx configuration test failed"
    fi

    # Reload nginx
    if systemctl is-active --quiet nginx; then
        systemctl reload nginx
    else
        systemctl start nginx
    fi
    systemctl enable nginx

    info "Nginx configured and running"
}

install_paperman_service() {
    info "Installing paperman service..."

    # Generate API key
    API_KEY=$(openssl rand -base64 32)

    # Save API key to file
    echo "$API_KEY" > ~/paperman-api-key.txt
    chmod 600 ~/paperman-api-key.txt

    info "Generated API key saved to ~/paperman-api-key.txt"
    echo ""
    echo "================================================"
    echo "API KEY (save this!): $API_KEY"
    echo "================================================"
    echo ""

    # Copy service file
    cp "$SCRIPT_DIR/paperman-server.service" /etc/systemd/system/

    # Add API key to service
    sed -i "/\[Service\]/a Environment=\"PAPERMAN_API_KEY=$API_KEY\"" /etc/systemd/system/paperman-server.service

    # Update paths to point at the project directory
    sed -i "s|/opt/paperman|$PROJ_DIR|g" /etc/systemd/system/paperman-server.service
    sed -i "s|/srv/papers|$PROJ_DIR/papers|g" /etc/systemd/system/paperman-server.service

    # Create papers directory
    mkdir -p "$PROJ_DIR/papers"

    # Reload systemd and start service
    systemctl daemon-reload
    systemctl start paperman-server
    systemctl enable paperman-server

    # Wait a moment for service to start
    sleep 2

    if systemctl is-active --quiet paperman-server; then
        info "Paperman service is running"
    else
        error "Paperman service failed to start. Check: journalctl -u paperman-server -n 50"
    fi
}

configure_firewall() {
    info "Configuring firewall..."

    if command -v ufw >/dev/null 2>&1; then
        # Using UFW
        ufw allow 443/tcp >/dev/null 2>&1 || true
        ufw allow 80/tcp >/dev/null 2>&1 || true
        ufw deny 8080/tcp >/dev/null 2>&1 || true
        info "Firewall configured with ufw"
    elif command -v iptables >/dev/null 2>&1; then
        # Using iptables
        iptables -A INPUT -p tcp --dport 443 -j ACCEPT 2>/dev/null || true
        iptables -A INPUT -p tcp --dport 80 -j ACCEPT 2>/dev/null || true
        iptables -A INPUT -p tcp --dport 8080 -i lo -j ACCEPT 2>/dev/null || true
        iptables -A INPUT -p tcp --dport 8080 -j DROP 2>/dev/null || true

        if command -v netfilter-persistent >/dev/null 2>&1; then
            netfilter-persistent save >/dev/null 2>&1 || true
        fi
        info "Firewall configured with iptables"
    else
        warn "No firewall detected (ufw or iptables)"
    fi
}

test_setup() {
    info "Testing setup..."

    # Get API key
    API_KEY=$(cat ~/paperman-api-key.txt)

    # Test 1: Status endpoint (no auth)
    echo -n "  Testing status endpoint... "
    if curl -k -s https://localhost/status | grep -q "running"; then
        echo -e "${GREEN}OK${NC}"
    else
        echo -e "${RED}FAILED${NC}"
        warn "Status endpoint test failed"
    fi

    # Test 2: Auth required
    echo -n "  Testing authentication (should fail)... "
    if curl -k -s https://localhost/search?q=test | grep -q "Invalid or missing API key"; then
        echo -e "${GREEN}OK${NC}"
    else
        echo -e "${YELLOW}UNEXPECTED${NC}"
    fi

    # Test 3: Auth with key
    echo -n "  Testing with API key... "
    if curl -k -s -H "X-API-Key: $API_KEY" https://localhost/search?q=test | grep -q "success"; then
        echo -e "${GREEN}OK${NC}"
    else
        echo -e "${RED}FAILED${NC}"
        warn "API key authentication test failed"
    fi
}

print_summary() {
    HOSTNAME=$(hostname -f 2>/dev/null || hostname)
    IP=$(hostname -I | awk '{print $1}')
    API_KEY=$(cat ~/paperman-api-key.txt)

    echo ""
    echo "================================================"
    echo "           SETUP COMPLETE! 🎉"
    echo "================================================"
    echo ""
    echo "Server URL:  https://$HOSTNAME"
    echo "         or  https://$IP"
    echo ""
    echo "API Key:     $API_KEY"
    echo "  (saved in ~/paperman-api-key.txt)"
    echo ""
    echo "Usage examples:"
    echo ""
    echo "  # Test status"
    echo "  curl -k https://$IP/status"
    echo ""
    echo "  # Search files"
    echo "  curl -k -H 'X-API-Key: $API_KEY' \\"
    echo "       https://$IP/search?q=test"
    echo ""
    echo "  # Download file"
    echo "  curl -k -H 'X-API-Key: $API_KEY' \\"
    echo "       https://$IP/file?path=doc.pdf -o doc.pdf"
    echo ""
    echo "Service management:"
    echo "  sudo systemctl status paperman-server"
    echo "  sudo systemctl status nginx"
    echo "  sudo journalctl -u paperman-server -f"
    echo ""
    echo "Logs:"
    echo "  sudo tail -f /var/log/nginx/paperman-access.log"
    echo "  sudo tail -f /var/log/nginx/paperman-error.log"
    echo ""
    echo "================================================"
}

# Main execution
main() {
    echo ""
    echo "================================================"
    echo "  Paperman Server Setup with Nginx"
    echo "================================================"
    echo ""

    check_root
    check_prerequisites
    check_files
    stop_apache
    generate_ssl_cert
    install_nginx_config
    install_paperman_service
    configure_firewall
    test_setup
    print_summary
}

# Run main function
main
