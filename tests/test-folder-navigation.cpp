#define main sioyek_application_main
#include "../pdf_viewer/main.cpp"
#undef main
#include "../pdf_viewer/blank_canvas.h"
#include <QTemporaryDir>
#include <QEventLoop>
#include <stdexcept>

static QString directory;
static int result = 1;
static void check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
static void settle() { QEventLoop loop; QTimer::singleShot(100, &loop, &QEventLoop::quit); loop.exec(); }
static std::wstring path(const QString& name) { return (directory + "/" + name).toStdWString(); }
static void expect(MainWidget* pane, const QString& name) {
    check(pane->doc() && pane->doc()->get_path() == path(name), "expected PDF in the invoking pane");
}
static void navigate(MainWidget* pane, bool next) {
    pane->setFocus();
    QKeyEvent press(QEvent::KeyPress, next ? Qt::Key_L : Qt::Key_D, Qt::ShiftModifier, next ? "L" : "D");
    QApplication::sendEvent(pane, &press);
    QKeyEvent release(QEvent::KeyRelease, press.key(), Qt::ShiftModifier, press.text());
    QApplication::sendEvent(pane, &release);
    settle();
}
static bool same_view(const OpenedBookState& a, const OpenedBookState& b) {
    return std::abs(a.offset_x-b.offset_x)<0.01 && std::abs(a.offset_y-b.offset_y)<0.01 &&
        std::abs(a.zoom_level-b.zoom_level)<0.001;
}
static void run() {
    try {
        auto owner = windows.front();
        owner->resize(1100,800); owner->activateWindow(); owner->setFocus(); settle();
        owner->open_document(path("a first.pdf"));
        owner->open_document(path("b second.pdf"));
        owner->open_document(path("c third.pdf"));
        owner->open_document(path("a first.pdf"));
        owner->toggle_split(); settle();
        auto other = owner->split_pane_.data(); check(other, "split created");
        other->open_document(path("b second.pdf"));
        auto saved = other->main_document_view->get_state().book_state;
        saved.zoom_level=1.6f; saved.offset_y=700; other->main_document_view->set_book_state(saved);
        owner->focus_split_pane(0); settle();
        const auto tabs=owner->window_tabs.paths();
        navigate(owner,true);
        expect(owner,"b second.pdf"); expect(other,"b second.pdf");
        check(owner->active_pane()==owner, "next file keeps left focus when right already displays target");
        check(same_view(other->main_document_view->get_state().book_state,saved), "companion reading position stays unchanged");
        check(owner->window_tabs.paths()==tabs, "opening a PDF already in the split does not duplicate its tab");
        owner->open_document(path("a first.pdf"));
        owner->focus_split_pane(1);
        const auto left=owner->main_document_view->get_state().book_state;
        navigate(other,false);
        expect(owner,"a first.pdf"); expect(other,"a first.pdf");
        check(owner->active_pane()==other && same_view(owner->main_document_view->get_state().book_state,left),
            "previous file keeps right focus and left view");
        navigate(other,false); expect(other,"c third.pdf");
        navigate(other,true); expect(other,"a first.pdf");
        check(owner->active_pane()==other, "alphabetical wraparound remains in right pane");
        owner->toggle_split(Qt::Vertical); settle();
        owner->open_document(path("b second.pdf")); other->open_document(path("a first.pdf"));
        owner->focus_split_pane(1); navigate(other,true);
        expect(other,"b second.pdf"); expect(owner,"b second.pdf");
        check(owner->active_pane()==other, "stacked split also keeps invoking pane focused");
        owner->toggle_split(Qt::Vertical); settle();
        owner->open_document(path("a first.pdf"));
        navigate(owner,true); expect(owner,"b second.pdf");
        navigate(owner,false); expect(owner,"a first.pdf");
        owner->open_document(path("alone/only.pdf"));
        const auto alone=owner->main_document_view->get_state().book_state;
        navigate(owner,true); navigate(owner,false); expect(owner,"alone/only.pdf");
        check(same_view(owner->main_document_view->get_state().book_state,alone), "one-PDF folder is a no-op");
        QFile::remove(QString::fromStdWString(path("alone/only.pdf")));
        navigate(owner,true); navigate(owner,false);
        check(owner->doc(), "missing current PDF is harmless");
        qInfo("PASS: next/previous folder shortcuts preserve focus and companion views in side-by-side and stacked splits; wrapping, unique tabs, single and missing PDFs");
        result=0; QCoreApplication::exit(0);
    } catch (const std::exception& error) { qCritical("FAIL: %s",error.what()); QCoreApplication::exit(1); }
}
static void schedule() { QTimer::singleShot(1200,qApp,run); }
Q_COREAPP_STARTUP_FUNCTION(schedule)
int main(int, char** argv) {
    QStandardPaths::setTestModeEnabled(true); QTemporaryDir temp;
    directory = QDir(temp.path()).canonicalPath();
    QDir(directory).mkdir("alone"); QDir(directory).mkdir("folder.pdf");
    for (auto name : {"a first.pdf", "b second.pdf", "c third.pdf", "alone/only.pdf", "ignored.txt"}) {
        QFile file(QString::fromStdWString(path(name))); file.open(QIODevice::WriteOnly); file.write(blank_canvas_pdf(3));
    }
    QByteArray first=QString::fromStdWString(path("a first.pdf")).toUtf8();
    char option[]="--instance-name", instance[]="sioyek-folder-navigation-test";
    char* args[]={argv[0],first.data(),option,instance};
    sioyek_application_main(4,args); return result;
}
