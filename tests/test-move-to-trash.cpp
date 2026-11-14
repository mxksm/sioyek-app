#define main sioyek_application_main
#include "../pdf_viewer/main.cpp"
#undef main
#include "../pdf_viewer/blank_canvas.h"
#include <QTemporaryDir>
#include <QEventLoop>
#include <QUuid>
#include <stdexcept>
static int result=1;
static QString folder, filename;
static void check(bool ok,const char* msg){if(!ok)throw std::runtime_error(msg);}
static void settle(int ms=100){QEventLoop l;QTimer::singleShot(ms,&l,&QEventLoop::quit);l.exec();}
static void key(QWidget* target,int code){QKeyEvent e(QEvent::KeyPress,code,Qt::NoModifier);QApplication::sendEvent(target,&e);settle();}
static void run(){try{
    auto w=windows.front();
    w->open_document(filename.toStdWString());
    auto path=w->doc()->get_path();
    check(QFileInfo(QString::fromStdWString(path)).canonicalFilePath()==QFileInfo(filename).canonicalFilePath(),"test operates only on its generated fixture");
    const auto checksum=w->doc()->get_checksum();
    w->execute_macro_if_enabled(L"move_to_trash");settle();
    check(!w->current_widget_stack.empty(),"command opens floating confirmation");
    auto picker=dynamic_cast<DocumentPicker*>(w->current_widget_stack.back());
    check(picker,"confirmation uses document picker");
    auto list=picker->findChild<QListView*>();
    check(list && list->currentIndex().row()==0,"Cancel is selected by default");
    key(picker->findChild<QLineEdit*>(),Qt::Key_Return);
    check(QFileInfo::exists(filename),"Cancel leaves PDF intact");
    w->toggle_split();settle();
    auto right=w->split_pane_.data();check(right && right->doc()->get_path()==path,"PDF open in both panes");
    w->execute_macro_if_enabled(L"move_to_trash");settle();
    picker=dynamic_cast<DocumentPicker*>(w->current_widget_stack.back());check(picker,"confirmation reopened");
    list=picker->findChild<QListView*>();list->setCurrentIndex(list->model()->index(1,0));
    key(picker->findChild<QLineEdit*>(),Qt::Key_Return);settle(200);
    check(!QFileInfo::exists(filename),"confirmed PDF moved out of original location");
    check(w->doc()->get_path()!=path && right->doc()->get_path()!=path,"both panes switch away from trashed PDF");
    check(w->window_tabs.index(path)<0,"trashed tab removed");
    std::vector<OpenedBookState> recents;w->db_manager->select_opened_book(checksum,recents);
    check(recents.empty(),"trashed PDF removed from recents");
    for(auto& tab:w->closed_tabs_)check(tab.path!=path,"trashed PDF excluded from reopen stack");
    check(!w->move_document_to_trash(blank_canvas_path().toStdWString()),"blank canvas protected");
    check(!w->move_document_to_trash(path),"missing file fails safely");
    // Remove only the uniquely named fixture that this test moved to macOS Trash.
    const auto trashed=QDir::home().filePath(".Trash/"+QFileInfo(filename).fileName());
    if(QFileInfo::exists(trashed)) check(QFile::remove(trashed),"test fixture cleaned up");
    qInfo("PASS: confirmation cancellation/default, real system Trash, split tabs, recents, reopen history, blank protection and missing file");
    result=0;QCoreApplication::exit(0);
}catch(const std::exception& e){qCritical("FAIL: %s",e.what());QCoreApplication::exit(1);}}
static void schedule(){QTimer::singleShot(1200,qApp,run);}Q_COREAPP_STARTUP_FUNCTION(schedule)
int main(int,char** argv){QStandardPaths::setTestModeEnabled(true);QTemporaryDir dir;folder=dir.path();
filename=folder+"/sioyek-trash-test-"+QUuid::createUuid().toString(QUuid::WithoutBraces)+".pdf";
QFile f(filename);f.open(QIODevice::WriteOnly);f.write(blank_canvas_pdf(2));f.close();
QByteArray path=filename.toUtf8();char option[]="--instance-name",name[]="sioyek-trash-test";char* args[]={argv[0],path.data(),option,name};sioyek_application_main(4,args);return result;}
