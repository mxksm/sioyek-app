#include "ink_lasso.h"
#include "main_widget.h"
#include "document_view.h"
#include "document.h"
#include "pdf_view_opengl_widget.h"
extern std::vector<MainWidget*> windows;
#include <QPainter>
#include <QPainterPath>
#include <QLineF>
#include <algorithm>
#include <cmath>
InkLasso::InkLasso(MainWidget* pane) : QWidget(pane->central_widget),pane_(pane) {
    setAttribute(Qt::WA_TransparentForMouseEvents);setAttribute(Qt::WA_NoSystemBackground);setFocusPolicy(Qt::NoFocus);
    setObjectName("inkLasso");
}
QPointF InkLasso::window_point(QPointF point) const {
    auto p=AbsoluteDocumentPos{float(point.x()),float(point.y())}.to_window(pane_->main_document_view);
    return QPointF(p.x,p.y);
}
void InkLasso::clear() {selected_.clear();polygon_.clear();drag_=Drag::None;transform_.reset();update();}
bool InkLasso::cancel_drag() {if(drag_==Drag::None)return false;drag_=Drag::None;polygon_.clear();transform_.reset();update();return true;}
void InkLasso::update_bounds() {
    bool first=true;
    for(const auto& item:selected_) for(const auto& point:item.drawing.points) {
        const QRectF rect(point.pos.x-point.thickness/2,point.pos.y-point.thickness/2,point.thickness,point.thickness);
        bounds_=first?rect:bounds_.united(rect);first=false;
    }
}
void InkLasso::press(QPointF point) {
    transform_.reset();start_=point;
    if(!selected_.empty()) {
        const QPointF corners[]={bounds_.topLeft(),bounds_.topRight(),bounds_.bottomRight(),bounds_.bottomLeft()};
        for(int i=0;i<4;++i) if(QLineF(window_point(corners[i]),window_point(point)).length()<12) {
            anchor_=corners[(i+2)%4];start_=corners[i];drag_=Drag::Scale;return;
        }
        if(bounds_.contains(point)) {drag_=Drag::Move;return;}
    }
    selected_.clear();polygon_.clear();polygon_<<point;drag_=Drag::Select;update();
}
void InkLasso::move(QPointF point) {
    if(drag_==Drag::Select) {
        if(polygon_.isEmpty() || QLineF(window_point(polygon_.back()),window_point(point)).length()>=2) polygon_<<point;
    } else if(drag_==Drag::Move) transform_=QTransform::fromTranslate(point.x()-start_.x(),point.y()-start_.y());
    else if(drag_==Drag::Scale) {
        const QPointF a=start_-anchor_,b=point-anchor_;
        const double denom=a.x()*a.x()+a.y()*a.y();
        const double scale=std::clamp(denom>0?(a.x()*b.x()+a.y()*b.y())/denom:1.0,0.05,20.0);
        transform_.reset();transform_.translate(anchor_.x(),anchor_.y());transform_.scale(scale,scale);transform_.translate(-anchor_.x(),-anchor_.y());
    }
    update();
}
void InkLasso::release(QPointF point) {
    move(point);
    if(drag_==Drag::Select && pane_->doc()) {
        selected_=pane_->doc()->select_ink(polygon_,pane_->opengl_widget->visible_drawing_mask);
        update_bounds();
    } else if((drag_==Drag::Move || drag_==Drag::Scale) && pane_->doc() && !transform_.isIdentity()) {
        std::vector<InkSelectionItem> updated;
        if(pane_->doc()->transform_ink(selected_,transform_,&updated)) {
            selected_=std::move(updated);update_bounds();pane_->doc()->persist_drawings();
            for(auto window:windows) if(window->doc()==pane_->doc()) window->invalidate_render();
        } else clear();
    }
    polygon_.clear();drag_=Drag::None;transform_.reset();pane_->invalidate_render();update();
}
void InkLasso::paintEvent(QPaintEvent*) {
    QPainter p(this);p.setRenderHint(QPainter::Antialiasing);
    QColor color=palette().color(QPalette::Highlight);if(color.lightness()<70)color=QColor(90,150,220);
    p.setPen(QPen(color,1.5,Qt::DashLine));p.setBrush(Qt::NoBrush);
    if(drag_==Drag::Select) {
        QPolygonF screen;for(auto point:polygon_)screen<<window_point(point);p.drawPolyline(screen);
        if(screen.size()>2) {QColor fill=color;fill.setAlpha(25);p.setBrush(fill);p.drawPolygon(screen);}
    }
    if(!selected_.empty()) {
        const auto rect=transform_.mapRect(bounds_);
        const QRectF screen(window_point(rect.topLeft()),window_point(rect.bottomRight()));
        p.drawRect(screen);p.setBrush(color);
        for(auto corner:{screen.topLeft(),screen.topRight(),screen.bottomLeft(),screen.bottomRight()})p.drawRect(QRectF(corner-QPointF(4,4),QSizeF(8,8)));
        if(drag_==Drag::Move || drag_==Drag::Scale) {
            p.setPen(QPen(color,2,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));p.setBrush(Qt::NoBrush);
            for(const auto& item:selected_) {
                QPolygonF points;
                for(const auto& point:item.drawing.points) points<<window_point(transform_.map(QPointF(point.pos.x,point.pos.y)));
                if(points.size()==1)p.drawEllipse(points.front(),2,2);else p.drawPolyline(points);
            }
        }
    }
}
