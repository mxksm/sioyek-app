#pragma once
#include <algorithm>
#include <cmath>

namespace brush_eraser {
struct Point { double x, y; };
inline double cross(Point a, Point b, Point c) {
    return (b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x);
}
inline double point_distance_squared(Point p, Point a, Point b) {
    double dx=b.x-a.x, dy=b.y-a.y, length=dx*dx+dy*dy;
    double t=length > 0 ? std::clamp(((p.x-a.x)*dx+(p.y-a.y)*dy)/length, 0.0, 1.0) : 0;
    double x=p.x-a.x-t*dx, y=p.y-a.y-t*dy;
    return x*x+y*y;
}
// Minimum distance between the swept brush segment and a stroke segment.
inline bool touches(Point a, Point b, Point c, Point d, double radius) {
    double ac=cross(a,b,c), ad=cross(a,b,d), ca=cross(c,d,a), cb=cross(c,d,b);
    if (((ac<0 && ad>0)||(ac>0 && ad<0)) &&
        ((ca<0 && cb>0)||(ca>0 && cb<0))) return true;
    double distance=std::min({point_distance_squared(a,c,d),point_distance_squared(b,c,d),
                             point_distance_squared(c,a,b),point_distance_squared(d,a,b)});
    return distance <= radius*radius;
}
}
