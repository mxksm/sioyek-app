#define main sioyek_application_main
#include "../pdf_viewer/main.cpp"
#undef main
#include "../pdf_viewer/blank_canvas.h"
#include <QTabBar>
#include <QToolButton>
#include <QTemporaryDir>
#include <QEventLoop>
#include <stdexcept>
static int result=1;
static void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
static void settle(){QEventLoop loop;QTimer::singleShot(120,&loop,&QEventLoop::quit);loop.exec();}
static void run(){try{
 auto owner=windows.front();auto initial=owner->doc()->get_path();owner->window_tabs={};owner->open_document(initial);owner->resize(1000,800);settle();auto bar=owner->floating_tabs_;
 check(bar && bar->isVisible(),"tabs visible");check(!owner->windowFlags().testFlag(Qt::WindowTitleHint),"no native title row");
 check(bar->parentWidget()==owner && bar->y()==8,"tabs overlay owner");check(abs(bar->geometry().center().x()-owner->rect().center().x())<=1,"tabs centered");
 auto geometry=owner->centralWidget()->geometry();check(geometry.top()==0,"PDF starts at top of window");
 for(int i=0;i<4;++i){owner->toggle_titlebar();settle();check(owner->centralWidget()->geometry()==geometry,"st preserves viewport");}
 auto path=owner->doc()->get_path();QFileInfo info(QString::fromStdWString(path));auto other=info.dir().filePath("second.pdf");QFile::copy(QString::fromStdWString(path),other);owner->open_document(other.toStdWString());settle();
 check(bar->count()==2,"two tabs");auto point=bar->tabRect(0).center();
 QMouseEvent press(QEvent::MouseButtonPress,point,bar->mapToGlobal(point),Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);QApplication::sendEvent(bar,&press);
 QMouseEvent release(QEvent::MouseButtonRelease,point,bar->mapToGlobal(point),Qt::LeftButton,Qt::NoButton,Qt::NoModifier);QApplication::sendEvent(bar,&release);settle();check(owner->doc()->get_path()==path,"tab selection opens document");
 owner->toggle_split();settle();check(owner->split_pane_ && bar->parentWidget()==owner,"split shares overlay");owner->toggle_split();settle();
 owner->grab().save("/tmp/sioyek-overlay-tabs-window.png");
 qInfo("PASS: titleless window, floating centered tabs, full PDF viewport, st preserves geometry, tab selection, split overlay");result=0;QCoreApplication::exit(0);
 }catch(const std::exception& e){qCritical("FAIL: %s",e.what());QCoreApplication::exit(1);}}
static void schedule(){QTimer::singleShot(1200,qApp,run);}Q_COREAPP_STARTUP_FUNCTION(schedule)
int main(int,char** argv){QStandardPaths::setTestModeEnabled(true);QTemporaryDir dir;auto path=dir.path()+"/first.pdf";QFile file(path);file.open(QIODevice::WriteOnly);file.write(blank_canvas_pdf(3));file.close();QByteArray first=path.toUtf8();char option[]="--instance-name",name[]="sioyek-overlay-tabs-test";char* args[]={argv[0],first.data(),option,name};sioyek_application_main(4,args);return result;}
