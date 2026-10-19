# Personal Sioyek fork

This fork contains the source changes previously applied by scripts in
[mxksm/sioyek](https://github.com/mxksm/sioyek): live status-bar theme refresh,
chronological drawing undo across pages, drawing redo, and a mouse brush eraser.

The drawing eraser removes whole visible strokes along the cursor path. Its radius
is 12 screen pixels. Erasing clears drawing redo history and is not undoable.
`toggle_brush_eraser` enters/exits erasing; Escape exits and drawing mode switches
back to the pen. Text annotations and PDF content are unaffected.

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
and reopen the installed app to load the new binary.

Build products and source stay available for incremental builds.
`SIOYEK_BUILD_CACHE` overrides the cache directory; `SIOYEK_QT_DIR` may point to
an existing Qt 6.8.1 `macos` directory. The Qt metadata workaround removes obsolete
AGL framework references; no application source is rewritten by the build.

## Checks

```sh
python3 tests/test-drawing-redo.py .
python3 tests/test-brush-eraser.py .
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
