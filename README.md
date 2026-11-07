# Sioyek — tabs, split view, and drawing

A macOS-focused fork of [Sioyek](https://github.com/ahrm/sioyek) for reading,
comparing, and annotating PDFs. It adds shared tabs, two-pane reading, drawing
tools, a temporary laser pointer, and a minimal interface that follows the
current theme.

## Features

- **Tabs and recent-tab switching.** Numbered tabs show filenames without the
  extension. Reorder them with the keyboard, or hold Ctrl+Tab to choose from a
  floating list in most-recently-used order. A quick tap switches without showing
  the panel; holding Ctrl keeps the current PDF visible until the key is released.
- **Two PDFs in one window.** Split into exactly two side-by-side panes sharing
  one tab bar. Each pane has its own document, reading position, and zoom. The
  divider is resizable, and focus can move between panes by keyboard or mouse.
- **Drawing and annotations.** Smooth freehand ink, configurable pen colors and
  width, chronological undo across pages, redo, and text boxes. The brush eraser
  removes the touched portions of strokes. Hold the right mouse button in drawing
  mode to erase temporarily, then release to return to the pen.
- **Presentation laser.** Draw a glowing red trail that stays visible while the
  mouse button is held, then fades from the beginning after release. Its fade
  duration is configurable, and trails are never saved as annotations.
- **Searchable document pickers.** Floating recent-file and folder panels search
  filenames, PDF titles, and first-page text. Long lists expand to the available
  window height and remain scrollable. Search supports reordered words,
  case-insensitive and accent-insensitive matching, and optional typo tolerance.
- **Theme-aware interface.** Minimal macOS tabs with square styling and no
  traffic-light buttons. The tab and status bars follow the PDF background;
  pickers and split indicators update with the theme. Explicit sRGB rendering
  keeps theme colors consistent with other sRGB applications.
- **Reading and reload fixes.** Page navigation stops at document boundaries,
  and reloading a repeatedly rebuilt PDF preserves the view without gradually
  shifting it. Rendering and input fixes improve page changes and live drawing.

Sioyek's existing reading tools, including bookmarks, highlights, reference
navigation, and portals, remain available.

## Install on macOS

Requires **macOS 14 or newer**, **Homebrew**, **Python 3**, and **Xcode command-line
tools**. Development and testing of this fork focus on macOS.

For the themes and shortcuts described below, follow the installation instructions
in the companion [configuration repository](https://github.com/mxksm/sioyek#installation).
Its `build-sioyek.sh` downloads this fork and builds the exact commit recorded in
`sioyek-app-revision`, keeping the app and configuration together.

To build directly from this repository:

```sh
git clone --branch development https://github.com/mxksm/sioyek-app.git
cd sioyek-app
bash scripts/build-macos-local.sh
```

The script initializes submodules, installs missing Homebrew dependencies,
downloads Qt 6.8.1, builds and signs the app, and installs it. Dependencies and Qt
are cached under `~/Library/Caches/sioyek-build`; subsequent builds reuse them.

The app is installed at `/Applications/sioyek.app`, or
`~/Applications/sioyek.app` if the system Applications folder is not writable.
The installer prints the destination and preserves a backup of the previous app.
Quit Sioyek with Cmd+Q, then open the printed path to use the new build.

Building this source checkout does not install the companion configuration or
replace existing user settings.

## Keyboard shortcuts

These are the macOS bindings supplied by the
[companion configuration](https://github.com/mxksm/sioyek). Customize them in
`keys_user.config`.

| Action | Shortcut |
| --- | --- |
| Open a PDF in a tab / close the current tab | Cmd+T / Cmd+W |
| Select a numbered tab | Cmd+1–9 |
| Move the current tab left / right, stopping at either end | Shift+, / Shift+. (`<` / `>`) |
| Cycle through recent tabs; release Ctrl to open the selection | Ctrl+Tab; add Shift to reverse |
| Toggle two-pane view | Cmd+Backslash |
| Focus the left / right pane | Ctrl+H / Ctrl+L |
| Switch focus to the other pane | Cmd+Option+W |
| Open a separate window / close the entire window | Ctrl+T / Ctrl+W |
| Search recent PDFs / browse the current PDF's folder | Shift+O / Ctrl+Shift+O |
| Remove the selected entry from recents, keeping the search text | Cmd+Delete |
| Toggle drawing | `p` or `x` |
| Choose red / green / blue ink | Cmd+R / Cmd+G / Cmd+B |
| Change pen width | Option+W |
| Toggle brush eraser / use rectangle eraser | Shift+E / Shift+P |
| Undo / redo drawing | Cmd+Z or Ctrl+Z / Cmd+Shift+Z |
| Add a text box / delete a visible annotation | Shift+A / `w`, then `a` |
| Toggle temporary laser | Shift+C |
| Show or hide the tab bar / status bar | `st` / `sb` |

In split view, tab selection and document pickers act on the focused pane.
Collapsing the split keeps that pane's PDF. Session restoration retains tabs
and the active document; windows reopen with a single pane.

In either document picker, use the arrow keys to select, Enter to open, and
Escape to return to the PDF. In the folder picker, Enter opens a selected folder;
Option+Up or Backspace in an empty search field goes to its parent. Explicit file
and folder paths also work. Removing a recent entry leaves the PDF on disk.

Drawing undo/redo applies to pen strokes. Erasing is not undoable. The brush
eraser affects ink, leaving text annotations and original PDF content intact.
Escape exits the eraser or clears the laser.

## Preferences

Edit `prefs_user.config` for colors, startup behavior, and search settings.
The companion configuration provides themes through Option+1–5, Option+8–9,
and Option+0.

```conf
# Restore windows and tabs at startup.
restore_all_windows_on_startup 1

# Tolerate small typos in document searches and other menus.
fuzzy_searching 1

# Laser lifetime after releasing the mouse, in milliseconds (100–60000).
laser_pointer_duration_ms 1450

# Preserve freehand shapes instead of straightening them after drawing.
simplify_freehand_drawings 0
```

PDF titles and first-page text are indexed locally in the background and cached
for the running session. Results can fill in as indexing completes. Image-only
scans need searchable text or title metadata for title-based matching.

## Credits and license

Based on [Sioyek by ahrm and contributors](https://github.com/ahrm/sioyek).
Distributed under the [GNU General Public License v3](LICENSE).
