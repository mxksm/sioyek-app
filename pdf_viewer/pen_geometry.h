#pragma once
#include <algorithm>
#include <cmath>
#include <vector>

// Display geometry only: saved ink and eraser/undo history stay untouched.
// Coordinates and widths are in logical screen pixels.
namespace pen_geometry {
struct Point { double x, y, width; };
inline double distance(Point a, Point b) { return std::hypot(a.x-b.x, a.y-b.y); }
inline Point mix(Point a, Point b, double t) {
    return {a.x+(b.x-a.x)*t, a.y+(b.y-a.y)*t, a.width+(b.width-a.width)*t};
}
inline std::vector<Point> clean(const std::vector<Point>& input) {
    std::vector<Point> out;
    out.reserve(input.size());
    for (auto p : input) {
        if (!std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.width) || p.width <= 0) continue;
        if (!out.empty() && distance(out.back(), p) < 1e-5) out.back().width = p.width;
        else out.push_back(p);
    }
    return out;
}
inline void quadratic(Point a, Point control, Point b, double tolerance,
                      std::vector<Point>& out, int depth = 0) {
    auto chord_mid = mix(a, b, .5);
    auto curve_mid = mix(mix(a, control, .5), mix(control, b, .5), .5);
    if (depth >= 8 || distance(chord_mid, curve_mid) <= tolerance) {
        out.push_back(b);
        return;
    }
    auto ac = mix(a, control, .5), cb = mix(control, b, .5);
    quadratic(a, ac, curve_mid, tolerance, out, depth+1);
    quadratic(curve_mid, cb, b, tolerance, out, depth+1);
}
inline std::vector<Point> smooth(const std::vector<Point>& input, double pixel_ratio = 1) {
    auto points = clean(input);
    if (points.size() < 3) return points;
    std::vector<Point> out{points.front()};
    out.reserve(points.size()*3);
    for (size_t i=1; i+1<points.size(); ++i) {
        auto a=points[i-1], p=points[i], b=points[i+1];
        double before=distance(a,p), after=distance(p,b);
        double dot=((p.x-a.x)*(b.x-p.x)+(p.y-a.y)*(b.y-p.y))/(before*after);
        dot=std::clamp(dot, -1.0, 1.0);
        // Preserve deliberate corners and reversals instead of rounding letters
        // or snapping a stroke to an inferred line/shape.
        if (dot < .2 || dot > .99999) { out.push_back(p); continue; }
        double sin_half=std::sqrt((1-dot)*.5);
        double deviation=std::min(.65, p.width*.25);
        double cut=std::min({before*.5, after*.5, 2*deviation/sin_half});
        auto entry=mix(p,a,cut/before), leave=mix(p,b,cut/after);
        if (distance(out.back(),entry)>1e-5) out.push_back(entry);
        quadratic(entry,p,leave,.15/std::max(1.0,pixel_ratio),out);
    }
    out.push_back(points.back());
    return clean(out);
}

// Union of segment quads and round joins/caps. Drawing with GL_LESS at a
// common depth covers each sample once, including translucent overlapping joins.
inline std::vector<float> mesh(const std::vector<Point>& input, double pixel_ratio = 1) {
    auto points=clean(input);
    std::vector<float> vertices;
    vertices.reserve(points.size()*90);
    auto vertex=[&](double x,double y) { vertices.push_back(float(x)); vertices.push_back(float(y)); };
    for (size_t i=0; i<points.size(); ++i) {
        auto p=points[i];
        double radius=p.width*.5;
        double physical_radius=radius*std::max(1.0,pixel_ratio);
        int sides=std::clamp(int(std::ceil(3.141592653589793 /
            std::acos(std::clamp(1-.15/std::max(.15,physical_radius), -1.0,1.0)))),12,96);
        sides = (sides+3)/4*4;
        for (int j=0; j<sides; ++j) {
            double a=6.283185307179586*j/sides, b=6.283185307179586*(j+1)/sides;
            vertex(p.x,p.y);
            vertex(p.x+radius*std::cos(a),p.y+radius*std::sin(a));
            vertex(p.x+radius*std::cos(b),p.y+radius*std::sin(b));
        }
        if (i+1 == points.size()) continue;
        auto q=points[i+1];
        double length=distance(p,q), nx=-(q.y-p.y)/length, ny=(q.x-p.x)/length;
        double r=q.width*.5;
        Point a{p.x+nx*radius,p.y+ny*radius,0}, b{p.x-nx*radius,p.y-ny*radius,0};
        Point c{q.x+nx*r,q.y+ny*r,0}, d{q.x-nx*r,q.y-ny*r,0};
        for (auto v : {a,b,c,b,d,c}) vertex(v.x,v.y);
    }
    return vertices;
}
}
