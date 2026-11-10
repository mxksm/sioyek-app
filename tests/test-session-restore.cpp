// Exercise actual startup routing, including duplicate documents and IPC commands.
#define main sioyek_application_main
#include "../pdf_viewer/main.cpp"
#undef main
#include "../pdf_viewer/blank_canvas.h"
#include <QTemporaryDir>
#include <stdexcept>

static QString directory;
static int result = 1;
static void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
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
        qInfo("PASS: duplicate canvas/PDF sessions, repeated launches, resize IPC, Escape, missing first PDF and explicit file launch");
        result = 0;
    } catch (const std::exception& e) { qCritical("FAIL: %s", e.what()); }
    delete seed;
    QCoreApplication::exit(result);
}
static void schedule() { QTimer::singleShot(1200, qApp, run); }
Q_COREAPP_STARTUP_FUNCTION(schedule)
int main(int argc, char** argv) {
    QStandardPaths::setTestModeEnabled(true); QTemporaryDir temp; directory = temp.path();
    char option[] = "--instance-name", name[] = "sioyek-session-restore-test";
    char* args[] = {argv[0], argv[1], option, name};
    sioyek_application_main(4,args); return result;
}
