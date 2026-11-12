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
- **Two PDFs in one window.** Split into exactly two side-by-side or stacked panes sharing
  one tab bar. Each pane has its own document, reading position, and zoom. The
  divider is resizable, and focus can move between panes by keyboard or mouse.
  Optional linked navigation applies movement, page jumps, zoom and rotation to both.
- **Drawing and annotations.** Smooth freehand ink, configurable pen colors and
  width, chronological undo across pages, redo, and text boxes. The brush eraser
  removes the touched portions of strokes. Hold the right mouse button in drawing
  mode to erase temporarily, then release to return to the pen. A lasso selects
  ink for moving and proportional scaling, with undo and redo.
- **Presentation laser.** Draw a glowing red trail that stays visible while the
  mouse button is held, then fades from the beginning after release. Its fade
  duration is configurable, and trails are never saved as annotations.
- **Searchable document pickers.** Floating recent-file and folder panels search
  filenames, PDF titles, and first-page text. Long lists expand to the available
  window height and remain scrollable. Search supports reordered words,
  case-insensitive and accent-insensitive matching, and optional typo tolerance.
  A PDF preview appears beside the results. Library content search finds phrases
  on any page and highlights them in the preview.
- **Theme-aware interface.** Minimal macOS tabs with square styling and no
  traffic-light buttons. Tabs form a centered, square floating strip that fits
  their labels and uses the same theme colors as the compact status panel.
  Overflow tabs scroll horizontally; pickers and split indicators also update
  with the theme. Errors appear in embedded
  floating panels, and pending shortcut prefixes show available continuations.
  Explicit sRGB rendering
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
| Open the blank canvas / close the current tab | Cmd+T / Cmd+W |
| Reopen the last closed tab | Cmd+Shift+T |
| Select a numbered tab | Cmd+1–9 |
| Move the current tab left / right, stopping at either end | Shift+, / Shift+. (`<` / `>`) |
| Cycle through recent tabs; release Ctrl to open the selection | Ctrl+Tab; add Shift to reverse |
| Toggle side-by-side / top-and-bottom panes | Cmd+Backslash / Cmd+Shift+Backslash |
| Focus the left / right pane | Ctrl+H / Ctrl+L |
| Focus the top / bottom pane | Ctrl+K / Ctrl+J |
| Link / unlink navigation in both panes | Cmd+L |
| Switch focus to the other pane | Cmd+Option+W |
| Open a separate window / close the entire window | Ctrl+T / Ctrl+W |
| Search recent PDFs / browse the current PDF's folder | Space, F, F / Space, F, D |
| Search library PDF names and titles | Space, F, L |
| Search text inside library PDFs | Space, F, W |
| Previous / next page in the picker preview | Ctrl+L / Ctrl+D |
| Go to a saved mark | Apostrophe, then the mark |
| Remove the selected entry from recents, keeping the search text | Cmd+Delete |
| Open the floating table of contents | `t` |
| Jump to a page | `p`, page number, Enter |
| Toggle drawing | `x` |
| Choose red / green / blue ink | Cmd+R / Cmd+G / Cmd+B |
| Change pen width | Option+W |
| Toggle brush eraser / use rectangle eraser | `c` / Shift+P |
| Toggle ink lasso | Option+V |
| Undo / redo drawing | Cmd+Z or Ctrl+Z / Cmd+Shift+Z |
| Add / edit / delete a text annotation | Shift+A / `sa`, then its label / `wa` |
| Toggle temporary laser | Shift+C |
| Show or hide the tab bar / status bar | `st` / `sb` |

Split view supports exactly two panes, either side by side or stacked. The other
layout's shortcut reorients the existing panes; repeating the current layout's
shortcut collapses the split. Both PDFs, their reading positions, and focus stay
intact when changing orientation. Drag the divider to adjust the proportions.
In split view, tab selection and document pickers act on the focused pane.
Closing the active tab returns to the most recently focused tab that is still open.
Cmd+Shift+T restores closed tabs in reverse closing order, including their tab
position and reading position, in the focused pane. Each window keeps its last
100 closed tabs until it is closed; missing files are skipped.
Collapsing the split keeps the focused pane's PDF and remembers the other pane's
PDF, reading position, and zoom for that window. Splitting again restores that
companion in its original pane and preserves focus while its tab remains open;
closing its tab makes the next split copy
the current PDF instead. Session restoration retains tabs and the active document;
windows reopen with a single pane.

In any document picker, use the arrow keys to select and Enter to open.
In the folder picker, Enter opens a selected folder;
Option+Up or Backspace in an empty search field goes to its parent. Explicit file
and folder paths also work. Removing a recent entry leaves the PDF on disk.

The table of contents (`t`) opens in a wide floating panel with chapter hierarchy
and page numbers. It starts in normal mode: `j`/`k` select sections, `gg`/`G`
jump to the first/last visible section, and Enter opens the selection. Press `i`
to search, then Esc to return to normal mode; Esc again closes it. Left/right
arrows collapse/expand chapters in normal mode. In split view, the panel spans
the window and opens sections in the pane that invoked it.

Bookmarks, highlights, marks, portals, the older document/window lists, and
configuration-file and custom-option menus use the same wide floating picker.
They start in normal mode: `j`/`k`, `gg`/`G`, `i` to search, and Enter to select.
Existing delete/edit actions remain available; Cmd+Delete removes the selected
entry even while the search field contains a query.

`sb` toggles a compact, square status panel at the bottom-right of each PDF pane.
It normally shows only the page count. Active tools add a short label; the pen
also shows its width and ink color. Search shows the current match, and zoom
appears for 1.5 seconds after changing it. No full-width strip is reserved.

Optional settings in `prefs_local.config`:

```conf
# Include the current chapter in the compact status panel.
status_bar_show_chapter 1
# Restore the legacy strip and status_bar_format customization if preferred.
# compact_status_bar 0
```

### PDF library

**Space F L** (`open_library`) searches the union of PDFs under your configured folders,
including nested folders and documents that have never been opened. In
`prefs_user.config`, set:

```conf
pdf_library_path ~/Documents
```

For multiple folders, separate them with semicolons:

```conf
pdf_library_path ~/Documents; ~/Downloads; ~/Books
```

Each folder includes its subfolders. Overlapping folders and repeated paths do
not duplicate results. If one folder is unavailable, the others still work.
Use `~/Documents/books` for a smaller collection, or `~` for your entire home
folder. Absolute paths and spaces are supported; do not add quotes. Semicolons
are reserved as separators. `~` expands
to the current user's home, so the setting works across computers. Reopen the
picker after changing the folders.

Search filenames, PDF titles, first-page text, or containing folder names. Each
row shows its containing folder (relative when using one root), and the footer shows the selected file's full
path. The same normal-mode controls work here, including `j/k`, `gg/G`, and `yy`.
Enter opens the selection in the pane that launched the picker.

Folder scanning and title extraction run in the background. The filename list
appears first; title matches become available as indexing finishes. Scan results
and titles are cached during the running session. Opening the picker rescans for
changes; while it stays visible, it checks again every 30 seconds. Scanning does
not add files to recents. Only readable PDFs in non-hidden folders are included;
symlinks are skipped to avoid cycles and leaving the chosen folder. Image-only
scans need text recognition or title metadata to match by title.

### Library content search and previews

**Space F L** opens the library filename/title picker. **Space F W**
(`search_library_contents`) searches text throughout the same configured library.
These are key sequences, pressed one after another. The companion configuration
moves “go to mark” from Space to apostrophe to leave Space available as a prefix.

Both searches show a PDF preview on the right. Preview pages use the originating
pane's reader color mode and page colors; the surrounding gap uses its outside-page
background. Theme changes update cached previews without reloading the PDF.
Selecting a content result previews
its matching page with the phrase highlighted; Enter opens that page in the
originating pane. **Ctrl+L / Ctrl+D** browse preview pages without opening the PDF.
In normal mode, **[ / ]** jump to the previous / next matching page; Enter opens
the currently previewed page. All matches on the previewed page are highlighted.
**Ctrl+U** remains an alias for the previous page.

Previews cache rendered pages and preload the two nearest results in each
direction. Cached selections display immediately. For an uncached PDF, the previous
image stays visible with a loading label until its replacement is ready. Caches
are bounded and invalidate when the PDF changes.

Recent and folder pickers also have previews. All retain the normal-mode search
editing and result navigation described below.

Content search matches a case-insensitive literal phrase, allowing normalized
whitespace. Results contain one row per PDF, with its number of matching pages.
The first search extracts text in the background; compressed text caches under
Sioyek's cache directory speed later searches and are refreshed when PDFs change.
Changing the query cancels older work. Results arrive as files are searched, with
a limit of 2,000 matching PDFs; narrow the query when the limit is reached.
Encrypted or unreadable PDFs are skipped. Scanned images need an OCR text layer.

Regular PDF search can highlight every visible occurrence with
`should_highlight_unselected_search 1`. The active occurrence uses
`search_highlight_color`; the others use `unselected_search_highlight_color`.
`n` / Shift+N still step forward/backward and wrap. Once the active match leaves
the viewport, the visible match nearest the center becomes active without moving
the PDF. If no match is visible, the next step starts from your reading position.

Normal reading preloads the two nearest pages in each direction so quick page
changes can use an already-rendered image. `prerendered_page_count` controls this
radius (`0` disables it). Visible pages take priority over background preloads,
and preloading uses the same bounded cache as normal rendering.

The shared page-image cache has a `render_cache_size_mb` budget (256 MiB by default)
in addition to `num_cached_pages`. After a short grace period, unused images are
evicted by age when either limit is exceeded, including large images left after
resizing. Images currently displayed in any pane/window stay protected, so this
is a soft cache limit, not a limit on total application memory.

### Moving and scaling ink

**Option+V** (`toggle_ink_lasso`) enters the lasso. Drag around strokes to select
them, then drag inside the selection box to move them. Drag a corner handle to
scale the selection proportionally, including pen width. During a drag, an outline
previews the change; releasing applies and saves it. Movement can cross pages.
**Cmd+Z / Cmd+Shift+Z** undo and redo moves and scaling as well as new strokes.
In lasso mode, **y** copies the selection, **d** cuts it, and **p** pastes at the
center of the visible pane. Pasted strokes stay selected for moving or scaling.
To paste on another page or PDF, navigate there and enter lasso mode if needed.
Each cut or paste is one **Cmd+Z / Cmd+Shift+Z** undo/redo step.

Named ink registers work like Vim: **"ay** copies to register `a`, **"ad** cuts
to it, and **"ap** pastes it. Registers `a`–`z` and the unnamed register are shared
across panes and windows for the current app session, with a combined 32 MiB cap.
They do not replace the system text clipboard. Pasted ink is saved normally.
Escape cancels a register prefix or active drag; otherwise it exits the lasso.
**x** returns to the pen; outside lasso mode, **p** still toggles the pen.

The lasso edits freehand ink on unrotated PDFs, including the persistent canvas.
It does not alter PDF text, text boxes or highlights. Transformation history is
kept in memory with bounded retention; saved drawings survive restarting the app.

### Linked split navigation

**Cmd+L** (`toggle_linked_navigation`) links or unlinks the two panes, creating a
split if necessary. Both pane indicators light up while linked. Movement such as
`j/k`, page steps, `gg/G` and numbered jumps, zoom, fitting and rotation apply to
both PDFs, including these actions inside custom macros. Each PDF respects its
own page boundaries. Tab selection, searches and annotations still use the focused
pane. Mouse-wheel scrolling is independent. Closing the split turns linking off.

### Vim-style search editing

All document pickers open in **normal mode**, ready to navigate results or copy a path.
Press `i` to type a search. Escape returns to normal mode; Escape in normal mode
closes the picker. In visual mode, Escape first clears the selection
and returns to normal mode. **Ctrl+C** closes immediately from any mode. A small
indicator beside the heading shows the current mode.
The search field uses the bundled JetBrains Mono font with a fixed-width block
cursor in normal mode.

| In normal mode | Action |
| --- | --- |
| `j` / `k` | Select the next / previous result, stopping at either end |
| `gg` / `G` | Select the first / last result in the current search |
| `i` / `a` | Insert before / after the cursor |
| `I` / `A` | Insert at the first non-space character / end of the query |
| `h` / `l`, `w` / `b` / `e` | Move by character or word |
| `W` / `B` / `E` | Move by whitespace-separated words |
| `0` / `^` / `$` | Move to the beginning / first non-space character / end |
| `v` / `V` | Select with motions / select the entire query |
| `x` | Delete the selection or character under the cursor |
| `yy` | Copy the selected file or folder's friendly, shell-quoted path |
| `y` in visual mode / `Vy` | Copy selected search text / the entire query |
| `dd`, `dw`, `cw`, `D`, `C` | Delete or change the query, a word, or the remaining text |
| `p` / `P` | Paste from the clipboard after / before the cursor |
| `u` / Ctrl+R | Undo / redo a search-text edit |
| `q` | Close the picker |

Motions and result navigation accept counts, such as `3w` or `5j`; `d`, `c`, and
`y` also combine with motions. Search updates as the text changes. Enter opens
the selected result in either mode. Cmd+Delete still removes a recent entry
without editing the query. These bindings provide single-line Vim-style editing;
they do not load a Vim or Neovim configuration.

### Command prompts

`?` is intentionally unbound.

`:` opens a wide, compact floating list of commands and readable keyboard shortcuts in **normal mode**, with the same Vim controls
as Space F F: `i` to search, `j` / `k` to navigate, `gg` / `G` for the first / last
result, and `yy` to copy the selected command name. Escape returns to normal
mode, then closes the list; Enter runs the selected command in either mode.
Commands that request text, such
as rename, bookmark text, and workspace names, use a matching floating editor.
All text-only prompts start in **insert mode**, ready to type. This includes PDF
text search, page numbers, annotation text, rename, and workspace names. Press Esc
for normal mode, with the same Vim motions and editing controls as document search. Use `i` or `a` to type, `cw` to change a
word, `cc` to replace the whole value, `v` to select, `x` to delete, and `u` /
Ctrl+R to undo / redo. Here `yy` copies the input text; `gg` / `G` move to its
beginning / end. Enter submits in either mode. Escape returns to normal mode,
then cancels when pressed again. Password prompts remain masked, ordinary input.

Rename starts with the current filename without its extension; the extension
is retained automatically.

Drawing undo/redo applies to pen strokes and lasso transformations. Erasing is not undoable. The brush
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

## Named workspaces

- **Cmd+Shift+S** (`save_workspace`): name and save the current window's layout.
  Saving the same name updates it; the last name is prefilled.
- **Cmd+Shift+P** (`open_workspace`): search saved workspaces and press Enter to
  restore one in the current window. Other windows stay as they are.

Workspaces save tab order, the selected PDF in each pane, each tab's zoom and
position, split orientation and proportions, and keyboard focus. They can include the hidden scratch
canvas, which remains shared across every workspace. Saved layouts are snapshots:
save again when you want to update one.

The floating picker uses the same Vim controls as document search: `i` to search,
Escape to return to normal mode, `j`/`k` and `gg`/`G` to navigate, Enter to open.
**Previous session (automatic)** restores the layout you had immediately before
the last workspace switch, including an unnamed set of tabs. Missing PDFs leave
the current layout intact and show their path.

In the workspace picker, **Cmd+Delete** removes the selected saved layout, including
while searching or editing the search. It selects the next result and leaves the
open PDFs and annotations untouched. The automatic Previous session entry can
also be removed; switching workspaces creates a new recovery entry.

Layouts are stored in `workspaces.json` in Sioyek's application-data directory
(on macOS, `~/Library/Application Support/sioyek/`). This file contains paths and
view settings, not copies of PDFs or drawings.

## Blank canvas

Cmd+T opens one persistent **100-page** blank PDF, shared across windows and panes. It has no
number or label in the tab bar and stays out of recents, the library and Ctrl+Tab.
Repeated Cmd+T reuses it. Closing the last PDF with Cmd+W returns to the canvas;
Cmd+W on the canvas leaves it open. Use the document pickers to open real PDFs.

Use the usual page navigation (`d`/`l`, `gg`/`G`) to move around the canvas.
Existing one-page canvases expand automatically, preserving page-one drawings and
keeping the original PDF as `blank.pdf.before-expansion`.

Draw with the usual pen and eraser shortcuts. Finished strokes, undo and redo
are saved automatically. The PDF and its drawing sidecar live in the `canvas`
folder inside Sioyek's application-data directory (on macOS,
`~/Library/Application Support/sioyek/canvas/`). Back up the whole folder: the
strokes are in `blank.pdf.sioyek.drawings`, separate from `blank.pdf`. Rebuilds
do not replace these files. An existing personal PDF named `blank.pdf` is untouched.


## Select PDF text with Vim motions

Press **`v`** to place a caret in the PDF and extend a live text selection.
It starts at the existing selection, text directly under the mouse, or text
near the center of the focused pane. There is no label-entry dialog.

- **`h/j/k/l`** or arrow keys: move by character or visual line.
- **`w/b/e`**: next word, previous word, or word end. **`W/B/E`** treat punctuation as part of a word.
- **`0/^/$`**: line start, first non-space character, or line end. **`gg/G`**: document start/end.
- **Counts**, such as **`3w`** or **`5j`**, repeat a motion. **`iw/aw`** select a word without/with surrounding whitespace.
- **`o`** switches the endpoint being moved. **`v`** switches between extending the selection and moving just the caret.
- **`y`** or **Cmd+C** copies and exits. **Escape/Enter** exits while leaving the selection available for highlighting.

The caret follows across pages, including blank pages, and works in either split pane.
The command is `visual_select`; the older label picker remains available as `keyboard_select`.

## Jump from a PDF to LaTeX source

On macOS, **Cmd+click** the PDF text to open the corresponding source location.
Alternatively, press **`sd`** (`synctex_select_line`) to label the visible text
lines, then type a label to jump to its source. Escape cancels the labels.
Both work in either split pane; Cmd+click also works while drawing without
leaving ink. The existing SyncTeX right-click mode remains available.

Compile with SyncTeX enabled (`-synctex=1`) and keep the `.synctex.gz` file
beside the PDF. VimTeX enables this in its default latexmk configuration:
open the PDF using `:VimtexView` so it supplies the callback to the existing
editor. For another editor, configure `inverse_search_command`, using `%1`
for the source path, `%2` for its line and `%3` for its column. Quote `%1`
when it is a separate command argument. A rotated PDF must be rotated back
before inverse search.

## Credits and license

Based on [Sioyek by ahrm and contributors](https://github.com/ahrm/sioyek).
Distributed under the [GNU General Public License v3](LICENSE).

### Friendly paths

`path_aliases` lists preferred folder paths, separated by semicolons. Put it in
`~/.config/sioyek/prefs_local.config`, since these folders belong to the local
computer. For example:

```
path_aliases ~/Documents/university; ~/Books
```

When a folder is a symlink, document pickers and the recent-tab switcher show
that friendly path instead of its resolved target (such as an iCloud directory).
The most specific matching target wins; missing folders are ignored. Canonical
paths remain in use internally for history, tabs and annotations.

In document pickers, `yy` copies the friendly absolute path. On macOS/Linux,
paths containing spaces or shell metacharacters are quoted automatically for
pasting into a terminal. Workspace and command names are copied unchanged.
This setting is intentionally absent from the shared `prefs_user.config`.
`prefs_local.config` is ignored by Git and must be configured separately on each Mac.
