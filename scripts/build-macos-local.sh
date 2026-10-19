#!/usr/bin/env bash
# Build this checkout and install it without changing the user's configuration.
set -euo pipefail
SOURCE_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
CACHE_DIR="${SIOYEK_BUILD_CACHE:-$HOME/Library/Caches/sioyek-build}"
QT_VERSION=6.8.1
mkdir -p "$CACHE_DIR"
cd "$SOURCE_DIR"

for dependency in freeglut mesa harfbuzz; do
    if [ ! -d "$(brew --prefix)/opt/$dependency" ]; then
        brew install --quiet "$dependency"
    fi
done

git submodule update --init --recursive --depth 1 -j8

if [ -n "${SIOYEK_QT_DIR:-}" ]; then
    QT_DIR="$SIOYEK_QT_DIR"
else
    QT_DIR="$CACHE_DIR/$QT_VERSION/macos"
    if [ ! -f "$CACHE_DIR/qt-$QT_VERSION.ready" ] || [ ! -x "$QT_DIR/bin/qmake" ]; then
        python3 -m venv "$CACHE_DIR/venv"
        "$CACHE_DIR/venv/bin/pip" install aqtinstall
        "$CACHE_DIR/venv/bin/aqt" install-qt mac desktop "$QT_VERSION" clang_64 -m all -O "$CACHE_DIR"
        touch "$CACHE_DIR/qt-$QT_VERSION.ready"
    fi
fi
if [ ! -x "$QT_DIR/bin/qmake" ]; then
    echo "Qt qmake was not found in $QT_DIR" >&2
    exit 1
fi

# Qt 6.8.1 references AGL, which current Apple SDKs no longer provide.
# This only repairs downloaded SDK metadata; application source stays unchanged.
find "$QT_DIR" -type f \( -name '*.prl' -o -name '*.pri' -o -name '*.conf' -o -name '*.pc' \) \
    -exec sed -Ei '' 's/-framework AGL//g' {} +

export Qt6_DIR="$QT_DIR"
export QT_PLUGIN_PATH="$QT_DIR/plugins"
export PKG_CONFIG_PATH="$QT_DIR/lib/pkgconfig"
export QML2_IMPORT_PATH="$QT_DIR/qml"
export PATH="$QT_DIR/bin:$PATH"
export MACOSX_DEPLOYMENT_TARGET=14
export MAKE_PARALLEL="${MAKE_PARALLEL:-$(sysctl -n hw.ncpu)}"
bash ./build_mac.sh
codesign --verify --deep --strict build/sioyek.app

INSTALL_DIR=/Applications
BIN_DIR=/opt/homebrew/bin
[ -w "$INSTALL_DIR" ] || INSTALL_DIR="$HOME/Applications"
[ -w "$BIN_DIR" ] || BIN_DIR="$HOME/.local/bin"
mkdir -p "$INSTALL_DIR" "$BIN_DIR"
if [ -e "$INSTALL_DIR/sioyek.app" ]; then
    BACKUP="$INSTALL_DIR/sioyek-backup-$(date +%Y%m%d-%H%M%S).app"
    echo "Preserving previous app at $BACKUP"
    mv "$INSTALL_DIR/sioyek.app" "$BACKUP"
fi
ditto build/sioyek.app "$INSTALL_DIR/sioyek.app"
ln -sf "$INSTALL_DIR/sioyek.app/Contents/MacOS/sioyek" "$BIN_DIR/sioyek"
echo "Installed: $INSTALL_DIR/sioyek.app"
echo "Quit Sioyek and reopen that app to use the new build."
