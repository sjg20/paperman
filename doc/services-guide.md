# Paperman Services Guide

This guide covers the nginx web server and paperman-server backend API
running on your-server.example.com.

## Overview

The system consists of two main components:

1. **Nginx Web Server** - Handles HTTPS, authentication, and serves static files
2. **Paperman-Server Backend** - Provides document search and API functionality

## System Architecture

```
Internet -> Nginx (Port 443 HTTPS) -> Paperman Backend (Port 8080)
                |
         Static Files (/var/www/paperman)
```

### Components

- **Domain:** your-server.example.com
- **SSL Certificates:** Let's Encrypt (in `/etc/letsencrypt/live/your-server.example.com/`)
- **Authentication:** HTTP Basic Auth (password file: `/etc/nginx/.htpasswd`)
- **Document Repository:** `/srv/papers`

## Nginx Web Server

### Service Management

```sh
# Check status
sudo systemctl status nginx

# Start/Stop/Restart
sudo systemctl start nginx
sudo systemctl stop nginx
sudo systemctl restart nginx

# Reload configuration (without downtime)
sudo systemctl reload nginx

# Test configuration before reloading
sudo nginx -t
```

### Configuration Files

- **Main config:** `/etc/nginx/nginx.conf`
- **Site config:** `/etc/nginx/sites-available/paperman-server`
- **Enabled via symlink:** `/etc/nginx/sites-enabled/paperman-server`

### URL Structure and Authentication

| URL Pattern | Auth Required | Purpose |
|---|---|---|
| `/` or `/index.html` | No | Main landing page |
| `/*.html` (other) | Yes | All other HTML pages (browse.html, search.html) |
| `/search/` | Yes | Search interface directory |
| `/search`, `/browse`, `/file`, `/status`, `/repos`, `/list`, `/thumbnail` | Yes | API endpoints (proxied to backend) |
| Static assets (CSS, JS, images) | No | Supporting files |

HTTP (port 80) requests automatically redirect to HTTPS (port 443).

### Logs

```sh
# Access logs
sudo tail -f /var/log/nginx/access.log

# Error logs
sudo tail -f /var/log/nginx/error.log
```

### Managing Authentication

```sh
# Add/update user password
sudo htpasswd /etc/nginx/.htpasswd username

# Remove user
sudo htpasswd -D /etc/nginx/.htpasswd username

# View users (hashed)
sudo cat /etc/nginx/.htpasswd
```

After changing passwords, no nginx reload is needed - changes take effect
immediately.

## Paperman-Server Backend

### Service Management

```sh
# Check status
sudo systemctl status paperman-server

# Start/Stop/Restart
sudo systemctl start paperman-server
sudo systemctl stop paperman-server
sudo systemctl restart paperman-server

# Enable/disable on boot
sudo systemctl enable paperman-server
sudo systemctl disable paperman-server
```

### Service Details

- **Binary:** `/opt/paperman/paperman-server`
- **Working Directory:** `/opt/paperman`
- **Document Path:** `/srv/papers`
- **Port:** 8080 (localhost only, not exposed externally)
- **API Key:** Set via environment variable `PAPERMAN_API_KEY`
- **Service File:** `/etc/systemd/system/paperman-server.service`
- **User/Group:** paperman

### API Endpoints

The backend provides these endpoints (all proxied through nginx):

- `GET /search` - Search documents
- `GET /browse` - Browse document tree
- `GET /file` - Retrieve files
- `GET /status` - Health check
- `GET /repos` - Repository information
- `GET /list` - List documents
- `GET /thumbnail` - Get document thumbnails

### Logs

```sh
# View recent logs
sudo journalctl -u paperman-server -n 50

# Follow logs in real-time
sudo journalctl -u paperman-server -f

# View logs since boot
sudo journalctl -u paperman-server -b

# View logs for specific time period
sudo journalctl -u paperman-server --since "2025-11-09 10:00:00"
```

### Troubleshooting

Check if backend is running:

```sh
sudo lsof -i :8080
```

Test direct backend connection (bypassing nginx):

```sh
curl -H "X-API-Key: YOUR_KEY" http://localhost:8080/status
```

## Common Operations

### Temporarily Disable Document API

**Option 1:** Stop backend service (API calls will fail, static pages
still work):

```sh
sudo systemctl stop paperman-server
sudo systemctl disable paperman-server
```

**Option 2:** Comment out in nginx config (requires reload). Edit
`/etc/nginx/sites-available/paperman-server` and comment out API location
blocks, then:

```sh
sudo nginx -t
sudo systemctl reload nginx
```

### Re-enable Document API

```sh
sudo systemctl enable paperman-server
sudo systemctl start paperman-server
```

### Update SSL Certificates

Let's Encrypt certificates auto-renew via certbot. To manually renew:

```sh
sudo certbot renew
sudo systemctl reload nginx
```

### Backup Configuration Files

```sh
# Backup nginx config
sudo cp /etc/nginx/sites-available/paperman-server /etc/nginx/sites-available/paperman-server.backup

# Backup password file
sudo cp /etc/nginx/.htpasswd /etc/nginx/.htpasswd.backup

# Backup systemd service
sudo cp /etc/systemd/system/paperman-server.service /etc/systemd/system/paperman-server.service.backup
```

### Restore from Backup

```sh
# Restore nginx config
sudo cp /etc/nginx/sites-available/paperman-server.backup /etc/nginx/sites-available/paperman-server
sudo nginx -t
sudo systemctl reload nginx

# Restore service file
sudo cp /etc/systemd/system/paperman-server.service.backup /etc/systemd/system/paperman-server.service
sudo systemctl daemon-reload
sudo systemctl restart paperman-server
```

## Security Notes

1. **API Key Security:** The API key is stored in the systemd service file
   and nginx config. These files are readable only by root.
2. **Password File:** `/etc/nginx/.htpasswd` uses bcrypt hashing and is
   readable only by root.
3. **Backend Binding:** paperman-server binds to `[::1]:8080` (IPv6
   localhost), not externally accessible.
4. **SSL/TLS:** Uses TLS 1.2 and 1.3 with strong cipher suites.
5. **Service Hardening:** paperman-server runs with `NoNewPrivileges=true`
   and `PrivateTmp=yes`.

## Static Files Location

Static web files are served from `/var/www/paperman/`:

```
/var/www/paperman/
  index.html          Main landing page (no auth)
  browse.html         Browse interface (requires auth)
  search.html         Search page (requires auth)
  search/
    index.html        Search interface directory (requires auth)
```

To update static files:

```sh
# Files should be owned by www-data
sudo chown www-data:www-data /var/www/paperman/*.html
sudo chmod 644 /var/www/paperman/*.html
```

## Quick Reference

### Check Everything is Running

```sh
sudo systemctl status nginx
sudo systemctl status paperman-server
sudo lsof -i :443
sudo lsof -i :8080
```

### View All Logs

```sh
# Nginx
sudo tail -f /var/log/nginx/access.log /var/log/nginx/error.log

# Paperman backend
sudo journalctl -u paperman-server -f
```

### Test Configuration

```sh
# Test nginx config
sudo nginx -t

# Test HTTPS endpoint
curl -I https://your-server.example.com/

# Test authenticated endpoint
curl -u username:password https://your-server.example.com/browse.html
```
