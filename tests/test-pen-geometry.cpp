#include "../pdf_viewer/pen_geometry.h"
#include <cassert>
#include <iostream>
#include <limits>
using namespace pen_geometry;
static bool near(double a,double b) { return std::abs(a-b)<1e-5; }
static double segment_distance(Point p,Point a,Point b) {
    double dx=b.x-a.x,dy=b.y-a.y,l=dx*dx+dy*dy;
    double t=l ? std::clamp(((p.x-a.x)*dx+(p.y-a.y)*dy)/l,0.0,1.0) : 0;
    return distance(p,mix(a,b,t));
}
int main() {
    assert(mesh({}).empty());
    auto invalid=std::numeric_limits<double>::quiet_NaN();
    auto dot=smooth({{1.25,2.75,5},{1.25,2.75,5},{invalid,0,5},{0,0,-1}});
    assert(dot.size()==1 && !mesh(dot).empty());
    std::vector<Point> line{{.25,.75,2},{10.25,.75,3},{20.25,.75,5}};
    auto result=smooth(line,2);
    assert(result.size()==3 && near(result.front().x,.25) && near(result.back().x,20.25));
    for(auto p:result) assert(near(p.y,.75));
    for (auto sharp : {std::vector<Point>{{0,0,5},{20,0,5},{20,20,5}},
                       std::vector<Point>{{0,0,5},{20,0,5},{0,0,5}}}) {
        result=smooth(sharp);
        assert(result.size()==3 && near(result[1].x,20) && near(result[1].y,0));
    }
    // Uneven samples must not overshoot or cut off endpoints; curvature sampling
    // becomes denser on Retina, and every generated point stays near actual ink.
    std::vector<Point> curve{{.25,.75,2},{1,1,2},{40,20,4},{41,21,5},{90,45,5}};
    auto normal=smooth(curve,1), retina=smooth(curve,2);
    assert(retina.size()>=normal.size());
    assert(near(retina.front().x,curve.front().x) && near(retina.back().x,curve.back().x));
    assert(near(retina.back().width,5));
    for(auto p:retina) {
        double closest=1e10;
        for(size_t i=1;i<curve.size();++i) closest=std::min(closest,segment_distance(p,curve[i-1],curve[i]));
        assert(closest<=.651 && p.width>=2 && p.width<=5);
    }
    auto vertices=mesh({{0,0,5},{0,0,5},{20,0,5}},2);
    assert(vertices.size()%6==0);
    double xmin=1e10,xmax=-1e10,ymin=1e10,ymax=-1e10;
    for(size_t i=0;i<vertices.size();i+=2) {
        assert(std::isfinite(vertices[i]) && std::isfinite(vertices[i+1]));
        xmin=std::min(xmin,double(vertices[i]));xmax=std::max(xmax,double(vertices[i]));
        ymin=std::min(ymin,double(vertices[i+1]));ymax=std::max(ymax,double(vertices[i+1]));
    }
    assert(near(xmin,-2.5) && near(xmax,22.5) && near(ymin,-2.5) && near(ymax,2.5));
    // Large samples, subpixel widths, acute turns and variable width never
    // produce zero-length normals or non-finite vertices.
    for(auto points : {curve, std::vector<Point>{{0,0,.1},{1e-6,0,.1},{.2,.3,20},{-.1,.1,.1}}})
        for(auto v:mesh(smooth(points,2),2)) assert(std::isfinite(v));
    std::cout << "PASS: bounded smooth curves, corners, endpoints, subpixels, round caps and degenerate input\n";
}
