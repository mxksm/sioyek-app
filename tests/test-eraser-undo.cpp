#define main sioyek_application_main
#include "../pdf_viewer/main.cpp"
#undef main
#include "../pdf_viewer/blank_canvas.h"
#include <QTemporaryDir>
#include <QEventLoop>
#include <stdexcept>

static int result=1;
static void check(bool ok,const char* message) { if(!ok) throw std::runtime_error(message); }
static void settle() { QEventLoop loop; QTimer::singleShot(100,&loop,&QEventLoop::quit);loop.exec(); }
static bool same(const std::vector<FreehandDrawing>& a,const std::vector<FreehandDrawing>& b) {
    if(a.size()!=b.size()) return false;
    for(size_t i=0;i<a.size();++i) {
        if(a[i].type!=b[i].type || a[i].alpha!=b[i].alpha || a[i].creattion_time!=b[i].creattion_time || a[i].points.size()!=b[i].points.size()) return false;
        for(size_t j=0;j<a[i].points.size();++j) {
            const auto& x=a[i].points[j]; const auto& y=b[i].points[j];
            if(x.pos.x!=y.pos.x || x.pos.y!=y.pos.y || x.thickness!=y.thickness) return false;
        }
    }
    return true;
}
static FreehandDrawing stroke(float y) {
    FreehandDrawing d;d.type='r';d.alpha=.7f;d.creattion_time=QDateTime::currentDateTime();
    d.points={{{100,y},2},{{300,y},4}};return d;
}
static void reset(Document* doc) { doc->delete_all_drawings();doc->persist_drawings();doc->load_drawings(); }
static void undo(MainWidget* pane,bool redo=false) {
    pane->setFocus();
#ifdef Q_OS_MACOS
    const auto command=Qt::ControlModifier;
#else
    const auto command=Qt::MetaModifier;
#endif
    QKeyEvent event(QEvent::KeyPress,Qt::Key_Z,command | (redo?Qt::ShiftModifier:Qt::NoModifier),redo?"Z":"z");
    QApplication::sendEvent(pane,&event);settle();
}
static void run() {
    try {
        auto pane=windows.front();pane->resize(1000,850);pane->activateWindow();pane->setFocus();settle();
        auto doc=pane->doc(); bool mask[26];std::fill(mask,mask+26,true);reset(doc);
        doc->add_freehand_drawing(stroke(150));doc->add_freehand_drawing(stroke(950));
        const auto first=doc->get_page_drawings(0), second=doc->get_page_drawings(1);
        check(first.size()==1 && second.size()==1,"fixture spans pages");
        const auto gesture=doc->begin_brush_erase();
        check(doc->erase_freehand_brush({150,150},{150,150},5,mask,gesture),"first cut");
        check(doc->erase_freehand_brush({250,150},{250,150},5,mask,gesture),"second cut in same drag");
        check(doc->erase_freehand_brush({200,950},{200,950},5,mask,gesture),"same drag crosses page");
        const auto cut0=doc->get_page_drawings(0),cut1=doc->get_page_drawings(1);
        check(cut0.size()==3 && cut1.size()==2,"partial erasing retains fragments");
        undo(pane);check(same(doc->get_page_drawings(0),first)&&same(doc->get_page_drawings(1),second),"Cmd+Z restores entire cross-page gesture exactly");
        undo(pane);check(doc->get_page_drawings(1).empty()&&same(doc->get_page_drawings(0),first),"next undo removes preceding drawn stroke");
        undo(pane,true);undo(pane,true);
        check(same(doc->get_page_drawings(0),cut0)&&same(doc->get_page_drawings(1),cut1),"redo preserves cut geometry and chronological order");
        undo(pane);
        check(!doc->erase_freehand_brush({-100,-100},{-100,-100},5,mask,doc->begin_brush_erase()),"miss leaves redo intact");
        undo(pane,true);check(same(doc->get_page_drawings(0),cut0),"redo survives no-op gesture");
        undo(pane);
        doc->erase_freehand_brush({220,150},{220,150},5,mask,doc->begin_brush_erase());
        const auto replacement=doc->get_page_drawings(0);undo(pane,true);
        check(same(doc->get_page_drawings(0),replacement),"new erasing invalidates old redo branch");
        undo(pane);
        doc->delete_page_intersecting_drawings(0,AbsoluteRect(fz_rect{90,140,310,160}),mask);
        check(doc->get_page_drawings(0).empty(),"rectangle erase removes stroke");
        undo(pane);check(same(doc->get_page_drawings(0),first),"rectangle erase is undoable");
        doc->delete_all_page_drawings(0);undo(pane);check(same(doc->get_page_drawings(0),first),"clear page is undoable");
        doc->delete_all_drawings();undo(pane);
        check(same(doc->get_page_drawings(0),first)&&same(doc->get_page_drawings(1),second),"clear document is undoable");
        auto selection=doc->select_ink(QPolygonF(QRectF(90,140,220,20)),nullptr);
        check(doc->transform_ink(selection,QTransform::fromTranslate(0,50)),"lasso move fixture");
        const auto moved=doc->get_page_drawings(0);
        doc->erase_freehand_brush({200,200},{200,200},5,mask);undo(pane);
        check(same(doc->get_page_drawings(0),moved),"undo erasing restores moved ink");
        undo(pane);check(same(doc->get_page_drawings(0),first),"preceding lasso move remains undoable");

        // Exercise actual mouse down/move/up grouping, both eraser modes.
        for(bool temporary:{false,true}) {
            reset(doc);pane->main_document_view->set_zoom_level(1,true,false);pane->main_document_view->set_offset_y(300);
            doc->add_freehand_drawing(stroke(250));const auto original=doc->get_page_drawings(0);
            pane->set_hand_drawing_mode(temporary);
            if(!temporary)pane->toggle_brush_eraser();
            const auto button=temporary?Qt::RightButton:Qt::LeftButton;
            auto mouse=[&](QEvent::Type type,float x,Qt::MouseButtons buttons) {
                const auto pos=AbsoluteDocumentPos{x,250}.to_window(pane->main_document_view);
                QPointF local(pos.x,pos.y),global=QPointF(pane->central_widget->mapToGlobal(QPoint(0,0)))+local;
                QMouseEvent event(type,local,global,type==QEvent::MouseMove?Qt::NoButton:button,buttons,Qt::NoModifier);
                QApplication::sendEvent(pane->central_widget,&event);
            };
            mouse(QEvent::MouseButtonPress,170,button);mouse(QEvent::MouseMove,190,button);mouse(QEvent::MouseButtonRelease,210,Qt::NoButton);
            check(!pane->brush_eraser_dragging,"release ends eraser drag");
            const auto erased=doc->get_page_drawings(0);check(!same(erased,original),"mouse drag erased ink");
            undo(pane);check(same(doc->get_page_drawings(0),original),"one Cmd+Z undoes whole mouse drag");
            undo(pane,true);check(same(doc->get_page_drawings(0),erased),"Cmd+Shift+Z reapplies whole drag");
            undo(pane);undo(pane);check(doc->get_page_drawings(0).empty(),"mouse erase did not destroy drawing history");
            pane->exit_freehand_drawing_mode();
        }
        qInfo("PASS: eraser gesture grouping, mouse brush/right-click, Cmd+Z/Cmd+Shift+Z, chronology, cross-page fidelity, lasso, rectangle and clear undo");
        result=0;QCoreApplication::exit(0);
    } catch(const std::exception& e) {qCritical("FAIL: %s",e.what());QCoreApplication::exit(1);}
}
static void schedule(){QTimer::singleShot(1200,qApp,run);}
Q_COREAPP_STARTUP_FUNCTION(schedule)
int main(int,char** argv) {
    QStandardPaths::setTestModeEnabled(true);QTemporaryDir dir;
    QFile file(dir.path()+"/erase.pdf");file.open(QIODevice::WriteOnly);file.write(blank_canvas_pdf(3));file.close();
    auto path=file.fileName().toUtf8();char option[]="--instance-name",instance[]="sioyek-eraser-undo-test";
    char* args[]={argv[0],path.data(),option,instance};sioyek_application_main(4,args);return result;
}
