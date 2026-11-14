#define main sioyek_application_main
#include "../pdf_viewer/main.cpp"
#undef main
#include "../pdf_viewer/blank_canvas.h"
#include <QTemporaryDir>
#include <QEventLoop>
#include <QGraphicsOpacityEffect>
#include <stdexcept>
extern int STATUS_BADGE_VISIBILITY, TAB_BAR_VISIBILITY, NAVIGATION_UI_DURATION, NAVIGATION_UI_FADE_DURATION;
static int result=1;
static QString folder;
static void check(bool ok,const char* msg){if(!ok)throw std::runtime_error(msg);}
static void settle(int ms=100){QEventLoop l;QTimer::singleShot(ms,&l,&QEventLoop::quit);l.exec();}
static void run(){try{
    auto w=windows.front();
    STATUS_BADGE_VISIBILITY=TAB_BAR_VISIBILITY=2;
    NAVIGATION_UI_DURATION=300;NAVIGATION_UI_FADE_DURATION=80;
    w->status_visibility_override_=w->tabs_visibility_override_=-1;
    w->activateWindow();w->setFocus();
    QCursor::setPos(w->mapToGlobal(QPoint(20,w->height()/2)));
    w->open_document((folder+"/two.pdf").toStdWString());w->validate_ui();settle(150);
    auto tabs=w->findChild<QTabBar*>("floatingDocumentTabs");check(tabs,"tab strip exists");
    check(w->status_label->isVisible()&&tabs->isVisible(),"new document reveals both overlays");
    for(int i=0;i<6;++i){w->validate_ui();w->refresh_titlebar();settle();}
    check(!w->status_label->isVisible()&&!tabs->isVisible(),"ordinary refreshes do not prolong reveal");
    w->dv()->goto_page(1);w->validate_ui();settle(120);
    check(w->status_label->isVisible()&&!tabs->isVisible(),"page transition reveals only status");
    auto effect=qobject_cast<QGraphicsOpacityEffect*>(w->status_label->graphicsEffect());
    check(effect&&effect->opacity()>0.99,"reveal finishes at full opacity");
    settle(450);check(!w->status_label->isVisible(),"page badge fades away");
    w->dv()->set_offset_y(w->dv()->get_offset_y()+w->doc()->get_page_height(1));w->validate_ui();settle(120);
    check(w->status_label->isVisible(),"scrolling across a boundary reveals badge");
    settle(450);
    w->opengl_widget->search_text(L"missing");w->validate_ui();settle(600);
    check(w->status_label->isVisible()&&w->status_label_left->text().contains("Match"),"search count stays visible past timeout");
    w->opengl_widget->cancel_search();w->validate_ui();settle(150);
    check(!w->status_label->isVisible(),"cancel search releases persistent badge");
    w->execute_macro_if_enabled(L"goto_mark");w->validate_ui();settle(600);
    check(w->status_label->isVisible() && w->status_label_left->text().contains("Awaiting key"),"pending key keeps badge visible");
    w->handle_escape();w->validate_ui();settle(150);
    check(!w->status_label->isVisible(),"cancel pending key releases badge");
    w->set_status_message(L"Saved workspace: test",400);w->validate_ui();settle(150);
    check(w->status_label->isVisible(),"confirmation reveals badge");settle(500);
    check(!w->status_label->isVisible(),"confirmation expires");
    w->select_tab(0);w->validate_ui();settle(130);
    check(tabs->isVisible()&&w->status_label->isVisible(),"tab change reveals both");
    auto snapshot=w->capture_workspace();settle(600);QString error;
    check(w->apply_workspace(snapshot,error),"workspace restored");settle(130);
    check(tabs->isVisible()&&w->status_label->isVisible(),"same-document workspace reveals both");
    w->toggle_statusbar();w->toggle_titlebar();settle(600);
    check(w->status_label->isVisible()&&tabs->isVisible(),"automatic cycles to always visible");
    w->toggle_statusbar();w->toggle_titlebar();settle(150);
    w->dv()->goto_page(0);w->validate_ui();w->opengl_widget->search_text(L"missing");w->validate_ui();settle(150);
    check(!w->status_label->isVisible()&&!tabs->isVisible(),"explicit hidden stays hidden");
    w->opengl_widget->cancel_search();w->toggle_statusbar();w->toggle_titlebar();
    w->toggle_split();settle(130);auto right=w->split_pane_.data();check(right,"split created");
    right->dv()->goto_page(2);right->validate_ui();settle(150);
    check(right->status_label->isVisible(),"split pane reveals its own badge");
    settle(600);check(!right->status_label->isVisible(),"split badge expires");
    w->reveal_navigation_ui(true);right->reveal_navigation_ui();
    w->validate_ui();right->validate_ui();w->refresh_titlebar();settle(100);
    w->grab().save("/tmp/sioyek-auto-navigation.png");
    qInfo("PASS: auto reveal, scroll/page/tab/workspace changes, search lifetime, confirmations, three modes, split panes and fades");
    result=0;QCoreApplication::exit(0);
}catch(const std::exception& e){qCritical("FAIL: %s",e.what());QCoreApplication::exit(1);}}
static void schedule(){QTimer::singleShot(1200,qApp,run);}Q_COREAPP_STARTUP_FUNCTION(schedule)
int main(int,char** argv){QStandardPaths::setTestModeEnabled(true);QTemporaryDir dir;folder=dir.path();
for(auto name:{"one.pdf","two.pdf"}){QFile f(folder+"/"+name);f.open(QIODevice::WriteOnly);f.write(blank_canvas_pdf(4));}
QByteArray path=(folder+"/one.pdf").toUtf8();char option[]="--instance-name",name[]="sioyek-auto-navigation-test";char* args[]={argv[0],path.data(),option,name};sioyek_application_main(4,args);return result;}
