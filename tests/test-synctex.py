#!/usr/bin/env python3
"""Test native SyncTeX clicks, split routing and command argument preservation."""
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
    helper.write_text('synctex-test-flags:\n' + ''.join(
        "\t@printf '%s\\n' '$(" + name + ")' > '" + str(Path(tmp) / name) + "'\n"
        for name in variables))
    subprocess.run(['make', '-s', '-f', 'Makefile', '-f', str(helper), 'synctex-test-flags'], check=True)
    expanded = {name: (Path(tmp) / name).read_text().strip() for name in variables}
def flags(name):
    return shlex.split(expanded[name])
with tempfile.TemporaryDirectory(prefix='sioyek-synctex-test-') as tmp:
    contents = Path(tmp) / 'sioyek.app' / 'Contents'
    binary = contents / 'MacOS' / 'sioyek'
    binary.parent.mkdir(parents=True)
    info = plistlib.loads((root / 'build/sioyek.app/Contents/Info.plist').read_bytes())
    info['CFBundleIdentifier'] = 'info.sioyek.synctex-test.' + uuid.uuid4().hex
    (contents / 'Info.plist').write_bytes(plistlib.dumps(info))
    (contents / 'Resources').symlink_to(root / 'build/sioyek.app/Contents/Resources')
    (contents / 'Frameworks').symlink_to(root / 'build/sioyek.app/Contents/Frameworks')
    (contents / 'PlugIns').symlink_to(root / 'build/sioyek.app/Contents/PlugIns')
    obj = str(Path(tmp) / 'main-test.o')
    subprocess.run(flags('CXX') + ['-c'] + flags('CXXFLAGS') + flags('INCPATH') +
                   ['-o', obj, 'tests/test-synctex.cpp'], check=True)
    objects = [item for item in flags('OBJECTS') if item != 'main.o']
    subprocess.run(flags('LINK') + flags('LFLAGS') + ['-o', str(binary), obj] + objects + flags('LIBS'), check=True)
    project = Path(tmp) / "source's space"
    project.mkdir()
    build = project / 'build'
    build.mkdir()
    source = project / 'notes.tex'
    source.write_text(r"""\documentclass{article}
\begin{document}
\noindent
Source jump marker on line four.
\par
Another paragraph to distinguish the target.
\end{document}
""")
    source.write_text(source.read_text().replace(r"\end{document}",
        "\n".join(r"\par Additional test line number " + str(i) + "." for i in range(40)) + r"""
\newpage
\thispagestyle{empty}
\noindent{\bfseries 4.1\quad Compatible endomorphisms}
\par\noindent We map $P^1 \to P^0$ with $x_i^2$ and $\widetilde{M}$ into the target.
\begin{equation}
AP=PV,\qquad a_{ij}=0\ \mathrm{unless}\ g_i\leq g_j,\qquad h_i<h_j.
\end{equation}
\begin{equation}
\pi:\mathcal E\longrightarrow\prod_{g\in\Gamma}\mathrm{Mat}_{m_g}(k),\qquad (U)\mapsto(U_{gg})+\sum_{i=1}^s e_i.
\end{equation}
\noindent First neighboring line containing $A^0$ and $B_1$.
\par\noindent Second neighboring line containing $C^2$ and $D_3$.
\par\noindent $x$.
\par\bigskip\noindent
\begin{minipage}[t]{.44\linewidth}Left column has its own line.\end{minipage}\hfill
\begin{minipage}[t]{.44\linewidth}Right column has its own line.\end{minipage}
\end{document}
"""))
    latex = shutil.which('pdflatex') or str(Path.home() / '.local/share/TinyTeX/bin/universal-darwin/pdflatex')
    subprocess.run([latex, '-interaction=nonstopmode', '-halt-on-error', '-synctex=1',
                    '-output-directory=build', 'notes.tex'], cwd=project, check=True,
                   stdout=subprocess.DEVNULL)
    capture = project / 'capture.py'
    capture.write_text('import json,sys\nwith open(sys.argv[1], "a") as f: f.write(json.dumps(sys.argv[2:]) + "\\n")\n')
    env = os.environ.copy()
    env.update(SIOYEK_TEST_SOURCE=str(source), SIOYEK_TEST_CAPTURE=str(capture),
               SIOYEK_TEST_OUTPUT=str(project / 'jumps.jsonl'), SIOYEK_TEST_PYTHON=sys.executable)
    subprocess.run([str(binary), str(build / 'notes.pdf')], check=True,
                   timeout=120, env=env)
