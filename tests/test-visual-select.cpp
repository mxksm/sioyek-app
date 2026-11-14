#define main sioyek_application_main
#include "../pdf_viewer/main.cpp"
#undef main
#include "../pdf_viewer/pdf_text_cursor.h"
#include <QTemporaryDir>
#include <QEventLoop>
#include <QClipboard>
#include <QMimeData>
#include <stdexcept>

static int result = 1;
static void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
static void settle() {
    QEventLoop loop; QTimer::singleShot(100, &loop, &QEventLoop::quit); loop.exec();
}
static void key(MainWidget* pane, int code, const QString& text = {}, Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
    QKeyEvent event(QEvent::KeyPress, code, modifiers, text);
    QApplication::sendEvent(pane, &event);
}
static void type(MainWidget* pane, const QString& text) {
    for (QChar c : text) key(pane, c.toUpper().unicode(), QString(c), c.isUpper() ? Qt::ShiftModifier : Qt::NoModifier);
}
static void run() {
    auto clipboard = std::make_unique<QMimeData>();
    if (const auto saved = QApplication::clipboard()->mimeData())
        for (const auto& format : saved->formats()) clipboard->setData(format, saved->data(format));
    try {
        auto owner = windows.front();
        const auto pdf = owner->doc()->get_path();
        check(owner->doc()->num_pages() == 3, "fixture includes an empty middle page");
        std::vector<fz_stext_char*> chars;
        get_flat_chars_from_stext_page(owner->doc()->get_stext_with_page_number(0), chars);
        std::wstring text;
        for (auto c : chars) text.push_back(c->c);
        const auto index = text.find(L"gamma");
        check(index != std::wstring::npos, "fixture word is present");
        DocumentRect target(fz_rect_from_quad(chars[index]->quad), 0);
        owner->resize(1000,800);
        for (auto orientation : {Qt::Horizontal, Qt::Vertical}) {
            owner->toggle_split(orientation); settle();
            for (auto pane : {owner, owner->split_pane_.data()}) {
                pane->open_document(pdf);
                pane->dv()->set_zoom_level(1.0f,true);
                pane->dv()->set_offset_x(0);
                pane->dv()->set_offset_y(target.to_absolute(pane->doc()).center().y);
                pane->setFocus(); settle();
                type(pane,"i");
                check(pane->pdf_text_cursor_ && !pane->pdf_text_cursor_->selecting() &&
                    pane->get_selected_text().empty(), "i places a normal caret without selecting");
                key(pane,Qt::Key_Escape);
                check(!pane->pdf_text_cursor_, "Escape removes the keyboard-placed caret");
                pane->dv()->selected_character_rects = {target.to_absolute(pane->doc())};
                type(pane,"v");
                check(pane->pdf_text_cursor_ && !pane->pending_command_instance,
                    "v enters direct selection with no label prompt");
                check(pane->dv()->text_cursor_rect.has_value(), "caret is rendered in the focused pane");
                type(pane,"e");
                check(pane->get_selected_text() == L"gamma", "e selects through the word end");
                type(pane,"h");
                check(pane->get_selected_text() == L"gamm", "h shrinks the selection live");
                type(pane,"biw");
                check(pane->get_selected_text() == L"gamma", "iw selects the current word");
                type(pane,"aw");
                check(pane->get_selected_text() == L"gamma ", "aw includes following whitespace");
                const auto end = pane->dv()->text_cursor_rect->rect.x0;
                type(pane,"o");
                check(pane->dv()->text_cursor_rect->rect.x0 < end && pane->get_selected_text() == L"gamma ",
                    "o swaps endpoints without changing the selection");
                type(pane,"v0w2wbiw");
                check(pane->get_selected_text() == L"gamma", "caret-only movement and counts find the expected word");
                const auto before = pane->dv()->text_cursor_rect.value();
                type(pane,"jk");
                const auto after = pane->dv()->text_cursor_rect.value();
                check(std::abs(before.rect.x0-after.rect.x0) < 0.1f && std::abs(before.rect.y0-after.rect.y0) < 0.1f,
                    "j/k preserve the desired text column");
                type(pane,"iw");
                type(pane,"y");
                check(pane->pdf_text_cursor_ && !pane->pdf_text_cursor_->selecting() && QApplication::clipboard()->text() == "gamma",
                    "y copies text and returns to normal mode");
                type(pane,"viw");
                key(pane,Qt::Key_C,"c",Qt::ControlModifier);
                check(pane->pdf_text_cursor_ && !pane->pdf_text_cursor_->selecting() && QApplication::clipboard()->text() == "gamma", "Cmd+C copies and returns to normal mode");
                type(pane,"v");
                key(pane,Qt::Key_Escape);
                check(pane->pdf_text_cursor_ && !pane->pdf_text_cursor_->selecting() && pane->get_selected_text().empty(), "Escape clears selection and keeps a normal caret");
                const auto line_cursor = pane->dv()->text_cursor_rect.value();
                type(pane,"yy");
                check(QApplication::clipboard()->text().contains("gamma") && QApplication::clipboard()->text().endsWith("\n"),
                    "yy copies the current line with a newline");
                check(pane->dv()->text_cursor_rect->rect.x0 == line_cursor.rect.x0, "yy leaves caret in place");
                key(pane,Qt::Key_Escape);
                check(!pane->pdf_text_cursor_, "second Escape restores PDF navigation");
                const auto point = target.to_window(pane->dv());
                WindowPos click{(point.x0+point.x1)/2,(point.y0+point.y1)/2};
                pane->handle_left_click(click,true,false,false,false,false);
                pane->handle_left_click(click,false,false,false,false,false);
                check(pane->pdf_text_cursor_ && !pane->pdf_text_cursor_->selecting(), "plain click places normal caret");
                const auto clicked = pane->dv()->text_cursor_rect.value();
                type(pane,"l");
                check(pane->dv()->text_cursor_rect->rect.x0 > clicked.rect.x0 && pane->get_selected_text().empty(),
                    "normal l moves through text without selecting");
                type(pane,"hv");
                check(pane->pdf_text_cursor_->selecting(), "v begins visual selection at clicked caret");
                key(pane,Qt::Key_Escape);
                type(pane,"G");
                check(pane->dv()->text_cursor_rect->page == 2, "G reaches the last text page across an empty page");
                type(pane,"0h");
                check(pane->dv()->text_cursor_rect->page == 0, "h crosses an empty page backwards");
                type(pane,"gg");
                const auto first = pane->dv()->text_cursor_rect.value();
                type(pane,"hkk");
                check(pane->dv()->text_cursor_rect->page == first.page && pane->dv()->text_cursor_rect->rect.x0 == first.rect.x0,
                    "motions stop at the document boundary");
                type(pane,"G");
                const auto last = pane->dv()->text_cursor_rect.value();
                type(pane,"ljj");
                check(pane->dv()->text_cursor_rect->page == last.page && pane->dv()->text_cursor_rect->rect.x0 == last.rect.x0,
                    "forward motions stop at the last character");
                auto visible = pane->dv()->text_cursor_rect->to_window(pane->dv());
                check(visible.y0 >= 0 && visible.y1 <= pane->dv()->get_view_height(), "the viewport follows the caret");
                pane->open_document(pdf);
                check(!pane->pdf_text_cursor_ && !pane->dv()->text_cursor_rect, "document changes clear caret state");
            }
        }
        type(owner,"v");
        owner->reload();
        check(!owner->pdf_text_cursor_ && !owner->dv()->text_cursor_rect, "PDF reload clears stale text positions");
        qInfo("PASS: live selections, words/counts, endpoint swap, vertical movement, clipboard, blank pages, bounds, both split orientations and reload");
        result = 0;
    } catch (const std::exception& error) { qCritical("FAIL: %s",error.what()); }
    QApplication::clipboard()->setMimeData(clipboard.release());
    QCoreApplication::exit(result);
}
static void await_document() {
    static int attempts = 0;
    if (!windows.empty() && windows.front()->doc()) { run(); return; }
    if (++attempts == 100) {
        qCritical("FAIL: fixture document did not open");
        QCoreApplication::exit(1);
        return;
    }
    QTimer::singleShot(100, qApp, await_document);
}
static void schedule() { QTimer::singleShot(1200,qApp,await_document); }
Q_COREAPP_STARTUP_FUNCTION(schedule)
int main(int, char** argv) {
    QStandardPaths::setTestModeEnabled(true);
    QTemporaryDir temp;
    qputenv("XDG_DATA_HOME",temp.path().toUtf8());
    char option[]="--instance-name", name[]="sioyek-visual-select-test";
    char* args[]={argv[0],argv[1],option,name};
    sioyek_application_main(4,args); return result;
}
