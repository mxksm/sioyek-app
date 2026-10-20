#pragma once
#include <QWidget>
#include <QPainter>
#include <QPainterPath>
#include <QElapsedTimer>
#include <QTimer>
#include <functional>
#include <array>
#include "laser_trail.h"
#include "pen_geometry.h"

// Transient presentation overlay: never enters Document or its undo/save history.
class LaserPointerOverlay : public QWidget {
public:
    struct Stroke { std::vector<QPointF> points; qint64 released = -1; };
    std::vector<Stroke> strokes;
    std::function<QPointF(QPointF)> project;
    QElapsedTimer clock;
    QTimer animation;
    explicit LaserPointerOverlay(QWidget* parent) : QWidget(parent) {
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setAttribute(Qt::WA_NoSystemBackground);
        setFocusPolicy(Qt::NoFocus);
        clock.start();
        animation.setInterval(16);
        animation.setTimerType(Qt::PreciseTimer);
        connect(&animation, &QTimer::timeout, this, [this] {
            const auto now = clock.elapsed();
            strokes.erase(std::remove_if(strokes.begin(), strokes.end(), [now](const Stroke& s) {
                return s.released >= 0 && now-s.released >= laser_trail::lifetime_ms;
            }), strokes.end());
            if (strokes.empty()) { animation.stop(); hide(); }
            else update();
        });
        hide();
    }
    bool drawing() const { return !strokes.empty() && strokes.back().released < 0; }
    void begin(QPointF p) {
        release();
        strokes.push_back({{p}, -1});
        setGeometry(parentWidget()->rect());
        show(); raise(); animation.start(); update();
    }
    void append(QPointF p) {
        if (!drawing()) return;
        if (strokes.back().points.back() != p) strokes.back().points.push_back(p);
        update();
    }
    void release() {
        if (drawing()) { strokes.back().released = clock.elapsed(); update(); }
    }
    void clear() { strokes.clear(); animation.stop(); hide(); update(); }
protected:
    void paintEvent(QPaintEvent*) override {
        if (!project) return;
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        const auto now = clock.elapsed();
        for (const auto& stroke : strokes) {
            std::vector<pen_geometry::Point> points;
            for (auto p : stroke.points) {
                auto screen = project(p);
                points.push_back({screen.x(), screen.y(), 3});
            }
            points = pen_geometry::smooth(points, devicePixelRatioF());
            if (points.empty()) continue;
            double length = 0;
            for (size_t i=1; i<points.size(); ++i) length += pen_geometry::distance(points[i-1],points[i]);
            const double elapsed = stroke.released < 0 ? -1 : now-stroke.released;
            std::array<QPainterPath,32> paths;
            double distance = 0;
            // Short segments give even fading even when input samples are far apart.
            for (size_t i=1; i<points.size(); ++i) {
                auto a=points[i-1], b=points[i];
                double segment=pen_geometry::distance(a,b);
                int pieces=std::max(1,int(std::ceil(segment/4)));
                for (int j=0; j<pieces; ++j) {
                    double alpha=laser_trail::opacity((distance+segment*(j+.5)/pieces)/std::max(1.0,length),elapsed);
                    if (alpha<=0) continue;
                    int bucket=std::clamp(int(std::ceil(alpha*32))-1,0,31);
                    auto from=pen_geometry::mix(a,b,double(j)/pieces), to=pen_geometry::mix(a,b,double(j+1)/pieces);
                    paths[bucket].moveTo(from.x,from.y); paths[bucket].lineTo(to.x,to.y);
                }
                distance+=segment;
            }
            if (points.size()==1) {
                double alpha=laser_trail::opacity(0,elapsed);
                if (alpha>0) {
                    auto& path=paths[std::clamp(int(std::ceil(alpha*32))-1,0,31)];
                    path.moveTo(points[0].x,points[0].y);
                    path.lineTo(points[0].x+.01,points[0].y);
                }
            }
            for (auto layer : {std::pair<double,double>{16,.07},{10,.15},{6,.35},{3,1},{1,.95}}) {
                for (int i=0; i<32; ++i) {
                    if (paths[i].isEmpty()) continue;
                    QColor color = layer.first==1 ? QColor(255,205,205) : QColor(255,35,45);
                    color.setAlphaF(layer.second*(i+1)/32);
                    painter.setPen(QPen(color,layer.first,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));
                    painter.drawPath(paths[i]);
                }
            }
        }
    }
};
