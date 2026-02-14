Installing Claude Code CLI on Your Server
=========================================

This guide will help you install and set up Claude Code (the AI coding
assistant CLI) on your server.

Prerequisites
-------------

-  Linux machine with internet access
-  Anthropic API key (get from https://console.anthropic.com/)

Installation Methods
--------------------

Method 1: Using npm (Recommended)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

This is the easiest method if you have Node.js installed.

Step 1: Install Node.js (if not already installed)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. code:: bash

   # Check if Node.js is installed
   node --version
   npm --version

   # If not installed, install Node.js 18+ and npm
   curl -fsSL https://deb.nodesource.com/setup_20.x | sudo -E bash -
   sudo apt-get install -y nodejs

   # Verify installation
   node --version  # Should be v20.x or higher
   npm --version

Step 2: Install Claude Code CLI
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. code:: bash

   # Install globally with npm
   sudo npm install -g @anthropic-ai/claude-code

   # Verify installation
   claude --version

Method 2: Download Binary (Alternative)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

If you don’t want to use npm:

.. code:: bash

   # Download the latest Linux binary
   cd /tmp
   curl -L https://github.com/anthropics/claude-code/releases/latest/download/claude-linux-x64 -o claude

   # Make executable
   chmod +x claude

   # Move to system path
   sudo mv claude /usr/local/bin/

   # Verify installation
   claude --version

Method 3: Build from Source (For Latest Features)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code:: bash

   # Install git if not present
   sudo apt install git

   # Clone the repository
   git clone https://github.com/anthropics/claude-code.git
   cd claude-code

   # Install dependencies
   npm install

   # Build
   npm run build

   # Link globally
   sudo npm link

   # Verify
   claude --version

--------------

Configuration
-------------

Step 1: Get Anthropic API Key
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

1. Go to https://console.anthropic.com/
2. Sign in or create an account
3. Navigate to API Keys
4. Create a new API key
5. Copy the key (starts with ``sk-ant-...``)

Step 2: Configure Claude Code
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code:: bash

   # Set up authentication
   claude auth login

   # When prompted, paste your API key
   # It will be stored securely in ~/.config/claude/

Or set the environment variable:

.. code:: bash

   # Add to ~/.bashrc or ~/.bash_profile
   export ANTHROPIC_API_KEY="sk-ant-your-key-here"

   # Reload shell
   source ~/.bashrc

Step 3: Verify Configuration
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code:: bash

   # Test Claude Code
   claude --help

   # Try a simple command
   claude "what is 2+2"

--------------

Basic Usage
-----------

Interactive Chat Mode
~~~~~~~~~~~~~~~~~~~~~

.. code:: bash

   # Start interactive session
   claude

   # You'll see a prompt where you can chat with Claude
   # Type your questions and press Enter
   # Type 'exit' or Ctrl+D to quit

One-off Commands
~~~~~~~~~~~~~~~~

.. code:: bash

   # Ask a question
   claude "explain how git works"

   # Code generation
   claude "write a python function to reverse a string"

   # File analysis
   claude "analyze this code" < script.py

   # Help with commands
   claude "how do I find large files in linux"

Working with Files
~~~~~~~~~~~~~~~~~~

.. code:: bash

   # Analyze a file
   claude --file mycode.py "explain what this code does"

   # Multiple files
   claude --file file1.js --file file2.js "find bugs in these files"

   # Edit files (creates backup)
   claude --edit mycode.py "add error handling to all functions"

Code Mode (for coding tasks)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code:: bash

   # Start in code mode
   claude code

   # Or use inline
   claude code "create a REST API server in Python"

--------------

Advanced Configuration
----------------------

Custom Configuration File
~~~~~~~~~~~~~~~~~~~~~~~~~

Create ``~/.config/claude/config.json``:

.. code:: json

   {
     "model": "claude-3-5-sonnet-20241022",
     "maxTokens": 4096,
     "temperature": 1.0,
     "defaultProject": "~/projects"
   }

Set Default Model
~~~~~~~~~~~~~~~~~

.. code:: bash

   # Use Opus (most capable)
   claude config set model claude-3-opus-20240229

   # Use Sonnet (balanced)
   claude config set model claude-3-5-sonnet-20241022

   # Use Haiku (fastest/cheapest)
   claude config set model claude-3-haiku-20240307

Enable Features
~~~~~~~~~~~~~~~

.. code:: bash

   # Enable MCP (Model Context Protocol) servers
   claude config set mcp.enabled true

   # Set workspace
   claude config set workspace ~/paperman

   # Enable auto-save
   claude config set autosave true

--------------

Using Claude Code with Paperman Development
-------------------------------------------

Analyze Paperman Code
~~~~~~~~~~~~~~~~~~~~~

.. code:: bash

   # Navigate to paperman directory
   cd ~/paperman

   # Start Claude Code in the paperman directory
   claude code

   # Ask questions about the code
   # "explain how the search server works"
   # "add logging to the PDF conversion function"
   # "review this code for security issues"

Git Integration
~~~~~~~~~~~~~~~

.. code:: bash

   # Review changes before commit
   git diff | claude "review these changes for bugs"

   # Generate commit messages
   git diff --staged | claude "write a conventional commit message for these changes"

   # Code review
   claude --file searchserver.cpp "review this code and suggest improvements"

Debugging Help
~~~~~~~~~~~~~~

.. code:: bash

   # Analyze error logs
   tail -100 /var/log/paperman.log | claude "what's causing this error"

   # Debug compilation errors
   make 2>&1 | claude "explain and fix these compilation errors"

--------------

SSH Usage (Running Claude on the Server from Dev Machine)
---------------------------------------------------------

If you want to use Claude on the server while working from your dev
machine:

Option 1: SSH and Run
~~~~~~~~~~~~~~~~~~~~~

.. code:: bash

   # From dev machine
   ssh your-server
   claude "your question here"

Option 2: SSH with Command
~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code:: bash

   # Run Claude command over SSH
   ssh your-server "claude 'explain how systemd works'"

Option 3: tmux/screen Session
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code:: bash

   # On the server, start a persistent session
   tmux new -s claude
   claude code

   # Detach with Ctrl+B, D
   # Reattach later with:
   tmux attach -t claude

--------------

Troubleshooting
---------------

“command not found: claude”
~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code:: bash

   # Check if installed
   which claude

   # If npm install, check npm global path
   npm config get prefix

   # Add to PATH if needed
   export PATH="$PATH:$(npm config get prefix)/bin"

   # Add to ~/.bashrc to make permanent
   echo 'export PATH="$PATH:'"$(npm config get prefix)/bin"'"' >> ~/.bashrc

“API key not found”
~~~~~~~~~~~~~~~~~~~

.. code:: bash

   # Check environment variable
   echo $ANTHROPIC_API_KEY

   # Or reconfigure
   claude auth login

   # Or set in config
   claude config set apiKey "sk-ant-your-key-here"

“Permission denied”
~~~~~~~~~~~~~~~~~~~

.. code:: bash

   # Fix permissions on config directory
   chmod 700 ~/.config/claude
   chmod 600 ~/.config/claude/*

“Module not found” (if using npm)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code:: bash

   # Reinstall
   sudo npm uninstall -g @anthropic-ai/claude-code
   sudo npm install -g @anthropic-ai/claude-code

   # Clear npm cache if issues persist
   sudo npm cache clean --force

Rate Limits / API Errors
~~~~~~~~~~~~~~~~~~~~~~~~

.. code:: bash

   # Check API usage at console.anthropic.com
   # You may need to add credits to your account

   # Reduce request rate
   claude config set rateLimit 10

   # Use cheaper model
   claude config set model claude-3-haiku-20240307

--------------

Useful Aliases
--------------

Add to ``~/.bashrc`` for convenience:

.. code:: bash

   # Quick Claude commands
   alias ask='claude'
   alias code-review='claude "review this code for bugs, security issues, and best practices"'
   alias explain='claude "explain this in simple terms"'
   alias debug='claude "help me debug this issue"'

   # Git helpers
   alias commit-msg='git diff --staged | claude "write a concise commit message"'
   alias review-pr='git diff main | claude "review these changes as if doing a PR review"'

   # System helpers
   alias logs-analyze='sudo journalctl -n 100 | claude "analyze these logs for errors"'
   alias disk-help='claude "I need help managing disk space on linux"'

Reload:

.. code:: bash

   source ~/.bashrc

--------------

Security Best Practices
-----------------------

Protect Your API Key
~~~~~~~~~~~~~~~~~~~~

.. code:: bash

   # Never commit API key to git
   echo "ANTHROPIC_API_KEY=*" >> .gitignore
   echo ".config/claude/" >> .gitignore

   # Use environment variables
   # Add to ~/.bashrc (not to git repo)
   export ANTHROPIC_API_KEY="sk-ant-..."

   # Restrict permissions on config
   chmod 600 ~/.config/claude/config.json

Sensitive Data
~~~~~~~~~~~~~~

-  Don’t send sensitive credentials or passwords to Claude
-  Be careful with proprietary code
-  Review responses before applying code changes
-  Use ``--dry-run`` flags when available

--------------

Cost Management
---------------

Monitor Usage
~~~~~~~~~~~~~

-  Check usage at https://console.anthropic.com/settings/usage
-  Set up billing alerts
-  Use Haiku model for simple tasks (cheaper)
-  Use Sonnet for balanced tasks
-  Reserve Opus for complex tasks

Cost-Saving Tips
~~~~~~~~~~~~~~~~

.. code:: bash

   # Use shorter context
   claude --max-tokens 1000 "quick question"

   # Use Haiku for simple tasks
   claude --model claude-3-haiku-20240307 "simple question"

   # Avoid large file uploads
   # Instead of uploading entire files, paste relevant snippets

--------------

Updating Claude Code
--------------------

Update via npm
~~~~~~~~~~~~~~

.. code:: bash

   # Update to latest version
   sudo npm update -g @anthropic-ai/claude-code

   # Check current version
   claude --version

   # Check for updates
   npm outdated -g @anthropic-ai/claude-code

Update via binary
~~~~~~~~~~~~~~~~~

.. code:: bash

   # Download latest version
   cd /tmp
   curl -L https://github.com/anthropics/claude-code/releases/latest/download/claude-linux-x64 -o claude
   chmod +x claude
   sudo mv claude /usr/local/bin/claude

   # Verify
   claude --version

--------------

Integration with paperman-server Development
--------------------------------------------

Example Workflow
~~~~~~~~~~~~~~~~

.. code:: bash

   # 1. Start working on paperman
   cd ~/paperman

   # 2. Start Claude Code
   claude code

   # 3. Ask for help
   # > "I want to add rate limiting to the search server"
   # > "Review searchserver.cpp for security issues"
   # > "Help me optimize the PDF conversion performance"

   # 4. Apply changes Claude suggests
   # Claude can directly edit files with your permission

   # 5. Test changes
   make && ./paperman-server test

   # 6. Get help with errors
   # If errors occur, paste them to Claude:
   # > "I'm getting this compilation error: [paste error]"

   # 7. Commit with AI-generated message
   git diff --staged | claude "write a commit message"

--------------

Quick Reference
---------------

Essential Commands
~~~~~~~~~~~~~~~~~~

.. code:: bash

   # Interactive mode
   claude

   # Code mode
   claude code

   # One-off question
   claude "your question"

   # Help
   claude --help

   # Version
   claude --version

   # Configuration
   claude config list
   claude config set key value
   claude config get key

Common Tasks
~~~~~~~~~~~~

.. code:: bash

   # Explain code
   claude --file mycode.cpp "explain this code"

   # Debug error
   claude "explain this error: [paste error]"

   # Generate code
   claude "write a bash script to backup a directory"

   # Review changes
   git diff | claude "review these changes"

   # Documentation
   claude "write documentation for this function" < function.cpp

--------------

Next Steps
----------

After installation:

1. ✅ Test with simple questions
2. ✅ Try analyzing paperman code
3. ✅ Use for debugging
4. ✅ Generate commit messages
5. ✅ Code reviews
6. ✅ Add useful aliases

--------------

Resources
---------

-  Official Docs: https://docs.anthropic.com/claude/docs/intro-to-claude
-  API Reference:
   https://docs.anthropic.com/claude/reference/getting-started-with-the-api
-  Claude Code GitHub: https://github.com/anthropics/claude-code
-  Console: https://console.anthropic.com/
-  Pricing: https://www.anthropic.com/pricing

--------------

Support
-------

If you encounter issues:

1. Check https://github.com/anthropics/claude-code/issues
2. Read the troubleshooting section above
3. Check API status at https://status.anthropic.com/
4. Review usage/billing at console.anthropic.com

You’re all set to use Claude Code on your server! 🎉
