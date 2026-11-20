#define main sioyek_application_main
#include "../pdf_viewer/main.cpp"
#undef main
#include <QTemporaryDir>
#include <QPdfWriter>
#include <QEventLoop>
#include <QListWidget>
#include <stdexcept>

static QString directory;
static int result = 1;
static void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
static void settle(int milliseconds = 100) {
    QEventLoop loop;
    QTimer::singleShot(milliseconds, &loop, &QEventLoop::quit); loop.exec();
}
static std::wstring path(int i) { return (directory + "/paper-" + QString::number(i) + ".pdf").toStdWString(); }
static void run(MainWidget* pane, const char* name) {
    auto command = pane->command_manager->get_command_with_name(pane, name);
    check(bool(command), "command registered"); command->run(); settle(30);
}
static void key(QWidget* widget, int code, Qt::KeyboardModifiers modifiers = Qt::NoModifier, QString text = {}) {
    QKeyEvent event(QEvent::KeyPress, code, modifiers, text); QApplication::sendEvent(widget, &event);
    QKeyEvent release(QEvent::KeyRelease, code, modifiers, text); QApplication::sendEvent(widget, &release);
    settle(30);
}
static void go(MainWidget* pane, int page) {
    pane->push_state(); pane->dv()->goto_page(page); pane->invalidate_render(); settle(50);
}
static bool same(const DocumentViewState& a, const DocumentViewState& b) {
    return a.document_path == b.document_path && std::abs(a.book_state.offset_y - b.book_state.offset_y) < 0.01f &&
        std::abs(a.book_state.zoom_level - b.book_state.zoom_level) < 0.0001f;
}
static void fixtures() {
    for (int file = 0; file < 3; ++file) {
        QPdfWriter writer(QString::fromStdWString(path(file))); writer.setResolution(72);
        writer.setPageSize(QPageSize(QSizeF(600, 750), QPageSize::Point)); writer.setPageMargins({});
        QPainter painter(&writer); painter.setFont(QFont("Helvetica", 13));
        for (int page = 0; page < 8; ++page) {
            if (page) writer.newPage();
            painter.drawText(55, 65, QString("Paper %1 | Section %2").arg(file+1).arg(page+1));
            for (int line = 0; line < 24; ++line)
                painter.drawText(55, 140 + line * 20, QString("Statement %1.%2: a useful result and its mathematical proof.").arg(page+1).arg(line+1));
        }
    }
}
static void test() {
    try {
        fixtures();
        auto pane = windows.front();
        pane->resize(1050, 780);
        pane->open_document(path(0));
        pane->dv()->goto_page(0); settle(300);
        pane->restore_navigation_history({});
        check(pane->history.size() == 1, "one initial stop");
        run(pane, "next_page"); run(pane, "next_page");
        check(pane->history.size() == 3 && pane->current_history_index == 2, "page jumps create stops");
        run(pane, "move_down"); settle(180);
        check(pane->history.size() == 3, "ordinary scrolling does not append a stop");
        settle(350);
        check(!pane->history.back().snippet.isEmpty(), "idle capture fills the history text excerpt");
        auto last = pane->dv()->get_state();
#ifdef Q_OS_MACOS
        const auto control = Qt::MetaModifier;
#else
        const auto control = Qt::ControlModifier;
#endif
        pane->activateWindow(); pane->setFocus();
        key(pane, Qt::Key_O, control, "o");
        check(pane->current_history_index == 1 && pane->get_current_page_number() == 1, "Control O goes back");
        key(pane, Qt::Key_I, control, "i");
        check(pane->current_history_index == 2 && same(last, pane->dv()->get_state()), "Control I returns to the scrolled departure point");
        auto rail = pane->findChild<QWidget*>("navigationHistorySidebar");
        auto stops = pane->findChild<QListWidget*>("navigationHistoryStops");
        check(rail && rail->isVisible() && stops && stops->currentRow() == 2, "sidebar marks current entry");
        check(rail->width() <= 50 && rail->geometry().right() < 70, "sidebar is a narrow edge rail");
        check(QApplication::focusWidget() == pane, "history leaves keyboard focus on PDF");
        const auto size = pane->opengl_widget->size();
        pane->toggle_navigation_sidebar();
        check(!rail->isVisible() && pane->opengl_widget->size() == size, "hiding rail never resizes or refits PDF");
        pane->toggle_navigation_sidebar();
        qInfo("PASS: real Ctrl+O/I, page jumps, scroll exclusion and compact sidebar");

        pane->prev_state(); go(pane, 5);
        check(pane->history.size() == 3 && pane->current_history_index == 2, "new jump branches and discards forward trail");
        pane->next_state(); check(pane->get_current_page_number() == 5, "forward stops at end");
        const auto count = pane->history.size();
        pane->push_state(); // failed/no-op navigation
        settle(); check(pane->history.size() == count, "no-op jumps do not add entries");
        pane->handle_command_types(pane->command_manager->get_command_with_name(pane, "goto_page_with_page_number"), 0);
        settle(); key(pane->text_command_line_edit, Qt::Key_Escape);
        key(pane->text_command_line_edit, Qt::Key_Escape);
        check(!pane->text_command_line_edit_container->isVisible(), "page prompt actually canceled");
        settle(); check(pane->history.size() == count, "canceling page prompt adds no stop");
        pane->dv()->set_zoom_level(1.23f, true);
        const auto from = pane->dv()->get_state();
        pane->open_document(path(1)); settle();
        check(pane->history.size() == count + 1, "opening another PDF records destination");
        pane->prev_state(); check(same(from, pane->dv()->get_state()), "cross-file back restores exact view");
        pane->next_state(); check(pane->doc()->get_path() == path(1), "cross-file forward returns to destination");
        go(pane, 3);
        pane->open_document(path(0)); settle();
        check(QFile::remove(QString::fromStdWString(path(1))), "remove fixture file");
        pane->prev_state(); check(pane->doc()->get_path() == path(0), "unavailable files are skipped without external dialogs");
        qInfo("PASS: branching, canceled prompts, cross-file positions and missing PDFs");

        pane->toggle_split(); settle();
        auto other = pane->split_pane_.data(); check(other, "split exists");
        other->open_document(path(2)); settle();
        other->restore_navigation_history({});
        const auto left = pane->dv()->get_state();
        const auto left_count = pane->history.size();
        run(other, "next_page"); run(other, "next_page");
        other->prev_state();
        check(same(left, pane->dv()->get_state()) && pane->history.size() == left_count, "split history is independent");
        pane->navigation_sidebar_visible_ = false;
        const auto snapshot = pane->capture_workspace();
        const auto left_json = pane->navigation_history_json();
        const auto right_json = other->navigation_history_json();
        go(other, 6); go(pane, 6);
        QString error;
        check(pane->apply_workspace(snapshot, error), qPrintable(error));
        other = pane->split_pane_;
        check(other && other->navigation_history_json() == right_json, "workspace restores right jump list and cursor");
        // Unavailable entries are deliberately filtered while loading.
        check(!pane->navigation_sidebar_visible_ && pane->history.size() < size_t(left_json["entries"].toArray().size()), "workspace preserves visibility and filters missing entries");
        other->next_state(); check(other->get_current_page_number() == 2, "forward trail survives workspace restore");
        qInfo("PASS: independent split trails and workspace persistence");

        pane->focus_split_pane(1);
        const auto before_preview = other->dv()->get_state();
        other->show_navigation_history(); settle(500);
        auto picker = dynamic_cast<DocumentPicker*>(other->current_widget_stack.back());
        check(picker && picker->isVisible() && picker->parentWidget() == pane &&
            picker->width() == std::min(1200, pane->width() - 48), "expanded history uses full-width floating picker");
        check(picker->findChild<QAbstractItemView*>("documentPickerList")->currentIndex().row() == other->current_history_index, "expanded view selects current stop");
        key(picker->findChild<QLineEdit*>("documentPickerSearch"), Qt::Key_K, Qt::NoModifier, "k");
        check(same(before_preview, other->dv()->get_state()), "preview selection does not move PDF");
        pane->grab().save("/tmp/sioyek-navigation-history-expanded.png");
        key(picker->findChild<QLineEdit*>("documentPickerSearch"), Qt::Key_Return);
        check(other->get_current_page_number() == 1, "Enter restores selected history point");
        check(same(pane->dv()->get_state(), pane->history[pane->current_history_index]), "expanded right history does not navigate left PDF");
        pane->navigation_sidebar_visible_ = true; pane->update_navigation_sidebar();
        other->update_navigation_sidebar(); settle(200);
        auto other_rail = other->findChild<QWidget*>("navigationHistorySidebar");
        check(other_rail && other_rail->isVisible(), "right history sidebar is visible after workspace restore");
        pane->grab().save("/tmp/sioyek-navigation-history-sidebar.png");
        qInfo("PASS: expanded preview is optional and targets the invoking split pane");

        for (int i = 0; i < 220; ++i) {
            other->push_state(); other->dv()->goto_page(i % 8); other->finish_navigation_jump();
        }
        check(other->history.size() == 200 && other->current_history_index == 199, "jump list has a bounded memory budget");
        other->update_navigation_sidebar(); settle();
        auto right_stops = other->findChild<QListWidget*>("navigationHistoryStops");
        check(right_stops->visualItemRect(right_stops->currentItem()).intersects(right_stops->viewport()->rect()), "current stop scrolls into view");
        other->dv()->goto_page(0); other->restore_navigation_history({});
        other->opengl_widget->search_text(L"Statement 7.1");
        for (int i = 0; i < 100 && other->opengl_widget->get_num_search_results() == 0; ++i) settle(30);
        check(other->opengl_widget->get_num_search_results() > 0, "search fixture produces matches");
        other->opengl_widget->cancel_search();
        other->goto_search_result(1);
        for (int i = 0; i < 100 && other->get_current_page_number() != 6; ++i) settle(30);
        settle(60);
        check(other->get_current_page_number() == 6 && other->history.size() == 2, "asynchronous search resume records its actual jump");
        other->prev_state(); check(other->get_current_page_number() == 0, "search jump returns to its starting page");
        qInfo("PASS: bounded history, visible selected stop, excerpts and asynchronous search jumps");
        result = 0; QCoreApplication::exit(0);
    } catch (const std::exception& error) {
        qCritical("FAIL: %s", error.what()); QCoreApplication::exit(1);
    }
}
static void schedule() { QTimer::singleShot(1200, qApp, test); }
Q_COREAPP_STARTUP_FUNCTION(schedule)
int main(int, char** argv) {
    QStandardPaths::setTestModeEnabled(true);
    QTemporaryDir temp; directory = temp.path();
    QFile::copy(QString::fromUtf8(argv[1]), directory + "/startup.pdf");
    QByteArray first = (directory + "/startup.pdf").toUtf8();
    char instance[] = "--instance-name", name[] = "sioyek-navigation-history-regression";
    char* args[] = {argv[0], first.data(), instance, name};
    sioyek_application_main(4, args); return result;
}
