#!/usr/bin/env python3
"""Test live Vim-style text selection against real PDF widgets and extraction."""
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
    helper.write_text('visual-select-test-flags:\n' + ''.join(
        "\t@printf '%s\\n' '$(" + name + ")' > '" + str(Path(tmp) / name) + "'\n"
        for name in variables))
    subprocess.run(['make', '-s', '-f', 'Makefile', '-f', str(helper), 'visual-select-test-flags'], check=True)
    expanded = {name: (Path(tmp) / name).read_text().strip() for name in variables}
def flags(name):
    return shlex.split(expanded[name])
with tempfile.TemporaryDirectory(prefix='sioyek-visual-select-test-') as tmp:
    contents = Path(tmp) / 'sioyek.app' / 'Contents'
    binary = contents / 'MacOS' / 'sioyek'
    binary.parent.mkdir(parents=True)
    info = plistlib.loads((root / 'build/sioyek.app/Contents/Info.plist').read_bytes())
    info['CFBundleIdentifier'] = 'info.sioyek.visual-select-test.' + uuid.uuid4().hex
    (contents / 'Info.plist').write_bytes(plistlib.dumps(info))
    shutil.copytree(root / 'build/sioyek.app/Contents/Resources', contents / 'Resources')
    shutil.copy2(root / 'pdf_viewer/keys.config', contents / 'Resources/keys.config')
    (contents / 'Frameworks').symlink_to(root / 'build/sioyek.app/Contents/Frameworks')
    (contents / 'PlugIns').symlink_to(root / 'build/sioyek.app/Contents/PlugIns')
    obj = str(Path(tmp) / 'main-test.o')
    subprocess.run(flags('CXX') + ['-c'] + flags('CXXFLAGS') + flags('INCPATH') +
                   ['-o', obj, 'tests/test-visual-select.cpp'], check=True)
    objects = [item for item in flags('OBJECTS') if item != 'main.o']
    subprocess.run(flags('LINK') + flags('LFLAGS') + ['-o', str(binary), obj] + objects + flags('LIBS'), check=True)
    project = Path(tmp) / "source's space"
    project.mkdir()
    build = project / 'build'
    build.mkdir()
    source = project / 'notes.tex'
    source.write_text(r"""\documentclass{article}
\pagestyle{empty}
\begin{document}
\noindent alpha beta gamma delta\\
epsilon zeta eta theta\\
one\_two, punctuation!
\newpage\null\newpage
\noindent last page omega
\end{document}
""")
    latex = shutil.which('pdflatex') or str(Path.home() / '.local/share/TinyTeX/bin/universal-darwin/pdflatex')
    subprocess.run([latex, '-interaction=nonstopmode', '-halt-on-error', '-synctex=1',
                    '-output-directory=build', 'notes.tex'], cwd=project, check=True,
                   stdout=subprocess.DEVNULL)
    subprocess.run([str(binary), str(build / 'notes.pdf')], check=True, timeout=120)
