#define main sioyek_application_main
#include "../pdf_viewer/main.cpp"
#undef main
#include "../pdf_viewer/blank_canvas.h"
#include <QTemporaryDir>
#include <QEventLoop>
#include <QPushButton>
#include <stdexcept>
extern bool SHOW_COMMAND_HINTS;
extern std::wstring STATUS_BADGE_POSITION;
extern std::wstring TAB_BAR_POSITION;
extern int NAVIGATION_UI_FADE_DURATION;
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
static void toggle_test_status(MainWidget* pane){
    const int target=pane->status_label->isVisible()?0:1;
    while(pane->status_visibility_mode()!=target)pane->toggle_statusbar();
    settle(220);
}
static void test_prefixes(MainWidget* pane){
    NAVIGATION_UI_FADE_DURATION=0;
    while(pane->status_visibility_mode()!=1)pane->toggle_statusbar();
    pane->setFocus();pane->should_show_status_label_=true;SHOW_COMMAND_HINTS=true;
    key(Qt::Key_Space," ");
    check(pane->input_handler->is_waiting_for_key(),"Space is pending");
    check(pane->status_label_left->text().contains("Awaiting key"),"Space updates status without executing a command");
    auto hints=pane->command_hints_label;
    check(hints->isVisible() && hints->text().contains("ff") && hints->text().contains("fl") && hints->text().contains("fw"),"Space lists file commands");
    check(pane->status_label->isVisible() && !hints->text().contains("Awaiting key"),"visible status owns the awaiting-key label");
    toggle_test_status(pane);settle();
    check(!pane->status_label->isVisible() && hints->text().contains("Awaiting key"),"hidden status moves heading into hints");
    toggle_test_status(pane);settle();
    check(pane->status_label->isVisible() && !hints->text().contains("Awaiting key"),"showing status removes duplicate heading");
    type("f");check(hints->isVisible() && hints->text().contains("<b>f</b>") && !hints->text().contains("<b>ff</b>"),"hints narrow after another prefix");
    QRect area(pane->central_widget->mapTo(pane,QPoint()),pane->central_widget->size());
    check(area.contains(hints->geometry()),"hints stay in invoking pane");
    for (const auto& position : {L"bottom_right",L"bottom_left",L"top_right",L"top_left",L"invalid"}) {
        STATUS_BADGE_POSITION=position;
        pane->update_status_geometry();pane->update_command_hints_position();settle();
        const auto badge=pane->status_label->geometry();
        const bool left=STATUS_BADGE_POSITION==L"bottom_left" || STATUS_BADGE_POSITION==L"top_left";
        const bool top=STATUS_BADGE_POSITION==L"top_left" || STATUS_BADGE_POSITION==L"top_right";
        check(area.contains(badge),"badge stays within its pane");
        check(left ? badge.left()==area.left()+8 : badge.right()==area.right()-8,"badge anchors to chosen horizontal edge");
        check(top ? badge.center().y()<area.center().y() : badge.bottom()==area.bottom()-8,"badge anchors to chosen vertical edge");
        check(area.contains(hints->geometry()),"hints fit for every badge corner");
        check(!badge.intersects(hints->geometry()),"hints do not overlap badge");
    }
    STATUS_BADGE_POSITION=L"bottom_right";
    pane->update_status_geometry();pane->update_command_hints_position();
    pane->tab_owner()->grab().save("/tmp/sioyek-prefix-hints.png");
    key(Qt::Key_Escape);check(!hints->isVisible() && !pane->status_label_left->text().contains("Awaiting key"),"Escape clears prefix and status");
    toggle_test_status(pane);key(Qt::Key_Space," ");
    check(hints->isVisible() && hints->text().contains("Awaiting key"),"new hints keep heading when status is hidden");
    key(Qt::Key_Escape);toggle_test_status(pane);
    type("m");check(!hints->isVisible() && pane->status_label_left->text().contains("Awaiting key"),"symbol prompt retains status label without hints");key(Qt::Key_Escape);
    type("g");check(hints->isVisible(),"g prefix has hints");type("g");check(!hints->isVisible() && !pane->input_handler->is_waiting_for_key(),"completed sequence hides hints");
    type("s");check(hints->isVisible(),"s prefix has hints");key(Qt::Key_Question,"?");
    check(!hints->isVisible() && !pane->input_handler->is_waiting_for_key(),"invalid continuation clears hints");
    type("3");check(pane->status_label_left->text().contains("Awaiting key"),"numeric count waits for command too");key(Qt::Key_Escape);
    SHOW_COMMAND_HINTS=false;key(Qt::Key_Space," ");check(!hints->isVisible() && pane->status_label_left->text().contains("Awaiting key"),"disabling hints preserves pending status");key(Qt::Key_Escape);SHOW_COMMAND_HINTS=true;
}
static void test_focus_badge(MainWidget* owner){
    auto other=owner->split_pane_.data();
    NAVIGATION_UI_FADE_DURATION=120;
    for(auto pane : {owner,other})
        while(pane->status_visibility_mode()!=2)pane->toggle_statusbar();
    for(const auto& position : {L"bottom_right",L"bottom_left",L"top_right",L"top_left"}) {
        STATUS_BADGE_POSITION=position;
        for(int side : {0,1,0}) {
            owner->focus_split_pane(side);settle();
            auto pane=side==0?owner:other;
            auto marker=pane->pane_focus_indicator_;
            check(marker && marker->isVisible(),"automatic mode keeps an active focus cue");
            check(!(side==0?other:owner)->pane_focus_indicator_->isVisible(),"inactive pane has no focus cue");
            const auto bounds=marker->geometry();
            const QRect area(pane->central_widget->mapTo(pane,QPoint()),pane->central_widget->size());
            const bool left=STATUS_BADGE_POSITION==L"bottom_left" || STATUS_BADGE_POSITION==L"top_left";
            const bool top=STATUS_BADGE_POSITION==L"top_left" || STATUS_BADGE_POSITION==L"top_right";
            check(left?bounds.left()==area.left()+8:bounds.right()==area.right()-8,"focus cue follows horizontal corner");
            check(top?bounds.center().y()<area.center().y():bounds.bottom()==area.bottom()-8,"focus cue follows vertical corner");
            pane->reveal_navigation_ui();pane->validate_ui();settle(160);
            check(marker->isVisible() && marker->geometry()==bounds,"focus cue stays steady while badge appears");
            check(!bounds.intersects(pane->status_label->geometry()),"focus cue does not overlap status badge");
            check(marker->height()==pane->status_label->height(),"focus cue and compact status badge have identical heights");
            pane->status_reveal_timer_->stop();pane->status_zoom_timer_->stop();
            pane->validate_ui();settle(200);
            check(!pane->status_label->isVisible(),"automatic badge fades out");
            check(marker->isVisible() && marker->geometry()==bounds,"focus cue stays steady after badge fades");
        }
    }
    NAVIGATION_UI_FADE_DURATION=0;
    STATUS_BADGE_POSITION=L"bottom_right";
    for(auto pane : {owner,other})while(pane->status_visibility_mode()!=1)pane->toggle_statusbar();
}
static void test_badge_picker(MainWidget* pane){
    QTemporaryDir dir;
    auto local=dir.path()+"/prefs_local.config";
    auto user=dir.path()+"/prefs_user.config";
    auto write=[](const QString& path,const QByteArray& data){QFile f(path);check(f.open(QIODevice::WriteOnly),"create local config fixture");f.write(data);};
    write(local,"# Keep this comment\nstatus_bar_show_chapter 0\nstatus_badge_position bottom_right\n");
    write(user,"source ./prefs_local.config\n");
    ConfigManager config(Path(L""),Path(L""),{Path(user.toStdWString())});
    auto original=pane->config_manager;
    pane->config_manager=&config;
    auto command=pane->command_manager->get_command_with_name(pane,"select_status_badge_position");
    check(bool(command),"badge corner command is registered");command->run();settle();
    key(Qt::Key_J,"j");key(Qt::Key_Return);
    pane->config_manager=original;
    check(STATUS_BADGE_POSITION==L"bottom_left","corner picker applies selected corner");
    QFile saved(local);check(saved.open(QIODevice::ReadOnly),"read saved local config");
    auto contents=saved.readAll();saved.close();
    check(contents.contains("# Keep this comment") && contents.contains("status_bar_show_chapter 0"),"preserve unrelated local preferences");
    check(contents.count("status_badge_position")==1 && contents.contains("status_badge_position bottom_left"),"replace rather than duplicate setting");
    STATUS_BADGE_POSITION=L"bottom_right";
    ConfigManager reopened(Path(L""),Path(L""),{Path(user.toStdWString())});
    check(STATUS_BADGE_POSITION==L"bottom_left","new config manager restores saved corner");
    pane->config_manager=&reopened;
    pane->show_status_badge_position_picker();settle();
    key(Qt::Key_J,"j");key(Qt::Key_Escape);
    pane->config_manager=original;
    check(STATUS_BADGE_POSITION==L"bottom_left","Escape leaves saved corner unchanged");
    STATUS_BADGE_POSITION=L"bottom_right";
    pane->config_manager=&reopened;
    TAB_BAR_POSITION=L"top_center";
    auto tab_command=pane->command_manager->get_command_with_name(pane,"select_tab_bar_position");
    check(bool(tab_command),"tab position command exists");tab_command->run();settle();
    key(Qt::Key_J,"j");key(Qt::Key_Return);
    check(TAB_BAR_POSITION==L"top_right","tab picker applies position");
    TAB_BAR_POSITION=L"top_center";
    ConfigManager restart(Path(L""),Path(L""),{Path(user.toStdWString())});
    check(TAB_BAR_POSITION==L"top_right","tab position survives restart");
    pane->show_tab_bar_position_picker();settle();key(Qt::Key_J,"j");key(Qt::Key_Escape);
    check(TAB_BAR_POSITION==L"top_right","cancel preserves tab position");
    pane->config_manager=original;
    auto owner=pane->tab_owner();
    while(owner->tabs_visibility_mode()!=1)owner->toggle_titlebar();
    for(const auto& position : {L"top_left",L"top_center",L"top_right",L"bottom_left",L"bottom_center",L"bottom_right"}) {
        TAB_BAR_POSITION=position;owner->update_floating_tabs();settle();
        auto tabs=owner->findChild<QTabBar*>("floatingDocumentTabs");
        check(tabs && tabs->isVisible(),"tabs visible for position test");
        auto bounds=tabs->geometry();auto name=QString::fromStdWString(position);
        check(name.startsWith("bottom")?bounds.bottom()==owner->height()-9:bounds.top()==8,"tabs follow top/bottom edge");
        check(name.endsWith("left")?bounds.left()==8:name.endsWith("right")?bounds.right()==owner->width()-9:std::abs(bounds.center().x()-owner->rect().center().x())<=1,"tabs follow horizontal alignment");
        STATUS_BADGE_POSITION=name.startsWith("bottom")?L"bottom_right":L"top_right";
        owner->update_split_focus();
        for(auto view : {owner,owner->split_pane_.data()}) {
            view->update_status_geometry();
            QRect badge(owner->mapFromGlobal(view->status_label->mapToGlobal(QPoint())),view->status_label->size());
            check(!badge.intersects(bounds),"status badge avoids tabs at every position");
        }
    }
    TAB_BAR_POSITION=L"top_center";STATUS_BADGE_POSITION=L"bottom_right";
    owner->update_floating_tabs();
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
    test_focus_badge(owner);
    test_badge_picker(pane);
    qInfo("PASS: embedded errors, key hints, badge corners in split panes, corner picker and restart persistence");result=0;QCoreApplication::exit(0);
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
