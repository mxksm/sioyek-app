#!/usr/bin/env python3
"""Verify new-tab dispatches through the native open picker, never the save picker."""
from pathlib import Path
import subprocess
import tempfile
root = Path(__file__).resolve().parent.parent
source = (root / 'pdf_viewer/ui.cpp').read_text()
start = source.index('std::wstring select_command_file_name(')
function = source[start:source.index('\n}', start)+2]
fixture = r'''
#include <cassert>
#include <string>
#include <optional>
#include <iostream>
using QString = std::string;
std::optional<QString> last_root;
bool cancel = false;
std::wstring select_document_file_name(std::optional<QString> root) {
    last_root = root; return cancel ? L"" : L"opened.pdf";
}
std::wstring select_any_file_name(std::optional<QString> root) { return L"save-picker"; }
'''
tests = r'''
int main() {
    assert(select_command_file_name("new_tab", "/documents") == L"opened.pdf");
    assert(last_root == "/documents");
    cancel = true;
    assert(select_command_file_name("new_tab", {}).empty());
    cancel = false;
    assert(select_command_file_name("open_document", {}) == L"opened.pdf");
    assert(select_command_file_name("source_config", {}) == L"save-picker");
    std::cout << "PASS: new-tab Open picker routing, initial folder and cancellation\n";
}
'''
with tempfile.TemporaryDirectory(prefix='sioyek-tab-picker-') as temp:
    cpp=Path(temp)/'test.cpp'; exe=Path(temp)/'test'
    cpp.write_text(fixture+function+tests)
    subprocess.run(['clang++','-std=c++17',str(cpp),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
