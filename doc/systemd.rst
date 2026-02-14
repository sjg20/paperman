Paperman Server Systemd Service
===============================

This directory contains a systemd service unit for running
paperman-server as a system service.

Files
-----

-  ``paperman-server.service`` - Systemd service unit file
-  ``install-service.sh`` - Installation script
-  ``SYSTEMD-README.md`` - This file

Installation
------------

1. **Edit the service file** to configure your repository path:

   .. code:: bash

      nano paperman-server.service

   Change this line to point to your papers directory:

   ::

      ExecStart=/opt/paperman/paperman-server /srv/papers

2. **Install the service** (requires root):

   .. code:: bash

      sudo ./install-service.sh

Usage
-----

Start the service
~~~~~~~~~~~~~~~~~

.. code:: bash

   sudo systemctl start paperman-server

Stop the service
~~~~~~~~~~~~~~~~

.. code:: bash

   sudo systemctl stop paperman-server

Check status
~~~~~~~~~~~~

.. code:: bash

   sudo systemctl status paperman-server

Enable autostart on boot
~~~~~~~~~~~~~~~~~~~~~~~~

.. code:: bash

   sudo systemctl enable paperman-server

Disable autostart
~~~~~~~~~~~~~~~~~

.. code:: bash

   sudo systemctl disable paperman-server

View logs
~~~~~~~~~

.. code:: bash

   # Follow logs in real-time
   sudo journalctl -u paperman-server -f

   # View last 50 lines
   sudo journalctl -u paperman-server -n 50

   # View logs since boot
   sudo journalctl -u paperman-server -b

Restart after configuration changes
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code:: bash

   sudo systemctl restart paperman-server

Configuration
-------------

The service runs with these settings:

-  **User**: paperman
-  **Group**: paperman
-  **Working Directory**: /opt/paperman
-  **Default Port**: 8080
-  **Repository**: /srv/papers (change in ExecStart line)
-  **Auto-restart**: On failure, with 5 second delay
-  **Private temp**: Yes (isolated /tmp)

Modifying the Service
---------------------

If you need to change the configuration:

1. Edit the service file:

   .. code:: bash

      sudo nano /etc/systemd/system/paperman-server.service

2. Reload systemd:

   .. code:: bash

      sudo systemctl daemon-reload

3. Restart the service:

   .. code:: bash

      sudo systemctl restart paperman-server

Advanced Options
----------------

Change port
~~~~~~~~~~~

Edit the service file and modify ExecStart:

::

   ExecStart=/opt/paperman/paperman-server -p 9000 /srv/papers

Multiple repositories
~~~~~~~~~~~~~~~~~~~~~

::

   ExecStart=/opt/paperman/paperman-server /srv/papers /home/paperman/archive

Troubleshooting
---------------

Service won’t start
~~~~~~~~~~~~~~~~~~~

.. code:: bash

   # Check detailed status
   sudo systemctl status paperman-server

   # Check logs for errors
   sudo journalctl -u paperman-server -n 100

Permission issues
~~~~~~~~~~~~~~~~~

-  Ensure the ``paperman-server`` binary is executable:
   ``chmod +x paperman-server``
-  Ensure the ``paperman`` binary is in the same directory
-  Verify the repository path exists and is readable by the service user

Port already in use
~~~~~~~~~~~~~~~~~~~

If port 8080 is already in use, add ``-p <port>`` to ExecStart to use a
different port.

Uninstallation
--------------

.. code:: bash

   # Stop and disable the service
   sudo systemctl stop paperman-server
   sudo systemctl disable paperman-server

   # Remove the service file
   sudo rm /etc/systemd/system/paperman-server.service

   # Reload systemd
   sudo systemctl daemon-reload

Security Notes
--------------

The service runs with: - ``NoNewPrivileges=true`` - Prevents privilege
escalation - ``PrivateTmp=yes`` - Isolated temporary directory - Runs as
non-root user (paperman)

For additional security, consider: - Using a dedicated service account -
Adding firewall rules to restrict access - Enabling HTTPS with a reverse
proxy (nginx/apache)
