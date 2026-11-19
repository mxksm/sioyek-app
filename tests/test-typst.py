#!/usr/bin/env python3
"""Test native Typst clicks, split routing and command argument preservation."""
import os
from pathlib import Path
import plistlib
import shlex
import subprocess
import tempfile
import uuid
import shutil
import sys
root = Path(__file__).resolve().parents[1]
os.chdir(root)
subprocess.run(['make', '-j4'], check=True)
# Let make evaluate qmake's architecture expressions instead of parsing them.
variables = ('CXX', 'CXXFLAGS', 'INCPATH', 'LINK', 'LFLAGS', 'OBJECTS', 'LIBS')
with tempfile.TemporaryDirectory(prefix='sioyek-build-flags-') as tmp:
    helper = Path(tmp) / 'flags.mk'
    helper.write_text('typst-test-flags:\n' + ''.join(
        "\t@printf '%s\\n' '$(" + name + ")' > '" + str(Path(tmp) / name) + "'\n"
        for name in variables))
    subprocess.run(['make', '-s', '-f', 'Makefile', '-f', str(helper), 'typst-test-flags'], check=True)
    expanded = {name: (Path(tmp) / name).read_text().strip() for name in variables}
def flags(name):
    return shlex.split(expanded[name])
with tempfile.TemporaryDirectory(prefix='sioyek-typst-test-') as tmp:
    contents = Path(tmp) / 'sioyek.app' / 'Contents'
    binary = contents / 'MacOS' / 'sioyek'
    binary.parent.mkdir(parents=True)
    info = plistlib.loads((root / 'build/sioyek.app/Contents/Info.plist').read_bytes())
    info['CFBundleIdentifier'] = 'info.sioyek.typst-test.' + uuid.uuid4().hex
    (contents / 'Info.plist').write_bytes(plistlib.dumps(info))
    (contents / 'Resources').symlink_to(root / 'build/sioyek.app/Contents/Resources')
    (contents / 'Frameworks').symlink_to(root / 'build/sioyek.app/Contents/Frameworks')
    (contents / 'PlugIns').symlink_to(root / 'build/sioyek.app/Contents/PlugIns')
    obj = str(Path(tmp) / 'main-test.o')
    subprocess.run(flags('CXX') + ['-c'] + flags('CXXFLAGS') + flags('INCPATH') +
                   ['-o', obj, 'tests/test-typst.cpp'], check=True)
    objects = [item for item in flags('OBJECTS') if item != 'main.o']
    subprocess.run(flags('LINK') + flags('LFLAGS') + ['-o', str(binary), obj] + objects + flags('LIBS'), check=True)
    import importlib.util
    import json
    import time
    import argparse
    bridge_path=root.parent/'sioyek/commands/typst_source.py'
    spec=importlib.util.spec_from_file_location('bridge',bridge_path)
    bridge=importlib.util.module_from_spec(spec);spec.loader.exec_module(bridge)
    project=Path(tmp).resolve()/'project space';project.mkdir()
    source=project/'main.typ'
    source.write_text('#set page(width: 300pt, height: 400pt, margin: 30pt)\nHello inverse search.\n')
    init=project/'init.lua'
    init.write_text('package.path = '+json.dumps(str(root.parent/'nvim/lua/?.lua')+';')+' .. package.path\n')
    server=str(project/'nvim.sock')
    editor=subprocess.Popen([bridge.executable('nvim'),'--headless','-u',str(init),'--listen',server],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
    pdf=source.with_suffix('.pdf')
    try:
        for _ in range(100):
            if Path(server).exists():break
            time.sleep(.02)
        bridge.compile_pdf(argparse.Namespace(source=str(source),server=server,input=[]))
        env=os.environ.copy();env.update(SIOYEK_TYPST_SERVER=server,SIOYEK_TYPST_SOURCE=str(source),SIOYEK_TYPST_NVIM=bridge.executable("nvim"))
        subprocess.run([str(binary),str(pdf)],env=env,check=True,timeout=50)
    finally:
        editor.terminate();editor.wait(timeout=5)
        bridge.registration(pdf).unlink(missing_ok=True)
