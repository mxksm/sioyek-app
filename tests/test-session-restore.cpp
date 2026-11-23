// Exercise actual startup routing, including duplicate documents and IPC commands.
#define main sioyek_application_main
#include "../pdf_viewer/main.cpp"
#undef main
#include "../pdf_viewer/blank_canvas.h"
#include <QTemporaryDir>
#include <QEventLoop>
#include <stdexcept>

extern bool OPEN_LAST_FILE_ON_STARTUP;
static QString directory;
static int result = 1;
static void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
static void settle() {
    QEventLoop loop;
    QTimer::singleShot(100, &loop, &QEventLoop::quit);
    loop.exec();
}
static void run() {
    auto seed = windows.front();
    seed->hide();
    windows.clear();
    try {
        standard_data_path = Path(directory.toStdWString());
        last_opened_file_address_path = standard_data_path.slash(L"session.txt");
        RESTORE_ALL_WINDOWS_ON_STARTUP = true;
        STARTUP_COMMANDS.clear();
        QString error;
        check(ensure_blank_canvas(error), "create isolated canvas");
        const auto canvas = blank_canvas_path().toStdWString();
        const QStringList launch{QCoreApplication::applicationFilePath()};
        const auto real = seed->doc()->get_path();
        auto reset = [&] {
            while (!windows.empty()) delete windows.back();
            auto first = new MainWidget(seed);
            windows.push_back(first);
            first->show();
            return first;
        };
        auto save = [&](const std::vector<std::wstring>& paths) {
            std::ofstream session(last_opened_file_address_path.get_path_utf8());
            for (const auto& path : paths) session << "[window]\n[active] 0\n" << utf8_encode(path) << "\n";
        };
        const QStringList resize = launch + QStringList{"--nofocus", "--execute-command", "fit_to_page_width"};
        for (const auto& path : {canvas, real}) {
            save({path, path});
            for (int cycle = 0; cycle < 3; ++cycle) {
                auto first = reset();
                handle_args(resize);
                check(windows.size() == 1 && !first->doc(), "early resize commands do not restore a session");
                check(handle_args(launch, nullptr, true) == first, "restoration targets the first window");
                check(windows.size() == 2, "restore exactly two saved windows");
                check(first->doc() && first->doc()->get_path() == path,
                    "first restored window opens its own document even if another window has it");
                check(windows[1]->doc() == first->doc(), "duplicate views share the same document");
                if (path == canvas) check(first->window_tabs.paths().empty(), "canvas stays invisible in tabs");
                for (int i = 0; i < 20; ++i) {
                    handle_args(resize);
                    QKeyEvent escape(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
                    QApplication::sendEvent(first, &escape);
                    check(windows.size() == 2, "resize and Escape do not multiply restored windows");
                }
                first->persist(true);
                check(get_last_saved_windows_states().size() == 2, "saved session keeps the same window count");
            }
        }
        save({(directory + "/missing.pdf").toStdWString(), real});
        auto empty = reset();
        handle_args(launch, nullptr, true);
        check(!empty->doc() && windows.size() == 2, "missing first PDF leaves only the intended restored windows");
        for (int i = 0; i < 20; ++i) handle_args(resize);
        check(windows.size() == 2, "missing first PDF cannot cause repeated session restoration");
        auto explicit_window = reset();
        handle_args(launch + QStringList{QString::fromStdWString(real)}, nullptr, true);
        check(windows.size() == 1 && explicit_window->doc(), "explicit PDF opens without restoring unrelated windows");
        // Save to disk through the actual close path, discard every window and
        // exercise startup routing again. Workspace-only JSON round trips miss
        // the exit-session path that used to omit all saved locations.
        auto first = explicit_window;
        settle();
        first->dv()->set_book_state(OpenedBookState{1.0f, 0.0f, 300.0f});
        first->restore_navigation_history({});
        first->save_navigation_state();
        check(first->save_workspace("Current workspace", error), "save named workspace fixture");
        first->dv()->set_offset_y(first->dv()->get_offset_y() + 30);
        first->save_navigation_state(); // Newer than the named snapshot.
        first->goto_navigation_history(0);
        first->navigation_sidebar_visibility_override_ = 2;
        first->toggle_split(Qt::Vertical);
        auto split = first->split_pane_.data();
        split->open_document(canvas);
        settle();
        split->dv()->set_book_state(OpenedBookState{1.0f, 0.0f, 300.0f});
        split->save_navigation_state();
        split->navigation_sidebar_visibility_override_ = 0;
        first->focus_split_pane(1);
        const auto first_history = first->navigation_history_json();
        const auto split_history = split->navigation_history_json();
        WindowState additional;
        additional.tabs = {real};
        auto second = MainWidget::create_restored_window(first, additional);
        settle();
        second->dv()->set_book_state(OpenedBookState{1.0f, 0.0f, 400.0f});
        second->save_navigation_state();
        const auto second_history = second->navigation_history_json();
        first->close();
        auto saved = get_last_saved_windows_states();
        check(saved.size() == 2 && saved[0].layout["panes"].toArray().size() == 2,
            "close stores both windows and split-pane layouts on disk");
        for (int cycle = 0; cycle < 2; ++cycle) {
            first = reset();
            handle_args(launch, nullptr, true);
            check(windows.size() == 3 && first->split_pane_, "startup restores two windows and the split");
            check(first->pdf_splitter_->orientation() == Qt::Vertical && first->active_pane() == first->split_pane_,
                "startup restores split orientation and focus");
            check(first->workspace_name_ == "Current workspace", "startup keeps the current workspace name");
            check(first->navigation_history_json() == first_history &&
                first->split_pane_->navigation_history_json() == split_history &&
                windows[1]->navigation_history_json() == second_history,
                "every pane restores latest saves, selection, comparison and visibility, not old named snapshot");
            first->close();
        }
        // The single-window startup mode reads the same metadata without
        // mistaking its JSON line for a PDF filename.
        RESTORE_ALL_WINDOWS_ON_STARTUP = false;
        OPEN_LAST_FILE_ON_STARTUP = true;
        first->persist(true);
        first = reset();
        handle_args(launch, nullptr, true);
        check(windows.size() == 2 && first->split_pane_ &&
            first->navigation_history_json() == first_history &&
            first->split_pane_->navigation_history_json() == split_history,
            "single-window startup restores saved locations too");
        check(first->window_tabs.paths().size() == 1, "session metadata never becomes a PDF tab");
        const auto to_remove = first->history;
        for (const auto& point : to_remove) first->remove_navigation_point(point.id);
        first->close();
        first = reset();
        handle_args(launch, nullptr, true);
        check(first->history.empty() && first->split_pane_->navigation_history_json() == split_history,
            "deleting all stops persists without resurrecting old entries or deleting the other pane's saves");
        qInfo("PASS: saved locations survive close/startup in both modes, multiple windows, split panes, named workspaces and deletion");
        qInfo("PASS: duplicate canvas/PDF sessions, repeated launches, resize IPC, Escape, missing first PDF and explicit file launch");
        result = 0;
    } catch (const std::exception& e) { qCritical("FAIL: %s", e.what()); }
    delete seed;
    QCoreApplication::exit(result);
}
static void schedule() { QTimer::singleShot(1200, qApp, run); }
Q_COREAPP_STARTUP_FUNCTION(schedule)
int main(int, char** argv) {
    QStandardPaths::setTestModeEnabled(true); QTemporaryDir temp; directory = temp.path();
    char option[] = "--instance-name", name[] = "sioyek-session-restore-test";
    char* args[] = {argv[0], argv[1], option, name};
    sioyek_application_main(4,args); return result;
}
