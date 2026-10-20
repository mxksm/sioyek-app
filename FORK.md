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
Brush motion queues a repaint immediately (including other panes showing the
same PDF), rather than waiting for the 200 ms maintenance timer. Distant segments
are rejected cheaply and untouched stroke points are not copied on each move.

Mouse drawing mode shows a filled circle in the current pen color, with a thin
contrasting outline and a centered hotspot. It updates when choosing a color or
changing themes; the eraser retains its hollow circle. The pen cursor is a fixed
size mode indicator rather than a preview of the stroke width.

PDF ink uses fractional pointer and render coordinates, adaptive curve sampling,
and round joins/caps. Gentle display smoothing stays within 0.65 logical pixels
of the recorded path and preserves sharp corners and endpoints. It applies to
both live and saved ink without changing stored annotations, undo/redo or eraser
data. This is independent of `simplify_freehand_drawings`, which remains disabled
in the personal config to avoid straightening strokes after drawing.

The macOS title bar uses the PDF background (normal, dark, or custom), with a
subtle one-point bottom separator, centered numbered PDF tabs, and no traffic-light
buttons. Its height approximates the retro WezTerm tab row configured here: 16pt
JetBrains Mono at default line height, rounded to 22 whole logical points.
Fractional point heights on Retina displays can make Qt's window geometry and
expose sizes disagree, preventing OpenGL frames from reaching the screen.
Dragging and double-clicking the bar still use native macOS behavior. Theme
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

`toggle_split` (Cmd+Backslash) toggles exactly two side-by-side PDF panes within
one window, with one shared tab bar. Splitting copies the current view and focuses
the right pane. Ctrl+H/L focuses left/right; Cmd+Option+W switches sides.
Clicking a PDF also focuses it; a subtle line marks the focused pane.
Cmd+1–9, tab clicks and Cmd+T affect that pane, with independent reading positions
and zoom even when both panes show the same PDF. The divider can be dragged;
it and the focus indicator share the active pane's theme color and update live like the tab bar separator.
Cmd+W removes a tab from the shared list; either pane displaying it switches to
its neighbor. Closing the last tab closes the window. Ctrl+T still opens a new
window, and Ctrl+W closes the entire window. Toggling the split off keeps the
focused PDF and all tabs. Saved sessions retain tabs and the active PDF, but
reopen with a single pane; press Cmd+Backslash to split again.

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
python3 tests/test-split-view.py # after a local macOS build
clang++ -std=c++17 tests/test-pen-geometry.cpp -o /tmp/sioyek-pen-test
/tmp/sioyek-pen-test
SIOYEK_QT_DIR=/path/to/Qt/6.8.1/macos bash tests/test-macos-titlebar.sh
```

These compile the actual drawing methods in small fixtures; they test drawing
order, restoration, eraser geometry, hidden colors, and history invalidation.
A full macOS build validates integration with Qt and the rest of Sioyek.
The split-view test runs the real application with copied PDFs and Qt test-mode
data, exercising shortcuts, focus, independent views, resizing, tab closure and
collapse. Set `SIOYEK_SPLIT_PREVIEW=1` to keep its window open for visual inspection.

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
