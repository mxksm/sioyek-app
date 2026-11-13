#define main sioyek_application_main
#include "../pdf_viewer/main.cpp"
#undef main
#include "../pdf_viewer/blank_canvas.h"
#include <QTemporaryDir>
#include <QPdfWriter>
#include <QClipboard>
#include <QEventLoop>
#include <stdexcept>
extern std::wstring CONTROL_CLICK_COMMAND, SHIFT_CLICK_COMMAND, ALT_CLICK_COMMAND;
static int result=1;
static void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
static void settle(){QEventLoop loop;QTimer::singleShot(100,&loop,&QEventLoop::quit);loop.exec();}
static void mouse(MainWidget* pane,QEvent::Type type,QPoint point,Qt::KeyboardModifiers mods,Qt::MouseButton button=Qt::LeftButton){
 QMouseEvent event(type,point,pane->mapToGlobal(point),type==QEvent::MouseMove?Qt::NoButton:button,type==QEvent::MouseButtonRelease?Qt::NoButton:button,mods);QApplication::sendEvent(pane,&event);
}
static void test(MainWidget* pane){
 pane->setFocus();pane->dv()->set_zoom_level(0.8f,true);pane->dv()->goto_offset_within_page(0,240);pane->invalidate_render();settle();
 std::vector<fz_stext_char*> chars;get_flat_chars_from_stext_page(pane->doc()->get_stext_with_page_number(0),chars);check(chars.size()>30,"fixture has selectable text");
 for(int line:{0,1}) for(auto mods:{Qt::ControlModifier,Qt::MetaModifier,Qt::ShiftModifier,Qt::AltModifier}){
  std::vector<fz_stext_char*> row;
  for(auto ch:chars) if((ch->origin.y<180)==(line==0))row.push_back(ch);
  check(row.size()>12,"line contains text");
  auto point=[&](fz_stext_char* ch){const auto rect=fz_rect_from_quad(ch->quad);auto pos=pane->dv()->document_to_window_pos_in_pixels_uncentered(DocumentPos{0,(rect.x0+rect.x1)/2,(rect.y0+rect.y1)/2});return QPoint(pos.x,pos.y);};
  auto begin=point(row.front()),end=point(row[row.size()-2]);
  pane->recent_clicks.clear();pane->custom_status_message.clear();
  const auto before=pane->dv()->get_offsets();
  mouse(pane,QEvent::MouseButtonPress,begin,Qt::NoModifier);
  mouse(pane,QEvent::MouseMove,end,Qt::NoModifier);
  mouse(pane,QEvent::MouseButtonRelease,end,mods);settle();
  check(!pane->is_selecting && !pane->is_dragging,"modifier added during selection still finishes the drag");
  check(pane->custom_status_message.empty(),"modifier added during selection does not execute click macro");
  check(!pane->main_document_view->selected_character_rects.empty(),"selected text remains highlighted");
  auto selected=pane->get_selected_text();check(!selected.empty(),"selection has text");
  QKeyEvent copy(QEvent::KeyPress,Qt::Key_C,Qt::ControlModifier,"c");QApplication::sendEvent(pane,&copy);
  check(QApplication::clipboard()->text()==QString::fromStdWString(selected),"Cmd C copies selection");
  const auto after=pane->dv()->get_offsets();check(before.x==after.x && before.y==after.y,"copy gesture leaves PDF position unchanged");
 }
 // Genuine modified clicks retain the modifier from press even if it is released first.
 const QPoint point(pane->width()/2,pane->height()/2);
 for(auto mods:{Qt::ShiftModifier,Qt::MetaModifier,Qt::AltModifier}){
  pane->recent_clicks.clear();pane->custom_status_message.clear();
  mouse(pane,QEvent::MouseButtonPress,point,mods);mouse(pane,QEvent::MouseButtonRelease,point,Qt::NoModifier);
  check(pane->custom_status_message==L"modified-click","genuine modified click still invokes its macro");
 }
}
static void run(){try{
 // Replace external/navigation actions with an observable harmless macro.
 CONTROL_CLICK_COMMAND=SHIFT_CLICK_COMMAND=ALT_CLICK_COMMAND=L"set_status_string(modified-click)";
 auto owner=windows.front()->tab_owner();
 {QPdfWriter writer(QString::fromStdWString(owner->doc()->get_path()));writer.setResolution(72);writer.setPageSize(QPageSize(QSizeF(600,600),QPageSize::Point));writer.setPageMargins(QMarginsF());QPainter painter(&writer);painter.setFont(QFont("Helvetica",16));painter.drawText(70,140,"(a) Numbered selection example");painter.drawText(70,220,"Ordinary text selection example");}
 owner->reload(false);settle();test(owner);owner->toggle_split();settle();test(owner->split_pane_);
 qInfo("PASS: numbered/plain mouse selection, modifiers changing mid-drag, Cmd C, stable view, genuine modified clicks, both panes");result=0;QCoreApplication::exit(0);
}catch(const std::exception& e){qCritical("FAIL: %s",e.what());QCoreApplication::exit(1);}}
static void schedule(){QTimer::singleShot(1200,qApp,run);}Q_COREAPP_STARTUP_FUNCTION(schedule)
int main(int,char** argv){QStandardPaths::setTestModeEnabled(true);QTemporaryDir dir;auto path=dir.path()+"/selection.pdf";
 {QFile file(path);file.open(QIODevice::WriteOnly);file.write(blank_canvas_pdf(1));}
 QByteArray first=path.toUtf8();char option[]="--instance-name",name[]="sioyek-selection-modifiers-test";char* args[]={argv[0],first.data(),option,name};sioyek_application_main(4,args);return result;}
