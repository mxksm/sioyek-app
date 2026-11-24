#define main sioyek_application_main
#include "../pdf_viewer/main.cpp"
#undef main
#include "../pdf_viewer/workspace_store.h"
#include <QTemporaryDir>
#include <QEventLoop>
#include <cmath>
#include <stdexcept>

static QString directory;
static int result = 1;
static void check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
static void settle(int ms = 100) { QEventLoop loop; QTimer::singleShot(ms, &loop, &QEventLoop::quit); loop.exec(); }
static void view(MainWidget* pane, float zoom, float y) {
    pane->dv()->set_book_state(OpenedBookState{zoom, 0.0f, y}); pane->invalidate_render();
}
static void expect(MainWidget* pane, float zoom, float y) {
    check(std::abs(pane->dv()->get_zoom_level() - zoom) < .01 &&
        std::abs(pane->dv()->get_offset_y() - y) < .1, "latest reading position and zoom restored");
}
static void run() {
    try {
        standard_data_path = Path(directory.toStdWString());
        last_opened_file_address_path = standard_data_path.slash(L"session.txt");
        auto owner = windows.front();
        owner->workspace_name_.clear(); owner->window_tabs = {};
        std::vector<std::wstring> paths;
        for (const auto name : {"a", "b", "c", "d"}) {
            QString path = directory + "/" + name + ".pdf";
            QFile file(path); check(file.open(QIODevice::WriteOnly), "create PDF");
            file.write(blank_canvas_pdf(10)); file.close(); paths.push_back(path.toStdWString());
        }
        const auto a=paths[0], b=paths[1], c=paths[2], d=paths[3];
        QString error;
        owner->open_document(a); view(owner,1.1,1100);
        owner->open_document(b); view(owner,1.2,1200);
        owner->open_document(c); view(owner,1.3,1300);
        check(owner->save_workspace("Reading",error), "save initial workspace");
        owner->move_current_tab(-1); // A C B
        owner->select_tab(2); view(owner,1.6,1600);
        owner->close_current_tab(); // B remains in the saved workspace.
        owner->open_document(d); view(owner,1.7,1700);
        owner->select_tab(1); owner->move_current_tab(-1); // C A D
        owner->select_tab(1); view(owner,1.8,1800);
        settle(1300); // Automatic save without a command, tab switch or explicit save.
        QJsonObject store; check(read_workspace_store(store,error), "read autosaved workspace");
        const auto saved = store["workspaces"].toObject()["Reading"].toObject();
        check(saved["tabs"].toArray() == QJsonArray{QString::fromStdWString(c),QString::fromStdWString(a),QString::fromStdWString(b)},
            "order updates while saved membership excludes additions and retains closed PDFs");
        const auto saved_views = saved["panes"].toArray()[0].toObject()["tab_views"].toArray();
        bool updated_a = false;
        for (const auto& item : saved_views) if (item.toObject()["path"].toString() == QString::fromStdWString(a))
            updated_a = std::abs(item.toObject()["view"].toObject()["zoom"].toDouble()-1.8) < .01;
        check(updated_a, "timer persists changed zoom before leaving the workspace");
        check(owner->load_workspace("Reading",error), "reload same workspace"); settle();
        check(owner->window_tabs.paths() == std::vector<std::wstring>{c,a,b}, "saved PDFs and new order restored");
        owner->select_tab(1); expect(owner,1.8,1800);
        owner->select_tab(2); expect(owner,1.6,1600);
        owner->select_tab(0); expect(owner,1.3,1300);
        // A second window that has not changed a view must not overwrite it.
        auto sibling = owner->handle_new_window(); settle();
        check(sibling->load_workspace("Reading",error), "load shared workspace in second window");
        sibling->select_tab(1); view(sibling,2.0,2000); sibling->persist(true);
        owner->persist(true);
        check(owner->load_workspace("Reading",error), "read updates made in other window");
        owner->select_tab(1); expect(owner,2.0,2000);
        sibling->close(); settle();
        // Switching workspaces flushes the latest change without waiting for the timer.
        check(owner->save_workspace("Other",error), "save second workspace");
        check(owner->load_workspace("Reading",error), "return to first workspace");
        owner->select_tab(2); view(owner,2.1,2100);
        check(owner->load_workspace("Other",error), "switch without saving");
        check(owner->load_workspace("Reading",error), "return without losing reading progress");
        owner->select_tab(2); expect(owner,2.1,2100);
        owner->close_current_tab(); owner->open_document(d);
        check(owner->save_workspace("Reading",error), "explicit save updates membership");
        check(owner->load_workspace("Reading",error), "reload new membership");
        check(owner->window_tabs.index(b)<0 && owner->window_tabs.index(d)>=0, "only explicit save adds/removes PDFs");
        owner->select_tab(owner->window_tabs.index(a)); owner->toggle_split(); settle();
        auto right = owner->split_pane_.data(); right->open_document(a);
        view(owner,1.2,1200); view(right,1.4,1400);
        check(owner->save_workspace("Split",error), "save independent split views");
        view(owner,2.2,2200); view(right,2.4,2400);
        owner->focus_split_pane(0); owner->toggle_split(); settle();
        check(owner->load_workspace("Split",error), "restore split after temporarily closing it"); settle();
        expect(owner,2.2,2200); expect(owner->split_pane_,2.4,2400);
        view(owner,2.5,2500); owner->close();
        QJsonObject closed; check(read_workspace_store(closed,error), "read workspace after close");
        auto fresh = new MainWidget(owner); windows.push_back(fresh);
        check(fresh->load_workspace("Split",error), "restore workspace in fresh window after close");
        fresh->show(); settle(); expect(fresh,2.5,2500);
        auto session = fresh->capture_workspace(false);
        session["workspace_name"] = "Split";
        auto session_panes = session["panes"].toArray();
        auto session_first = session_panes[0].toObject();
        auto newer_view = session_first["view"].toObject();
        newer_view["zoom"] = 2.6; newer_view["y"] = 2600;
        session_first["view"] = newer_view; session_panes[0] = session_first;
        session["panes"] = session_panes;
        fresh->restore_session_layout(session);
        check(fresh->load_workspace("Split",error), "newer quit-session progress updates named workspace");
        expect(fresh,2.6,2600);
        qInfo("PASS: automatic reading state, closed/open tab membership, reorder, independent windows/splits, workspace switches and exit");
        result = 0;
    } catch(const std::exception& e) { qCritical("FAIL: %s", e.what()); }
    QCoreApplication::exit(result);
}
static void schedule() { QTimer::singleShot(1200,qApp,run); }
Q_COREAPP_STARTUP_FUNCTION(schedule)
int main(int, char** argv) {
    QStandardPaths::setTestModeEnabled(true); QTemporaryDir temp; directory=temp.path();
    char option[]="--instance-name", name[]="sioyek-workspace-reading-test";
    char* args[]={argv[0],argv[1],option,name}; sioyek_application_main(4,args); return result;
}
