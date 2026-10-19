#!/bin/bash
set -euo pipefail
source_dir="$(cd "$(dirname "$0")/.." && pwd)"
qt_dir="${SIOYEK_QT_DIR:?Set SIOYEK_QT_DIR to a Qt 6 macos directory}"
test_binary="$(mktemp -t sioyek-titlebar-test)"
trap 'rm -f "$test_binary"' EXIT
clang++ -std=c++17 -fPIC -F "$qt_dir/lib" \
    -I "$qt_dir/lib/QtWidgets.framework/Headers" \
    -I "$qt_dir/lib/QtGui.framework/Headers" \
    -I "$qt_dir/lib/QtCore.framework/Headers" \
    -framework Cocoa -framework QuartzCore -framework QtWidgets -framework QtGui -framework QtCore \
    -Wl,-rpath,"$qt_dir/lib" "$source_dir/tests/test-macos-titlebar.mm" -o "$test_binary"
"$test_binary"
