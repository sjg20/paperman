Migrating from Apache to Nginx
==============================

This guide will help you safely switch from Apache to nginx for the
paperman server.

Pre-Migration Checklist
-----------------------

Before starting, gather this information:

.. code:: bash

   # Check what Apache is serving
   sudo apache2ctl -S

   # Check if anything else uses Apache
   sudo netstat -tlnp | grep apache

   # Backup Apache config
   sudo cp -r /etc/apache2 /etc/apache2.backup.$(date +%Y%m%d)

   # List enabled Apache sites
   ls -la /etc/apache2/sites-enabled/

Step-by-Step Migration
----------------------

Step 1: Install Nginx
~~~~~~~~~~~~~~~~~~~~~

.. code:: bash

   # On your server
   sudo apt update
   sudo apt install nginx

   # Nginx will fail to start if Apache is on port 80/443 - this is expected

Step 2: Stop Apache (Temporary)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code:: bash

   # Stop Apache but don't disable yet (in case we need to roll back)
   sudo systemctl stop apache2

   # Verify it's stopped
   sudo systemctl status apache2

   # Check ports are free
   sudo netstat -tlnp | grep -E ':(80|443)'

Step 3: Install Nginx Configuration
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code:: bash

   # Copy the nginx config file to the server (if not already there)
   # From dev machine:
   # scp nginx-paperman.conf your-server:/tmp/

   # On your server:
   sudo cp /tmp/nginx-paperman.conf /etc/nginx/sites-available/paperman

   # Edit the configuration
   sudo nano /etc/nginx/sites-available/paperman

**Update these lines:**

.. code:: nginx

   server_name your-server.example.com;  # Change to your domain or IP

   # Update SSL certificate paths:
   # If you had Let's Encrypt with Apache, the certs are already there:
   ssl_certificate /etc/letsencrypt/live/your-server.example.com/fullchain.pem;
   ssl_certificate_key /etc/letsencrypt/live/your-server.example.com/privkey.pem;

   # OR for self-signed:
   ssl_certificate /etc/nginx/ssl/paperman.crt;
   ssl_certificate_key /etc/nginx/ssl/paperman.key;

Step 4: SSL Certificates
~~~~~~~~~~~~~~~~~~~~~~~~

**Option A: Reuse Existing Let’s Encrypt Certificates**

If you had Let’s Encrypt working with Apache:

.. code:: bash

   # The certificates in /etc/letsencrypt/ can be used by nginx
   # Just update the paths in nginx config (already done above)

   # Install certbot for nginx (for renewals)
   sudo apt install python3-certbot-nginx

**Option B: Create New Self-Signed Certificate**

.. code:: bash

   # Create nginx SSL directory
   sudo mkdir -p /etc/nginx/ssl

   # Generate self-signed certificate
   sudo openssl req -x509 -nodes -days 365 -newkey rsa:2048 \
       -keyout /etc/nginx/ssl/paperman.key \
       -out /etc/nginx/ssl/paperman.crt \
       -subj "/CN=your-server"

   # Set permissions
   sudo chmod 600 /etc/nginx/ssl/paperman.key
   sudo chmod 644 /etc/nginx/ssl/paperman.crt

**Option C: Get Fresh Let’s Encrypt Certificate**

.. code:: bash

   # With Apache stopped and nginx not yet started
   sudo certbot certonly --standalone -d your-server.example.com

   # This will create certificates in:
   # /etc/letsencrypt/live/your-server.example.com/

Step 5: Configure Nginx
~~~~~~~~~~~~~~~~~~~~~~~

.. code:: bash

   # Remove default nginx site
   sudo rm /etc/nginx/sites-enabled/default

   # Enable paperman site
   sudo ln -s /etc/nginx/sites-available/paperman /etc/nginx/sites-enabled/

   # Test nginx configuration
   sudo nginx -t

   # If you see errors, check the SSL certificate paths and server_name

Expected output:

::

   nginx: the configuration file /etc/nginx/nginx.conf syntax is ok
   nginx: configuration file /etc/nginx/nginx.conf test is successful

Step 6: Start Nginx
~~~~~~~~~~~~~~~~~~~

.. code:: bash

   # Start nginx
   sudo systemctl start nginx

   # Check status
   sudo systemctl status nginx

   # Check it's listening on 80 and 443
   sudo netstat -tlnp | grep nginx

Expected output:

::

   tcp  0  0  0.0.0.0:80     0.0.0.0:*  LISTEN  1234/nginx
   tcp  0  0  0.0.0.0:443    0.0.0.0:*  LISTEN  1234/nginx

Step 7: Test the Setup
~~~~~~~~~~~~~~~~~~~~~~

.. code:: bash

   # From another machine (or the server itself)
   # Test HTTP redirect to HTTPS
   curl -I http://your-server.example.com
   # Should see: HTTP/1.1 301 Moved Permanently

   # Test HTTPS (status endpoint - no auth needed)
   curl -k https://your-server.example.com/status
   # Should see: {"status":"running",...}

   # Test with API key
   curl -k -H "X-API-Key: YOUR-KEY" https://your-server.example.com/search?q=test

   # Test PDF conversion
   curl -k -H "X-API-Key: YOUR-KEY" \
       "https://your-server.example.com/file?path=test.jpg&type=pdf" \
       -o test.pdf

Step 8: Disable Apache (Permanent)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

**Only do this after confirming nginx works!**

.. code:: bash

   # Disable Apache from starting on boot
   sudo systemctl disable apache2

   # Optionally, remove Apache (if nothing else needs it)
   # sudo apt remove apache2
   # OR just leave it installed but disabled

Step 9: Enable Nginx on Boot
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code:: bash

   # Enable nginx to start on boot
   sudo systemctl enable nginx

   # Verify it's enabled
   sudo systemctl is-enabled nginx

Step 10: Update Certbot (if using Let’s Encrypt)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code:: bash

   # If using Let's Encrypt, update renewal to use nginx
   sudo certbot renew --dry-run --nginx

   # Edit renewal config if needed
   sudo nano /etc/letsencrypt/renewal/your-server.example.com.conf

Change:

::

   authenticator = webroot

to:

::

   authenticator = nginx
   installer = nginx

--------------

Rollback Plan (If Something Goes Wrong)
---------------------------------------

If nginx doesn’t work and you need to go back to Apache:

.. code:: bash

   # Stop nginx
   sudo systemctl stop nginx
   sudo systemctl disable nginx

   # Start Apache again
   sudo systemctl start apache2
   sudo systemctl enable apache2

   # Your Apache config is still in /etc/apache2/
   # Your backup is in /etc/apache2.backup.YYYYMMDD/

--------------

Verification Checklist
----------------------

After migration, verify:

-  ☐ Nginx is running: ``sudo systemctl status nginx``
-  ☐ Ports 80 and 443 are listening: ``sudo netstat -tlnp | grep nginx``
-  ☐ HTTP redirects to HTTPS: ``curl -I http://your-server/``
-  ☐ HTTPS works: ``curl -k https://your-server/status``
-  ☐ API key authentication works
-  ☐ PDF conversion works
-  ☐ SSL certificate is valid (or self-signed warning expected)
-  ☐ Paperman-server is running:
   ``sudo systemctl status paperman-server``
-  ☐ Logs are accessible:
   ``sudo tail -f /var/log/nginx/paperman-access.log``
-  ☐ Nginx starts on boot: ``sudo systemctl is-enabled nginx``
-  ☐ Apache is disabled: ``sudo systemctl is-enabled apache2`` (should
   say “disabled”)

--------------

Nginx-Specific Commands
-----------------------

Useful Nginx Commands
~~~~~~~~~~~~~~~~~~~~~

.. code:: bash

   # Test configuration
   sudo nginx -t

   # Reload configuration (no downtime)
   sudo systemctl reload nginx

   # Restart nginx
   sudo systemctl restart nginx

   # View configuration
   sudo nginx -T

   # Check what sites are enabled
   ls -la /etc/nginx/sites-enabled/

   # View logs
   sudo tail -f /var/log/nginx/access.log
   sudo tail -f /var/log/nginx/error.log
   sudo tail -f /var/log/nginx/paperman-access.log
   sudo tail -f /var/log/nginx/paperman-error.log

   # Check nginx version
   nginx -v

Configuration Changes
~~~~~~~~~~~~~~~~~~~~~

After editing ``/etc/nginx/sites-available/paperman``:

.. code:: bash

   # Always test first
   sudo nginx -t

   # If OK, reload (no downtime)
   sudo systemctl reload nginx

--------------

Performance Tuning for Nginx
----------------------------

If you want to optimize nginx for paperman:

.. code:: bash

   sudo nano /etc/nginx/nginx.conf

Add/modify in the ``http`` block:

.. code:: nginx

   http {
       # ... existing config ...

       # Worker processes (set to number of CPU cores)
       worker_processes auto;

       # Connection limits
       worker_connections 1024;

       # File upload size (for future features)
       client_max_body_size 100M;

       # Compression
       gzip on;
       gzip_vary on;
       gzip_types text/plain text/css application/json application/javascript text/xml application/xml;

       # Buffer sizes for proxying
       proxy_buffer_size 128k;
       proxy_buffers 4 256k;
       proxy_busy_buffers_size 256k;

       # Keep-alive
       keepalive_timeout 65;
   }

--------------

Firewall Configuration
----------------------

.. code:: bash

   # If using ufw
   sudo ufw allow 'Nginx Full'  # Allows both 80 and 443
   sudo ufw delete allow 'Apache Full'  # Remove Apache rule

   # Or specifically:
   sudo ufw allow 443/tcp
   sudo ufw allow 80/tcp

   # Check status
   sudo ufw status

--------------

Monitoring
----------

Set up log rotation (automatic with nginx)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Nginx automatically rotates logs. Check configuration:

.. code:: bash

   cat /etc/logrotate.d/nginx

Monitor in real-time
~~~~~~~~~~~~~~~~~~~~

.. code:: bash

   # Watch access logs
   sudo tail -f /var/log/nginx/paperman-access.log

   # Watch error logs
   sudo tail -f /var/log/nginx/paperman-error.log

   # Watch both
   sudo tail -f /var/log/nginx/paperman-*.log

Check error log for issues
~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code:: bash

   # Recent errors
   sudo tail -100 /var/log/nginx/error.log

   # Search for specific errors
   sudo grep -i error /var/log/nginx/paperman-error.log

--------------

Troubleshooting
---------------

“Address already in use”
~~~~~~~~~~~~~~~~~~~~~~~~

.. code:: bash

   # Check what's using port 80/443
   sudo netstat -tlnp | grep -E ':(80|443)'

   # If Apache is still running
   sudo systemctl stop apache2

“SSL certificate not found”
~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code:: bash

   # Check certificate files exist
   sudo ls -la /etc/letsencrypt/live/your-server.example.com/
   # OR
   sudo ls -la /etc/nginx/ssl/

   # Check permissions
   sudo chmod 600 /etc/nginx/ssl/paperman.key
   sudo chmod 644 /etc/nginx/ssl/paperman.crt

“502 Bad Gateway”
~~~~~~~~~~~~~~~~~

.. code:: bash

   # Paperman-server not running
   sudo systemctl status paperman-server
   sudo systemctl start paperman-server

   # Check if listening on 8080
   sudo netstat -tlnp | grep 8080

   # Check paperman logs
   sudo journalctl -u paperman-server -n 50

Configuration test fails
~~~~~~~~~~~~~~~~~~~~~~~~

.. code:: bash

   # Get detailed error
   sudo nginx -t

   # Common issues:
   # - Missing semicolon
   # - Wrong file path
   # - Duplicate server_name
   # - Invalid directive

Let’s Encrypt renewal fails
~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code:: bash

   # Test renewal
   sudo certbot renew --dry-run

   # If fails, check nginx is running
   sudo systemctl status nginx

   # Check port 80 is accessible (needed for renewal)
   curl -I http://your-server.example.com

--------------

Differences from Apache
-----------------------

Configuration Location
~~~~~~~~~~~~~~~~~~~~~~

-  **Apache**: ``/etc/apache2/sites-available/``
-  **Nginx**: ``/etc/nginx/sites-available/``

Reload Command
~~~~~~~~~~~~~~

-  **Apache**: ``sudo systemctl reload apache2``
-  **Nginx**: ``sudo systemctl reload nginx`` or
   ``sudo nginx -s reload``

Test Configuration
~~~~~~~~~~~~~~~~~~

-  **Apache**: ``sudo apache2ctl configtest``
-  **Nginx**: ``sudo nginx -t``

Logs
~~~~

-  **Apache**: ``/var/log/apache2/``
-  **Nginx**: ``/var/log/nginx/``

Modules
~~~~~~~

-  **Apache**: Enable with ``a2enmod``, ``a2dismod``
-  **Nginx**: Built-in or compile-time modules

--------------

Why Nginx is Better for Paperman
--------------------------------

1. **Better performance** - Lower memory footprint
2. **Faster static file serving** - Important for file downloads
3. **Better for reverse proxy** - Designed for it
4. **Simpler configuration** - Less complex than Apache
5. **Better for concurrent connections** - Event-driven architecture
6. **Lower resource usage** - Important if the server runs other
   services

--------------

Migration Complete!
-------------------

Once everything is working:

1. ✅ Nginx is running and serving HTTPS
2. ✅ Paperman API is accessible with API key
3. ✅ PDF conversion works
4. ✅ Apache is disabled
5. ✅ Auto-start on boot configured
6. ✅ Logs are being written

You’ve successfully migrated from Apache to nginx! 🎉
