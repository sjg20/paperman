Paperman
========

Paperman is a viewer for image files such as PDF, JPEG and its own variant of
PaperPort's .max file format. It also includes a search server with a REST API
for querying paper repositories.

.. image:: 1.jpeg
   :width: 30%

.. image:: all.png
   :width: 30%

Features
--------

- Simple GUI based around stacks and pages
- View previews and browse through pages
- Move pages in and out of stacks
- Navigate through directories
- Move stacks between directories
- Print stacks and pages, including page annotations
- Full undo/redo
- Scanning support with presets
- PDF, JPEG and TIFF conversion
- OCR engine with full-text search
- Search server with REST API

.. toctree::
   :maxdepth: 2
   :caption: User Guide

   cli
   server
   api
   app

.. toctree::
   :maxdepth: 2
   :caption: Deployment

   deployment
   deployment-guide
   systemd
   apache-to-nginx
   services-guide

.. toctree::
   :maxdepth: 2
   :caption: Development

   claude-setup-nginx
   install-claude
