#!/usr/bin/env python3
"""Exercise the actual patched erase method without launching the GUI."""
from pathlib import Path
import subprocess
import tempfile
import sys
source = (Path(sys.argv[1])/'pdf_viewer/document.cpp').read_text()
start = source.index('bool Document::erase_freehand_brush(')
end = source.index('\n}', start)+2
fixture = r'''
#include "brush_eraser.h"
#include <cassert>
#include <map>
#include <vector>
#include <mutex>
#include <iostream>
struct AbsoluteDocumentPos {float x,y;};
struct FreehandDrawingPoint {AbsoluteDocumentPos pos; float thickness;};
struct FreehandDrawing {std::vector<FreehandDrawingPoint> points; char type;};
struct Document {
 std::mutex drawings_mutex;
 std::map<int,std::vector<FreehandDrawing>> page_freehand_drawings;
 std::vector<int> undone_freehand_drawings;
 bool is_drawings_dirty=false;
 bool erase_freehand_brush(AbsoluteDocumentPos,AbsoluteDocumentPos,float,const bool*);
};
int main() {
 using brush_eraser::touches;
 assert(touches({0,0},{100,0},{50,-50},{50,50},1));
 assert(touches({0,0},{0,0},{-100,0},{100,0},1));
 assert(touches({0,0},{10,0},{5,0},{20,0},1));
 assert(!touches({0,0},{10,0},{20,0},{30,0},1));
 assert(!touches({0,0},{100,0},{0,3},{100,3},2));
 assert(touches({0,0},{100,0},{0,2},{100,2},2));
 Document doc; bool mask[26]; std::fill(mask,mask+26,true);
 doc.undone_freehand_drawings={1};
 doc.page_freehand_drawings[0]={{{{{50,-100},1},{{50,100},1}},'r'}};
 doc.page_freehand_drawings[3]={{{{{80,-100},1},{{80,100},1}},'n'}};
 mask['n'-'a']=false;
 assert(!doc.erase_freehand_brush({-20,0},{-10,0},1,mask));
 assert(!doc.is_drawings_dirty && !doc.undone_freehand_drawings.empty());
 assert(doc.erase_freehand_brush({0,0},{100,0},1,mask));
 assert(doc.page_freehand_drawings[0].empty());
 assert(doc.page_freehand_drawings[3].size()==1);
 assert(doc.is_drawings_dirty && doc.undone_freehand_drawings.empty());
 mask['n'-'a']=true;
 assert(doc.erase_freehand_brush({80,0},{80,0},1,mask));
 // Dot stroke, including its visible thickness.
 doc.page_freehand_drawings[0]={{{{{0,0},5}},'r'}};
 assert(doc.erase_freehand_brush({6,0},{6,0},1,mask));
 // An empty stroke is harmless.
 doc.page_freehand_drawings[0]={{{},'r'}};
 assert(!doc.erase_freehand_brush({0,0},{0,0},1,mask));
 std::cout << "PASS: swept paths, dots, thickness, hidden colors, multiple pages, history invalidation\n";
}
'''
# Define the extracted method before main.
fixture=fixture.replace('int main() {',source[start:end]+'\nint main() {')
with tempfile.TemporaryDirectory() as tmp:
    cpp=Path(tmp)/'brush.cpp'; exe=Path(tmp)/'brush'
    cpp.write_text(fixture)
    subprocess.run(['clang++','-std=c++17','-I'+str(Path(sys.argv[1])/'pdf_viewer'),str(cpp),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
