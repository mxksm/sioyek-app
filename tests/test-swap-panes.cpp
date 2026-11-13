#define main sioyek_application_main
#include "../pdf_viewer/main.cpp"
#undef main
#include "../pdf_viewer/blank_canvas.h"
#include <QTemporaryDir>
#include <QEventLoop>
#include <stdexcept>
static int result=1;
static QString first_path,second_path;
static void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
static void settle(){QEventLoop loop;QTimer::singleShot(100,&loop,&QEventLoop::quit);loop.exec();}
static void shortcut(int code,Qt::KeyboardModifiers mods=Qt::ControlModifier|Qt::MetaModifier){
 auto focus=windows.front()->tab_owner()->focusWidget();check(focus,"focus exists");QKeyEvent key(QEvent::KeyPress,code,mods,QString(QChar(code+32)));QApplication::sendEvent(focus,&key);settle();
}
static void expect(MainWidget* pane,const DocumentViewState& state){
 auto actual=pane->main_document_view->get_state();
 check(actual.document_path==state.document_path,"pane PDF swapped");
 check(std::abs(actual.book_state.offset_y-state.book_state.offset_y)<0.01 && std::abs(actual.book_state.offset_x-state.book_state.offset_x)<0.01 && std::abs(actual.book_state.zoom_level-state.book_state.zoom_level)<0.001,"reading position and zoom preserved");
}
static void run(){try{
 auto owner=windows.front()->tab_owner();owner->activateWindow();owner->setFocus();settle();
 auto initial=owner->main_document_view->get_state();shortcut(Qt::Key_H);expect(owner,initial);check(!owner->split_pane_,"swap without a split is a no-op");
 owner->toggle_split();settle();auto other=owner->split_pane_.data();other->open_document(second_path.toStdWString());settle();
 const auto tabs=owner->window_tabs.paths();
 for(auto orientation:{Qt::Horizontal,Qt::Vertical}){
  if(owner->pdf_splitter_->orientation()!=orientation)owner->toggle_split(orientation);
  for(bool same_pdf:{false,true}){
   owner->open_document(first_path.toStdWString());other->open_document((same_pdf?first_path:second_path).toStdWString());
   owner->main_document_view->set_zoom_level(1.2f,true);owner->main_document_view->goto_offset_within_page(0,180);
   other->main_document_view->set_zoom_level(1.6f,true);other->main_document_view->goto_offset_within_page(2,260);settle();
   const auto first=owner->main_document_view->get_state(),second=other->main_document_view->get_state();
   const auto sizes=owner->pdf_splitter_->sizes();owner->focus_split_pane(0);
   shortcut(Qt::Key_H);expect(owner,second);expect(other,first);check(owner->active_pane()==other,"focus follows swapped PDF to second pane");
   shortcut(Qt::Key_L);expect(owner,first);expect(other,second);check(owner->active_pane()==owner,"second shortcut swaps back and restores focus");
   shortcut(Qt::Key_L,Qt::MetaModifier);check(owner->active_pane()==other,"Ctrl L still focuses second pane");expect(owner,first);expect(other,second);
   shortcut(Qt::Key_H,Qt::MetaModifier);check(owner->active_pane()==owner,"Ctrl H still focuses first pane");
   check(owner->pdf_splitter_->orientation()==orientation && owner->pdf_splitter_->sizes()==sizes,"swap preserves orientation and divider position");
   check(owner->window_tabs.paths()==tabs,"swap preserves tab order and count");
  }
 }
 qInfo("PASS: both swap shortcuts, focus navigation, orientations, same-PDF independent views, positions, tabs, and no split");result=0;QCoreApplication::exit(0);
}catch(const std::exception& e){qCritical("FAIL: %s",e.what());QCoreApplication::exit(1);}}
static void schedule(){QTimer::singleShot(1200,qApp,run);}Q_COREAPP_STARTUP_FUNCTION(schedule)
int main(int,char** argv){QStandardPaths::setTestModeEnabled(true);QTemporaryDir dir;first_path=dir.path()+"/first.pdf";second_path=dir.path()+"/second.pdf";for(auto path:{first_path,second_path}){QFile file(path);file.open(QIODevice::WriteOnly);file.write(blank_canvas_pdf(4));}
 QByteArray first=first_path.toUtf8();char option[]="--instance-name",name[]="sioyek-swap-panes-test";char* args[]={argv[0],first.data(),option,name};sioyek_application_main(4,args);return result;}
