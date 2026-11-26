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
 QByteArray settings="source_theme catppuccin_latte\n";
 for(int slot=0;slot<10;++slot) settings += "new_macro _theme_slot_"+QByteArray::number(slot)+" set_theme_slot("+QByteArray::number(slot)+")\n";
 write(prefs,settings+"source ./prefs_local.config\n");
 write(local,"theme_slot_1 nord\nhighlight_color_r #E53935\n");auto original=read(prefs);
 auto owner=windows.front()->tab_owner();auto previous=owner->config_manager;
 ConfigManager manager(default_config_path,Path(dir.filePath("auto.config").toStdWString()),{Path(prefs.toStdWString())});owner->config_manager=&manager;
 check(!QFile::exists(state) && read(local).contains("theme catppuccin_latte\n"),"fresh choice saved locally without state file");
 check(manager.available_themes().size()>=26,"full theme collection");
 check(manager.theme_metadata("cocoa").name=="Cocoa","palette has descriptive name");
 check(manager.theme_metadata("tokyo_dracula").name=="Tokyo Dracula","hybrid palette identifies its influences");
 check(manager.theme_metadata("rose_pine").name==QString::fromUtf8("Rosé Pine"),"UTF-8 names preserved");check(manager.active_theme()=="catppuccin_latte","fresh default");check(manager.config_watch_paths().contains(local),"local preferences watched");
 std::vector<std::string> changed;QString error;
 for(const auto& name:manager.available_themes())check(manager.select_theme(name,changed,error),"all palettes load");
 check(read(prefs)==original,"switching never rewrites preferences");
 auto before=read(local);check(!manager.select_theme("../prefs_user",changed,error) && read(local)==before,"invalid names preserve state");
 owner->setFocus();type(" th");check(!owner->current_widget_stack.empty(),"Space th opens picker");check(owner->command_hints_label->styleSheet().contains("border: 0;"),"key hints have no border");
 auto picker=owner->current_widget_stack.back();auto edit=picker->findChild<QLineEdit*>();check(edit && edit->isReadOnly(),"picker starts normal");check(edit->placeholderText()=="Search themes…","theme-specific search prompt");
 auto view=picker->findChild<QAbstractItemView*>();check(view && view->currentIndex().row()>15,"current theme starts near end of list");
 check(view->viewport()->rect().contains(view->visualRect(view->currentIndex())),"current theme is fully visible immediately on opening");
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
 const std::pair<int,QString> favorites[]={{0,"paper_white"},{1,"nord"},{2,"monokai_midnight"},{3,"slate"},{4,"tokyo_dracula"},{5,"cocoa"},{8,"gruvbox_light"},{9,"catppuccin_latte"}};
 for(const auto& [slot,name]:favorites){owner->setFocus();key(Qt::Key_0+slot,QString::number(slot),Qt::AltModifier);check(manager.active_theme()==name,"Option+number chooses its renamed palette");}
 // Startup restores the split before running startup_commands. Exercise that
 // order with either pane focused and either split orientation.
 for (auto orientation : {Qt::Horizontal, Qt::Vertical}) for (int focus : {0,1}) {
  owner->set_light_mode();owner->toggle_split(orientation);owner->focus_split_pane(focus);
  const auto layout=owner->capture_workspace();owner->toggle_split(orientation);
  owner->set_light_mode();owner->restore_session_layout(layout);
  check(owner->split_pane_,"session restores split");
  owner->execute_macro_if_enabled(L"toggle_custom_color");settle();
  for (auto pane : {owner,owner->split_pane_.data()})
   check(pane->opengl_widget->get_current_color_mode()==PdfViewOpenGLWidget::ColorPalette::Custom,"startup theme applies to both restored panes");
  owner->active_pane()->toggle_dark_mode();
  for (auto pane : {owner,owner->split_pane_.data()})
   check(pane->opengl_widget->get_current_color_mode()==PdfViewOpenGLWidget::ColorPalette::Dark,"dark mode applies to both panes");
  owner->active_pane()->toggle_custom_color_mode();
  for (auto pane : {owner,owner->split_pane_.data()})
   check(pane->opengl_widget->get_current_color_mode()==PdfViewOpenGLWidget::ColorPalette::Custom,"custom mode toggles together from either pane");
  owner->toggle_split(orientation);settle();
 }
 owner->toggle_split();settle();owner->apply_theme("dracula");settle();
 check(owner->opengl_widget->get_current_color_mode()==PdfViewOpenGLWidget::ColorPalette::Custom && owner->split_pane_->opengl_widget->get_current_color_mode()==PdfViewOpenGLWidget::ColorPalette::Custom,"both panes themed");
 auto red=manager.get_config<float>(L"highlight_color_r");check(abs(red[0]-229.f/255)<0.001f,"fixed pen colors retained");
 owner->show_theme_picker();settle();
 const auto saved_before_preview=read(local);
 check(manager.active_theme()=="dracula","opening picker preserves current theme");
 key(Qt::Key_K,"k");
 check(manager.active_theme()!="dracula","normal-mode navigation previews selected theme");
 check(read(local)==saved_before_preview,"preview does not write saved theme");
 check(owner->opengl_widget->get_current_color_mode()==PdfViewOpenGLWidget::ColorPalette::Custom && owner->split_pane_->opengl_widget->get_current_color_mode()==PdfViewOpenGLWidget::ColorPalette::Custom,"preview updates both split panes");
 key(Qt::Key_J,"j");check(manager.active_theme()=="dracula","moving back previews original row");
 key(Qt::Key_Up);check(manager.active_theme()!="dracula","arrow navigation also previews");
 key(Qt::Key_Escape);check(manager.active_theme()=="dracula" && read(local)==saved_before_preview,"Escape restores original without saving");
 owner->show_theme_picker();settle();type("icocoa");check(manager.active_theme()=="cocoa","search result previews");
 key(Qt::Key_Escape);check(manager.active_theme()=="cocoa","first Escape in insert mode returns to normal");
 key(Qt::Key_Return);check(manager.active_theme()=="cocoa" && read(local).contains("theme cocoa\n"),"Enter commits preview");
 owner->show_theme_picker();settle();type("iparchment");check(manager.active_theme()=="parchment","next preview starts from committed theme");
 key(Qt::Key_Escape);key(Qt::Key_Escape);check(manager.active_theme()=="cocoa" && read(local).contains("theme cocoa\n"),"cancel search preview restores committed theme");
 owner->apply_theme("dracula");
 owner->toggle_split();settle();owner->resize(1100,800);owner->show_theme_picker();settle();owner->grab().save("/tmp/sioyek-theme-picker.png");
 check(read(prefs)==original,"GUI switching also preserves preferences");
 ConfigManager reopened(default_config_path,Path(dir.filePath("auto.config").toStdWString()),{Path(prefs.toStdWString())});check(reopened.active_theme()=="dracula","selection survives reload/restart");

 // Recreate the old layout and verify migration, then explicit local precedence.
 write(local,"# Personal settings\ntheme_slot_1 nord\nstatus_badge_position top_left\n");
 write(state,"cocoa\n");
 ConfigManager migrated(default_config_path,Path(dir.filePath("auto.config").toStdWString()),{Path(prefs.toStdWString())});
 check(migrated.active_theme()=="cocoa" && read(local).contains("theme cocoa\n") && !QFile::exists(state),"legacy selection migrates and old state is removed");
 check(read(local).contains("# Personal settings") && read(local).contains("status_badge_position top_left"),"migration preserves local preferences");
 write(state,"dracula\n");
 ConfigManager explicit_choice(default_config_path,Path(dir.filePath("auto.config").toStdWString()),{Path(prefs.toStdWString())});
 check(explicit_choice.active_theme()=="cocoa" && !QFile::exists(state),"explicit local theme wins over stale legacy state");
 check(explicit_choice.save_status_badge_position("bottom_left",error),"badge corner saved beside theme");
 check(read(local).contains("theme cocoa\n") && read(local).contains("status_badge_position bottom_left"),"saving corner preserves theme");
 check(explicit_choice.select_theme("nord",changed,error),"save another theme");
 check(read(local).contains("status_badge_position bottom_left") && read(local).count("theme nord\n")==1,"theme save preserves corner without duplicates");
 owner->config_manager=previous;
 qInfo("PASS: all palettes, immutable preferences, saved selection, invalid names, Space th Vim search, local Option slot, split panes, pen overrides");result=0;QCoreApplication::exit(0);
 }catch(const std::exception& e){qCritical("FAIL: %s",e.what());QCoreApplication::exit(1);}}
static void schedule(){QTimer::singleShot(1200,qApp,run);}Q_COREAPP_STARTUP_FUNCTION(schedule)
int main(int,char** argv){QStandardPaths::setTestModeEnabled(true);QTemporaryDir dir;auto path=dir.path()+"/test.pdf";write(path,blank_canvas_pdf(3));QByteArray first=path.toUtf8();char option[]="--instance-name",name[]="sioyek-themes-test";char* args[]={argv[0],first.data(),option,name};sioyek_application_main(4,args);return result;}
