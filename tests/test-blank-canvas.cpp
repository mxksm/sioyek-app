#define main sioyek_application_main
#include "../pdf_viewer/main.cpp"
#undef main
#include "../pdf_viewer/blank_canvas.h"
#include <QTemporaryDir>
#include <QEventLoop>
#include <QJsonDocument>
#include <stdexcept>

extern std::wstring ANNOTATIONS_DIR_PATH;
static QString directory;
static int result = 1;
static void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
static void settle(int ms = 150) {
    QEventLoop loop; QTimer::singleShot(ms, &loop, &QEventLoop::quit); loop.exec();
}
static QByteArray read(const QString& path) {
    QFile file(path); check(file.open(QIODevice::ReadOnly), "read saved file"); return file.readAll();
}
static int saved_strokes(const QString& path) {
    const auto drawings = QJsonDocument::fromJson(read(path)).object()["drawings"].toObject();
    int count = 0; for (const auto& value : drawings) count += value.toArray().size(); return count;
}
static void run() {
    try {
        standard_data_path = Path((directory + "/migration").toStdWString());
        QDir().mkpath(QFileInfo(blank_canvas_path()).absolutePath());
        const auto legacy = blank_canvas_pdf(1);
        {
            QFile old(blank_canvas_path()); check(old.open(QIODevice::WriteOnly), "write original canvas"); old.write(legacy);
            QFile ink(blank_canvas_path() + ".sioyek.drawings"); check(ink.open(QIODevice::WriteOnly), "write existing ink");
            ink.write(R"({"drawings":{"0":[{"creation_time":"2026-11-07T12:00:00","type":114,"alpha":1,"point_xs":[100,140],"point_ys":[100,140],"point_thicknesses":[5,5]}]}})");
        }
        const auto old_ink = read(blank_canvas_path() + ".sioyek.drawings");
        QString migration_error;
        check(ensure_blank_canvas(migration_error), "upgrade original canvas");
        check(read(blank_canvas_path() + ".before-expansion") == legacy, "keep original PDF backup");
        check(read(blank_canvas_path() + ".sioyek.drawings") == old_ink, "expansion never rewrites existing ink");
        {
            auto pane = windows.front();
            DocumentManager fresh(pane->mupdf_context, pane->db_manager, pane->checksummer);
            auto expanded = fresh.get_document(blank_canvas_path().toStdWString());
            bool invalid = false; check(expanded->open(&invalid, true), "expanded canvas opens");
            check(expanded->num_pages() == 100 && expanded->num_freehand_drawings() == 1, "100 pages retain original stroke");
            check(expanded->get_page_width(0) == 612 && expanded->get_page_height(0) == 792 &&
                expanded->get_page_drawings(0)[0].points[0].pos.y == 100, "page one geometry and ink coordinates unchanged");
            FreehandDrawing ink; ink.type = 'r'; ink.alpha = 1; ink.creattion_time = QDateTime::currentDateTime();
            ink.points.push_back({DocumentPos{99, 100, 100}.to_absolute(expanded), 5});
            expanded->add_freehand_drawing(ink); expanded->persist_drawings(); expanded->load_drawings();
            check(expanded->get_page_drawings(99).size() == 1, "last canvas page supports persistent drawing");
        }
        const auto expanded_bytes = read(blank_canvas_path());
        check(ensure_blank_canvas(migration_error) && read(blank_canvas_path()) == expanded_bytes,
            "expansion is idempotent and never shrinks existing pages");
        standard_data_path = Path(directory.toStdWString());
        last_opened_file_address_path = standard_data_path.slash(L"session.txt");
        RESTORE_ALL_WINDOWS_ON_STARTUP = true;
        auto pane = windows.front();
        const auto real = pane->doc()->get_path();
        pane->window_tabs = {}; pane->record_current_tab(false);
        pane->setFocus(); QApplication::setActiveWindow(pane);
        pane->execute_macro_if_enabled(L"new_tab"); settle();
        check(pane->doc() && is_blank_canvas(pane->doc()->get_path()), "new_tab opens canvas without a picker");
        auto canvas = pane->doc();
        check(canvas->num_pages() == blank_canvas_pages, "canvas has 100 readable blank pages");
        check(pane->get_current_tab_index() == -1 && pane->window_tabs.paths().size() == 1,
            "canvas has no numbered tab");
        const auto original_pdf = read(blank_canvas_path());
        const QString sidecar = QString::fromStdWString(canvas->get_drawings_file_path());
        ANNOTATIONS_DIR_PATH = (directory + "/other-annotations").toStdWString();
        check(QString::fromStdWString(canvas->get_drawings_file_path()) == sidecar,
            "canvas drawings remain beside their PDF regardless of annotation preference");
        pane->set_hand_drawing_mode(true);
        pane->start_drawing();
        pane->handle_drawing_move(QPointF(pane->width()/2, pane->height()/2), -1);
        pane->finish_drawing(QPointF(pane->width()/2+70, pane->height()/2+30), true);
        check(saved_strokes(sidecar) == 1, "finished pen stroke is immediately saved");
        pane->handle_undo_drawing(); check(saved_strokes(sidecar) == 0, "undo is immediately saved");
        pane->handle_redo_drawing(); check(saved_strokes(sidecar) == 1, "redo is immediately saved");
        for (int i = 0; i < 6; ++i) {
            pane->select_tab(0); pane->execute_macro_if_enabled(L"new_tab");
            pane->execute_macro_if_enabled(L"new_tab");
            check(pane->doc() == canvas && pane->window_tabs.paths().size() == 1,
                "repeated canvas visits reuse one document without duplicate tabs");
        }
        check(read(blank_canvas_path()) == original_pdf && saved_strokes(sidecar) == 1,
            "reopening never regenerates the canvas or discards ink");
        pane->cycle_recent_tab(1); pane->finish_recent_tab_cycle(true);
        check(pane->doc()->get_path() == real, "Ctrl+Tab from canvas selects the most recent real PDF");
        pane->execute_macro_if_enabled(L"new_tab");
        pane->persist(true);
        auto sessions = get_last_saved_windows_states();
        check(sessions.size() == 1 && is_blank_canvas(sessions[0].tabs[sessions[0].selected_tab()]),
            "session records the canvas as active");
        pane->window_tabs = {}; pane->open_tabs(sessions[0].tabs);
        check(pane->window_tabs.paths() == std::vector<std::wstring>{real}, "restoring never numbers the canvas");
        pane->open_document(real);
        pane->handle_open_prev_doc(); settle();
        auto picker = dynamic_cast<DocumentPicker*>(pane->current_widget_stack.back());
        check(picker, "recents picker opens");
        auto view = picker->findChild<QListView*>(); check(view, "recents list exists");
        for (int row=0; row<view->model()->rowCount(); ++row)
            check(!is_blank_canvas(view->model()->index(row,0).data(Qt::ToolTipRole).toString().toStdWString()),
                "canvas is absent from recents");
        pane->pop_all_widgets();
        pane->open_blank_canvas(); pane->toggle_split(); settle();
        check(pane->split_pane_ && pane->split_pane_->doc() == canvas, "split panes share the same canvas object");
        pane->focus_split_pane(1); pane->split_pane_->select_tab(0);
        pane->split_pane_->close_current_tab(); settle();
        check(pane->isVisible() && pane->window_tabs.paths().empty() && pane->doc() == canvas &&
            pane->split_pane_->doc() == canvas, "last real tab closes to canvas in both panes without closing window");
        pane->split_pane_->close_current_tab();
        check(pane->isVisible() && saved_strokes(sidecar) == 1, "Cmd+W on canvas is harmless");
        pane->persist(true); sessions = get_last_saved_windows_states();
        check(sessions.size() == 1 && sessions[0].tabs.size() == 1 && is_blank_canvas(sessions[0].tabs[0]),
            "canvas-only window survives session persistence");
        auto second = MainWidget::create_restored_window(pane, sessions[0]); settle();
        check(second->doc() == canvas && second->window_tabs.paths().empty(),
            "a restored second window reuses the global canvas without a visible tab");
        // Read with an independent Document, proving the save survives the in-memory cache.
        {
            DocumentManager fresh(pane->mupdf_context, pane->db_manager, pane->checksummer);
            auto reloaded = fresh.get_document(canvas->get_path());
            bool invalid = false; check(reloaded->open(&invalid, true), "fresh canvas document opens");
            check(reloaded->num_freehand_drawings() == 1, "ink loads from disk in a fresh document");
        }
        // A blocked atomic-save temporary file must leave the old drawing file intact.
        const auto good_save = read(sidecar);
        const QString parent = QFileInfo(sidecar).absolutePath();
        check(QFile::setPermissions(parent, QFileDevice::ReadOwner | QFileDevice::ExeOwner), "make canvas folder read-only");
        canvas->undo_freehand_drawing(); canvas->persist_drawings();
        check(read(sidecar) == good_save, "failed save preserves previous strokes");
        check(QFile::setPermissions(parent, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner), "restore folder permissions");
        canvas->persist_drawings();
        check(saved_strokes(sidecar) == 0, "failed save remains dirty and can be retried");
        pane->handle_redo_drawing();
        qInfo("PASS: singleton hidden canvas, pen autosave, undo/redo, recents, MRU, session restore, split panes, atomic-save retry");
        result = 0; QCoreApplication::exit(0);
    } catch (const std::exception& e) { qCritical("FAIL: %s", e.what()); QCoreApplication::exit(1); }
}
static void schedule() { QTimer::singleShot(1200, qApp, run); }
Q_COREAPP_STARTUP_FUNCTION(schedule)
int main(int argc, char** argv) {
    QStandardPaths::setTestModeEnabled(true); QTemporaryDir temp; directory = temp.path();
    char option[] = "--instance-name", name[] = "sioyek-blank-canvas-test";
    char* args[] = {argv[0], argv[1], option, name};
    sioyek_application_main(4,args); return result;
}
