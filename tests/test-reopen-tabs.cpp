#define main sioyek_application_main
#include "../pdf_viewer/main.cpp"
#undef main
#include <QTemporaryDir>
#include <QEventLoop>
#include <QListWidget>
#include <stdexcept>

static QString directory;
static int result = 1;
static void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
static void settle(int milliseconds = 120) {
    QEventLoop loop;
    QTimer::singleShot(milliseconds, &loop, &QEventLoop::quit);
    loop.exec();
}
static std::wstring path(int i) { return (directory + "/" + QString::number(i) + ".pdf").toStdWString(); }
static void expect(MainWidget* pane, int i) {
    if (!pane->doc() || pane->doc()->get_path() != path(i))
        qWarning() << "Expect" << i << "got" << (pane->doc() ? QString::fromStdWString(pane->doc()->get_path()) : "empty");
    check(pane->doc() && pane->doc()->get_path() == path(i), "expected reopened document in focused pane");
}
static void reopen(MainWidget* pane) {
    pane->setFocus();
    QKeyEvent press(QEvent::KeyPress, Qt::Key_T, Qt::ControlModifier | Qt::ShiftModifier, "T");
    QApplication::sendEvent(pane, &press);
    QKeyEvent release(QEvent::KeyRelease, Qt::Key_T, Qt::ControlModifier | Qt::ShiftModifier, "T");
    QApplication::sendEvent(pane, &release);
    settle();
}
static void last_tab(MainWidget* pane) {
    pane->setFocus();
    QKeyEvent event(QEvent::KeyPress,Qt::Key_9,Qt::ControlModifier,"9");
    QApplication::sendEvent(pane,&event);settle();
}
static void run_test() {
    try {
        auto pane=windows.front(); pane->window_tabs={}; pane->closed_tabs_.clear();
        pane->open_document(path(0));
        for(int i=1;i<4;++i) pane->open_document(path(i));
        pane->select_tab(0);last_tab(pane);expect(pane,3);
        for(int i=4;i<12;++i) pane->open_document(path(i));
        pane->select_tab(0);last_tab(pane);expect(pane,11);
        for(int i=4;i<12;++i) pane->close_tab(path(i));
        pane->closed_tabs_.clear();
        pane->select_tab(0); settle();
        const auto original = pane->main_document_view->get_state().book_state;
        pane->close_tab(path(3));
        expect(pane, 0); // Closing a background tab must not change the document.
        pane->open_document(path(3));
        pane->close_current_tab();
        expect(pane, 0); // Return to the opener, not the adjacent tab (2).
        const auto returned = pane->main_document_view->get_state().book_state;
        check(std::abs(original.offset_y-returned.offset_y)<0.1 &&
              std::abs(original.zoom_level-returned.zoom_level)<0.001, "return preserves reading position");
        pane->open_document(path(3));
        pane->closed_tabs_.clear();
        pane->select_tab(1); settle();
        auto saved=pane->main_document_view->get_state().book_state;
        saved.zoom_level=1.4f; saved.offset_y=500; pane->main_document_view->set_book_state(saved);
        pane->close_current_tab();
        pane->close_tab(path(3));
        check(pane->closed_tabs_.size()==2,"two closed tabs recorded");
        reopen(pane); expect(pane,3);
        reopen(pane); expect(pane,1);
        check((pane->window_tabs.paths()==std::vector<std::wstring>{path(0),path(1),path(2),path(3)}),"reopening restores visual order");
        auto restored=pane->main_document_view->get_state().book_state;
        check(std::abs(restored.zoom_level-saved.zoom_level)<0.001 && std::abs(restored.offset_y-saved.offset_y)<0.1,"reading position and zoom restored");
        reopen(pane); expect(pane,1);
        pane->close_current_tab(); pane->open_document(path(1)); reopen(pane);
        check(pane->window_tabs.paths().size()==4,"manual reopening never creates duplicate tabs");
        pane->close_tab(path(2)); pane->close_tab(path(3));
        QFile::remove(QString::fromStdWString(path(3)));
        reopen(pane); expect(pane,2);
        check(pane->closed_tabs_.empty(),"missing file skipped to next restorable tab");
        pane->toggle_split(); settle(); auto right=pane->split_pane_.data();
        right->open_document(path(1)); pane->focus_split_pane(1);
        const auto left_before_last=pane->doc()->get_path();
        last_tab(right);check(right->doc()->get_path()==pane->window_tabs.paths().back() && pane->doc()->get_path()==left_before_last,"Cmd 9 changes only focused split");
        right->open_document(path(1));
        right->close_current_tab();
        const auto left=pane->doc()->get_path();
        reopen(right); expect(right,1);
        check(pane->doc()->get_path()==left,"restore affects focused split pane only");
        pane->toggle_split(); settle();
        for(const auto& file:pane->window_tabs.paths()) pane->close_tab(file);
        check(pane->window_tabs.paths().empty(),"closing last real tab shows canvas");
        last_tab(pane);check(pane->window_tabs.paths().empty(),"Cmd 9 on empty window leaves canvas alone");
        const auto count=pane->closed_tabs_.size(); pane->close_current_tab();
        check(pane->closed_tabs_.size()==count,"hidden canvas is never recorded as closed");
        reopen(pane); check(pane->window_tabs.paths().size()==1,"restore works from canvas");
        last_tab(pane);check(pane->doc()->get_path()==pane->window_tabs.paths().front(),"Cmd 9 works with one tab");
        qInfo("PASS: Cmd Shift T, closed-tab order, saved view, split ownership, duplicate/missing files and canvas");
        result=0; QCoreApplication::exit(0);
    } catch(const std::exception& error) { qCritical("FAIL: %s",error.what()); QCoreApplication::exit(1); }
}
static void schedule_test() { QTimer::singleShot(1200, qApp, run_test); }
Q_COREAPP_STARTUP_FUNCTION(schedule_test)
int main(int argc, char** argv) {
    QStandardPaths::setTestModeEnabled(true);
    QTemporaryDir temp;
    directory = temp.path();
    for (int i = 0; i < 12; ++i) QFile::copy(QString::fromUtf8(argv[1]), QString::fromStdWString(path(i)));
    QByteArray first = QString::fromStdWString(path(0)).toUtf8();
    char option[] = "--instance-name";
    char name[] = "sioyek-reopen-tabs-regression";
    char* args[] = {argv[0], first.data(), option, name};
    sioyek_application_main(4, args);
    return result;
}
