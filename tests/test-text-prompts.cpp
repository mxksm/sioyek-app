#define main sioyek_application_main
#include "../pdf_viewer/main.cpp"
#undef main
#include "../pdf_viewer/blank_canvas.h"
#include <QTemporaryDir>
#include <QEventLoop>
#include <QClipboard>
#include <stdexcept>
extern int NAVIGATION_UI_FADE_DURATION;
static int result=1;
static void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
static void settle(){QEventLoop loop;QTimer::singleShot(70,&loop,&QEventLoop::quit);loop.exec();}
static void key(int code,QString text={},Qt::KeyboardModifiers modifiers=Qt::NoModifier){auto target=windows.front()->tab_owner()->focusWidget();check(target,"focus exists");QKeyEvent event(QEvent::KeyPress,code,modifiers,text);QApplication::sendEvent(target,&event);settle();}
static void type(QString text){for(auto c:text)key(c.toUpper().unicode(),QString(c));}
static void test_case_toggle(QLineEdit* edit){
    QApplication::clipboard()->setText("keep clipboard");
    edit->setText("aBc DéF 12");type("0v2l~");
    check(edit->text()=="AbC DéF 12" && edit->isReadOnly() && !edit->hasSelectedText(),"visual tilde toggles selected letters and returns to normal");
    check(QApplication::clipboard()->text()=="keep clipboard","case toggle preserves clipboard");
    type("u");check(edit->text()=="aBc DéF 12","case toggle is one undoable edit");
    type("02lv2h~");check(edit->text()=="AbC DéF 12","backward visual selection toggles same range");
    edit->setText("aBcD");type("03~");check(edit->text()=="AbCD" && edit->cursorPosition()==3,"normal tilde accepts counts and advances");
    edit->setText(QString::fromUtf8("Éé Αβ 𐐀𐐨 é🙂 12!"));type("0V~");
    check(edit->text()==QString::fromUtf8("éÉ αΒ 𐐨𐐀 É🙂 12!"),"Unicode letters toggle without damaging graphemes or emoji");
    edit->clear();type("~");check(edit->text().isEmpty(),"empty field case toggle is harmless");
}
static void test_substitute(QLineEdit* edit){
    edit->setText("abcdef");type("02lxu");check(edit->text()=="abcdef" && edit->cursorPosition()==2,"x undo restores the same normal-mode character");
    edit->setText("abcdef");type("0l3xu");check(edit->text()=="abcdef" && edit->cursorPosition()==1,"counted deletion undo returns to first restored character");
    edit->setText(QString::fromUtf8("a🙂bc"));type("0lxu");check(edit->text()==QString::fromUtf8("a🙂bc") && edit->cursorPosition()==1,"undo restores Unicode cursor boundary");

    edit->setText("alpha");type("0ls");check(edit->text()=="apha" && !edit->isReadOnly(),"s removes current character and enters insert mode");
    type("s");check(edit->text()=="aspha","insert-mode s stays literal");key(Qt::Key_Escape);
    edit->setText("abcdef");type("03sX");check(edit->text()=="Xdef","counted s replaces multiple characters");key(Qt::Key_Escape);
    edit->setText("abcdef");type("0v2lsX");check(edit->text()=="Xdef","visual s replaces forward selection");key(Qt::Key_Escape);
    edit->setText("abcdef");type("02lv2hsX");check(edit->text()=="Xdef","visual s replaces backward selection");key(Qt::Key_Escape);
    edit->setText(QString::fromUtf8("é🙂z"));type("02sX");check(edit->text()=="Xz","s counts graphemes without splitting combining marks or emoji");key(Qt::Key_Escape);
    edit->setText("abc");type("$9sX");check(edit->text()=="abX","s stops at end of text");key(Qt::Key_Escape);
    edit->clear();type("sX");check(edit->text()=="X" && !edit->isReadOnly(),"s enters insert mode on empty field");key(Qt::Key_Escape);
}
static void drag_prompt(FloatingTextPrompt* prompt, const QPoint& point, const QPoint& delta) {
    QWidget* target = prompt->childAt(point);
    if (!target) target = prompt;
    const QPoint global = prompt->mapToGlobal(point);
    auto send = [&](QEvent::Type type, QPoint position, Qt::MouseButton button, Qt::MouseButtons buttons) {
        QMouseEvent event(type, target->mapFromGlobal(position), position, button, buttons, Qt::NoModifier);
        QApplication::sendEvent(target, &event);
    };
    send(QEvent::MouseButtonPress, global, Qt::LeftButton, Qt::LeftButton);
    send(QEvent::MouseMove, global + delta, Qt::NoButton, Qt::LeftButton);
    send(QEvent::MouseButtonRelease, global + delta, Qt::LeftButton, Qt::NoButton);
}
static void test_prompt_drag(FloatingTextPrompt* prompt, QPlainTextEdit* note) {
    const QRect text = note->geometry();
    const QList<QPoint> points = {
        {2, prompt->height()/2}, {prompt->width()-3, prompt->height()/2},
        {prompt->width()/2, 2}, {prompt->width()/2, prompt->height()-3},
        {text.center().x(), text.top()-2}, {text.center().x(), text.bottom()+2},
        prompt->heading->geometry().center()
    };
    for (const auto& point : points) {
        const QPoint before = prompt->pos();
        drag_prompt(prompt, point, {8,8});
        check(prompt->pos()==before+QPoint(8,8), "all note panel margins and heading drag");
        prompt->move(before);
    }
    const QPoint before = prompt->pos();
    drag_prompt(prompt, text.center(), {8,8});
    check(prompt->pos()==before, "drag inside text editor never moves panel");
    note->setFocus();
}
static void test(MainWidget* pane){
    pane->setFocus();pane->dv()->goto_page(0);pane->validate_render();settle();
    type("A");pane->rect_select_begin=AbsoluteDocumentPos{50,50};key(Qt::Key_Escape);
    check(!pane->rect_select_begin && !pane->rect_select_end,"Escape clears unfinished rectangle corners");
    type("A");check(pane->pending_command_instance && pane->rect_select_mode,"Shift A requests a text rectangle");
    pane->pending_command_instance->set_rect_requirement(AbsoluteRect(fz_rect{50,50,250,110}));
    pane->advance_command(std::move(pane->pending_command_instance));settle();
    auto edit=pane->text_command_line_edit;auto prompt=pane->text_command_line_edit_container;
    auto note=prompt->findChild<QPlainTextEdit*>();

    check(prompt->isVisible() && note && note->isVisible() && !note->isReadOnly() && prompt->height()>200,"new annotation opens tall multiline editor in insert mode");
    test_prompt_drag(static_cast<FloatingTextPrompt*>(prompt),note);
    type("First note");key(Qt::Key_Escape);type("02lxu");check(note->toPlainText()=="First note" && note->textCursor().position()==2,"multiline x undo restores original cursor");type("0V~");check(note->toPlainText()=="fIRST NOTE","annotation supports visual case toggle");type("u");key(Qt::Key_Return,{},Qt::ControlModifier);
    check(!prompt->isVisible() && pane->doc()->get_bookmarks().back().description==L"First note","typing immediately creates and saves annotation");
    type("sa");check(pane->is_waiting_for_symbol(),"sa offers annotation labels");
    type(QString::fromStdString(get_tags(pane->dv()->get_visible_bookmark_indices().size()).back()));
    check(prompt->isVisible() && !note->isReadOnly() && note->toPlainText()=="First note","keyboard label reopens saved text in insert mode");
    type(" revised");key(Qt::Key_Return,{},Qt::ControlModifier);
    check(pane->doc()->get_bookmarks().back().description==L"First note revised","keyboard editing saves revision");
    pane->set_selected_bookmark_index(-1);type("sa");type(QString::fromStdString(get_tags(pane->dv()->get_visible_bookmark_indices().size()).back()));
    type(" discarded");key(Qt::Key_Escape);check(prompt->isVisible() && note->isReadOnly(),"first Escape enters normal mode");
    key(Qt::Key_Escape);key(Qt::Key_Escape);
    check(prompt->isVisible() && note->isReadOnly(),"repeated Escape never closes an edited note");
    type(":oops");key(Qt::Key_Return);check(prompt->isVisible(),"unknown ex command keeps editor open");
    key(Qt::Key_Escape);check(prompt->isVisible() && pane->tab_owner()->focusWidget()==note,"Escape cancels ex command only");
    type(":q");key(Qt::Key_Return);
    check(!prompt->isVisible() && pane->doc()->get_bookmarks().back().description==L"First note revised",":q restores existing note text");
    const auto count=pane->doc()->get_bookmarks().size();type("A");
    pane->pending_command_instance->set_rect_requirement(AbsoluteRect(fz_rect{50,120,250,180}));pane->advance_command(std::move(pane->pending_command_instance));settle();
    type("First line");key(Qt::Key_Return);type("Second line");
    check(note->toPlainText()=="First line\nSecond line","Enter inserts newline");
    key(Qt::Key_Escape);type("voThird line");
    check(note->toPlainText()=="First line\nSecond line\nThird line" && !note->isReadOnly(),"visual o opens a new line below");
    auto heading=pane->text_command_line_edit_label;
    const auto old_pos=prompt->pos();const auto point=heading->rect().center();
    const auto global=heading->mapToGlobal(point);
    QMouseEvent press(QEvent::MouseButtonPress,point,global,Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);QApplication::sendEvent(heading,&press);
    QMouseEvent move(QEvent::MouseMove,point+QPoint(0,80),global+QPoint(0,80),Qt::NoButton,Qt::LeftButton,Qt::NoModifier);QApplication::sendEvent(heading,&move);
    QMouseEvent release(QEvent::MouseButtonRelease,point,global+QPoint(0,80),Qt::LeftButton,Qt::NoButton,Qt::NoModifier);QApplication::sendEvent(heading,&release);settle();
    check(prompt->pos()!=old_pos && prompt->parentWidget()->rect().contains(prompt->geometry()),"note can be dragged and remains inside window");
    pane->tab_owner()->grab().save("/tmp/sioyek-multiline-note.png");
    key(Qt::Key_Escape);key(Qt::Key_Escape);type(":q");key(Qt::Key_Return);
    check(pane->doc()->get_bookmarks().size()==count && pane->selected_bookmark_index==-1,"cancel removes unfinished annotation and clears selection");
    type("sa");type(QString::fromStdString(get_tags(pane->dv()->get_visible_bookmark_indices().size()).back()));
    key(Qt::Key_Return);type("new line");key(Qt::Key_Escape);type("k0v2l~");
    check(note->toPlainText().startsWith("fIRst note revised\nnew line"),"multiline Vim movement and case toggle edit the intended line");
    pane->setFocus();pane->handle_escape();settle();
    check(prompt->isVisible(),"Escape outside editor also protects unsaved note");
    check(prompt->findChildren<QPushButton*>().empty(),"note has no confirmation buttons");
    type(":wq");
    pane->tab_owner()->grab().save("/tmp/sioyek-note-ex-command.png");
    key(Qt::Key_Return);
    check(!prompt->isVisible() && pane->doc()->get_bookmarks().back().description==L"fIRst note revised\nnew line",":wq saves multiline note using keyboard");
    for(const auto& command:{"rename","save_workspace","goto_page_with_page_number","search"}){
        pane->advance_command(pane->command_manager->get_command_with_name(pane,command));settle();
        check(prompt->isVisible() && !edit->isReadOnly(),"text-only command starts ready to type");key(Qt::Key_Escape);key(Qt::Key_Escape);
    }
    pane->show_textbar(L"Text",L"alpha beta");settle();check(!edit->isReadOnly(),"generic text prompt uses insert default");
    key(Qt::Key_Escape);type("0cwdelta");check(edit->text()=="delta beta","normal-mode Vim editing remains available");key(Qt::Key_Escape);key(Qt::Key_Escape);
    pane->show_textbar(L"Case test",L"Ab");settle();type("~");check(edit->text()=="Ab~","insert-mode tilde stays literal");key(Qt::Key_Escape);
    test_case_toggle(edit);test_substitute(edit);key(Qt::Key_Escape);
    type(":");check(!pane->current_widget_stack.empty() && pane->current_widget_stack.back()->findChild<QLineEdit*>()->isReadOnly(),"command picker still starts in normal mode");
    test_case_toggle(pane->current_widget_stack.back()->findChild<QLineEdit*>());test_substitute(pane->current_widget_stack.back()->findChild<QLineEdit*>());key(Qt::Key_Escape);
}
static void test_focus(MainWidget* owner){
    auto right=owner->split_pane_.data();
    owner->status_visibility_override_=right->status_visibility_override_=1;
    owner->validate_ui();right->validate_ui();owner->focus_split_pane(0);settle();
    check(owner->status_label->styleSheet()!=right->status_label->styleSheet(),"active pane has distinct status badge color");
    check(!owner->pane_focus_indicator_->isVisible(),"visible status needs no extra focus badge");
    owner->status_visibility_override_=right->status_visibility_override_=0;owner->focus_split_pane(1);owner->validate_ui();right->validate_ui();settle();
    check(right->pane_focus_indicator_->isVisible() && !owner->pane_focus_indicator_->isVisible(),"hidden status still identifies active pane");
    check(right->central_widget->rect().contains(right->pane_focus_indicator_->geometry()),"focus badge fits inside its pane");
}
static void run(){try{NAVIGATION_UI_FADE_DURATION=0;auto owner=windows.front();owner->resize(1200,900);owner->activateWindow();settle();test(owner);owner->toggle_split();settle();auto right=owner->split_pane_.data();check(right,"split exists");test(right);test_focus(owner);qInfo("PASS: annotation creation/edit/cancel by keyboard, insert defaults, Vim editing, normal command lists, both split panes");result=0;QCoreApplication::exit(0);}catch(const std::exception& e){qCritical("FAIL: %s",e.what());QCoreApplication::exit(1);}}
static void schedule(){QTimer::singleShot(1200,qApp,run);}Q_COREAPP_STARTUP_FUNCTION(schedule)
int main(int,char** argv){QStandardPaths::setTestModeEnabled(true);QTemporaryDir dir;auto path=dir.path()+"/text.pdf";QFile file(path);file.open(QIODevice::WriteOnly);file.write(blank_canvas_pdf(3));file.close();QByteArray first=path.toUtf8();char option[]="--instance-name",name[]="sioyek-text-prompts-test";char* args[]={argv[0],first.data(),option,name};sioyek_application_main(4,args);return result;}
