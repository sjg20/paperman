# Deploying Paperman Server on Palo with HTTPS

This guide covers deploying the paperman search server with HTTPS and authentication on your palo machine.

## Option 1: Using Apache (Recommended - Already Installed)

Since you already have Apache running, this is the simplest option.

### Step 1: Enable Required Apache Modules

```bash
# On palo machine
sudo a2enmod ssl
sudo a2enmod proxy
sudo a2enmod proxy_http
sudo a2enmod headers
sudo a2enmod rewrite
sudo systemctl restart apache2
```

### Step 2: Create SSL Certificate

**Option A: Self-Signed Certificate (for testing/private use)**

```bash
# Create self-signed certificate
sudo mkdir -p /etc/ssl/private
sudo openssl req -x509 -nodes -days 365 -newkey rsa:2048 \
    -keyout /etc/ssl/private/paperman.key \
    -out /etc/ssl/certs/paperman.crt \
    -subj "/CN=palo.local"

# Set permissions
sudo chmod 600 /etc/ssl/private/paperman.key
```

**Option B: Let's Encrypt (for public access)**

```bash
# Install certbot
sudo apt update
sudo apt install certbot python3-certbot-apache

# Get certificate (requires domain name and port 80 open)
sudo certbot --apache -d palo.yourdomain.com
```

### Step 3: Install Apache Configuration

```bash
# Copy configuration file to Apache
sudo cp apache-paperman.conf /etc/apache2/sites-available/paperman.conf

# Edit the configuration
sudo nano /etc/apache2/sites-available/paperman.conf
# Update ServerName to your actual domain or IP

# Enable the site
sudo a2ensite paperman

# Test configuration
sudo apache2ctl configtest

# If OK, reload Apache
sudo systemctl reload apache2
```

### Step 4: Deploy Paperman Server

```bash
# Copy files to palo
scp paperman-server paperman palo:/home/user/

# On palo, install systemd service
scp paperman-server.service palo:/home/user/
ssh palo
cd /home/user
sudo ./install-service.sh

# Edit service to add API key
sudo nano /etc/systemd/system/paperman-server.service
```

Add to the `[Service]` section:
```ini
Environment="PAPERMAN_API_KEY=your-strong-random-key-here"
```

```bash
# Reload and start
sudo systemctl daemon-reload
sudo systemctl start paperman-server
sudo systemctl enable paperman-server
sudo systemctl status paperman-server
```

### Step 5: Test Access

```bash
# From another machine
# Status (no auth required)
curl https://palo.yourdomain.com/status

# Search with API key
curl -H "X-API-Key: your-strong-random-key-here" \
     https://palo.yourdomain.com/search?q=test

# For self-signed cert, use -k to skip verification (testing only!)
curl -k https://palo.local/status
```

---

## Option 2: Using Nginx (Alternative)

If you prefer nginx over Apache:

### Step 1: Install Nginx

```bash
# On palo
sudo apt update
sudo apt install nginx

# Stop Apache if you want nginx on port 80/443
sudo systemctl stop apache2
sudo systemctl disable apache2
```

### Step 2: Create SSL Certificate

Same as Apache Option A or B above.

### Step 3: Install Nginx Configuration

```bash
# Copy configuration
sudo cp nginx-paperman.conf /etc/nginx/sites-available/paperman

# Edit configuration
sudo nano /etc/nginx/sites-available/paperman
# Update server_name and SSL certificate paths

# Enable site
sudo ln -s /etc/nginx/sites-available/paperman /etc/nginx/sites-enabled/

# Remove default site (optional)
sudo rm /etc/nginx/sites-enabled/default

# Test configuration
sudo nginx -t

# If OK, reload
sudo systemctl reload nginx
```

### Step 4: Deploy Paperman Server

Same as Apache Step 4 above.

---

## Firewall Configuration

### Allow HTTPS through firewall

```bash
# UFW (if using)
sudo ufw allow 443/tcp
sudo ufw allow 80/tcp  # For Let's Encrypt or HTTP redirect
sudo ufw status

# Or iptables
sudo iptables -A INPUT -p tcp --dport 443 -j ACCEPT
sudo iptables -A INPUT -p tcp --dport 80 -j ACCEPT
sudo netfilter-persistent save
```

### Block direct access to port 8080 (optional security)

```bash
# Only allow localhost to access paperman-server directly
sudo ufw deny 8080/tcp

# Or with iptables
sudo iptables -A INPUT -p tcp --dport 8080 -i lo -j ACCEPT
sudo iptables -A INPUT -p tcp --dport 8080 -j DROP
sudo netfilter-persistent save
```

---

## Security Checklist

- [ ] SSL/TLS enabled (HTTPS)
- [ ] API key authentication enabled
- [ ] Firewall configured (443 open, 8080 blocked externally)
- [ ] Strong API key generated (use `openssl rand -base64 32`)
- [ ] Systemd service running as non-root user
- [ ] Logs being monitored (`sudo journalctl -u paperman-server -f`)
- [ ] Automatic updates enabled on palo
- [ ] Backups configured

---

## Generating Strong API Key

```bash
# Generate a strong random API key
openssl rand -base64 32

# Example output:
# XzY9mK3nP8qR2vB5jN7hL4wT6dC1sF0g

# Use this in your systemd service:
Environment="PAPERMAN_API_KEY=XzY9mK3nP8qR2vB5jN7hL4wT6dC1sF0g"
```

---

## Troubleshooting

### Check if services are running

```bash
# Paperman server
sudo systemctl status paperman-server

# Apache
sudo systemctl status apache2

# Nginx
sudo systemctl status nginx
```

### View logs

```bash
# Paperman server logs
sudo journalctl -u paperman-server -n 50
sudo journalctl -u paperman-server -f  # Follow

# Apache logs
sudo tail -f /var/log/apache2/paperman-error.log
sudo tail -f /var/log/apache2/paperman-access.log

# Nginx logs
sudo tail -f /var/log/nginx/paperman-error.log
sudo tail -f /var/log/nginx/paperman-access.log
```

### Test SSL certificate

```bash
# Check certificate
openssl s_client -connect palo.yourdomain.com:443 -showcerts

# Check what's listening on ports
sudo netstat -tlnp | grep -E ':(80|443|8080)'
```

### Common issues

**"Connection refused"**
- Check firewall: `sudo ufw status`
- Check service running: `sudo systemctl status paperman-server`
- Check port binding: `sudo netstat -tlnp | grep 8080`

**"502 Bad Gateway"**
- Paperman server not running: `sudo systemctl start paperman-server`
- Wrong proxy_pass URL in config

**"401 Unauthorized"**
- Check API key matches in systemd service and client request
- Check logs: `sudo journalctl -u paperman-server | grep Authentication`

**SSL certificate warnings**
- Self-signed cert: Expected, use `-k` flag with curl or add exception in browser
- Let's Encrypt: Check domain name and renewal

---

## Client Configuration Examples

### Browser Bookmarklet

Create a bookmarklet to add API key automatically:

```javascript
javascript:(function(){
  var url = 'https://palo.yourdomain.com/search?q=' +
            encodeURIComponent(window.getSelection().toString() || prompt('Search:'));
  fetch(url, {
    headers: {'X-API-Key': 'your-api-key-here'}
  }).then(r=>r.json()).then(console.log);
})();
```

### Python Script

```python
#!/usr/bin/env python3
import requests

API_KEY = "your-api-key-here"
BASE_URL = "https://palo.yourdomain.com"

def search_papers(query):
    response = requests.get(
        f"{BASE_URL}/search",
        params={'q': query},
        headers={'X-API-Key': API_KEY},
        verify=True  # Set to False for self-signed certs (not recommended)
    )
    return response.json()

if __name__ == "__main__":
    results = search_papers("invoice")
    print(f"Found {results['count']} files")
    for file in results.get('files', []):
        print(f"  - {file['name']}")
```

### Shell Script

```bash
#!/bin/bash
# paperman-search.sh

API_KEY="your-api-key-here"
BASE_URL="https://palo.yourdomain.com"

search() {
    curl -s -H "X-API-Key: $API_KEY" \
         "$BASE_URL/search?q=$1" | jq .
}

download() {
    curl -H "X-API-Key: $API_KEY" \
         "$BASE_URL/file?path=$1" -o "$2"
}

# Usage:
# ./paperman-search.sh search invoice
# ./paperman-search.sh download path/to/file.pdf output.pdf
```

---

## Monitoring and Maintenance

### Set up log rotation

Apache and nginx handle their own log rotation. For paperman-server:

```bash
# Create /etc/logrotate.d/paperman-server
sudo nano /etc/logrotate.d/paperman-server
```

```
/var/log/paperman-server/*.log {
    daily
    rotate 14
    compress
    delaycompress
    notifempty
    create 0640 sglass sglass
    sharedscripts
    postrotate
        systemctl reload paperman-server > /dev/null
    endscript
}
```

### Monitor with systemd

```bash
# Enable email notifications on failure (requires mail setup)
sudo systemctl edit paperman-server.service
```

Add:
```ini
[Service]
OnFailure=status-email@%n.service
```

---

## Performance Tuning

For Apache, increase timeout for large PDF conversions:

```apache
# In /etc/apache2/sites-available/paperman.conf
ProxyTimeout 120
Timeout 120
```

For nginx:

```nginx
# In /etc/nginx/sites-available/paperman
proxy_connect_timeout 120s;
proxy_send_timeout 120s;
proxy_read_timeout 120s;
```
