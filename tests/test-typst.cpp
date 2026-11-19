#define main sioyek_application_main
#include "../pdf_viewer/main.cpp"
#undef main
#include <QEventLoop>
#include <QTemporaryDir>
#include <stdexcept>
static int result=1;
static void check(bool ok,const char* msg){if(!ok)throw std::runtime_error(msg);}
static void settle(int ms=80){QEventLoop loop;QTimer::singleShot(ms,&loop,&QEventLoop::quit);loop.exec();}
static void key(MainWidget* pane,int code,QString text){QKeyEvent event(QEvent::KeyPress,code,Qt::NoModifier,text);QApplication::sendEvent(pane,&event);}
static QString remote(QString expression){QProcess p;p.start(qEnvironmentVariable("SIOYEK_TYPST_NVIM"),{"--server",qEnvironmentVariable("SIOYEK_TYPST_SERVER"),"--remote-expr",expression});check(p.waitForFinished(5000),"editor answers");return QString::fromUtf8(p.readAllStandardOutput()).trimmed();}
static void wait_jump(MainWidget* pane){
    for(int i=0;i<150 && pane->typst_source_process_;++i)settle();
    check(!pane->typst_source_process_,"inverse search completed asynchronously");
    check(QFileInfo(remote("expand('%:p')")).canonicalFilePath()==QFileInfo(qEnvironmentVariable("SIOYEK_TYPST_SOURCE")).canonicalFilePath(),"native PDF jump reaches correct Typst source");
    check(remote("line('.')")=="2","native PDF jump reaches correct Typst line");
}
static void run(){try{
    auto owner=windows.front();owner->resize(1000,800);owner->activateWindow();owner->setFocus();settle();
    owner->toggle_split();settle();
    for(auto pane : {owner,owner->split_pane_.data()}){
        pane->setFocus();pane->dv()->set_zoom_level(1,true);pane->dv()->set_offset_y(200);pane->dv()->set_offset_x(0);pane->validate_render();settle();
        remote("execute('enew')");
        key(pane,Qt::Key_S,"s");key(pane,Qt::Key_D,"d");
        check(pane->is_waiting_for_symbol(),"sd displays Typst line labels");
        auto lines=pane->opengl_widget->get_highlight_word_rects();check(!lines.empty(),"Typst lines are selectable");
        auto tag=get_tags(lines.size()).front();
        for(char c:tag)key(pane,QChar(c).toUpper().unicode(),QString(QChar(c)));
        if(!pane->typst_source_process_) {
            for(auto label:owner->findChildren<QLabel*>())if(label->isVisible())qWarning()<<label->objectName()<<label->text();
            qWarning()<<QString::fromStdWString(pane->doc()->get_path())<<QDir::homePath();
        }
        check(bool(pane->typst_source_process_),"sd starts Typst helper");wait_jump(pane);
        remote("execute('enew')");
        auto point=pane->dv()->document_to_window_pos_in_pixels_uncentered(DocumentPos{0,45,36});
        pane->synctex_under_pos(WindowPos(point.x,point.y));
        check(bool(pane->typst_source_process_),"mouse source lookup uses Typst helper");wait_jump(pane);
    }
    qInfo("PASS: native sd line labels and PDF-coordinate lookup reach Typst source in both split panes");result=0;
}catch(const std::exception& e){qCritical("FAIL: %s",e.what());}QCoreApplication::exit(result);}
static void schedule(){QTimer::singleShot(1200,qApp,run);}Q_COREAPP_STARTUP_FUNCTION(schedule)
int main(int, char** argv) {
    QStandardPaths::setTestModeEnabled(true);
    QTemporaryDir temp;
    qputenv("XDG_DATA_HOME", temp.path().toUtf8());
    char option[] = "--instance-name", name[] = "sioyek-typst-test";
    char* args[] = {argv[0], argv[1], option, name};
    sioyek_application_main(4, args);
    return result;
}
