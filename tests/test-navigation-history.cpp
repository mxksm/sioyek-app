#define main sioyek_application_main
#include "../pdf_viewer/main.cpp"
#undef main
#include <QTemporaryDir>
#include <QPdfWriter>
#include <QEventLoop>
#include <QListWidget>
#include <stdexcept>

extern int NAVIGATION_UI_DURATION, NAVIGATION_UI_FADE_DURATION;
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
    auto command = pane->command_manager->get_command_with_name(pane, "goto_page_with_page_number");
    command->set_text_requirement(std::to_wstring(page + 1)); command->run();
    pane->invalidate_render(); settle(50);
}
static bool same(const DocumentViewState& a, const DocumentViewState& b) {
    return a.document_path == b.document_path && std::abs(a.book_state.offset_x - b.book_state.offset_x) < 0.01f &&
        std::abs(a.book_state.offset_y - b.book_state.offset_y) < 0.01f &&
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
        NAVIGATION_UI_FADE_DURATION = 0;
        auto pane = windows.front();
        pane->resize(1050, 780);
        pane->open_document(path(0)); settle(300);
        pane->restore_navigation_history({});
        pane->navigation_sidebar_visibility_override_ = 1;
#ifdef Q_OS_MACOS
        const auto control = Qt::MetaModifier;
#else
        const auto control = Qt::ControlModifier;
#endif
        // Use the actual page command, not a test-only history hook.
        go(pane, 2);
        run(pane, "next_page"); run(pane, "previous_page");
        pane->execute_macro_if_enabled(L"goto_end;fit_to_page_width;center_page"); settle();
        pane->execute_macro_if_enabled(L"goto_beginning;fit_to_page_width;center_page"); settle();
        auto mark = pane->command_manager->get_command_with_name(pane, "set_mark");
        mark->set_symbol_requirement('a'); mark->run(); settle();
        check(pane->history.empty(), "jumps, marks, gg/G and page movement do not save anything");
        go(pane, 0);
        pane->dv()->set_zoom_level(1.23f, true); settle();
        pane->activateWindow(); pane->setFocus();
        key(pane, Qt::Key_S, control, "s");
        check(pane->history.size() == 1 && pane->current_history_index == 0, "Ctrl S saves exactly one location");
        const auto first = pane->history[0];
        check(same(first, pane->dv()->get_state()) && !first.snippet.isEmpty(), "save captures exact position, zoom and excerpt");
        const auto single_save = pane->navigation_history_json().value("entries");
        for (bool selected : {true, false}) {
            for (int code : {Qt::Key_A, Qt::Key_I, Qt::Key_O}) {
                pane->open_document(path(1)); settle(); go(pane, 4);
                pane->current_history_index = selected ? 0 : -1;
                key(pane, code, control, QString(QChar(code)).toLower());
                check(pane->current_history_index == 0 && same(first, pane->dv()->get_state()),
                    "all history shortcuts restore the sole saved PDF, position and zoom");
                key(pane, code, control, QString(QChar(code)).toLower());
                check(same(first, pane->dv()->get_state()) && pane->navigation_history_json().value("entries") == single_save,
                    "repeated single-save navigation is stable and does not mutate the save");
            }
        }
        qInfo("PASS: Ctrl A/I/O all restore a single saved location, including an unselected history cursor");
        go(pane, 2); run(pane, "save_navigation_state");
        const auto second = pane->history[1];
        pane->open_document(path(2)); settle(); go(pane, 4);
        pane->dv()->set_zoom_level(1.45f, true); settle();
        run(pane, "save_navigation_state");
        const auto third = pane->history[2];
        key(pane, Qt::Key_O, control, "o");
        check(pane->current_history_index == 1 && same(second, pane->dv()->get_state()), "Ctrl O restores previous saved PDF and exact view");
        key(pane, Qt::Key_O, control, "o");
        check(pane->current_history_index == 0 && same(first, pane->dv()->get_state()), "Ctrl O traverses reverse chronological order");
        key(pane, Qt::Key_I, control, "i");
        check(same(second, pane->dv()->get_state()), "Ctrl I goes forward through saves");
        go(pane, 6); run(pane, "move_down"); run(pane, "zoom_in"); settle(350);
        pane->prev_state(); settle();
        check(same(second, pane->dv()->get_state()), "back after unsaved reading restores selected stop first");
        pane->prev_state(); settle();
        check(same(first, pane->dv()->get_state()), "another back continues to the older stop");
        const auto fixed = pane->navigation_history_json().value("entries");
        go(pane, 7); mark->run(); settle();
        pane->open_document(path(2)); settle();
        for (int i = 0; i < 6; ++i) run(pane, "goto_recent_tab");
        pane->close_current_tab(); settle(); pane->reopen_closed_tab(); settle();
        pane->opengl_widget->search_text(L"Statement 7.1");
        for (int i = 0; i < 100 && pane->opengl_widget->get_num_search_results() == 0; ++i) settle(30);
        check(pane->opengl_widget->get_num_search_results() > 0, "search produces matches");
        pane->opengl_widget->cancel_search(); pane->goto_search_result(1);
        for (int i = 0; i < 100 && pane->get_current_page_number() != 6; ++i) settle(30);
        settle(250);
        check(pane->get_current_page_number() == 6, "asynchronous search actually navigates");
        check(pane->navigation_history_json().value("entries") == fixed, "search, marks, scroll, zoom and tab management never add or mutate saves");
        pane->opengl_widget->cancel_search();
        pane->goto_navigation_history(0); settle(); go(pane, 5);
        run(pane, "save_navigation_state");
        check(pane->history.size() == 4 && same(third, pane->history[2]), "saving from an older stop appends without deleting newer entries");
        run(pane, "save_navigation_state");
        check(pane->history.size() == 5 && pane->history[3].id != pane->history[4].id, "every explicit save appends even at same location");
        qInfo("PASS: manual Ctrl S, immutable views, reverse traversal, no automatic collection or branching");

        auto rail = pane->findChild<QWidget*>("navigationHistorySidebar");
        auto stops = pane->findChild<QListWidget*>("navigationHistoryStops");
        check(rail && rail->isVisible() && stops && stops->currentRow() == 4, "sidebar selects last save");
        check(rail->width() <= 50 && rail->geometry().right() < 70, "sidebar remains narrow");
        pane->goto_navigation_history(0); pane->goto_navigation_history(2); settle();
        for (int i = 0; i < 6; ++i) {
            key(pane, Qt::Key_A, control, "a");
            check(same(pane->dv()->get_state(), i % 2 ? third : first), "Ctrl A alternates arbitrary saved stops");
        }
        const auto comparison = pane->navigation_history_json();
        pane->restore_navigation_history(comparison);
        pane->toggle_recent_history(); settle();
        check(same(first, pane->dv()->get_state()), "comparison pair survives serialization");
        pane->remove_navigation_point(pane->previous_navigation_point_id_);
        pane->toggle_recent_history(); settle();
        check(pane->current_history_index >= 0, "deleted alternate falls back safely");
        qInfo("PASS: sidebar and comparison pair");

        pane->toggle_split(); settle();
        auto other = pane->split_pane_.data(); check(other, "split exists");
        other->open_document(path(2)); settle(); other->restore_navigation_history({});
        other->navigation_sidebar_visibility_override_ = 1;
        const auto left = pane->dv()->get_state();
        const auto left_entries = pane->navigation_history_json().value("entries");
        go(other, 1); run(other, "save_navigation_state");
        go(other, 2); run(other, "save_navigation_state");
        other->prev_state(); settle();
        check(same(left, pane->dv()->get_state()) && pane->navigation_history_json().value("entries") == left_entries, "split saved lists are independent");
        pane->navigation_sidebar_visibility_override_ = 0;
        const auto workspace = pane->capture_workspace();
        const auto left_json = pane->navigation_history_json();
        const auto right_json = other->navigation_history_json();
        go(other, 6); run(other, "save_navigation_state"); go(pane, 6);
        QString error;
        check(pane->apply_workspace(workspace, error), qPrintable(error));
        other = pane->split_pane_;
        check(other && other->navigation_history_json() == right_json && pane->navigation_history_json() == left_json, "workspace restores exact saved lists, traversal, comparison and visibility");
        other->next_state(); settle();
        check(other->get_current_page_number() == 2, "forward sequence survives workspace restore");
        pane->focus_split_pane(1);
        const auto before_preview = other->dv()->get_state();
        other->show_navigation_history(); settle(300);
        auto picker = dynamic_cast<DocumentPicker*>(other->current_widget_stack.back());
        check(picker && picker->isVisible() && picker->parentWidget() == pane && picker->width() == std::min(1200, pane->width() - 48), "expanded picker uses full window width");
        auto search = picker->findChild<QLineEdit*>("documentPickerSearch");
        key(search, Qt::Key_K, Qt::NoModifier, "k");
        check(same(before_preview, other->dv()->get_state()), "preview selection does not navigate");
        key(search, Qt::Key_Return); settle();
        check(other->get_current_page_number() == 1 && same(pane->dv()->get_state(), left), "Enter restores invoking pane only");
        qInfo("PASS: split isolation, workspace persistence and floating preview");

        other->restore_navigation_history({});
        other->navigation_sidebar_visibility_override_ = 1;
        std::vector<MainWidget::NavigationPoint> saved;
        for (int i = 0; i < 22; ++i) {
            other->dv()->goto_page(i % 8); other->save_navigation_state(); saved.push_back(other->history.back());
        }
        check(other->history.size() == 15 && other->current_history_index == 14, "saved locations capped at 15");
        for (int i = 0; i < 15; ++i) check(other->history[i].id == saved[i+7].id, "cap evicts only oldest saves");
        other->update_navigation_sidebar(); settle();
        auto right_stops = other->findChild<QListWidget*>("navigationHistoryStops");
        for (int font_size : {10, 16, 11}) {
            right_stops->setFont(QFont("JetBrains Mono", font_size));
            other->update_navigation_sidebar(); settle(80);
            const auto last_row = right_stops->visualItemRect(right_stops->item(14));
            const int gap = right_stops->viewport()->rect().bottom() - last_row.bottom();
            check(gap >= -1 && gap <= 2, "sidebar has no trailing gap at different font sizes");
        }
        other->history[2].snippet = "uniquedeletiontarget";
        ++other->navigation_history_revision_;
        const auto removed_id = other->history[2].id;
        const auto reading = other->dv()->get_state();
        other->show_navigation_history(); settle(200);
        picker = dynamic_cast<DocumentPicker*>(other->current_widget_stack.back());
        search = picker->findChild<QLineEdit*>("documentPickerSearch");
        auto list = picker->findChild<QAbstractItemView*>("documentPickerList");
        search->setText("uniquedeletiontarget"); settle(150);
        check(list->model()->rowCount() == 1, "filtered search isolates saved location");
        key(search, Qt::Key_Backspace, Qt::ControlModifier);
        check(other->history.size() == 14 && search->text() == "uniquedeletiontarget", "Cmd Backspace deletes filtered entry not query");
        check(std::none_of(other->history.begin(), other->history.end(), [removed_id](const auto& p) { return p.id == removed_id; }), "correct saved entry deleted");
        check(same(reading, other->dv()->get_state()), "deleting leaves live PDF unchanged");
        search->clear(); settle();
        while (list->model()->rowCount() > 0) key(search, Qt::Key_Backspace, Qt::ControlModifier);
        key(search, Qt::Key_Escape); settle(300);
        go(other, 3); mark = other->command_manager->get_command_with_name(other, "set_mark");
        mark->set_symbol_requirement('b'); mark->run(); settle();
        check(other->history.empty() && other->navigation_history_json().value("entries").toArray().isEmpty(), "deleted entries stay deleted after reading and marking");
        qInfo("PASS: cap, sidebar fit and filtered deletion without resurrection");

        other->open_document(path(0)); settle(); go(other, 0); run(other, "save_navigation_state");
        other->open_document(path(1)); settle(); go(other, 3); run(other, "save_navigation_state");
        other->open_document(path(2)); settle(); go(other, 5); run(other, "save_navigation_state");
        check(QFile::remove(QString::fromStdWString(path(1))), "remove fixture file");
        other->prev_state(); settle();
        check(other->current_history_index == 0 && other->doc()->get_path() == path(0), "missing PDFs skipped when traversing saves");
        auto available = other->navigation_history_json();
        other->restore_navigation_history(available);
        check(other->history.size() == 2, "workspace restore filters unavailable files");
        NAVIGATION_UI_DURATION = 120; NAVIGATION_UI_FADE_DURATION = 40;
        other->navigation_sidebar_visibility_override_ = 1;
        other->update_navigation_sidebar(); settle(60);
        auto other_rail = other->findChild<QWidget*>("navigationHistorySidebar");
        QCursor::setPos(other->mapToGlobal(other->rect().center()));
        const auto size = other->opengl_widget->size();
        other->toggle_navigation_sidebar(); settle(80);
        check(!other_rail->isVisible() && other->opengl_widget->size() == size, "hiding sidebar never resizes PDF");
        other->toggle_navigation_sidebar(); settle(50);
        check(other_rail->isVisible(), "automatic reveals on mode change");
        settle(300); check(!other_rail->isVisible(), "automatic rail fades");
        other->next_state(); settle(50); check(other_rail->isVisible(), "visiting saved stop reveals rail");
        settle(300); go(other, 2); settle(100);
        check(!other_rail->isVisible(), "unsaved page jumps never reveal rail");
        run(other, "save_navigation_state"); check(other_rail->isVisible(), "explicit save reveals rail");
        other->restore_navigation_history({}); other->toggle_recent_history(); other->prev_state(); other->next_state();
        check(other->history.empty(), "empty list navigation is harmless");
        qInfo("PASS: unavailable PDFs, sidebar visibility and empty navigation");
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
