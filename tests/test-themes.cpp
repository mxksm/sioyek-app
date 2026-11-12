#define main sioyek_application_main
#include "../pdf_viewer/main.cpp"
#undef main
#include "../pdf_viewer/blank_canvas.h"
#include <QTemporaryDir>
#include <QEventLoop>
#include <QTabBar>
#include <stdexcept>
static int result=1;
static void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
static QByteArray read(const QString& path){QFile f(path);check(f.open(QIODevice::ReadOnly),"read fixture");return f.readAll();}
static void write(const QString& path,const QByteArray& content){QFile f(path);check(f.open(QIODevice::WriteOnly),"write fixture");f.write(content);}
static void settle(){QEventLoop loop;QTimer::singleShot(100,&loop,&QEventLoop::quit);loop.exec();}
static void key(int code,QString text={},Qt::KeyboardModifiers modifiers=Qt::NoModifier){auto target=windows.front()->tab_owner()->focusWidget();check(target,"focus exists");QKeyEvent event(QEvent::KeyPress,code,modifiers,text);QApplication::sendEvent(target,&event);settle();}
static void type(QString text){for(auto c:text)key(c.toUpper().unicode(),QString(c));}
static void run(){try{
 QTemporaryDir dir;QDir(dir.path()).mkdir("themes");auto source=QDir(QDir::current().filePath("../sioyek/themes"));
 for(const auto& file:source.entryList({"*.config"},QDir::Files))check(QFile::copy(source.filePath(file),dir.filePath("themes/"+file)),"copy palettes");
 const auto prefs=dir.filePath("prefs_user.config"),local=dir.filePath("prefs_local.config"),state=dir.filePath("theme_state");
 write(prefs,"source_theme catppuccin_latte\nnew_macro _theme_slot_1 set_theme_slot(1)\nsource ./prefs_local.config\n");
 write(local,"theme_slot_1 nord\nhighlight_color_r #E53935\n");auto original=read(prefs);
 auto owner=windows.front()->tab_owner();auto previous=owner->config_manager;
 ConfigManager manager(default_config_path,Path(dir.filePath("auto.config").toStdWString()),{Path(prefs.toStdWString())});owner->config_manager=&manager;
 check(manager.available_themes().size()>=26,"full theme collection");
 check(manager.theme_metadata("gray_5").name=="Cocoa","legacy palette has descriptive name");
 check(manager.theme_metadata("wezterm").name=="Tokyo Dracula","hybrid palette identifies its influences");
 check(manager.theme_metadata("rose_pine").name==QString::fromUtf8("Rosé Pine"),"UTF-8 names preserved");check(manager.active_theme()=="catppuccin_latte","fresh default");check(manager.config_watch_paths().contains(local),"local preferences watched");
 std::vector<std::string> changed;QString error;
 for(const auto& name:manager.available_themes())check(manager.select_theme(name,changed,error),"all palettes load");
 check(read(prefs)==original,"switching never rewrites preferences");
 auto before=read(state);check(!manager.select_theme("../prefs_user",changed,error) && read(state)==before,"invalid names preserve state");
 owner->setFocus();type(" th");check(!owner->current_widget_stack.empty(),"Space th opens picker");check(owner->command_hints_label->styleSheet().contains("border: 0;"),"key hints have no border");
 auto picker=owner->current_widget_stack.back();auto edit=picker->findChild<QLineEdit*>();check(edit && edit->isReadOnly(),"picker starts normal");check(edit->placeholderText()=="Search themes…","theme-specific search prompt");
 for (const auto size : {QSize(1400,900),QSize(700,1100),QSize(1100,400)}) {
  owner->resize(size);settle();
  check(picker->width()==std::min(1050,owner->width()-48),"theme picker uses broad width");
  check(picker->height()<=picker->width()*3/4,"theme picker cannot become a tall column");
  check(!owner->floating_tabs_ || !owner->floating_tabs_->isVisible() || picker->geometry().top()>owner->floating_tabs_->geometry().bottom()+8,"picker clears floating tabs");
  check(owner->rect().contains(picker->geometry()),"picker stays inside window");
 }
 type("inord");check(edit->text()=="nord","picker searchable");key(Qt::Key_Escape);key(Qt::Key_Return);
 check(manager.active_theme()=="nord","Enter selects theme");
 owner->apply_theme("paper_sepia");owner->setFocus();key(Qt::Key_1,"1",Qt::AltModifier);check(manager.active_theme()=="nord","Option slot respects local override");
 owner->toggle_split();settle();owner->apply_theme("dracula");settle();
 check(owner->opengl_widget->get_current_color_mode()==PdfViewOpenGLWidget::ColorPalette::Custom && owner->split_pane_->opengl_widget->get_current_color_mode()==PdfViewOpenGLWidget::ColorPalette::Custom,"both panes themed");
 auto red=manager.get_config<float>(L"highlight_color_r");check(abs(red[0]-229.f/255)<0.001f,"fixed pen colors retained");
 owner->toggle_split();settle();owner->resize(1100,800);owner->show_theme_picker();settle();owner->grab().save("/tmp/sioyek-theme-picker.png");
 check(read(prefs)==original,"GUI switching also preserves preferences");
 ConfigManager reopened(default_config_path,Path(dir.filePath("auto.config").toStdWString()),{Path(prefs.toStdWString())});check(reopened.active_theme()=="dracula","selection survives reload/restart");
 owner->config_manager=previous;
 qInfo("PASS: all palettes, immutable preferences, saved selection, invalid names, Space th Vim search, local Option slot, split panes, pen overrides");result=0;QCoreApplication::exit(0);
 }catch(const std::exception& e){qCritical("FAIL: %s",e.what());QCoreApplication::exit(1);}}
static void schedule(){QTimer::singleShot(1200,qApp,run);}Q_COREAPP_STARTUP_FUNCTION(schedule)
int main(int,char** argv){QStandardPaths::setTestModeEnabled(true);QTemporaryDir dir;auto path=dir.path()+"/test.pdf";write(path,blank_canvas_pdf(3));QByteArray first=path.toUtf8();char option[]="--instance-name",name[]="sioyek-themes-test";char* args[]={argv[0],first.data(),option,name};sioyek_application_main(4,args);return result;}
