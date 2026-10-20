#pragma once
#include <QWidget>
#include <QPainter>
#include <QPaintEvent>
#include <QImage>
#include <QElapsedTimer>
#include <QTimer>
#include <functional>
#include <map>
#include <array>
#include <vector>
#include <cmath>
#include "laser_trail.h"

// Transient presentation overlay: never enters Document or its undo/save history.
class LaserPointerOverlay : public QWidget {
    static constexpr int tile_size = 128; // logical pixels; storage is limited to visible tiles
    struct Tile {
        QImage ink, order, faded;
        explicit Tile(qreal ratio) {
            const int pixels = int(std::ceil(tile_size*ratio));
            ink = QImage(pixels,pixels,QImage::Format_ARGB32_Premultiplied);
            order = QImage(pixels,pixels,QImage::Format_RGB32);
            ink.setDevicePixelRatio(ratio); order.setDevicePixelRatio(ratio);
            ink.fill(Qt::transparent); order.fill(Qt::black);
        }
    };
public:
    struct Stroke {
        std::vector<QPointF> points;
        qint64 released = -1;
        std::map<std::pair<int,int>,Tile> tiles;
        size_t rasterized = 0;
        double length = 0;
        QPointF last_screen;
    };
    std::vector<Stroke> strokes;
    std::function<QPointF(QPointF)> project;
    QElapsedTimer clock;
    QTimer animation;
    // Useful for regression checks: held frames must not rerasterize old segments.
    size_t rasterized_segments = 0;
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
            if (std::none_of(strokes.begin(),strokes.end(),[](const Stroke& s){return s.released>=0;}))
                animation.stop();
            if (strokes.empty()) hide();
            update();
        });
        hide();
    }
    bool drawing() const { return !strokes.empty() && strokes.back().released < 0; }
    void begin(QPointF p) {
        release();
        strokes.emplace_back();
        strokes.back().points.push_back(p);
        setGeometry(parentWidget()->rect());
        show(); raise(); update();
    }
    void append(QPointF p) {
        if (!drawing() || strokes.back().points.back()==p) return;
        const auto previous=strokes.back().points.back();
        strokes.back().points.push_back(p);
        if (project) update(QRectF(project(previous),project(p)).normalized().adjusted(-10,-10,10,10).toAlignedRect());
    }
    void release() {
        if (drawing()) {
            strokes.back().released = clock.elapsed();
            animation.start(); update();
        }
    }
    void clear() { strokes.clear(); animation.stop(); hide(); update(); }
private:
    QPointF cached_origin, cached_x, cached_y;
    QSize cached_size;
    qreal cached_ratio = 0;

    void rasterize(Stroke& stroke, QPointF from, QPointF to, qreal ratio) {
        ++rasterized_segments;
        const double segment=std::hypot(to.x()-from.x(),to.y()-from.y());
        const double start=stroke.length;
        stroke.length+=segment;
        const QRect area=QRectF(from,to).normalized().adjusted(-10,-10,10,10).toAlignedRect().intersected(rect());
        if (area.isEmpty()) return;
        // Chronology is stored separately from the glow, so fading never strokes a
        // growing vector path. No antialiasing/blending may alter these integer IDs.
        for (int y=area.top()/tile_size;y<=area.bottom()/tile_size;++y) {
            for (int x=area.left()/tile_size;x<=area.right()/tile_size;++x) {
                auto& tile=stroke.tiles.try_emplace({x,y},ratio).first->second;
                const QPointF origin(x*tile_size,y*tile_size);
                QPainter ink(&tile.ink);
                ink.setRenderHint(QPainter::Antialiasing);
                ink.translate(-origin);
                for (auto layer : {std::pair<double,double>{16,.07},{10,.15},{6,.35},{3,1},{1,.95}}) {
                    QColor color=layer.first==1 ? QColor(255,205,205) : QColor(255,35,45);
                    color.setAlphaF(layer.second);
                    ink.setPen(QPen(color,layer.first,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));
                    if (from==to) ink.drawPoint(to); else ink.drawLine(from,to);
                }
                ink.end();
                QPainter order(&tile.order);
                order.translate(-origin);
                order.setCompositionMode(QPainter::CompositionMode_Source);
                const int steps=std::max(1,int(std::ceil(segment/4)));
                for (int step=0;step<steps;++step) {
                    const unsigned id=1+unsigned(std::min(16777214.0,(start+segment*(step+.5)/steps)*8));
                    order.setPen(QPen(QColor::fromRgb(0xff000000 | id),20,Qt::SolidLine,Qt::RoundCap));
                    if (from==to) order.drawPoint(to);
                    else order.drawLine(from+(to-from)*(double(step)/steps),from+(to-from)*(double(step+1)/steps));
                }
            }
        }
    }
    void refresh_cache() {
        const auto origin=project({0,0}), x=project({1,0}), y=project({0,1});
        const qreal ratio=devicePixelRatioF();
        if (origin!=cached_origin || x!=cached_x || y!=cached_y || size()!=cached_size || ratio!=cached_ratio) {
            for (auto& stroke:strokes) { stroke.tiles.clear(); stroke.rasterized=0; stroke.length=0; }
            cached_origin=origin; cached_x=x; cached_y=y; cached_size=size(); cached_ratio=ratio;
        }
        for (auto& stroke:strokes) {
            // Only new samples are projected and rasterized during ordinary drawing.
            for (size_t i=stroke.rasterized;i<stroke.points.size();++i) {
                const QPointF screen=project(stroke.points[i]);
                rasterize(stroke,i==0?screen:stroke.last_screen,screen,ratio);
                stroke.last_screen=screen;
            }
            stroke.rasterized=stroke.points.size();
        }
    }
protected:
    void paintEvent(QPaintEvent* event) override {
        if (!project) return;
        refresh_cache();
        QPainter painter(this);
        const auto now=clock.elapsed();
        for (auto& stroke:strokes) {
            std::array<unsigned,1024> alpha;
            const double elapsed=stroke.released<0 ? -1 : now-stroke.released;
            const bool fading=elapsed>250;
            const double order_scale=1023/std::max(1.0,stroke.length*8);
            if (fading) {
                for (size_t i=0;i<alpha.size();++i)
                    alpha[i]=unsigned(255*laser_trail::opacity(double(i)/1023,elapsed)+.5);
            }
            for (auto& entry:stroke.tiles) {
                const QPoint origin(entry.first.first*tile_size,entry.first.second*tile_size);
                if (!event->region().intersects(QRect(origin,QSize(tile_size,tile_size)))) continue;
                auto& tile=entry.second;
                if (!fading) { painter.drawImage(origin,tile.ink); continue; }
                if (tile.faded.isNull()) {
                    tile.faded=QImage(tile.ink.size(),QImage::Format_ARGB32_Premultiplied);
                    tile.faded.setDevicePixelRatio(cached_ratio);
                }
                for (int y=0;y<tile.ink.height();++y) {
                    const auto* ink=reinterpret_cast<const QRgb*>(tile.ink.constScanLine(y));
                    const auto* order=reinterpret_cast<const QRgb*>(tile.order.constScanLine(y));
                    auto* output=reinterpret_cast<QRgb*>(tile.faded.scanLine(y));
                    for (int x=0;x<tile.ink.width();++x) {
                        const QRgb pixel=ink[x];
                        if (!pixel) { output[x]=0; continue; }
                        const int id=int(order[x]&0xffffff)-1;
                        const unsigned a=alpha[std::clamp(int(id*order_scale),0,1023)];
                        output[x]=qRgba(qRed(pixel)*a/255,qGreen(pixel)*a/255,qBlue(pixel)*a/255,qAlpha(pixel)*a/255);
                    }
                }
                painter.drawImage(origin,tile.faded);
            }
        }
    }
};
