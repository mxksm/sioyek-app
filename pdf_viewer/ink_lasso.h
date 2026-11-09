#pragma once
#include <QWidget>
#include <QPolygonF>
#include <QTransform>
#include "book.h"
class MainWidget;
class InkLasso : public QWidget {
    MainWidget* pane_;
    QPolygonF polygon_;
    std::vector<InkSelectionItem> selected_;
    QRectF bounds_;
    QPointF start_, anchor_;
    QTransform transform_;
    enum class Drag { None, Select, Move, Scale } drag_ = Drag::None;
    QPointF window_point(QPointF point) const;
    void update_bounds();
public:
    explicit InkLasso(MainWidget* pane);
    void press(QPointF point);
    void move(QPointF point);
    void release(QPointF point);
    void clear();
    bool cancel_drag();
    void paintEvent(QPaintEvent*) override;
    bool has_selection() const { return !selected_.empty(); }
};
