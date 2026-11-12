#define main sioyek_application_main
#include "../pdf_viewer/main.cpp"
#undef main
#include "../pdf_viewer/blank_canvas.h"
#include <QTemporaryDir>
#include <QEventLoop>
#include <QPushButton>
#include <stdexcept>
extern bool SHOW_COMMAND_HINTS;
static int result=1;
static void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
static void settle(int ms=60){QEventLoop loop;QTimer::singleShot(ms,&loop,&QEventLoop::quit);loop.exec();QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);}
static void key(int code,QString text={},Qt::KeyboardModifiers mods=Qt::NoModifier){
    auto target=windows.front()->tab_owner()->focusWidget();check(target,"focus exists");
    QKeyEvent event(QEvent::KeyPress,code,mods,text);QApplication::sendEvent(target,&event);settle();
}
static void type(QString text){for(auto c:text)key(c.toUpper().unicode(),QString(c));}
static int visible_windows(){int count=0;for(auto w:QApplication::topLevelWidgets())if(w->isVisible())++count;return count;}
static QWidget* error_panel(MainWidget* owner, int window_count){
    auto overlay=owner->findChild<QWidget*>("floatingErrorOverlay",Qt::FindDirectChildrenOnly);
    check(overlay && overlay->isVisible() && !overlay->isWindow(),"error is an embedded child, not a native window");
    check(visible_windows()==window_count,"error does not create an extra window");
    check(owner->focusWidget()==overlay,"error owns keyboard focus");
    check(overlay->geometry()==owner->rect(),"backdrop covers full window");
    auto panel=overlay->findChild<QWidget*>("floatingErrorPanel");
    check(panel && overlay->rect().contains(panel->geometry()),"error panel fits inside owner");
    return overlay;
}
static void test_prefixes(MainWidget* pane){
    pane->setFocus();pane->should_show_status_label_=true;SHOW_COMMAND_HINTS=true;
    key(Qt::Key_Space," ");
    check(pane->input_handler->is_waiting_for_key(),"Space is pending");
    check(pane->status_label_left->text().contains("Awaiting key"),"Space updates status without executing a command");
    auto hints=pane->command_hints_label;
    check(hints->isVisible() && hints->text().contains("ff") && hints->text().contains("fl") && hints->text().contains("fw"),"Space lists file commands");
    type("f");check(hints->isVisible() && hints->text().contains("<b>f</b>") && !hints->text().contains("<b>ff</b>"),"hints narrow after another prefix");
    QRect area(pane->central_widget->mapTo(pane,QPoint()),pane->central_widget->size());
    check(area.contains(hints->geometry()),"hints stay in invoking pane");
    pane->tab_owner()->grab().save("/tmp/sioyek-prefix-hints.png");
    key(Qt::Key_Escape);check(!hints->isVisible() && !pane->status_label_left->text().contains("Awaiting key"),"Escape clears prefix and status");
    type("g");check(hints->isVisible(),"g prefix has hints");type("g");check(!hints->isVisible() && !pane->input_handler->is_waiting_for_key(),"completed sequence hides hints");
    type("s");check(hints->isVisible(),"s prefix has hints");key(Qt::Key_Question,"?");
    check(!hints->isVisible() && !pane->input_handler->is_waiting_for_key(),"invalid continuation clears hints");
    type("3");check(pane->status_label_left->text().contains("Awaiting key"),"numeric count waits for command too");key(Qt::Key_Escape);
    SHOW_COMMAND_HINTS=false;key(Qt::Key_Space," ");check(!hints->isVisible() && pane->status_label_left->text().contains("Awaiting key"),"disabling hints preserves pending status");key(Qt::Key_Escape);SHOW_COMMAND_HINTS=true;
}
static void run(){try{
    auto owner=windows.front();owner->resize(1200,900);owner->activateWindow();owner->setFocus();settle();
    const int count=visible_windows();const auto original_size=owner->size();
    type("t");auto overlay=error_panel(owner,count);
    check(overlay->findChild<QLabel*>("floatingErrorMessage")->text().contains("table of contents"),"missing TOC uses embedded error");
    const auto y=owner->dv()->get_offset_y();type("jj");key(Qt::Key_Tab,{},Qt::MetaModifier);
    check(owner->dv()->get_offset_y()==y && owner->focusWidget()==overlay,"keys do not navigate PDF behind error");
    owner->resize(1050,800);settle();error_panel(owner,count);owner->grab().save("/tmp/sioyek-embedded-error.png");
    key(Qt::Key_Escape);check(!owner->findChild<QWidget*>("floatingErrorOverlay"),"Escape dismisses error");
    check(owner->focusWidget()==owner,"dismiss restores invoking focus");owner->resize(original_size);settle();
    test_prefixes(owner);
    owner->toggle_split();settle();auto pane=owner->split_pane_.data();check(pane,"split exists");pane->setFocus();
    pane->dv()->goto_page(0);pane->validate_render();settle();
    const auto lines=pane->visible_source_lines();check(!lines.empty(),"fixture has a visible source line");
    type("sd");check(pane->is_waiting_for_symbol(),"sd waits for a line label");
    type(QString::fromStdString(get_tags(lines.size()).front()));
    overlay=error_panel(owner,count);
    check(overlay->findChild<QLabel*>("floatingErrorMessage")->text().contains("SyncTeX"),"missing SyncTeX uses embedded error");
    key(Qt::Key_Return);check(owner->focusWidget()==pane,"error restores split-pane focus");
    test_prefixes(pane);
    pane->show_error_message(L"First error");pane->show_error_message(L"Replacement <not HTML>");settle();
    overlay=error_panel(owner,count);check(owner->findChildren<QWidget*>("floatingErrorOverlay").size()==1,"repeated errors replace rather than stack");
    check(overlay->findChild<QLabel*>("floatingErrorMessage")->textFormat()==Qt::PlainText,"error content remains plain text");
    overlay->findChild<QPushButton*>()->click();settle();check(owner->focusWidget()==pane,"Dismiss button preserves invoking pane");
    qInfo("PASS: embedded TOC/SyncTeX errors, focus, no native windows, Space/g/s/count hints, cancellation, split panes");result=0;QCoreApplication::exit(0);
}catch(const std::exception& e){qCritical("FAIL: %s",e.what());QCoreApplication::exit(1);}}
static void schedule(){QTimer::singleShot(1200,qApp,run);}Q_COREAPP_STARTUP_FUNCTION(schedule)
int main(int,char** argv){QStandardPaths::setTestModeEnabled(true);QTemporaryDir dir;auto path=dir.path()+"/errors.pdf";
QByteArray pdf="%PDF-1.4\n";std::vector<int> offsets{0};
auto object=[&](QByteArray body){offsets.push_back(pdf.size());pdf+=QByteArray::number(offsets.size()-1)+" 0 obj\n"+body+"\nendobj\n";};
object("<< /Type /Catalog /Pages 2 0 R >>");object("<< /Type /Pages /Kids [3 0 R 4 0 R 5 0 R] /Count 3 >>");
for(int i=0;i<3;++i)object("<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Resources << /Font << /F1 6 0 R >> >> /Contents 7 0 R >>");
object("<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>");
QByteArray stream="BT /F1 14 Tf 72 672 Td (A source line without SyncTeX.) Tj ET\n";
object("<< /Length "+QByteArray::number(stream.size())+" >>\nstream\n"+stream+"endstream");
int xref=pdf.size();pdf+="xref\n0 8\n0000000000 65535 f \n";
for(size_t i=1;i<offsets.size();++i)pdf+=QByteArray::number(offsets[i]).rightJustified(10,'0')+" 00000 n \n";
pdf+="trailer\n<< /Size 8 /Root 1 0 R >>\nstartxref\n"+QByteArray::number(xref)+"\n%%EOF\n";
QFile file(path);file.open(QIODevice::WriteOnly);file.write(pdf);file.close();
QByteArray first=path.toUtf8();char option[]="--instance-name",name[]="sioyek-errors-hints-test";char* args[]={argv[0],first.data(),option,name};sioyek_application_main(4,args);return result;}
