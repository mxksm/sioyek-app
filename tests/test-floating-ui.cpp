#define main sioyek_application_main
#include "../pdf_viewer/main.cpp"
#undef main
#include "../pdf_viewer/blank_canvas.h"
#include <QTemporaryDir>
#include <QEventLoop>
#include <QClipboard>
#include <stdexcept>
extern bool COMPACT_STATUS_BAR;
extern bool STATUS_BAR_SHOW_CHAPTER;
extern int STATUS_BADGE_VISIBILITY, TAB_BAR_VISIBILITY, NAVIGATION_UI_FADE_DURATION;
static int result=1;
static void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
static void settle(int ms=60){QEventLoop loop;QTimer::singleShot(ms,&loop,&QEventLoop::quit);loop.exec();}
static void key(int code,QString text={},Qt::KeyboardModifiers mods=Qt::NoModifier){
    auto owner=windows.front();
    auto target=owner->focusWidget();
    // Embedded split panes can lose native-window focus during synthetic input.
    // Continue dispatching to the visible picker whose initial focus was checked.
    if (!target) for (auto pane : {owner,owner->split_pane_.data()}) {
        if (pane && !pane->current_widget_stack.empty()) {
            auto menu=pane->current_widget_stack.back();
            if (menu->isVisible()) target=menu->findChild<QLineEdit*>();
        }
    }
    check(target,"focus exists");
    QKeyEvent event(QEvent::KeyPress,code,mods,text);QApplication::sendEvent(target,&event);settle();
}
static void type(QString text){for(auto c:text)key(c.toUpper().unicode(),QString(c));}
static void check_menu(MainWidget* pane){
    auto owner=pane->tab_owner();auto picker=dynamic_cast<FloatingListSelector*>(pane->current_widget_stack.back());
    check(picker,"legacy menu uses shared floating selector");
    check(picker->parentWidget()==owner && owner->rect().contains(picker->geometry()) &&
        (picker->geometry().center()-owner->rect().center()).manhattanLength()<=2,"menu centered over full window");
    check(picker->width()==std::min(1050,owner->width()-48),"menu uses full floating width");
    auto edit=picker->findChild<QLineEdit*>();
    check(edit && edit->isReadOnly() && owner->focusWidget()==edit,"normal mode and focused editor by default");
    auto y=pane->dv()->get_offset_y();type("ggkkkkGjjjj");
    for(int i=0;i<4;++i){key(Qt::Key_Down);key(Qt::Key_Up);}
    check(y==pane->dv()->get_offset_y(),"menu navigation never scrolls PDF behind it");
    key(Qt::Key_Escape);check(pane->current_widget_stack.empty(),"Escape closes menu");
}
static void test_menus(MainWidget* pane){
    pane->handle_goto_bookmark();settle();check_menu(pane);
    pane->handle_goto_highlight();settle();check_menu(pane);
    pane->handle_show_marks();settle();check_menu(pane);
    pane->handle_goto_portal_list();settle();check_menu(pane);
    pane->handle_goto_window();settle();check_menu(pane);
    pane->handle_keys_user_all();settle();check_menu(pane);
    pane->handle_prefs_user_all();settle();check_menu(pane);
    pane->show_custom_option_list({L"Alpha",L"Beta"});settle();check_menu(pane);
    int selected=-1,deleted=-1;
    auto menu=new FilteredSelectTableWindowClass<int>(true,false,{{L"Alpha",L"Beta",L"Gamma"},{L"One",L"Two",L"Three"}},
        {10,20,30},1,[&](int* value){selected=*value;},pane,[&](int* value){deleted=*value;});
    pane->set_current_widget(menu);pane->show_current_widget("Test entries");settle();
    check(menu->get_view()->currentIndex().data().toString()=="Beta","initial selection uses proxy index");
    type("yy");check(QApplication::clipboard()->text()=="Beta","yy copies selected entry");
    type("iBeta");
    key(Qt::Key_Backspace,{},Qt::ControlModifier);
    check(deleted==20 && menu->findChild<QLineEdit*>()->text()=="Beta","Cmd Delete deletes filtered entry without deleting query");
    key(Qt::Key_Escape);type("ccGamma");key(Qt::Key_Escape);type("yy");
    check(QApplication::clipboard()->text()=="Gamma","yank remains correct after deletion");
    key(Qt::Key_Return);check(selected==30 && pane->current_widget_stack.empty(),"filtered selection maps to correct value and closes");
    auto single=new FilteredSelectWindowClass<int>(true,{L"Alpha",L"Beta"},{10,20},[&](int* value){selected=*value;},pane);
    pane->set_current_widget(single);pane->show_current_widget("Single-column menu");settle();type("jyy");
    check(QApplication::clipboard()->text()=="Beta","single-column yy copies entry");
    key(Qt::Key_Return);check(selected==20 && pane->current_widget_stack.empty(),"single-column dispatch closes");
}
static void check_status(MainWidget* pane){
    pane->should_show_status_label_=true;pane->validate_ui();
    check(pane->status_label->isVisible(),"compact status visible");
    check(pane->status_label->width()<pane->central_widget->width()/2,"status only occupies text width");
    QRect area(pane->central_widget->mapTo(pane,QPoint()),pane->central_widget->size());
    check(area.contains(pane->status_label->geometry()) && area.right()-pane->status_label->geometry().right()==8 &&
        area.bottom()-pane->status_label->geometry().bottom()==8,"status sits in bottom-right of its own PDF pane");
    auto text=QString::fromStdWString(pane->get_status_string(false));
    check(!text.contains('[') && !text.contains("h:a") && text.contains(" / "),"plain page count without permanent highlight clutter");
}
static void run(){try{
    STATUS_BADGE_VISIBILITY=TAB_BAR_VISIBILITY=1;NAVIGATION_UI_FADE_DURATION=0;
    auto owner=windows.front();owner->resize(1200,900);owner->activateWindow();QApplication::setActiveWindow(owner);owner->setFocus();settle();
    test_menus(owner);check_status(owner);
    owner->set_hand_drawing_mode(true);owner->set_freehand_thickness(5);owner->validate_ui();
    check(owner->status_label_right->text()=="Pen 5" && owner->status_ink_swatch_->isVisible(),"pen status shows width and ink swatch");
    owner->status_label->grab().save("/tmp/sioyek-status-pen.png");
    owner->set_hand_drawing_mode(false);owner->toggle_brush_eraser();owner->validate_ui();
    check(owner->status_label_right->text()=="Eraser" && !owner->status_ink_swatch_->isVisible(),"eraser replaces pen status");owner->stop_brush_eraser();
    owner->validate_ui();float zoom=owner->dv()->get_zoom_level();owner->dv()->set_zoom_level(zoom*1.1f,true);owner->validate_ui();
    check(QString::fromStdWString(owner->get_status_string(false)).contains('%'),"zoom appears after changing it");settle(1700);owner->validate_ui();
    check(!QString::fromStdWString(owner->get_status_string(false)).contains('%'),"zoom disappears after brief delay");
    owner->opengl_widget->search_text(L"absent");settle();owner->validate_ui();
    check(QString::fromStdWString(owner->get_status_string(false)).contains("Match 0 / 0"),"search exposes compact match counter");
    owner->toggle_statusbar();owner->validate_ui();settle();check(!owner->status_label->isVisible(),"sb hides status even during search");
    owner->opengl_widget->cancel_search();owner->toggle_statusbar();
    owner->toggle_split();settle();auto right=owner->split_pane_.data();check(right,"split exists");
    right->dv()->goto_page(2);right->validate_ui();check_status(owner);check_status(right);
    check(QString::fromStdWString(right->get_status_string(false)).startsWith("3 /"),"split panes have independent counters");
    test_menus(right);owner->pdf_splitter_->setSizes({400,600});settle();check_status(owner);check_status(right);
    owner->toggle_split(Qt::Vertical);settle();check_status(owner);check_status(right);
    owner->on_config_file_changed(nullptr);right->on_config_file_changed(nullptr);
    owner->grab().save("/tmp/sioyek-compact-status-window.png");
    qInfo("PASS: floating list/table menus, Vim selection/deletion, compact status, zoom timeout, split geometry");result=0;QCoreApplication::exit(0);
}catch(const std::exception& e){qCritical("FAIL: %s",e.what());QCoreApplication::exit(1);}}
static void schedule(){QTimer::singleShot(1200,qApp,run);}Q_COREAPP_STARTUP_FUNCTION(schedule)
int main(int,char** argv){QStandardPaths::setTestModeEnabled(true);QTemporaryDir dir;auto path=dir.path()+"/menus.pdf";
QFile file(path);file.open(QIODevice::WriteOnly);file.write(blank_canvas_pdf(3));file.close();
QByteArray first=path.toUtf8();char option[]="--instance-name",name[]="sioyek-floating-ui-test";char* args[]={argv[0],first.data(),option,name};sioyek_application_main(4,args);return result;}
