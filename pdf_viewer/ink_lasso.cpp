#include "ink_lasso.h"
#include "main_widget.h"
#include "document_view.h"
#include "document.h"
#include "pdf_view_opengl_widget.h"
extern std::vector<MainWidget*> windows;
#include <QPainter>
#include <QPainterPath>
#include <QLineF>
#include <QKeyEvent>
#include <memory>
#include <set>
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
void InkLasso::clear() {if(awaiting_register_)pane_->set_status_message(L"");awaiting_register_=false;register_='"';selected_.clear();polygon_.clear();drag_=Drag::None;transform_.reset();update();}
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
namespace {
// Shared across panes/windows, independent of document lifetimes. Named and unnamed
// registers share storage; replacing a register releases its old strokes.
using InkClipboard = std::vector<FreehandDrawing>;
std::map<char,std::shared_ptr<const InkClipboard>> ink_registers;
size_t ink_bytes(const InkClipboard& strokes) {
    size_t bytes=0;
    for (const auto& stroke : strokes) bytes+=sizeof(stroke)+stroke.points.size()*sizeof(FreehandDrawingPoint);
    return bytes;
}
}
void InkLasso::changed() {
    pane_->doc()->persist_drawings();
    for (auto window : windows) {
        auto refresh=[&](MainWidget* view) {
            if (view && view->doc()==pane_->doc()) {
                if (view->ink_lasso_ && view->ink_lasso_!=this) view->ink_lasso_->clear();
                view->invalidate_render();
            }
        };
        refresh(window); refresh(window->split_pane_.data());
    }
    pane_->invalidate_render(); update();
}
bool InkLasso::yank(bool cut, char reg) {
    if (!pane_->doc() || drag_!=Drag::None || (reg!='"' && (reg<'a' || reg>'z'))) return false;
    auto copied=std::make_shared<InkClipboard>();
    if (!pane_->doc()->copy_ink(selected_,copied.get())) {
        pane_->set_status_message(L"Lasso: select ink first"); return false;
    }
    size_t bytes=ink_bytes(*copied);
    std::set<const InkClipboard*> counted;
    for (const auto& [name,strokes] : ink_registers) {
        if (name!='"' && name!=reg && counted.insert(strokes.get()).second) bytes+=ink_bytes(*strokes);
    }
    if (bytes>32*1024*1024) {
        pane_->set_status_message(L"Ink registers are full (32 MiB); reuse a named register or select less ink"); return false;
    }
    if (cut && !pane_->doc()->replace_ink(selected_,{})) return false;
    ink_registers[reg]=copied; ink_registers['"']=std::move(copied);
    if (cut) {clear();changed();}
    pane_->set_status_message(cut?L"Ink cut":L"Ink yanked");
    return true;
}
bool InkLasso::paste(QPointF center, char reg) {
    if (!pane_->doc() || pane_->is_rotated() || drag_!=Drag::None) return false;
    auto it=ink_registers.find(reg);
    if (it==ink_registers.end()) {pane_->set_status_message(L"Ink register is empty");return false;}
    auto strokes=*it->second;
    QRectF bounds; bool first=true;
    for (const auto& stroke : strokes) for (const auto& point : stroke.points) {
        QRectF r(point.pos.x-point.thickness/2,point.pos.y-point.thickness/2,point.thickness,point.thickness);
        bounds=first?r:bounds.united(r);first=false;
    }
    const QPointF delta=center-bounds.center();
    auto time=QDateTime::currentDateTime();
    for (auto& stroke : strokes) {
        stroke.creattion_time=time; time=time.addMSecs(1);
        for (auto& point : stroke.points) {point.pos.x+=delta.x();point.pos.y+=delta.y();}
    }
    std::vector<InkSelectionItem> pasted;
    if (!pane_->doc()->replace_ink({},strokes,&pasted)) return false;
    selected_=std::move(pasted);update_bounds();changed();
    pane_->set_status_message(L"Ink pasted; drag to move or scale");
    return true;
}
bool InkLasso::handle_key(QKeyEvent* event) {
    const auto mods=event->modifiers();
    if (mods & (Qt::ControlModifier|Qt::MetaModifier|Qt::AltModifier)) {
        if(awaiting_register_)pane_->set_status_message(L"");
        awaiting_register_=false;register_='"';return false;
    }
    if (event->key()==Qt::Key_Escape && (awaiting_register_ || register_!='"')) {
        awaiting_register_=false;register_='"';pane_->set_status_message(L"");return true;
    }
    if (event->text()=="\"") {
        awaiting_register_=true;pane_->set_status_message(L"Ink register: a-z", 0);return true;
    }
    if (awaiting_register_) {
        pane_->set_status_message(L"");
        awaiting_register_=false;
        const auto text=event->text();
        register_=(text.size()==1 && text[0]>='a' && text[0]<='z')?text[0].toLatin1():'"';
        return true;
    }
    const char reg=register_;register_='"';
    if (mods & Qt::ShiftModifier) return false;
    const int key=event->key();
    if (key!=Qt::Key_Y && key!=Qt::Key_D && key!=Qt::Key_P) return false;
    // Holding an operator must not create many pastes or cut the following selection.
    if (event->isAutoRepeat()) return true;
    if (key==Qt::Key_P) {
        const QPointF center=pane_->central_widget->mapTo(pane_,pane_->central_widget->rect().center());
        paste(pane_->laser_position(center),reg);
    } else yank(key==Qt::Key_D,reg);
    return true;
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
