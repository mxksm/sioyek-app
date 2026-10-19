#!/usr/bin/env python3
"""Exercise the actual erase method without launching the GUI."""
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
#include <random>
struct AbsoluteDocumentPos {float x,y;};
struct FreehandDrawingPoint {AbsoluteDocumentPos pos; float thickness;};
struct FreehandDrawing {std::vector<FreehandDrawingPoint> points; char type; float alpha=1; long long creattion_time=123;};
struct Document {
 std::mutex drawings_mutex;
 std::map<int,std::vector<FreehandDrawing>> page_freehand_drawings;
 std::vector<int> undone_freehand_drawings;
 bool is_drawings_dirty=false;
 bool erase_freehand_brush(AbsoluteDocumentPos,AbsoluteDocumentPos,float,const bool*);
};
bool near(double a, double b) { return std::abs(a-b)<1e-4; }
int main() {
 using brush_eraser::Point;
 using brush_eraser::erased_interval;
 auto middle=erased_interval({-100,0},{100,0},{0,0},{0,0},10);
 assert(middle && near(middle->first,0.45) && near(middle->second,0.55));
 auto sweep=erased_interval({-100,0},{100,0},{-20,0},{20,0},10);
 assert(sweep && near(sweep->first,0.35) && near(sweep->second,0.65));
 assert(!erased_interval({-100,11},{100,11},{-20,0},{20,0},10));
 assert(erased_interval({0,0},{0,0},{0,0},{0,0},10));
 assert(!erased_interval({20,0},{20,0},{0,0},{0,0},10));
 // Independent sampled-distance oracle for arbitrary orientations and cap hits.
 std::mt19937 random(42);
 std::uniform_real_distribution<double> coord(-100,100);
 for (int i=0;i<5000;++i) {
   Point a{coord(random),coord(random)}, b{coord(random),coord(random)};
   Point from{coord(random),coord(random)}, to{coord(random),coord(random)};
   auto interval=erased_interval(a,b,from,to,12);
   for (int j=0;j<=100;++j) {
     double t=j/100.0;
     Point p{a.x+t*(b.x-a.x),a.y+t*(b.y-a.y)};
     bool inside=brush_eraser::point_distance_squared(p,from,to)<144;
     bool clipped=interval && t>=interval->first && t<=interval->second;
     assert(inside==clipped);
   }
 }
 Document doc; bool mask[26]; std::fill(mask,mask+26,true);
 doc.undone_freehand_drawings={1};
 doc.page_freehand_drawings[0]={{{{{-100,0},2},{{100,0},2}},'r',0.6f,456}};
 doc.page_freehand_drawings[3]={{{{{-100,0},2},{{100,0},2}},'n'}};
 mask['n'-'a']=false;
 assert(!doc.erase_freehand_brush({0,20},{0,30},10,mask));
 assert(!doc.is_drawings_dirty && !doc.undone_freehand_drawings.empty());
 assert(doc.erase_freehand_brush({0,0},{0,0},10,mask));
 auto& pieces=doc.page_freehand_drawings[0];
 assert(pieces.size()==2 && pieces[0].points.size()==2 && pieces[1].points.size()==2);
 assert(near(pieces[0].points.front().pos.x,-100));
 assert(near(pieces[0].points.back().pos.x,-12));
 assert(near(pieces[1].points.front().pos.x,12));
 assert(near(pieces[1].points.back().pos.x,100));
 for (auto& piece: pieces) {
   assert(piece.type=='r' && piece.alpha==0.6f && piece.creattion_time==456);
   for (auto& p:piece.points) assert(p.thickness==2);
 }
 assert(doc.page_freehand_drawings[3].size()==1); // Hidden color untouched.
 assert(doc.is_drawings_dirty && doc.undone_freehand_drawings.empty());
 // Repeat passes must not delete or reconnect the remaining fragments.
 doc.erase_freehand_brush({0,0},{0,0},10,mask);
 assert(pieces.size()==2 && near(pieces[0].points.back().pos.x,-12));
 assert(doc.erase_freehand_brush({40,0},{60,0},10,mask));
 assert(pieces.size()==3);
 assert(near(pieces[1].points.front().pos.x,12) && near(pieces[1].points.back().pos.x,28));
 assert(near(pieces[2].points.front().pos.x,72) && near(pieces[2].points.back().pos.x,100));
 // Many sampled points crossing the brush must become two continuous pieces.
 Document sampled;
 FreehandDrawing line{{},'r'};
 for (int x=-50;x<=50;++x) line.points.push_back({{float(x),0},1});
 sampled.page_freehand_drawings[0]={line};
 assert(sampled.erase_freehand_brush({0,-100},{0,100},5,mask));
 auto& sampled_pieces=sampled.page_freehand_drawings[0];
 assert(sampled_pieces.size()==2);
 assert(near(sampled_pieces[0].points.back().pos.x,-6));
 assert(near(sampled_pieces[1].points.front().pos.x,6));
 // Whole-segment removal, endpoint trim, dots, and zero-length segments.
 Document end;
 end.page_freehand_drawings[0]={{{{{0,0},1},{{100,0},1}},'r'}};
 assert(end.erase_freehand_brush({0,0},{0,0},10,mask));
 assert(end.page_freehand_drawings[0].size()==1);
 assert(near(end.page_freehand_drawings[0][0].points.front().pos.x,11));
 assert(end.erase_freehand_brush({0,0},{100,0},10,mask));
 assert(end.page_freehand_drawings[0].empty());
 end.page_freehand_drawings[0]={{{{{0,0},1},{{0,0},1}},'r'}, {{{{0,0},5}},'r'}, {{},'r'}};
 assert(end.erase_freehand_brush({0,0},{0,0},1,mask));
 assert(end.page_freehand_drawings[0].size()==1 && end.page_freehand_drawings[0][0].points.empty());
 // Cut endpoints preserve interpolated pressure/width.
 end.page_freehand_drawings[0]={{{{{-100,0},1},{{100,0},3}},'r'}};
 assert(end.erase_freehand_brush({0,0},{0,0},10,mask));
 assert(near(end.page_freehand_drawings[0][0].points.back().thickness,1.87));
 assert(near(end.page_freehand_drawings[0][1].points.front().thickness,2.13));
 std::cout << "PASS: partial cuts, repeated passes, swept brush, metadata, pressure, hidden colors, geometry oracle\n";
}

'''
# Define the extracted method before main.
fixture=fixture.replace('int main() {',source[start:end]+'\nint main() {')
with tempfile.TemporaryDirectory() as tmp:
    cpp=Path(tmp)/'brush.cpp'; exe=Path(tmp)/'brush'
    cpp.write_text(fixture)
    subprocess.run(['clang++','-std=c++17','-I'+str(Path(sys.argv[1])/'pdf_viewer'),str(cpp),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
