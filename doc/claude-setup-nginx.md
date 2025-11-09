# Instructions for Claude Code: Set Up Paperman with Nginx

**TO CLAUDE CODE ON THE SERVER:**

You are running on a Linux server. Your task is to set up the paperman search server with nginx as a reverse proxy with HTTPS.

## Context

You have these files in the current directory:
- `paperman` - Main application binary
- `paperman-server` - Search server binary
- `nginx-paperman.conf` - nginx configuration template
- `paperman-server.service` - Systemd service file
- `setup-paperman-nginx.sh` - Setup script (execute this)

## Your Task

Execute the setup script and help the user through any issues:

```bash
sudo ./setup-paperman-nginx.sh
```

The script will:
1. Check prerequisites (nginx, openssl, systemd)
2. Create SSL certificates
3. Install nginx configuration
4. Install systemd service with API key
5. Configure firewall
6. Start services
7. Test everything

## Step-by-Step Guidance

### 1. Prerequisites Check

Verify these are installed:
- nginx
- openssl
- systemd
- curl (for testing)

If missing, install with:
```bash
sudo apt update
sudo apt install nginx openssl curl
```

### 2. Stop Apache (if running)

```bash
sudo systemctl stop apache2
sudo systemctl disable apache2
```

### 3. Generate SSL Certificate

Create self-signed certificate:
```bash
sudo mkdir -p /etc/nginx/ssl
sudo openssl req -x509 -nodes -days 365 -newkey rsa:2048 \
    -keyout /etc/nginx/ssl/paperman.key \
    -out /etc/nginx/ssl/paperman.crt \
    -subj "/CN=$(hostname)"
sudo chmod 600 /etc/nginx/ssl/paperman.key
sudo chmod 644 /etc/nginx/ssl/paperman.crt
```

### 4. Install Nginx Configuration

```bash
# Copy configuration
sudo cp nginx-paperman.conf /etc/nginx/sites-available/paperman

# Edit to set correct hostname
HOSTNAME=$(hostname -f)
sudo sed -i "s/your-server.example.com/$HOSTNAME/g" /etc/nginx/sites-available/paperman

# Enable site
sudo rm -f /etc/nginx/sites-enabled/default
sudo ln -s /etc/nginx/sites-available/paperman /etc/nginx/sites-enabled/

# Test configuration
sudo nginx -t

# If test passes, reload
sudo systemctl reload nginx || sudo systemctl start nginx
sudo systemctl enable nginx
```

### 5. Install Paperman Service

```bash
# Generate strong API key
API_KEY=$(openssl rand -base64 32)
echo "Generated API Key: $API_KEY"
echo "Save this key - you'll need it to access the API!"
echo "$API_KEY" > ~/paperman-api-key.txt
chmod 600 ~/paperman-api-key.txt

# Copy service file
sudo cp paperman-server.service /etc/systemd/system/

# Add API key to service
sudo sed -i "/\[Service\]/a Environment=\"PAPERMAN_API_KEY=$API_KEY\"" /etc/systemd/system/paperman-server.service

# Update paths in service file (assuming files are in current directory)
CURRENT_DIR=$(pwd)
sudo sed -i "s|/opt/paperman|$CURRENT_DIR|g" /etc/systemd/system/paperman-server.service
sudo sed -i "s|/srv/papers|$CURRENT_DIR/papers|g" /etc/systemd/system/paperman-server.service

# Create papers directory if it doesn't exist
mkdir -p $CURRENT_DIR/papers

# Reload and start
sudo systemctl daemon-reload
sudo systemctl start paperman-server
sudo systemctl enable paperman-server
sudo systemctl status paperman-server
```

### 6. Configure Firewall

```bash
# Allow HTTPS
sudo ufw allow 443/tcp || sudo iptables -A INPUT -p tcp --dport 443 -j ACCEPT

# Allow HTTP (for redirects)
sudo ufw allow 80/tcp || sudo iptables -A INPUT -p tcp --dport 80 -j ACCEPT

# Block direct access to 8080 from external (security)
sudo ufw deny 8080/tcp || {
    sudo iptables -A INPUT -p tcp --dport 8080 -i lo -j ACCEPT
    sudo iptables -A INPUT -p tcp --dport 8080 -j DROP
}

# Save iptables if using
which netfilter-persistent && sudo netfilter-persistent save
```

### 7. Test the Setup

```bash
# Get the API key
API_KEY=$(cat ~/paperman-api-key.txt)

# Test status endpoint (no auth needed)
curl -k https://localhost/status

# Test search with API key
curl -k -H "X-API-Key: $API_KEY" https://localhost/search?q=test

# Check logs
sudo journalctl -u paperman-server -n 20
sudo tail -20 /var/log/nginx/paperman-error.log
```

### 8. Verify Everything

Check these items:
- [ ] nginx is running: `sudo systemctl status nginx`
- [ ] paperman-server is running: `sudo systemctl status paperman-server`
- [ ] Port 443 is listening: `sudo netstat -tlnp | grep 443`
- [ ] Port 8080 is listening: `sudo netstat -tlnp | grep 8080`
- [ ] Status endpoint works: `curl -k https://localhost/status`
- [ ] API key authentication works
- [ ] Firewall is configured
- [ ] Services start on boot

## Troubleshooting

### Issue: "nginx: command not found"
```bash
sudo apt update
sudo apt install nginx
```

### Issue: "Address already in use" (port 80 or 443)
```bash
# Check what's using the port
sudo netstat -tlnp | grep -E ':(80|443)'

# If Apache is still running
sudo systemctl stop apache2
sudo systemctl disable apache2
```

### Issue: "502 Bad Gateway"
```bash
# Check if paperman-server is running
sudo systemctl status paperman-server

# Check if it's listening on 8080
sudo netstat -tlnp | grep 8080

# Check logs
sudo journalctl -u paperman-server -n 50
```

### Issue: "nginx test failed"
```bash
# See detailed error
sudo nginx -t

# Common fixes:
# - Check SSL certificate paths exist
# - Check syntax (semicolons, braces)
# - Check file permissions
```

### Issue: Can't access from external network
```bash
# Check firewall
sudo ufw status
sudo iptables -L -n

# Check nginx is listening on all interfaces
sudo netstat -tlnp | grep nginx

# Check logs
sudo tail -f /var/log/nginx/paperman-error.log
```

## Success Criteria

When complete, you should see:

1. **Nginx status:**
   ```
   ● nginx.service - A high performance web server
   Active: active (running)
   ```

2. **Paperman status:**
   ```
   ● paperman-server.service - Paperman Search Server
   Active: active (running)
   ```

3. **Status endpoint works:**
   ```json
   {"status":"running","repository":"..."}
   ```

4. **API authentication works:**
   ```bash
   # Without key - fails
   curl -k https://localhost/search?q=test
   # Returns: {"error":"Invalid or missing API key..."}

   # With key - works
   curl -k -H "X-API-Key: YOUR-KEY" https://localhost/search?q=test
   # Returns: {"success":true,...}
   ```

## Final Steps

Once everything is working:

1. **Save the API key** - Located in `~/paperman-api-key.txt`
2. **Note the server URL** - `https://$(hostname -f)` or `https://$(hostname -I | awk '{print $1}')`
3. **Test from external machine** if applicable
4. **Set up monitoring** (optional):
   ```bash
   # Watch logs
   sudo journalctl -u paperman-server -f

   # Watch nginx logs
   sudo tail -f /var/log/nginx/paperman-access.log
   ```

## Report to User

When complete, provide:
1. ✅ Status of all services
2. ✅ API key (from ~/paperman-api-key.txt)
3. ✅ Server URL
4. ✅ Test results
5. ✅ Any warnings or issues encountered

Tell the user:
- How to access the server
- How to use the API key
- Where logs are located
- How to check service status

## Remember

- Always run commands with `sudo` where needed
- Check for errors after each step
- If something fails, check logs and explain the issue to the user
- The API key is sensitive - handle it securely
- Test everything before declaring success

Good luck! 🚀
