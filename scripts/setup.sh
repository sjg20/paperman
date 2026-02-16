#!/bin/bash
# Install build dependencies for paperman
#
# Usage:
#   scripts/setup.sh          Install everything (Qt5, Flutter, Android SDK)
#   scripts/setup.sh --qt     Install Qt5/C++ dependencies only
#
# This is also available as 'make setup'.

set -e

FLUTTER_VERSION=3.41.1
ANDROID_CMDLINE_TOOLS_VERSION=13114758

# Paths — override via environment if desired
FLUTTER_DIR="${FLUTTER_DIR:-$HOME/flutter}"
ANDROID_HOME="${ANDROID_HOME:-$HOME/android-sdk}"
JAVA_HOME="${JAVA_HOME:-/usr/lib/jvm/java-21-openjdk-amd64}"

qt_only=false
if [ "$1" = "--qt" ]; then
    qt_only=true
fi

# --- Debian packages ---

echo "Installing Debian packages..."
sudo apt-get update -qq

# Qt5/C++ build deps (desktop app + server)
sudo apt-get install -y \
    build-essential \
    qt5-qmake qtbase5-dev qtbase5-dev-tools libqt5sql5-sqlite \
    libpoppler-qt5-dev libpodofo-dev \
    libtiff-dev libsane-dev libjpeg-dev zlib1g-dev \
    imagemagick tesseract-ocr tesseract-ocr-eng \
    python3-reportlab python3-pil python3-numpy \
    python3-sphinx python3-sphinx-rtd-theme \
    poppler-utils

if $qt_only; then
    echo "Done (Qt5 only)."
    exit 0
fi

# Flutter Linux desktop build deps
sudo apt-get install -y \
    clang cmake ninja-build pkg-config unzip lld \
    libgtk-3-dev liblzma-dev libstdc++-12-dev

# Java (needed by Android/Gradle)
sudo apt-get install -y openjdk-21-jdk-headless

# --- Flutter SDK ---

if [ -x "$FLUTTER_DIR/bin/flutter" ]; then
    echo "Flutter already installed at $FLUTTER_DIR"
else
    echo "Installing Flutter $FLUTTER_VERSION..."
    curl -fSL -o /tmp/flutter.tar.xz \
        "https://storage.googleapis.com/flutter_infra_release/releases/stable/linux/flutter_linux_${FLUTTER_VERSION}-stable.tar.xz"
    mkdir -p "$(dirname "$FLUTTER_DIR")"
    tar xf /tmp/flutter.tar.xz -C "$(dirname "$FLUTTER_DIR")"
    rm /tmp/flutter.tar.xz
fi

export PATH="$FLUTTER_DIR/bin:$PATH"
export JAVA_HOME
export ANDROID_HOME

# --- Android SDK ---

if [ -d "$ANDROID_HOME/cmdline-tools/latest" ]; then
    echo "Android SDK already installed at $ANDROID_HOME"
else
    echo "Installing Android command-line tools..."
    curl -fSL -o /tmp/cmdline-tools.zip \
        "https://dl.google.com/android/repository/commandlinetools-linux-${ANDROID_CMDLINE_TOOLS_VERSION}_latest.zip"
    mkdir -p "$ANDROID_HOME/cmdline-tools"
    unzip -q -o /tmp/cmdline-tools.zip -d "$ANDROID_HOME/cmdline-tools"
    mv "$ANDROID_HOME/cmdline-tools/cmdline-tools" "$ANDROID_HOME/cmdline-tools/latest"
    rm /tmp/cmdline-tools.zip
fi

export PATH="$ANDROID_HOME/cmdline-tools/latest/bin:$PATH"

SDKMANAGER="$ANDROID_HOME/cmdline-tools/latest/bin/sdkmanager"

echo "Accepting Android SDK licences..."
printf 'y\ny\ny\ny\ny\ny\ny\ny\n' | "$SDKMANAGER" --licenses > /dev/null 2>&1 || true

echo "Installing Android SDK components..."
"$SDKMANAGER" --install \
    "platform-tools" \
    "platforms;android-35" \
    "build-tools;35.0.0"

# --- Verify ---

echo ""
echo "Verifying setup..."
flutter --version
java -version 2>&1 | head -1
echo ""

# Remind user about PATH if needed
if ! command -v flutter &>/dev/null; then
    echo "Add to your shell profile:"
    echo "  export FLUTTER_DIR=$FLUTTER_DIR"
    echo "  export ANDROID_HOME=$ANDROID_HOME"
    echo "  export JAVA_HOME=$JAVA_HOME"
    echo '  export PATH="$FLUTTER_DIR/bin:$ANDROID_HOME/cmdline-tools/latest/bin:$PATH"'
fi

echo "Done."
