#define main sioyek_application_main
#include "../pdf_viewer/main.cpp"
#undef main
#include "../pdf_viewer/blank_canvas.h"
#include <QTemporaryDir>
#include <QEventLoop>
#include <stdexcept>
static int result=1;
static void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
static void settle(){QEventLoop loop;QTimer::singleShot(70,&loop,&QEventLoop::quit);loop.exec();}
static void key(int code,QString text={}){auto target=windows.front()->tab_owner()->focusWidget();check(target,"focus exists");QKeyEvent event(QEvent::KeyPress,code,Qt::NoModifier,text);QApplication::sendEvent(target,&event);settle();}
static void type(QString text){for(auto c:text)key(c.toUpper().unicode(),QString(c));}
static void test(MainWidget* pane){
    pane->setFocus();pane->dv()->goto_page(0);pane->validate_render();settle();
    type("A");check(pane->pending_command_instance && pane->rect_select_mode,"Shift A requests a text rectangle");
    pane->pending_command_instance->set_rect_requirement(AbsoluteRect(fz_rect{50,50,250,110}));
    pane->advance_command(std::move(pane->pending_command_instance));settle();
    auto edit=pane->text_command_line_edit;auto prompt=pane->text_command_line_edit_container;
    check(prompt->isVisible() && !edit->isReadOnly(),"new annotation starts in insert mode");
    type("First note");key(Qt::Key_Return);
    check(!prompt->isVisible() && pane->doc()->get_bookmarks().back().description==L"First note","typing immediately creates and saves annotation");
    type("sa");check(pane->is_waiting_for_symbol(),"sa offers annotation labels");
    type(QString::fromStdString(get_tags(pane->dv()->get_visible_bookmark_indices().size()).back()));
    check(prompt->isVisible() && !edit->isReadOnly() && edit->text()=="First note","keyboard label reopens saved text in insert mode");
    type(" revised");key(Qt::Key_Return);
    check(pane->doc()->get_bookmarks().back().description==L"First note revised","keyboard editing saves revision");
    pane->set_selected_bookmark_index(-1);type("sa");type(QString::fromStdString(get_tags(pane->dv()->get_visible_bookmark_indices().size()).back()));
    type(" discarded");key(Qt::Key_Escape);check(prompt->isVisible() && edit->isReadOnly(),"first Escape enters normal mode");
    key(Qt::Key_Escape);check(!prompt->isVisible() && pane->doc()->get_bookmarks().back().description==L"First note revised","second Escape cancels live edits");
    const auto count=pane->doc()->get_bookmarks().size();type("A");
    pane->pending_command_instance->set_rect_requirement(AbsoluteRect(fz_rect{50,120,250,180}));pane->advance_command(std::move(pane->pending_command_instance));settle();
    type("Cancel this new note");key(Qt::Key_Escape);key(Qt::Key_Escape);
    check(pane->doc()->get_bookmarks().size()==count && pane->selected_bookmark_index==-1,"cancel removes unfinished annotation and clears selection");
    for(const auto& command:{"rename","save_workspace","goto_page_with_page_number","search"}){
        pane->advance_command(pane->command_manager->get_command_with_name(pane,command));settle();
        check(prompt->isVisible() && !edit->isReadOnly(),"text-only command starts ready to type");key(Qt::Key_Escape);key(Qt::Key_Escape);
    }
    pane->show_textbar(L"Text",L"alpha beta");settle();check(!edit->isReadOnly(),"generic text prompt uses insert default");
    key(Qt::Key_Escape);type("0cwdelta");check(edit->text()=="delta beta","normal-mode Vim editing remains available");key(Qt::Key_Escape);key(Qt::Key_Escape);
    type(":");check(!pane->current_widget_stack.empty() && pane->current_widget_stack.back()->findChild<QLineEdit*>()->isReadOnly(),"command picker still starts in normal mode");key(Qt::Key_Escape);
}
static void run(){try{auto owner=windows.front();owner->resize(1200,900);owner->activateWindow();settle();test(owner);owner->toggle_split();settle();auto right=owner->split_pane_.data();check(right,"split exists");test(right);qInfo("PASS: annotation creation/edit/cancel by keyboard, insert defaults, Vim editing, normal command lists, both split panes");result=0;QCoreApplication::exit(0);}catch(const std::exception& e){qCritical("FAIL: %s",e.what());QCoreApplication::exit(1);}}
static void schedule(){QTimer::singleShot(1200,qApp,run);}Q_COREAPP_STARTUP_FUNCTION(schedule)
int main(int,char** argv){QStandardPaths::setTestModeEnabled(true);QTemporaryDir dir;auto path=dir.path()+"/text.pdf";QFile file(path);file.open(QIODevice::WriteOnly);file.write(blank_canvas_pdf(3));file.close();QByteArray first=path.toUtf8();char option[]="--instance-name",name[]="sioyek-text-prompts-test";char* args[]={argv[0],first.data(),option,name};sioyek_application_main(4,args);return result;}
