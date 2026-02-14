"""
Load paperman server configuration from config file.

Searches (in order):
  1. ~/.config/paperman/config
  2. ~/.paperman.conf

Config file format (INI):
  [server]
  url = https://your-server.example.com
  api_key = your-key-here
"""

import os
import configparser

DEFAULT_SERVER = "https://your-server.example.com"
DEFAULT_API_KEY = ""

CONFIG_PATHS = [
    os.path.expanduser("~/.config/paperman/config"),
    os.path.expanduser("~/.paperman.conf"),
]


def load_config():
    """Return (server_url, api_key) from the first config file found."""
    config = configparser.ConfigParser()

    for path in CONFIG_PATHS:
        if os.path.isfile(path):
            config.read(path)
            break

    server = config.get("server", "url", fallback=DEFAULT_SERVER)
    api_key = config.get("server", "api_key", fallback=DEFAULT_API_KEY)
    return server, api_key
