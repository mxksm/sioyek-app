#!/usr/bin/env python3
"""Compile the patched drawing methods against a small document fixture."""
from pathlib import Path
import subprocess
import sys
import tempfile

source = (Path(sys.argv[1]) / 'pdf_viewer/document.cpp').read_text()
names = ['add_freehand_drawing', 'undo_freehand_drawing', 'redo_freehand_drawing',
         'delete_all_page_drawings', 'delete_all_drawings']
methods = []
for name in names:
    start = source.index('void Document::' + name + '(')
    end = source.index('\n}', start) + 2
    methods.append(source[start:end])

fixture = r'''
#include <cassert>
#include <map>
#include <mutex>
#include <vector>
#include <iostream>
// Millisecond timestamps stand in for Qt dates; test the actual document methods.
using QDateTime = long long;
struct Point { int pos; float thickness; };
struct FreehandDrawing {
    std::vector<Point> points;
    QDateTime creattion_time;
    char type;
    float alpha;
};
struct DocumentPos { int page; };
struct Document {
    std::mutex drawings_mutex;
    std::map<int, std::vector<FreehandDrawing>> page_freehand_drawings;
    std::vector<std::pair<int, FreehandDrawing>> undone_freehand_drawings;
    bool is_drawings_dirty = false;
    DocumentPos absolute_to_page_pos_uncentered(int pos) { return {pos}; }
    void add_freehand_drawing(FreehandDrawing);
    void undo_freehand_drawing();
    void redo_freehand_drawing();
    void delete_all_page_drawings(int);
    void delete_all_drawings();
};
'''
tests = r'''
int main() {
    Document d;
    d.undo_freehand_drawing(); d.redo_freehand_drawing();
    assert(!d.is_drawings_dirty);
    d.add_freehand_drawing({{{8,5}},1000,'r',0.6f});
    d.add_freehand_drawing({{{2,7}},1100,'n',0.9f});
    d.add_freehand_drawing({{{8,3}},1200,'r',1.0f});
    d.undo_freehand_drawing(); d.undo_freehand_drawing();
    assert(d.page_freehand_drawings[8].size()==1);
    assert(d.page_freehand_drawings[2].empty());
    d.redo_freehand_drawing();
    auto restored=d.page_freehand_drawings[2].back();
    assert(restored.creattion_time==1100 && restored.type=='n');
    assert(restored.points[0].thickness==7 && restored.alpha==0.9f);
    d.redo_freehand_drawing(); d.redo_freehand_drawing();
    assert(d.page_freehand_drawings[8].size()==2);
    d.undo_freehand_drawing();
    assert(d.page_freehand_drawings[8].size()==1);
    d.add_freehand_drawing({{{3,5}},1300,'r',1.0f});
    d.redo_freehand_drawing();
    assert(d.page_freehand_drawings[8].size()==1);
    assert(d.undone_freehand_drawings.empty());
    d.undo_freehand_drawing(); d.delete_all_page_drawings(2);
    assert(d.undone_freehand_drawings.empty());
    d.undo_freehand_drawing(); d.delete_all_drawings(); d.redo_freehand_drawing();
    assert(d.page_freehand_drawings.empty());
    std::cout << "PASS: cross-page undo/redo, stroke fidelity, empty history, new-stroke and erase invalidation\n";
}
'''
with tempfile.TemporaryDirectory() as directory:
    cpp = Path(directory) / 'redo.cpp'
    exe = Path(directory) / 'redo-test'
    cpp.write_text(fixture + '\n'.join(methods) + tests)
    subprocess.run(['clang++', '-std=c++17', str(cpp), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
