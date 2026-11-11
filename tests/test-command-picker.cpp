#define main sioyek_application_main
#include "../pdf_viewer/main.cpp"
#undef main
#include "../pdf_viewer/blank_canvas.h"
#include <QTemporaryDir>
#include <QEventLoop>
#include <QTableView>
#include <QClipboard>
#include <QTextBlock>
#include <stdexcept>

static int result=1;
static void check(bool ok,const char* message) { if(!ok) throw std::runtime_error(message); }
static void settle(int ms=80) { QEventLoop loop; QTimer::singleShot(ms,&loop,&QEventLoop::quit); loop.exec(); }
static void key(int code,QString text={},Qt::KeyboardModifiers mods=Qt::NoModifier) {
    auto target=QApplication::focusWidget(); check(target,"focus exists");
    QKeyEvent press(QEvent::KeyPress,code,mods,text); QApplication::sendEvent(target,&press);
    if(auto focus=QApplication::focusWidget()) {
        QKeyEvent release(QEvent::KeyRelease,code,mods,text); QApplication::sendEvent(focus,&release);
    }
    settle();
}
static CommandSelector* open(MainWidget* pane) {
    pane->setFocus(); key(Qt::Key_Colon,":",Qt::ShiftModifier);
    check(!pane->current_widget_stack.empty(),"colon opens a command picker");
    auto picker=dynamic_cast<CommandSelector*>(pane->current_widget_stack.back());
    check(picker,"colon uses CommandSelector");
    auto owner=pane->tab_owner();
    check(picker->parentWidget()==owner && owner->rect().contains(picker->geometry()) &&
        (picker->geometry().center()-owner->rect().center()).manhattanLength()<=2,"picker floats centered over the whole window");
    check(picker->width()==std::min(1050,owner->width()-48) && picker->height()<=720 && picker->height()<owner->height(),"picker has floating margins");
    auto edit=picker->findChild<QLineEdit*>();
    check(edit && edit->hasFocus() && edit->isReadOnly(),"command search starts in normal mode");
    auto hit=owner->childAt(edit->mapTo(owner,edit->rect().center()));
    check(hit && (hit==edit || edit->isAncestorOf(hit)),"command search is above PDF surfaces");
    return picker;
}
static void test_upstream(MainWidget* widget) {
    TocNode leaf; leaf.title=L"leaf"; TocNode root; root.children={&leaf};
    std::vector<TocNode*> roots{&root};
    check(get_toc_node_from_indices(roots,{0,0})==&leaf,"nested TOC with one root remains selectable");
    check(!get_toc_node_from_indices(roots,{}) && !get_toc_node_from_indices(roots,{2}) &&
        !get_toc_node_from_indices(roots,{-1}),"invalid TOC selections rejected");
    widget->advance_command(widget->command_manager->get_command_with_name(widget,"move_down_smooth"));
    widget->advance_command(widget->command_manager->get_command_with_name(widget,"move_up_smooth"));
    widget->set_last_performed_command({});
    check(!widget->is_moving(),"overlapping smooth commands stop on release");
    check(widget->movement_timer.isValid(),"movement uses an initialized monotonic timer");
    widget->handle_toggle_smooth_scroll_mode();
    check(widget->smooth_scroll_timer.isValid(),"smooth scrolling has its own monotonic timer");
    widget->handle_toggle_smooth_scroll_mode();
}
static void type(QString text) {
    for (auto ch : text) key(ch.toUpper().unicode(), QString(ch));
}
static void test_prompt(MainWidget* pane) {
    pane->advance_command(pane->command_manager->get_command_with_name(pane,"rename")); settle();
    auto prompt=dynamic_cast<FloatingTextPrompt*>(pane->text_command_line_edit_container);
    auto edit=pane->text_command_line_edit;
    check(prompt && prompt->isVisible() && edit->isReadOnly(),"rename opens floating prompt in normal mode");
    check(edit->text()==QFileInfo(QString::fromStdWString(pane->doc()->get_path())).completeBaseName(),"rename defaults to current filename without extension");
    auto owner=pane->tab_owner();
    check(prompt->parentWidget()==owner && (prompt->geometry().center()-owner->rect().center()).manhattanLength()<=2,"text prompt centers over both panes");
    check(edit->cursorPosition()==0 && !edit->hasSelectedText(),"normal cursor starts on first character");
    key(Qt::Key_Escape); check(!prompt->isVisible() && !pane->pending_command_instance,"normal Escape cancels command");
    pane->show_textbar(L"New Name",L"alpha beta"); settle();
    prompt->grab().save("/tmp/sioyek-text-prompt.png");
    type("yy"); check(QApplication::clipboard()->text()=="alpha beta","yy yanks prompt text");
    type("w"); check(edit->cursorPosition()==6,"w moves to next word");
    type("cw"); check(!edit->isReadOnly() && edit->text()=="alpha ","cw changes current word");
    type("gamma"); key(Qt::Key_Escape);
    check(prompt->isVisible() && edit->isReadOnly() && edit->text()=="alpha gamma","Escape returns insert to normal without closing");
    type("u"); check(edit->text()=="alpha beta","normal undo restores the word before cw");
    key(Qt::Key_R,"r",Qt::MetaModifier); check(edit->text()=="alpha gamma","Ctrl R redoes edit on macOS");
    type("0vlx"); check(edit->text()=="pha gamma","visual selection can be deleted");
    type("cc"); type("replacement"); key(Qt::Key_Escape); key(Qt::Key_Escape);
    check(!prompt->isVisible(),"second Escape closes prompt");
    pane->show_textbar(L"Name",L"saved"); settle();
    check(edit->isReadOnly() && edit->cursorPosition()==0,"reopening resets normal mode and cursor");
    auto y=pane->main_document_view->get_offset_y();
    key(Qt::Key_Down); key(Qt::Key_Up); type("jk");
    check(pane->main_document_view->get_offset_y()==y,"prompt navigation does not move underlying PDF");
    key(Qt::Key_Escape);
    pane->show_textbar(L"Password",L"",true); settle();
    check(!edit->isReadOnly() && edit->echoMode()==QLineEdit::Password,"password prompt stays masked and editable");
    key(Qt::Key_Escape);
    pane->advance_command(pane->command_manager->get_command_with_name(pane,"rename")); settle();
    const auto old=QString::fromStdWString(pane->doc()->get_path());
    type("A"); type("-renamed"); key(Qt::Key_Return);
    check(!prompt->isVisible() && !QFileInfo::exists(old) && QFileInfo::exists(QString::fromStdWString(pane->doc()->get_path())),"Enter submits actual rename from invoking pane");
}
static void test_cheatsheet(MainWidget* pane) {
    pane->setFocus(); key(Qt::Key_Question, "?", Qt::ShiftModifier);
    check(pane->current_widget_stack.empty(), "? is unbound");
    pane->set_current_widget(new CommandCheatsheet(pane)); pane->show_current_widget(); settle();
    auto sheet = dynamic_cast<CommandCheatsheet*>(pane->current_widget_stack.back());
    check(sheet, "explicit cheatsheet command remains available");
    auto text = sheet->findChild<QPlainTextEdit*>(); auto search = sheet->findChild<QLineEdit*>();
    check(text && text->hasFocus() && text->isReadOnly(), "cheatsheet starts as a read-only normal-mode document");
    auto owner = pane->tab_owner();
    check(sheet->parentWidget() == owner && owner->rect().contains(sheet->geometry()) &&
        (sheet->geometry().center()-owner->rect().center()).manhattanLength()<=2, "cheatsheet spans and centers on split owner");
    const auto old_size = owner->size(); owner->resize(old_size + QSize(40, 20)); settle();
    check((sheet->geometry().center()-owner->rect().center()).manhattanLength()<=2, "cheatsheet follows owner resize");
    owner->resize(old_size); settle();
    const auto content = text->toPlainText();
    check(content.contains("cheatsheet") && content.contains("toggle_dark_mode") && content.contains("OTHER COMMANDS"), "sheet includes live shortcuts and other commands");
    const auto before = pane->main_document_view->get_offset_y();
    type("ggkkkk"); key(Qt::Key_Up); key(Qt::Key_Return);
    check(text->textCursor().blockNumber()==0 && !pane->current_widget_stack.empty(), "top navigation clamps; heading Enter does nothing");
    type("3j"); check(text->textCursor().blockNumber()==3, "counted j moves document cursor");
    type("0w"); check(text->textCursor().positionInBlock()>0, "word motion moves across row");
    type("0hhhh"); check(text->textCursor().blockNumber()==3 && text->textCursor().positionInBlock()==0, "h stays on current line");
    type("e"); check(!text->textCursor().atBlockEnd(), "e rests on a character rather than after the word");
    type("gg"); type("G"); key(Qt::Key_Down);
    check(text->textCursor().blockNumber()==text->document()->blockCount()-1, "G reaches end and clamps");
    type("gg");
#ifdef Q_OS_MACOS
    key(Qt::Key_D, {}, Qt::MetaModifier);
#else
    key(Qt::Key_D, {}, Qt::ControlModifier);
#endif
    check(text->textCursor().blockNumber()>0, "Ctrl+D moves half a screen");
    check(pane->main_document_view->get_offset_y()==before, "cheatsheet navigation never moves PDF");
    type("gg/"); check(search->hasFocus() && search->isVisible(), "/ focuses search");
    key(Qt::Key_Up); key(Qt::Key_Down);
    check(pane->main_document_view->get_offset_y()==before, "search field arrow keys never move PDF");
    type("toggle"); key(Qt::Key_Return);
    auto first = text->textCursor().position(); type("n");
    check(text->textCursor().position()!=first, "n advances in-place search");
    type("N"); check(text->textCursor().position()==first, "N returns to previous match");
    check(text->toPlainText()==content && text->extraSelections().size()>2, "search highlights matches without filtering document");
    type("/"); search->setText("no_matching_command_xyz"); key(Qt::Key_Escape);
    check(text->hasFocus() && text->textCursor().position()==first, "Escape cancels search and restores cursor");
    type("/"); search->setText("toggle_dark_mode"); key(Qt::Key_Return);
    const auto yank_position = text->textCursor().position();
    type("yy"); check(QApplication::clipboard()->text().contains("toggle_dark_mode") &&
        text->textCursor().position()==yank_position, "yy yanks current command row without moving cursor");
    sheet->grab().save("/tmp/sioyek-cheatsheet.png");
    auto callback = pane->on_command_done; int calls=0; std::string selected;
    pane->on_command_done = [&](std::string name, std::string) { ++calls; selected=name; };
    key(Qt::Key_Return);
    check(calls==1 && selected=="toggle_dark_mode" && pane->current_widget_stack.empty(), "Enter dispatches once to invoking pane after closing");
    pane->on_command_done=callback;
    pane->setFocus(); pane->set_current_widget(new CommandCheatsheet(pane)); pane->show_current_widget(); settle();
    sheet=dynamic_cast<CommandCheatsheet*>(pane->current_widget_stack.back());
    search=sheet->findChild<QLineEdit*>();
    type("/"); search->setText("rename"); key(Qt::Key_Return); key(Qt::Key_Return);
    check(pane->current_widget_stack.empty() && pane->text_command_line_edit_container->isVisible() && pane->pending_command_instance,
        "executing rename opens its actual floating prompt");
    key(Qt::Key_Escape);
}

static void run() {
    try {
        auto owner=windows.front(); owner->resize(1000,760); settle();
        QApplication::setActiveWindow(owner);
        test_upstream(owner);
        test_cheatsheet(owner);
        test_prompt(owner);
        auto picker=open(owner); auto edit=picker->findChild<QLineEdit*>();
        type("i"); type("toggle");
        check(!edit->isReadOnly() && edit->text()=="toggle","i enters insert mode and filters commands");
        key(Qt::Key_Escape);
        check(edit->isReadOnly() && !owner->current_widget_stack.empty(),"first Escape returns to normal mode");
        type("j"); check(picker->get_view()->currentIndex().row()==1,"j selects next command");
        type("k"); check(picker->get_view()->currentIndex().row()==0,"k selects previous command");
        type("G"); check(picker->get_view()->currentIndex().row()==picker->get_view()->model()->rowCount()-1,"G selects last command");
        type("gg"); check(picker->get_view()->currentIndex().row()==0,"gg selects first command");
        type("yy"); check(QApplication::clipboard()->text()==picker->get_view()->currentIndex().siblingAtColumn(0).data().toString(),"yy copies selected command name");
        type("0vlx"); check(edit->text()=="ggle","visual editing changes command query");
        type("u"); check(edit->text()=="toggle","u restores query");
        edit->setText("toggle"); settle();
        picker->grab().save("/tmp/sioyek-command-picker.png");
        check(picker->get_view()->model()->rowCount()>1,"command filtering produces results");
        key(Qt::Key_Escape); check(owner->current_widget_stack.empty(),"Escape dismisses command picker");
        owner->toggle_split(); settle();
        auto right=owner->split_pane_.data(); check(right,"split pane exists");
        test_prompt(right);
        test_cheatsheet(right);
        for(auto pane:{owner,right}) {
            picker=open(pane); edit=picker->findChild<QLineEdit*>();
            auto before=pane->main_document_view->get_offset_y();
            edit->selectAll();
            for(int i=0;i<3;++i) key(Qt::Key_Up);
            check(pane->main_document_view->get_offset_y()==before,"top boundary navigation never moves PDF");
            owner->pdf_splitter_->setSizes({650,350}); settle();
            check((picker->geometry().center()-owner->rect().center()).manhattanLength()<=2,"picker remains centered after split resize");
            edit->setText("zzzz_no_command_matches"); settle();
            check(picker->get_view()->model()->columnCount()==2,"empty search preserves command and shortcut columns");
            key(Qt::Key_Return); check(!pane->current_widget_stack.empty(),"Enter on empty results does nothing");
            edit->setText("=background_color"); settle();
            check(picker->get_view()->model()->rowCount()>0 && picker->get_view()->model()->index(0,0).data().toString().startsWith("setconfig_"),
                "configuration-prefix search preserved");
            edit->setText("2"); settle(); key(Qt::Key_Return);
            check(pane->current_widget_stack.empty() && pane->main_document_view->get_center_page_number()==1,"numeric jump applies to invoking pane");
            picker=open(pane); edit=picker->findChild<QLineEdit*>();
            auto original=pane->on_command_done; int calls=0; std::string selected;
            pane->on_command_done=[&](std::string name,std::string){++calls;selected=name;};
            edit->setText("3"); settle(); key(Qt::Key_Return);
            check(calls==1 && selected=="3","numeric command dispatched exactly once");
            picker=open(pane); edit=picker->findChild<QLineEdit*>();
            edit->setText("toggle_dark_mode"); settle();
            picker->on_select(picker->get_view()->model()->index(0,1));
            check(calls==2 && selected=="toggle_dark_mode","shortcut column dispatches the command name");
            pane->on_command_done=original;
        }
        qInfo("PASS: floating cheatsheet navigation, search and dispatch in both panes; command picker and prompt regressions");
        result=0; QCoreApplication::exit(0);
    } catch(const std::exception& e) { qCritical("FAIL: %s",e.what()); QCoreApplication::exit(1); }
}
static void schedule(){QTimer::singleShot(1200,qApp,run);}
Q_COREAPP_STARTUP_FUNCTION(schedule)
int main(int argc,char** argv) {
    QStandardPaths::setTestModeEnabled(true); QTemporaryDir dir;
    const auto path=dir.path()+"/commands.pdf"; QFile file(path); file.open(QIODevice::WriteOnly);file.write(blank_canvas_pdf(3));file.close();
    QByteArray first=path.toUtf8();char option[]="--instance-name",name[]="sioyek-command-picker-test";
    char* args[]={argv[0],first.data(),option,name};sioyek_application_main(4,args);return result;
}
