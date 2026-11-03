#!/usr/bin/env python3
"""Check that resize/IPC launches never register a foreground macOS application."""
import os
import json
import time
import uuid
from pathlib import Path
import plistlib
import shlex
import subprocess
import tempfile
root = Path(__file__).resolve().parents[1]
os.chdir(root)
subprocess.run(['make', '-j4'], check=True)
# Let make evaluate qmake's architecture expressions instead of parsing them.
variables = ('CXX', 'CXXFLAGS', 'INCPATH', 'LINK', 'LFLAGS', 'OBJECTS', 'LIBS')
with tempfile.TemporaryDirectory(prefix='sioyek-build-flags-') as tmp:
    helper = Path(tmp) / 'flags.mk'
    helper.write_text('split-test-flags:\n' + ''.join(
        "\t@printf '%s\\n' '$(" + name + ")' > '" + str(Path(tmp) / name) + "'\n"
        for name in variables))
    subprocess.run(['make', '-s', '-f', 'Makefile', '-f', str(helper), 'split-test-flags'], check=True)
    expanded = {name: (Path(tmp) / name).read_text().strip() for name in variables}
def flags(name):
    return shlex.split(expanded[name])
with tempfile.TemporaryDirectory(prefix='sioyek-startup-test-') as tmp:
    contents = Path(tmp) / 'sioyek.app' / 'Contents'
    binary = contents / 'MacOS' / 'sioyek'
    binary.parent.mkdir(parents=True)
    info = plistlib.loads((root / 'build/sioyek.app/Contents/Info.plist').read_bytes())
    # Use current bundle policy even before the packaging step has run.
    info.pop('LSUIElement', None)
    assert not plistlib.loads((root / 'resources/Info.plist').read_bytes()).get('LSUIElement', False)
    # A failed test must not leave a macOS crash-restoration prompt for the next run.
    info['CFBundleIdentifier'] = 'info.sioyek.startup-test.' + uuid.uuid4().hex
    (contents / 'Info.plist').write_bytes(plistlib.dumps(info))
    (contents / 'Resources').symlink_to(root / 'build/sioyek.app/Contents/Resources')
    (contents / 'Frameworks').symlink_to(root / 'build/sioyek.app/Contents/Frameworks')
    (contents / 'PlugIns').symlink_to(root / 'build/sioyek.app/Contents/PlugIns')
    obj = str(Path(tmp) / 'main-test.o')
    subprocess.run(flags('CXX') + ['-c'] + flags('CXXFLAGS') + flags('INCPATH') +
                   ['-o', obj, 'tests/test-macos-startup.cpp'], check=True)
    probe = str(Path(tmp) / 'startup-probe.o')
    subprocess.run(flags('CXX') + ['-c'] + flags('CXXFLAGS') + flags('INCPATH') +
                   ['-o', probe, 'tests/test-macos-startup-probe.mm'], check=True)
    objects = [item for item in flags('OBJECTS') if item != 'main.o']
    subprocess.run(flags('LINK') + flags('LFLAGS') + ['-o', str(binary), obj, probe] + objects + flags('LIBS'), check=True)
    subprocess.run([str(binary), 'ipc-delay'], check=True, timeout=10)
    pdf = Path(tmp) / 'startup.pdf'
    pdf.write_bytes((root / 'tutorial.pdf').read_bytes())
    for mode in ('cli', 'launch-services'):
        ready = Path(tmp) / ('ready-' + mode)
        instance = 'sy-' + uuid.uuid4().hex[:8]
        arguments = ['primary', str(pdf), str(ready), instance]
        command = [str(binary)] if mode == 'cli' else ['open', '-W', '-n', '-a', str(contents.parent), '--args']
        primary = subprocess.Popen(command + arguments)
        try:
            deadline = time.monotonic() + 10
            while not ready.exists():
                if primary.poll() is not None or time.monotonic() > deadline:
                    raise RuntimeError(f'Primary failed to restore two windows: exit={primary.poll()}')
                time.sleep(0.05)
            if os.environ.get('SIOYEK_TEST_YABAI'):
                pid = int(ready.read_text())
                deadline = time.monotonic() + 5
                while True:
                    tracked = json.loads(subprocess.check_output(['yabai', '-m', 'query', '--windows']))
                    tiled = [window for window in tracked if window['pid'] == pid and
                             window['has-ax-reference'] and not window['is-floating']]
                    if len(tiled) == 2:
                        break
                    if time.monotonic() > deadline:
                        raise RuntimeError(f'Yabai did not tile both windows: {tiled}')
                    time.sleep(0.1)
                print(f'PASS: Yabai automatically tracks and tiles both {mode} windows')
            for _ in range(5):
                subprocess.run([str(binary), 'secondary', 'fit_to_page_width', str(ready), instance],
                               check=True, timeout=5)
            subprocess.run([str(binary), 'secondary', 'quit', str(ready), instance],
                           check=True, timeout=5)
            primary.wait(timeout=5)
            assert primary.returncode == 0
            print(f'PASS: {mode} launch, two windows, repeated resize commands, and clean quit')
        finally:
            if primary.poll() is None:
                primary.terminate()
                primary.wait(timeout=5)
