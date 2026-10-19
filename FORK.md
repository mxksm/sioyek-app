# Personal Sioyek fork

This fork contains the source changes previously applied by scripts in
[mxksm/sioyek](https://github.com/mxksm/sioyek): live status-bar theme refresh,
chronological drawing undo across pages, drawing redo, a mouse brush eraser,
and a theme-matched macOS title bar.

The drawing eraser cuts visible strokes along the cursor path, retaining the
uncovered pieces with their original color, opacity and interpolated width. Its radius
is 12 screen pixels. Erasing clears drawing redo history and is not undoable.
`toggle_brush_eraser` enters/exits erasing; Escape exits and drawing mode switches
back to the pen. Text annotations and PDF content are unaffected.

The macOS title bar uses the PDF background (normal, dark, or custom), with a
subtle one-point bottom separator, centered numbered PDF tabs, and no traffic-light
buttons. Its height matches the retro WezTerm tab row configured here: 16pt
JetBrains Mono at default line height, rounded to backing pixels (22pt at 1x,
21.5pt at 2x). Dragging and double-clicking the bar still use native macOS behavior. Theme
changes apply immediately, and `toggle_titlebar` (`st` in the config repo) still
hides/shows it. In this fork the main window follows the PDF instead of
`macos_titlebar_color`.

Tabs show `1: filename` without the PDF extension, with a semibold label on the
active tab and the full path in a tooltip. Click to switch; scroll the row when
it overflows. `new_tab` (Cmd+T) opens a file picker; cancellation changes nothing,
and choosing an already open PDF selects its existing tab. `close_tab` (Cmd+W)
closes the current tab and selects its neighbor, or closes the window for the
last tab. `goto_tab_1` through `goto_tab_9` use Cmd+1 through Cmd+9. Ctrl+T still
opens a separate window. Closing tabs preserves the PDF and its annotations.
Each window owns its tab order and reading positions/zoom, so the same PDF can
be read independently in two windows. Session files retain tab order and active
selection. Legacy sessions still load. New tabs reveal the bar if `st` hid it.
The reduced native height uses AppKit's guarded `setCustomTitlebarHeight:` frame
hook; if a future macOS removes it, the bar retains the native height. AppKit
continues to own content layout and Qt frame margins, so document coordinates
are not shifted manually.

On macOS, the Qt/OpenGL surfaces and native title bar explicitly use sRGB,
matching [WezTerm's color-space setup](https://github.com/wezterm/wezterm/blob/main/window/src/os/macos/window.rs).
This makes equal hex theme colors consistent across the apps, instead of
interpreting Sioyek's colors in the monitor's default profile. The fix also applies
when the title bar is hidden. Screenshot RGB values can still differ from the
configured hex when a capture is stored in a different color profile; compare
both apps in the same screenshot/profile. No theme-specific hex offsets are used.

## Build on macOS

For normal use, pull the config repository and run its `build-sioyek.sh`.
It reads `sioyek-app-revision`, fetches that exact commit into
`~/Library/Caches/sioyek-build/source`, and invokes this fork's build script.
Qt 6.8.1 is cached in `~/Library/Caches/sioyek-build`. No token is needed to
clone this public fork. Existing user configs are not changed during installation.

To work directly on the source:

```sh
git clone --branch development https://github.com/mxksm/sioyek-app.git
cd sioyek-app
bash scripts/build-macos-local.sh
```

The script initializes submodules, installs missing Homebrew dependencies,
downloads Qt on the first build, compiles, verifies the signature, and installs
the app. It requires macOS 14 or newer, Xcode command-line tools, Homebrew,
and Python 3. The installation goes to `/Applications` when writable, otherwise
`~/Applications`; a timestamped backup of the previous app is kept. Quit Sioyek
and reopen the installed app to load the new binary. When installation falls
back to `~/Applications/sioyek.app`, an older `/Applications/sioyek.app` is left
in place. Open the exact path printed by the installer (for example,
`open "$HOME/Applications/sioyek.app"`); launching by app name can select the
older system-wide copy.

Build products and source stay available for incremental builds.
`SIOYEK_BUILD_CACHE` overrides the cache directory; `SIOYEK_QT_DIR` may point to
an existing Qt 6.8.1 `macos` directory. The Qt metadata workaround removes obsolete
AGL framework references; no application source is rewritten by the build.

## Checks

```sh
python3 tests/test-drawing-redo.py .
python3 tests/test-brush-eraser.py .
clang++ -std=c++17 tests/test-window-tabs.cpp -o /tmp/sioyek-window-tabs-test
/tmp/sioyek-window-tabs-test
python3 tests/test-tab-session.py
python3 tests/test-tab-picker.py
SIOYEK_QT_DIR=/path/to/Qt/6.8.1/macos bash tests/test-macos-titlebar.sh
```

These compile the actual drawing methods in small fixtures; they test drawing
order, restoration, eraser geometry, hidden colors, and history invalidation.
A full macOS build validates integration with Qt and the rest of Sioyek.

## Publishing updates

Commit and push source changes to this fork's `development` branch. After
validation, put that commit's full hash into `sioyek-app-revision` in the config
repo and commit/push it with any associated key bindings. This keeps other
laptops on a tested revision until the config repo explicitly selects a new one.

Upstream updates are deliberate merges, not part of routine builds:

```sh
git remote add upstream https://github.com/ahrm/sioyek.git # once
git fetch upstream
git merge upstream/development
```

Resolve conflicts, build and test before publishing the updated revision.

The macOS native test checks title centering after resize, document-name updates,
hidden controls, explicit sRGB colors, numbered Unicode tab labels, click callbacks,
overflow scrolling, theme refresh, and repeated title-bar hide/show.
