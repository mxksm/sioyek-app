#!/usr/bin/env python3
"""Compile the real session reader against legacy, current and malformed sessions."""
from pathlib import Path
import subprocess
import tempfile
root = Path(__file__).resolve().parent.parent
source = (root / 'pdf_viewer/utils.cpp').read_text()
start = source.index('std::vector<WindowState> get_last_saved_windows_states()')
reader = source[start:source.index('\n}', start) + 2]
header = (root / 'pdf_viewer/utils.h').read_text()
start = header.index('struct WindowState {')
state = header[start:header.index('\n};', start) + 3]
fixture = r'''
#include <cassert>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <iostream>
struct Path { std::string value; std::string get_path_utf8() { return value; } };
Path last_opened_file_address_path;
std::wstring utf8_decode(const std::string& text) { return std::wstring(text.begin(), text.end()); }
'''
test = r'''
int main(int argc, char** argv) {
    last_opened_file_address_path.value = argv[1];
    auto write = [](const char* content) { std::ofstream f(last_opened_file_address_path.value); f << content; };
    write("/a.pdf\n/b.pdf\n");
    auto states = get_last_saved_windows_states();
    assert(states.size() == 1 && states[0].tabs.size() == 2 && states[0].selected_tab() == 0);
    write("[active] 1\n/a.pdf\n/b.pdf\n");
    states = get_last_saved_windows_states();
    assert(states[0].selected_tab() == 1 && states[0].tabs[0] == L"/a.pdf");
    write("[window] abc\n[active] 1\n/a.pdf\n/b.pdf\n[window] def\n[active] 0\n/c.pdf\n");
    states = get_last_saved_windows_states();
    assert(states.size() == 2 && states[0].geometry_hex == "abc" && states[1].geometry_hex == "def");
    assert(states[0].selected_tab() == 1 && states[1].tabs.size() == 1 && states[1].tabs[0] == L"/c.pdf");
    for (const char* invalid : {"[active] -1\n/a.pdf\n", "[active] 999\n/a.pdf\n", "[active] invalid\n/a.pdf\n"}) {
        write(invalid); states = get_last_saved_windows_states();
        assert(states[0].selected_tab() == 0 && states[0].tabs.size() == 1);
    }
    write(""); assert(get_last_saved_windows_states().empty());
    std::cout << "PASS: legacy session, stable tab order/selection, independent windows and invalid selection fallback\n";
}
'''
with tempfile.TemporaryDirectory(prefix='sioyek-tab-session-') as temp:
    cpp = Path(temp) / 'test.cpp'
    exe = Path(temp) / 'test'
    cpp.write_text(fixture + state + '\n' + reader + '\n' + test)
    subprocess.run(['clang++', '-std=c++17', str(cpp), '-o', str(exe)], check=True)
    subprocess.run([str(exe), str(Path(temp) / 'session.txt')], check=True)
