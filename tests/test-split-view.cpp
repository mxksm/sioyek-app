// Link with the real application objects, replacing only the entry point.
#define main sioyek_application_main
#include "../pdf_viewer/main.cpp"
#undef main
#include <QTemporaryDir>
#include <QEventLoop>
#include <QPointer>
#include <QPdfWriter>
#include <stdexcept>

static QString test_directory;
static int test_result = 1;
extern float MENU_SCREEN_WDITH_RATIO;
static void check(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
static void settle() {
    QEventLoop loop;
    QTimer::singleShot(120, &loop, &QEventLoop::quit);
    loop.exec();
}
static void shortcut(int key, Qt::KeyboardModifiers modifiers) {
    auto target = QApplication::focusWidget();
    check(target != nullptr, "keyboard focus exists");
    QString text;
    if (key >= Qt::Key_A && key <= Qt::Key_Z) text = QChar(key + (modifiers.testFlag(Qt::ShiftModifier) ? 0 : 32));
    if (key == Qt::Key_Less || key == Qt::Key_Greater) text = QChar(key);
    QKeyEvent press(QEvent::KeyPress, key, modifiers, text);
    QApplication::sendEvent(target, &press);
    QKeyEvent release(QEvent::KeyRelease, key, modifiers, text);
    if (QApplication::focusWidget()) QApplication::sendEvent(QApplication::focusWidget(), &release);
    settle();
}
static bool test_file_picker(MainWidget* pane, bool filesystem) {
    pane->setFocus();
    shortcut(Qt::Key_O, filesystem ? Qt::ShiftModifier | Qt::MetaModifier : Qt::ShiftModifier);
    check(!pane->current_widget_stack.empty(), "file shortcut creates a picker in the focused pane");
    auto picker=dynamic_cast<BaseSelectorWidget*>(pane->current_widget_stack.back());
    check(picker, "file shortcut creates a selector");
    check(bool(dynamic_cast<FileSelector*>(picker))==filesystem, "correct picker handles each shortcut");
    auto edit=picker->findChild<QLineEdit*>();
    check(edit && edit->hasFocus(), "picker receives keyboard focus");
    auto fits_pane=[&] {
        const QRect bounds(pane->central_widget->mapTo(pane,QPoint()),pane->central_widget->size());
        return bounds.contains(picker->geometry()) &&
            std::abs(picker->width()-int(bounds.width()*MENU_SCREEN_WDITH_RATIO))<=1;
    };
    bool fits=fits_pane();
    if (auto splitter=pane->tab_owner()->pdf_splitter_) {
        splitter->setSizes(pane->split_owner_ ? QList<int>{450,550} : QList<int>{550,450});
        settle();
        fits=fits_pane() && fits;
    }
    auto hit=pane->window()->childAt(edit->mapTo(pane->window(),edit->rect().center()));
    const bool visible=hit && (hit==edit || edit->isAncestorOf(hit));
    qInfo() << "Picker" << (filesystem?"files":"recent") << "split" << bool(pane->tab_owner()->split_pane_)
        << "right" << bool(pane->split_owner_) << "fits pane:" << fits << "above PDF:" << visible;
    const auto before=edit->text();
    shortcut(Qt::Key_Q,Qt::NoModifier);
    check(edit->text()!=before, "picker accepts filtering input");
    shortcut(Qt::Key_Escape,Qt::NoModifier);
    check(pane->current_widget_stack.empty() && pane->hasFocus(), "Escape closes picker and returns focus to its pane");
    return fits && visible;
}
static void test_laser_cache() {
    QWidget host; host.resize(440,200);
    LaserPointerOverlay trail(&host);
    double offset=0;
    trail.project=[&](QPointF p){return p+QPointF(0,offset);};
    const double ratio=trail.devicePixelRatioF();
    QImage image(host.size()*ratio,QImage::Format_ARGB32_Premultiplied);
    image.setDevicePixelRatio(ratio);
    auto snapshot=[&] {
        image.fill(Qt::black);
        trail.render(&image,QPoint(),QRegion(),QWidget::DrawChildren);
    };
    auto red=[&](int x,int y) {
        auto color=image.pixelColor(int(x*ratio),int(y*ratio));
        return color.red()-color.green();
    };
    // A sparse, fast gesture must fade along its length, not all at once.
    trail.begin({40,80}); trail.append({360,80}); snapshot();
    check(red(70,80)>10 && red(330,80)>10, "cached trail covers both ends");
    const auto segments=trail.rasterized_segments;
    snapshot();
    check(trail.rasterized_segments==segments, "stationary laser reuses its cache");
    offset=30; snapshot();
    check(trail.rasterized_segments==segments+2 && red(70,110)>10 && red(70,80)==0,
        "view transform rebuilds cached trail at its new position");
    trail.release();
    for (int i=0;i<6;++i) settle();
    snapshot();
    check(red(70,110)==0 && red(330,110)>10, "cached fade erases head before tail, even with sparse input");
    trail.clear();
    offset=0;
    // A new point rounds the live tip into a quadratic, removing its old chord.
    // Check pixels on the curve, the obsolete corner, and the actual cursor tip.
    trail.begin({40,120}); trail.append({120,120}); snapshot();
    trail.append({120,40}); snapshot();
    check(red(110,110)>10 && red(120,120)==0 && red(120,40)>10,
        "laser rounds sample corners, removes the old tip and still reaches the cursor");
    const auto incremental=image.copy();
    trail.clear();
    trail.begin({40,120}); trail.append({120,120}); trail.append({120,40}); snapshot();
    check(image==incremental, "curve appearance is independent of input/paint batching");
    trail.release(); snapshot();
    check(image==incremental, "release preserves the held curve without a shape jump");
    trail.clear();
    // A dense sequence of caps must not turn an even line into bright beads.
    trail.begin({40,140});
    for (int x=44;x<=360;x+=4) { trail.append({double(x),140}); snapshot(); }
    for (int x=80;x<330;++x) {
        check(std::abs(red(x,144)-red(82,144))<=2, "glow brightness stays even across sample joins");
    }
    trail.clear();
    // The later vertical pass must not keep a horizontal stub alive beside it.
    // Previously its 20px chronology mask prolonged the old line's bright core.
    trail.begin({40,100}); trail.append({360,100}); trail.append({360,180});
    trail.append({200,180}); trail.append({200,20}); snapshot();
    check(red(209,100)>30, "crossing fixture has earlier horizontal ink");
    while (trail.clock.elapsed()<1100) settle();
    trail.release(); trail.animation.stop();
    trail.strokes.back().released=trail.clock.elapsed()-900;
    snapshot();
    check(red(180,100)==0 && red(209,100)<3 && red(207,100)<25,
        "earlier crossing arm and glow disappear on their own schedule");
    check(red(200,100)>30 && red(200,70)>30, "later crossing remains visible");
    if (qEnvironmentVariableIsSet("SIOYEK_LASER_SCREENSHOT"))
        image.save(qEnvironmentVariable("SIOYEK_LASER_SCREENSHOT"));
    trail.clear();
    const int saved_duration=LASER_POINTER_DURATION_MS;
    LASER_POINTER_DURATION_MS=2900;
    trail.begin({40,100}); trail.append({360,100}); trail.release(); trail.animation.stop();
    check(trail.strokes.back().duration_ms==2900, "release uses configured duration");
    trail.strokes.back().released=trail.clock.elapsed()-700;
    snapshot();
    check(red(70,100)>20, "longer configured lifetime slows fading");
    trail.clear();
    LASER_POINTER_DURATION_MS=100;
    trail.begin({40,100}); trail.append({360,100}); trail.release();
    settle();
    check(trail.strokes.empty(), "configured lifetime also controls cleanup");
    LASER_POINTER_DURATION_MS=saved_duration;
    trail.begin({200,100});
    for (int i=0;i<1200;++i)
        trail.append({220+150*std::sin(i*.04),100+70*std::sin(i*.065)});
    snapshot(); trail.release(); trail.animation.stop();
    qint64 longest_fade_frame=0;
    for (int i=0;i<12;++i) {
        trail.strokes.back().released=trail.clock.elapsed()-600-i*40;
        QElapsedTimer timer; timer.start(); snapshot();
        longest_fade_frame=std::max(longest_fade_frame,timer.elapsed());
    }
    qInfo() << "Laser fade: longest frame for a 1200-point self-crossing stroke:" << longest_fade_frame << "ms";
    check(longest_fade_frame<100, "dense released strokes keep fading responsive");
    trail.clear();
    qInfo("PASS: laser crossings fade independently and duration controls fading and expiry");
}

static void test_split() {
    try {
        qApp->setQuitOnLastWindowClosed(false);
        windows.front()->config_manager->deserialize_config("laser_pointer_duration_ms", L"2900");
        check(LASER_POINTER_DURATION_MS==2900, "laser duration is available through the config parser");
        windows.front()->config_manager->deserialize_config("laser_pointer_duration_ms", L"1450");
        test_laser_cache();
        check(windows.size() == 1, "one initial window");
        QPointer<MainWidget> left = windows.front();
        if (qEnvironmentVariableIsSet("SIOYEK_PEN_PREVIEW")) {
            auto path = test_directory + "/Pen-rendering-check.pdf";
            {
                QPdfWriter writer(path); writer.setResolution(72);
                QPainter painter(&writer);
                painter.setFont(QFont("Helvetica", 12));
                painter.drawText(30, 30, "Pen rendering: curves, diagonals, corners and round caps");
                painter.end();
            }
            left->open_document(path.toStdWString());
            left->resize(900, 850); settle();
            left->main_document_view->fit_to_page_width();
            left->main_document_view->goto_page(0);
            for (int row=0; row<3; ++row) {
                FreehandDrawing ink;
                ink.type = row==1 ? 'n' : 'r'; ink.alpha = row==2 ? .5 : 1;
                ink.creattion_time = QDateTime::currentDateTime();
                for (int i=0; i<160; ++i) {
                    float x=50+i*3, y=100+row*100+25*std::sin(i*.12);
                    auto absolute=DocumentPos{0,x,y}.to_absolute(left->doc());
                    ink.points.push_back({absolute,float(1+row*2)});
                }
                left->doc()->add_freehand_drawing(ink);
            }
            for (auto points : {std::vector<QPointF>{{50,410},{200,445},{350,480},{500,515}},
                                std::vector<QPointF>{{50,580},{200,580},{200,650},{350,580},{500,650}}}) {
                FreehandDrawing ink; ink.type='n'; ink.alpha=1; ink.creattion_time=QDateTime::currentDateTime();
                for(auto p:points) ink.points.push_back({DocumentPos{0,float(p.x()),float(p.y())}.to_absolute(left->doc()),5});
                left->doc()->add_freehand_drawing(ink);
            }
            left->invalidate_render();
            qInfo() << "PREVIEW" << QCoreApplication::applicationFilePath();
            test_result=0;
            QTimer::singleShot(240000,qApp,[]{QCoreApplication::quit();});
            return;
        }
        auto a = get_canonical_path((test_directory + "/a.pdf").toStdWString());
        auto b = get_canonical_path((test_directory + "/b.pdf").toStdWString());
        auto c = get_canonical_path((test_directory + "/c.pdf").toStdWString());
        left->open_document(a);
        for (const auto& path : left->window_tabs.paths())
            if (path != a) left->close_tab(path);
        left->open_document(b); left->open_document(c); left->select_tab(0);
        left->resize(1000, 700); settle();
        QApplication::setActiveWindow(left);
        left->setFocus();
        shortcut(Qt::Key_Less, Qt::ShiftModifier);
        check(left->window_tabs.paths() == std::vector<std::wstring>({a,b,c}), "left endpoint does not wrap");
        shortcut(Qt::Key_Greater, Qt::ShiftModifier);
        shortcut(Qt::Key_Greater, Qt::ShiftModifier);
        shortcut(Qt::Key_Greater, Qt::ShiftModifier);
        check(left->window_tabs.paths() == std::vector<std::wstring>({b,c,a}) &&
            left->doc()->get_path() == a && left->get_current_tab_index() == 2,
            "Shift+period moves current tab right and stops at endpoint");
        check(left->get_current_tabs_file_names() == L"[active] 2\n" + b + L"\n" + c + L"\n" + a,
            "session records the reordered tabs and active selection");
        shortcut(Qt::Key_Less, Qt::ShiftModifier);
        shortcut(Qt::Key_Less, Qt::ShiftModifier);
        check(left->window_tabs.paths() == std::vector<std::wstring>({a,b,c}), "Shift+comma moves tab left");
        shortcut(Qt::Key_Backslash, Qt::ControlModifier);
        auto right = left->split_pane_;
        check(right && right->window() == left, "two panes in one native window");
        check(left->pdf_splitter_->count() == 2, "exactly two panes");
        check(left->active_pane() == right, "split focuses right");
        check(left->main_window_width < left->width() * 0.6, "left coordinates use pane width");
        check(right->main_window_width < left->width() * 0.6, "right coordinates use pane width");
        bool pickers_ok=true;
        for (auto pane:{left.data(),right.data()})
            for (bool filesystem:{false,true}) pickers_ok=test_file_picker(pane,filesystem) && pickers_ok;
        for (MainWidget* pane : {left.data(), right.data()}) {
            pane->toggle_freehand_drawing_mode();
            auto mouse = [pane](QEvent::Type type, QPointF point, Qt::MouseButton button, Qt::MouseButtons buttons) {
                auto global = QPointF(pane->central_widget->mapToGlobal(QPoint(0, 0))) + point;
                QMouseEvent event(type, point, global, button, buttons, Qt::NoModifier);
                QApplication::sendEvent(pane->central_widget, &event);
            };
            mouse(QEvent::MouseButtonPress, {100, 100}, Qt::LeftButton, Qt::LeftButton);
            mouse(QEvent::MouseMove, {110, 110}, Qt::NoButton, Qt::LeftButton);
            mouse(QEvent::MouseMove, {130, 120}, Qt::NoButton, Qt::LeftButton);
            check(pane->opengl_widget->current_drawing.points.size() == 3, "mouse movement reaches the drawing pane");
            auto previous = pane->opengl_widget->current_drawing.points.back().pos;
            mouse(QEvent::MouseMove, {131.25, 121.25}, Qt::NoButton, Qt::LeftButton);
            auto fractional = pane->opengl_widget->current_drawing.points.back().pos;
            check(std::abs((fractional.x-previous.x)*pane->dv()->get_zoom_level()-1.25) < .002,
                "fractional pen coordinates survive input conversion");
            mouse(QEvent::MouseButtonRelease, {130, 120}, Qt::LeftButton, Qt::NoButton);
            const float y = pane == left ? 220 : 350;
            const char pen_color = pane->current_freehand_type;
            const float pen_width = pane->freehand_thickness;
            mouse(QEvent::MouseButtonPress, {80, y}, Qt::LeftButton, Qt::LeftButton);
            mouse(QEvent::MouseMove, {300, y}, Qt::NoButton, Qt::LeftButton);
            mouse(QEvent::MouseButtonRelease, {300, y}, Qt::LeftButton, Qt::NoButton);
            const int before_erase = pane->doc()->num_freehand_drawings();
            mouse(QEvent::MouseButtonPress, {190, y-30}, Qt::RightButton, Qt::RightButton);
            check(pane->temporary_eraser_active && !pane->brush_eraser_enabled &&
                pane->cursor().hotSpot() == QPoint(14,14), "right press temporarily shows brush eraser");
            mouse(QEvent::MouseMove, {190, y+30}, Qt::NoButton, Qt::RightButton);
            check(pane->doc()->num_freehand_drawings() == before_erase + 1,
                "right drag splits a stroke locally instead of deleting the whole stroke");
            mouse(QEvent::MouseButtonRelease, {190, y+30}, Qt::RightButton, Qt::NoButton);
            check(!pane->temporary_eraser_active && !pane->brush_eraser_dragging && pane->should_draw(false) &&
                pane->cursor().hotSpot() == QPoint(9,9), "right release returns directly to the pen");
            check(pane->current_freehand_type == pen_color && pane->freehand_thickness == pen_width,
                "temporary erasing preserves pen color and thickness");
            const int after_erase = pane->doc()->num_freehand_drawings();
            mouse(QEvent::MouseButtonPress, {80, y+60}, Qt::LeftButton, Qt::LeftButton);
            mouse(QEvent::MouseMove, {300, y+60}, Qt::NoButton, Qt::LeftButton);
            mouse(QEvent::MouseButtonRelease, {300, y+60}, Qt::LeftButton, Qt::NoButton);
            check(pane->doc()->num_freehand_drawings() == after_erase + 1, "left drag draws again without a keyboard toggle");
            mouse(QEvent::MouseButtonPress, {190, y+60}, Qt::RightButton, Qt::RightButton);
            mouse(QEvent::MouseButtonRelease, {190, y+60}, Qt::RightButton, Qt::NoButton);
            check(pane->doc()->num_freehand_drawings() == after_erase + 2, "a right click erases under the brush too");
            mouse(QEvent::MouseButtonPress, {350, y}, Qt::RightButton, Qt::RightButton);
            QEvent deactivate(QEvent::WindowDeactivate);
            QApplication::sendEvent(left, &deactivate);
            check(!pane->temporary_eraser_active && pane->should_draw(false), "deactivation cannot leave temporary eraser stuck");
            mouse(QEvent::MouseButtonPress, {350, y}, Qt::RightButton, Qt::RightButton);
            mouse(QEvent::MouseMove, {350, y}, Qt::NoButton, Qt::NoButton);
            check(!pane->temporary_eraser_active, "missing release is recovered from mouse button state");
            pane->exit_freehand_drawing_mode();
            pane->toggle_brush_eraser();
            mouse(QEvent::MouseButtonPress, {190, y}, Qt::LeftButton, Qt::LeftButton);
            mouse(QEvent::MouseButtonRelease, {190, y}, Qt::LeftButton, Qt::NoButton);
            check(pane->brush_eraser_enabled && !pane->temporary_eraser_active,
                "dedicated eraser mode still uses the left button and stays enabled");
            pane->stop_brush_eraser();
        }
        qInfo("PASS: right-button brush erasing, pen restoration and interrupted drags in both panes");
        // Erasing must paint without waiting for the 200 ms maintenance timer,
        // including the other pane showing the same document.
        left->validation_interval_timer->stop(); right->validation_interval_timer->stop();
        settle(); settle();
        int left_frames=0, right_frames=0;
        auto lc=QObject::connect(left->opengl_widget, &QOpenGLWidget::frameSwapped, [&]{++left_frames;});
        auto rc=QObject::connect(right->opengl_widget, &QOpenGLWidget::frameSwapped, [&]{++right_frames;});
        right->erase_at_cursor({110.25,110.25});
        settle();
        QObject::disconnect(lc); QObject::disconnect(rc);
        check(left_frames>0 && right_frames>0, "eraser repaints both panes without the maintenance timer");
        right->brush_eraser_dragging=false;
        left->validation_interval_timer->start(); right->validation_interval_timer->start();
        // Laser gestures are temporary in either pane and do not enter ink history.
        for (MainWidget* pane : {left.data(), right.data()}) {
            pane->setFocus();
            shortcut(Qt::Key_C, Qt::ShiftModifier);
            check(pane->laser_pointer_enabled, "Shift+C enables laser");
            const int saved_ink=pane->doc()->num_freehand_drawings();
            auto mouse = [pane](QEvent::Type type, QPointF point, Qt::MouseButton button, Qt::MouseButtons buttons) {
                auto global=QPointF(pane->central_widget->mapToGlobal(QPoint(0,0)))+point;
                QMouseEvent event(type,point,global,button,buttons,Qt::NoModifier);
                QApplication::sendEvent(pane->central_widget,&event);
            };
            mouse(QEvent::MouseButtonPress,{100,150},Qt::LeftButton,Qt::LeftButton);
            // Sustain a dense ten-second-equivalent stroke, painting between batches.
            // The cache must rasterize each new point once, never the entire history.
            QImage frame(pane->central_widget->size()*pane->devicePixelRatioF(),QImage::Format_ARGB32_Premultiplied);
            frame.setDevicePixelRatio(pane->devicePixelRatioF());
            QElapsedTimer stroke_timer; stroke_timer.start();
            for (int batch=0;batch<120;++batch) {
                for (int sample=0;sample<10;++sample) {
                    const int i=batch*10+sample;
                    mouse(QEvent::MouseMove,{250+150*std::sin(i*.04),250+100*std::sin(i*.065)},Qt::NoButton,Qt::LeftButton);
                }
                pane->laser_pointer_->render(&frame);
            }
            qInfo() << "Laser: 1200 samples and 120 frames:" << stroke_timer.elapsed() << "ms";
            const auto rendered=pane->laser_pointer_->rasterized_segments;
            for (int i=0;i<5;++i) pane->laser_pointer_->render(&frame);
            check(pane->laser_pointer_->rasterized_segments==rendered, "held frames reuse rasterized trail");
            mouse(QEvent::MouseMove,{300,250},Qt::NoButton,Qt::LeftButton);
            pane->laser_pointer_->render(&frame);
            check(pane->laser_pointer_->rasterized_segments==rendered+1, "new sample renders only one segment");
            check(!pane->laser_pointer_->animation.isActive(), "held-only trail has no animation timer");
            if (pane==left) {
                // Exercise real event-loop painting for a continuous four-second stroke.
                QEventLoop live; QTimer feeder; feeder.setTimerType(Qt::PreciseTimer); feeder.setInterval(8);
                QElapsedTimer elapsed; elapsed.start(); qint64 last=0, longest_gap=0; int count=0;
                QObject::connect(&feeder,&QTimer::timeout,&live,[&] {
                    const auto now=elapsed.elapsed(); longest_gap=std::max(longest_gap,now-last); last=now;
                    mouse(QEvent::MouseMove,{250+150*std::sin(count*.045),250+100*std::sin(count*.07)},Qt::NoButton,Qt::LeftButton);
                    if (++count==500) { feeder.stop(); live.quit(); }
                });
                feeder.start(); live.exec();
                qInfo() << "Laser live stroke:" << elapsed.elapsed() << "ms, longest event gap:" << longest_gap << "ms";
                check(longest_gap<250 && elapsed.elapsed()<7000, "long live gesture keeps the event loop responsive");
            } else for (int i=0;i<14;++i) settle();
            check(pane->laser_pointer_->drawing() && pane->laser_pointer_->strokes.size()==1,
                "held laser remains visible beyond its release lifetime");
            mouse(QEvent::MouseButtonRelease,{310,255},Qt::LeftButton,Qt::NoButton);
            check(!pane->laser_pointer_->drawing(), "release starts laser fade");
            check(pane->laser_pointer_->animation.isActive(), "release starts animation timer");
            check(pane->doc()->num_freehand_drawings()==saved_ink, "laser leaves saved ink untouched");
            mouse(QEvent::MouseButtonPress,{200,300},Qt::LeftButton,Qt::LeftButton);
            check(pane->laser_pointer_->strokes.size()==2, "new stroke preserves the fading trail");
            mouse(QEvent::MouseButtonRelease,{200,300},Qt::LeftButton,Qt::NoButton);
            for (int i=0;i<14;++i) settle();
            check(pane->laser_pointer_->strokes.empty() && !pane->laser_pointer_->animation.isActive(),
                "trails expire and animation stops");
            shortcut(Qt::Key_X, Qt::NoModifier);
            check(!pane->laser_pointer_enabled && pane->should_draw(false), "x switches from laser to permanent ink");
            shortcut(Qt::Key_P, Qt::NoModifier);
            check(!pane->should_draw(false), "p toggles the same pen mode as x");
            shortcut(Qt::Key_C, Qt::ShiftModifier);
            mouse(QEvent::MouseButtonPress,{100,150},Qt::LeftButton,Qt::LeftButton);
            shortcut(Qt::Key_Escape, Qt::NoModifier);
            check(!pane->laser_pointer_enabled && pane->laser_pointer_->strokes.empty(), "Escape clears laser");
        }
        right->setFocus();
        shortcut(Qt::Key_3, Qt::ControlModifier);
        check(left->doc()->get_path() == a && right->doc()->get_path() == c, "tab change only affects right");
        shortcut(Qt::Key_H, Qt::MetaModifier);
        check(left->active_pane() == left && left->hasFocus(), "keyboard focus left");
        shortcut(Qt::Key_2, Qt::ControlModifier);
        check(left->doc()->get_path() == b && right->doc()->get_path() == c, "tab change only affects left");
        left->main_document_view->set_zoom_level(1.3f, true);
        right->select_tab(1); right->main_document_view->set_zoom_level(2.1f, true);
        left->select_tab(0); left->select_tab(1);
        right->select_tab(2); right->select_tab(1); settle();
        check(std::abs(left->main_document_view->get_zoom_level() - 1.3f) < .01, "left view state survives tab switches");
        check(std::abs(right->main_document_view->get_zoom_level() - 2.1f) < .01, "same PDF has independent right view state");
        shortcut(Qt::Key_L, Qt::MetaModifier);
        check(left->active_pane() == right && right->hasFocus(), "keyboard focus right");
        shortcut(Qt::Key_W, Qt::ControlModifier | Qt::AltModifier);
        check(left->active_pane() == left, "keyboard focus toggle");
        right->select_tab(2);
        const auto left_view = left->dv()->get_state().book_state;
        const auto right_view = right->dv()->get_state().book_state;
        shortcut(Qt::Key_Less, Qt::ShiftModifier);
        check(left->window_tabs.paths() == std::vector<std::wstring>({b,a,c}), "left pane reorders shared tabs");
        check(left->doc()->get_path() == b && right->doc()->get_path() == c,
            "moving a tab does not switch either PDF");
        left->focus_split_pane(1);
        shortcut(Qt::Key_Less, Qt::ShiftModifier);
        check(left->window_tabs.paths() == std::vector<std::wstring>({b,c,a}) &&
            right->get_current_tab_index() == 1, "right pane moves its own active tab in shared order");
        check(left->dv()->get_state().book_state.offset_y == left_view.offset_y &&
            right->dv()->get_state().book_state.offset_y == right_view.offset_y &&
            left->dv()->get_zoom_level() == left_view.zoom_level &&
            right->dv()->get_zoom_level() == right_view.zoom_level, "tab movement preserves both view states");
        shortcut(Qt::Key_3, Qt::ControlModifier);
        check(right->doc()->get_path() == a, "number shortcuts follow the new order");
        shortcut(Qt::Key_Less, Qt::ShiftModifier);
        shortcut(Qt::Key_Less, Qt::ShiftModifier);
        check(left->window_tabs.paths() == std::vector<std::wstring>({a,b,c}), "tab order restored for remaining tests");
        right->select_tab(1);
        left->focus_split_pane(0);
        qInfo("PASS: tab movement shortcuts, endpoints, session order and independent split views");
        if (qEnvironmentVariableIsSet("SIOYEK_SPLIT_PREVIEW")) {
            left->main_document_view->fit_to_page_width();
            right->select_tab(2); right->main_document_view->fit_to_page_width();
            right->main_document_view->goto_page(1);
            left->invalidate_render(); right->invalidate_render();
            qInfo() << "PREVIEW" << QCoreApplication::applicationFilePath();
            test_result = 0;
            QTimer::singleShot(240000, qApp, [] { QCoreApplication::quit(); });
            return;
        }
        left->pdf_splitter_->setSizes({650, 350}); settle();
        check(left->main_window_width > right->main_window_width, "divider updates pane geometry");
        right->close_tab(b); settle();
        check(left->window_tabs.paths().size() == 2, "tab removed from shared list");
        check(left->doc()->get_path() != b && right->doc()->get_path() != b, "closed PDF replaced in both panes");
        left->focus_split_pane(1); right->select_tab(0); settle();
        shortcut(Qt::Key_Backslash,Qt::ControlModifier);
        check(!left->split_pane_ && !left->pdf_splitter_, "toggle collapses instead of nesting");
        check(left->doc()->get_path() == a, "collapse keeps focused PDF");
        check(left->window_tabs.paths().size() == 2, "collapse preserves shared tabs");
        check(left->main_window_width == left->width(), "full pane geometry restored");
        for (bool filesystem:{false,true}) pickers_ok=test_file_picker(left,filesystem) && pickers_ok;
        check(pickers_ok, "file pickers fit either pane and stay above the PDF after collapsing split view");
        left->toggle_split(); settle();
        check(left->pdf_splitter_->count() == 2, "repeat split stays at two panes");
        for (auto pane:{left.data(),left->split_pane_.data()})
            for (bool filesystem:{false,true}) check(test_file_picker(pane,filesystem), "pickers work after splitting again");
        left->focus_split_pane(0);
        shortcut(Qt::Key_Backslash,Qt::ControlModifier);
        check(!left->split_pane_, "keyboard collapse also works with the left pane focused");
        for (bool filesystem:{false,true}) check(test_file_picker(left,filesystem), "pickers survive collapse from the left pane");
        left->toggle_split(); settle();
        left->close_tab(a); settle();
        left->close_current_tab(); settle();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        check(!left && windows.empty(), "closing last shared tab destroys both panes safely");
        qInfo("PASS: split geometry, focus, shared tabs, independent views, close and collapse");
        test_result = 0;
        QCoreApplication::exit(0);
    } catch (const std::exception& error) {
        qCritical("FAIL: %s", error.what());
        QCoreApplication::exit(1);
    }
}
static void schedule_split_test() { QTimer::singleShot(1200, qApp, test_split); }
Q_COREAPP_STARTUP_FUNCTION(schedule_split_test)
int main(int argc, char** argv) {
    QStandardPaths::setTestModeEnabled(true);
    QTemporaryDir directory;
    test_directory = directory.path();
    for (auto name : {"a.pdf", "b.pdf", "c.pdf"})
        QFile::copy(QString::fromLocal8Bit(argv[1]), test_directory + "/" + name);
    QByteArray first = (test_directory + "/a.pdf").toUtf8();
    char instance[] = "--instance-name";
    char name[] = "sioyek-split-regression";
    char* args[] = {argv[0], first.data(), instance, name};
    sioyek_application_main(4, args);
    return test_result;
}
