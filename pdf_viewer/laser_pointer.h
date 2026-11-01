#pragma once
#include <QWidget>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QImage>
#include <QElapsedTimer>
#include <QTimer>
#include <functional>
#include <map>
#include <optional>
#include <vector>
#include <cmath>
#include "laser_trail.h"
#include "pen_geometry.h"

extern int LASER_POINTER_DURATION_MS;

// Transient presentation overlay: never enters Document or its undo/save history.
class LaserPointerOverlay : public QWidget {
    static constexpr int tile_size = 128; // logical pixels; storage is limited to visible tiles
    struct FadePiece {
        QPainterPath path;
        double fraction;
    };
    struct Tile {
        QImage coverage, ink, faded;
        std::vector<FadePiece> fade_pieces;
        explicit Tile(qreal ratio) {
            const int pixels = int(std::ceil(tile_size*ratio));
            ink = QImage(pixels,pixels,QImage::Format_ARGB32_Premultiplied);
            coverage = QImage(pixels,pixels,QImage::Format_RGB32);
            ink.setDevicePixelRatio(ratio);
            coverage.setDevicePixelRatio(ratio);
            ink.fill(Qt::transparent);
            coverage.fill(Qt::black);
        }
    };
public:
    struct Stroke {
        std::vector<QPointF> points;
        qint64 released = -1;
        int duration_ms = 1450;
        bool fade_prepared = false;
        std::map<std::pair<int,int>,Tile> tiles;
        // Only the live tip is provisional; older curve pieces are immutable.
        std::map<std::pair<int,int>,std::optional<Tile>> tip_backup;
        size_t rasterized = 0;
        double length = 0, committed_length = 0;
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
                return s.released >= 0 && now-s.released >= s.duration_ms;
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
        auto& points=strokes.back().points;
        points.push_back(p);
        if (project) {
            // The new sample rounds the preceding corner and replaces its tip.
            QRectF dirty(project(p),QSizeF());
            for (size_t i=points.size()>3?points.size()-3:0;i<points.size();++i)
                dirty=dirty.united(QRectF(project(points[i]),QSizeF(.001,.001)));
            update(dirty.adjusted(-10,-10,10,10).toAlignedRect());
        }
    }
    void release() {
        if (drawing()) {
            strokes.back().duration_ms = std::clamp(LASER_POINTER_DURATION_MS, 100, 60000);
            strokes.back().released = clock.elapsed();
            animation.start(); update();
        }
    }
    void clear() { strokes.clear(); animation.stop(); hide(); update(); }
private:
    QPointF cached_origin, cached_x, cached_y;
    QSize cached_size;
    qreal cached_ratio = 0;

    void rasterize(Stroke& stroke, QPointF from, QPointF control, QPointF to,
                   qreal ratio, bool provisional=false) {
        QPainterPath path(from);
        path.quadTo(control,to);
        // Use the pen's subpixel curve subdivision for arc length and fade order.
        // The visible glow is stroked as a native quadratic, not separate lines.
        std::vector<pen_geometry::Point> curve{{from.x(),from.y(),1}};
        pen_geometry::quadratic(curve.front(),{control.x(),control.y(),1},
            {to.x(),to.y(),1},.15/std::max(1.0,ratio),curve);
        double segment=0;
        for (size_t i=1;i<curve.size();++i) segment+=pen_geometry::distance(curve[i-1],curve[i]);
        stroke.length+=segment;
        const QRect area=path.controlPointRect().adjusted(-10,-10,10,10).toAlignedRect().intersected(rect());
        if (area.isEmpty()) return;
        // Held strokes keep the incremental raster cache; fading is prepared
        // separately after release so self-crossings retain independent ages.
        for (int y=area.top()/tile_size;y<=area.bottom()/tile_size;++y) {
            for (int x=area.left()/tile_size;x<=area.right()/tile_size;++x) {
                const auto key=std::make_pair(x,y);
                if (provisional) {
                    auto existing=stroke.tiles.find(key);
                    stroke.tip_backup.emplace(key,existing==stroke.tiles.end()
                        ? std::nullopt : std::optional<Tile>(existing->second));
                }
                auto& tile=stroke.tiles.try_emplace(key,ratio).first->second;
                const QPointF origin(x*tile_size,y*tile_size);
                QPainter coverage(&tile.coverage);
                coverage.setRenderHint(QPainter::Antialiasing);
                coverage.translate(-origin);
                // Two coverage masks share an opaque image: red = glow, blue =
                // bright core. Lighten unions their coverage without repeatedly
                // alpha-blending round caps into bright beads at sample positions.
                coverage.setCompositionMode(QPainter::CompositionMode_Lighten);
                for (auto layer : {std::pair<double,int>{16,18},{10,54},{6,124},{3,255},{1,242}}) {
                    QColor mask=layer.first==1 ? QColor(0,0,layer.second) : QColor(layer.second,0,0);
                    coverage.setPen(QPen(mask,layer.first,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));
                    if (from==to && from==control) coverage.drawPoint(to); else coverage.drawPath(path);
                }
                coverage.end();
                const QRect local=area.translated(-x*tile_size,-y*tile_size);
                const QRect pixels=QRectF(local.x()*ratio,local.y()*ratio,
                    local.width()*ratio,local.height()*ratio).toAlignedRect().intersected(tile.ink.rect());
                for (int row=pixels.top();row<=pixels.bottom();++row) {
                    const auto* mask=reinterpret_cast<const QRgb*>(tile.coverage.constScanLine(row));
                    auto* ink=reinterpret_cast<QRgb*>(tile.ink.scanLine(row));
                    for (int col=pixels.left();col<=pixels.right();++col) {
                        const int core=qBlue(mask[col]), glow=qRed(mask[col])*(255-core)/255;
                        ink[col]=qRgba(core+glow,(205*core+35*glow)/255,
                            (205*core+45*glow)/255,core+glow);
                    }
                }

            }
        }
    }
    void refresh_cache() {
        const auto origin=project({0,0}), x=project({1,0}), y=project({0,1});
        const qreal ratio=devicePixelRatioF();
        if (origin!=cached_origin || x!=cached_x || y!=cached_y || size()!=cached_size || ratio!=cached_ratio) {
            for (auto& stroke:strokes) {
                stroke.tiles.clear(); stroke.tip_backup.clear();
                stroke.fade_prepared=false;
                stroke.rasterized=0; stroke.length=stroke.committed_length=0;
            }
            cached_origin=origin; cached_x=x; cached_y=y; cached_size=size(); cached_ratio=ratio;
        }
        for (auto& stroke:strokes) {
            if (stroke.rasterized==stroke.points.size()) continue;
            for (auto& old:stroke.tip_backup) {
                if (old.second) stroke.tiles.insert_or_assign(old.first,std::move(*old.second));
                else stroke.tiles.erase(old.first);
            }
            stroke.tip_backup.clear();
            stroke.length=stroke.committed_length;
            // Only new samples are projected and rasterized during ordinary drawing.
            for (size_t i=stroke.rasterized;i<stroke.points.size();++i) {
                ++rasterized_segments;
                const QPointF screen=project(stroke.points[i]);
                if (i==0) rasterize(stroke,screen,screen,screen,ratio);
                else {
                    const QPointF previous=project(stroke.points[i-1]);
                    const QPointF from=i==1 ? previous : (project(stroke.points[i-2])+previous)*.5;
                    rasterize(stroke,from,previous,(previous+screen)*.5,ratio);
                }
            }
            stroke.rasterized=stroke.points.size();
            stroke.committed_length=stroke.length;
            if (stroke.points.size()>1) {
                const QPointF end=project(stroke.points.back());
                const QPointF from=(project(stroke.points[stroke.points.size()-2])+end)*.5;
                // Reach the cursor immediately. Replace only this half-segment
                // when a new sample supplies the next curve's tangent.
                rasterize(stroke,from,(from+end)*.5,end,ratio,true);
            }
        }
    }
    void prepare_fade(Stroke& stroke) {
        if (stroke.fade_prepared) return;
        // Split exact quadratics into short pieces only once after release.
        // Opacity is applied to each piece BEFORE coverage is combined, so a
        // later crossing cannot prolong an earlier line's core or glow.
        double distance = 0;
        auto curve = [&](QPointF from, QPointF control, QPointF to) {
            std::vector<pen_geometry::Point> flattened{{from.x(),from.y(),1}};
            pen_geometry::quadratic(flattened.front(),{control.x(),control.y(),1},
                {to.x(),to.y(),1},.15/std::max(1.0,cached_ratio),flattened);
            double length = 0;
            for (size_t i=1;i<flattened.size();++i)
                length += pen_geometry::distance(flattened[i-1],flattened[i]);
            QPainterPath whole(from); whole.quadTo(control,to);
            if (!whole.controlPointRect().adjusted(-10,-10,10,10).intersects(rect())) {
                distance += length; return;
            }
            const int count = std::max(1,int(std::ceil(length/4)));
            auto point = [&](double t) { return from*((1-t)*(1-t)) + control*(2*t*(1-t)) + to*(t*t); };
            std::vector<double> arc(count+1,0);
            for (int i=0;i<count;++i) {
                const auto delta=point(double(i+1)/count)-point(double(i)/count);
                arc[i+1]=arc[i]+std::hypot(delta.x(),delta.y());
            }
            for (int i=0;i<count;++i) {
                const double t0=double(i)/count, t1=double(i+1)/count;
                const QPointF a=point(t0), b=point(t1);
                const QPointF c=a+((control-from)*(1-t0)+(to-control)*t0)*(t1-t0);
                QPainterPath path(a); path.quadTo(c,b);
                const QRect area=path.controlPointRect().adjusted(-10,-10,10,10).toAlignedRect().intersected(rect());
                if (area.isEmpty()) continue;
                const double fraction=(distance+length*(arc[i]+arc[i+1])*.5/std::max(1e-9,arc.back()))
                    /std::max(1.0,stroke.length);
                for (int y=area.top()/tile_size;y<=area.bottom()/tile_size;++y)
                    for (int x=area.left()/tile_size;x<=area.right()/tile_size;++x) {
                        auto tile=stroke.tiles.find({x,y});
                        if (tile!=stroke.tiles.end()) tile->second.fade_pieces.push_back({path,fraction});
                    }
            }
            distance += length;
        };
        for (size_t i=0;i<stroke.points.size();++i) {
            const QPointF screen=project(stroke.points[i]);
            if (i==0) curve(screen,screen,screen);
            else {
                const QPointF previous=project(stroke.points[i-1]);
                const QPointF from=i==1 ? previous : (project(stroke.points[i-2])+previous)*.5;
                curve(from,previous,(previous+screen)*.5);
            }
        }
        if (stroke.points.size()>1) {
            const QPointF end=project(stroke.points.back());
            const QPointF from=(project(stroke.points[stroke.points.size()-2])+end)*.5;
            curve(from,(from+end)*.5,end);
        }
        stroke.fade_prepared=true;
    }
protected:
    void paintEvent(QPaintEvent* event) override {
        if (!project) return;
        refresh_cache();
        QPainter painter(this);
        const auto now=clock.elapsed();
        for (auto& stroke:strokes) {
            const double elapsed=stroke.released<0 ? -1 : now-stroke.released;
            const bool fading=elapsed*laser_trail::lifetime_ms/stroke.duration_ms>250;
            if (fading) prepare_fade(stroke);
            for (auto& entry:stroke.tiles) {
                const QPoint origin(entry.first.first*tile_size,entry.first.second*tile_size);
                if (!event->region().intersects(QRect(origin,QSize(tile_size,tile_size)))) continue;
                auto& tile=entry.second;
                if (!fading) { painter.drawImage(origin,tile.ink); continue; }
                tile.coverage.fill(Qt::black);
                QPainter coverage(&tile.coverage);
                coverage.setRenderHint(QPainter::Antialiasing);
                coverage.setCompositionMode(QPainter::CompositionMode_Lighten);
                coverage.translate(-origin);
                for (const auto& piece:tile.fade_pieces) {
                    const double opacity=laser_trail::opacity(piece.fraction,elapsed,stroke.duration_ms);
                    if (opacity<=0) continue;
                    for (auto layer : {std::pair<double,int>{16,18},{10,54},{6,124},{3,255},{1,242}}) {
                        const int value=int(layer.second*opacity+.5);
                        const QColor mask=layer.first==1 ? QColor(0,0,value) : QColor(value,0,0);
                        coverage.setPen(QPen(mask,layer.first,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));
                        if (piece.path.elementCount()==1) coverage.drawPoint(piece.path.currentPosition());
                        else coverage.drawPath(piece.path);
                    }
                }
                coverage.end();
                if (tile.faded.isNull()) {
                    tile.faded=QImage(tile.ink.size(),QImage::Format_ARGB32_Premultiplied);
                    tile.faded.setDevicePixelRatio(cached_ratio);
                }
                for (int y=0;y<tile.ink.height();++y) {
                    const auto* mask=reinterpret_cast<const QRgb*>(tile.coverage.constScanLine(y));
                    auto* output=reinterpret_cast<QRgb*>(tile.faded.scanLine(y));
                    for (int x=0;x<tile.ink.width();++x) {
                        const int core=qBlue(mask[x]), glow=qRed(mask[x])*(255-core)/255;
                        output[x]=qRgba(core+glow,(205*core+35*glow)/255,
                            (205*core+45*glow)/255,core+glow);
                    }
                }
                painter.drawImage(origin,tile.faded);
            }
        }
    }
};
