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
static void settle() {
    QEventLoop loop;
    QTimer::singleShot(120, &loop, &QEventLoop::quit);
    loop.exec();
}
#ifdef Q_OS_MACOS
static const auto control = Qt::MetaModifier;
static const auto control_key = Qt::Key_Meta;
#else
static const auto control = Qt::ControlModifier;
static const auto control_key = Qt::Key_Control;
#endif
static void step(QWidget* target, bool backwards = false, bool repeat = false) {
    auto modifiers = Qt::KeyboardModifiers(control);
    if (backwards) modifiers |= Qt::ShiftModifier;
    int key = backwards ? Qt::Key_Backtab : Qt::Key_Tab;
    QKeyEvent press(QEvent::KeyPress, key, modifiers, "\t", repeat);
    QApplication::sendEvent(target, &press);
    QKeyEvent release(QEvent::KeyRelease, key, modifiers, "\t", repeat);
    QApplication::sendEvent(target, &release);
    settle();
}
static void release_control(QWidget* target) {
    QKeyEvent release(QEvent::KeyRelease, control_key, Qt::NoModifier);
    QApplication::sendEvent(target, &release);
    settle();
}
static std::wstring path(int i) { return (directory + "/" + QString::number(i) + ".pdf").toStdWString(); }
static void expect(MainWidget* pane, int i) {
    if (!pane->doc() || pane->doc()->get_path() != path(i))
        qWarning() << "Expect" << i << "got" << (pane->doc() ? QString::fromStdWString(pane->doc()->get_path()) : "empty");
    check(pane->doc() && pane->doc()->get_path() == path(i), "expected MRU document in focused pane");
}
static void run_test() {
    try {
        auto pane = windows.front();
        // Startup may seed a tutorial tab; keep this fixture independent of it.
        pane->window_tabs = {};
        pane->open_document(path(0));
        for (int i = 1; i < 4; ++i) pane->open_document(path(i));
        pane->select_tab(1); // MRU 1,3,2,0
        pane->setFocus(); settle();
        const auto visual_order = pane->window_tabs.paths();
        check(visual_order.size() == 4, "four fixture tabs loaded");
        step(pane); expect(pane, 3);
        auto switcher = pane->findChild<QWidget*>("recentTabSwitcher");
        auto list = pane->findChild<QListWidget*>("recentTabList");
        check(switcher && switcher->isVisible() && list, "switcher appears while Control is held");
        check(QApplication::focusWidget() == pane, "switcher leaves PDF keyboard focus intact");
        const QStringList expected_labels = {"2: 1", "4: 3", "3: 2", "1: 0"};
        for (int i = 0; i < list->count(); ++i)
            check(list->item(i)->text() == expected_labels[i], "switcher displays MRU order with original tab numbers");
        check(list->currentRow() == 1, "switcher highlights the first preview");
        step(pane, false, true); expect(pane, 2);
        check(list->currentRow() == 2 && list->item(0)->text() == expected_labels[0], "highlight moves without reordering rows");
        step(pane); expect(pane, 0);
        step(pane); expect(pane, 1);
        step(pane, true); expect(pane, 0);
        step(pane, true); expect(pane, 2);
        release_control(pane); // MRU 2,1,3,0
        check(!switcher->isVisible(), "Control release dismisses the switcher");
        step(pane); expect(pane, 1); release_control(pane);
        step(pane); expect(pane, 2); release_control(pane);
        check(pane->window_tabs.paths() == visual_order, "MRU switching keeps tab numbers stable");
        qInfo("PASS: held Control cycles all tabs, wraps, reverses and commits on release");

        // Selectors must not steal Tab or swallow the Control release.
        pane->handle_goto_loaded_document(); settle();
        auto edit = QApplication::focusWidget();
        check(edit != pane, "selector has keyboard focus");
        step(edit); expect(pane, 1);
        release_control(QApplication::focusWidget());
        pane->pop_all_widgets(); pane->setFocus();
        step(pane); expect(pane, 2); release_control(pane);
        qInfo("PASS: selector focus does not consume the tab-switch shortcut");

        pane->toggle_split(); settle();
        auto right = pane->split_pane_.data();
        check(right, "split pane exists");
        right->select_tab(3);
        pane->focus_split_pane(0);
        pane->select_tab(0);
        pane->focus_split_pane(1); // MRU starts with right's 3, then left's 0
        step(right); expect(right, 0); expect(pane, 0);
        check(switcher->parentWidget() == pane &&
            (switcher->geometry().center() - pane->rect().center()).manhattanLength() <= 2,
            "split view uses one switcher centered over the full window");
        check(QApplication::focusWidget() == right, "switcher preserves right-pane focus");
        step(right); expect(right, 2); expect(pane, 0);
        release_control(right);
        qInfo("PASS: shared MRU tabs switch only the focused split pane");

        QEvent deactivate(QEvent::WindowDeactivate);
        step(right); expect(right, 3);
        QApplication::sendEvent(pane, &deactivate);
        check(!switcher->isVisible(), "deactivation dismisses switcher");
        step(right); expect(right, 2); release_control(right);
        pane->close_tab(path(3));
        step(right); expect(right, 0); release_control(right);
        qInfo("PASS: window deactivation commits the cycle and closed tabs are skipped");

        // More tabs than visible rows must keep the highlighted choice in view.
        for (int i = 4; i < 14; ++i) {
            QFile::copy(QString::fromStdWString(path(0)), QString::fromStdWString(path(i)));
            right->open_document(path(i));
        }
        pane->resize(900, 640); settle();
        for (int i = 0; i < 11; ++i) step(right);
        check(list->currentRow() == 11, "long cycle reaches later MRU entries");
        check(list->viewport()->rect().contains(list->visualItemRect(list->currentItem()).center()),
            "long list scrolls to the highlighted entry");
        check(pane->rect().contains(switcher->geometry()), "switcher stays within resized window");
        if (qEnvironmentVariableIsSet("SIOYEK_MRU_SCREENSHOT"))
            pane->grab().save(qEnvironmentVariable("SIOYEK_MRU_SCREENSHOT"));
        release_control(right);
        step(right);
        QKeyEvent escape(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
        QApplication::sendEvent(right, &escape);
        check(!switcher->isVisible(), "another command dismisses switcher");
        qInfo("PASS: MRU overlay order, highlight, focus, split centering, scrolling and dismissal");
        result = 0;
        QCoreApplication::exit(0);
    } catch (const std::exception& error) {
        qCritical("FAIL: %s", error.what());
        QCoreApplication::exit(1);
    }
}
static void schedule_test() { QTimer::singleShot(1200, qApp, run_test); }
Q_COREAPP_STARTUP_FUNCTION(schedule_test)
int main(int argc, char** argv) {
    QStandardPaths::setTestModeEnabled(true);
    QTemporaryDir temp;
    directory = temp.path();
    for (int i = 0; i < 4; ++i) QFile::copy(QString::fromUtf8(argv[1]), QString::fromStdWString(path(i)));
    QByteArray first = QString::fromStdWString(path(0)).toUtf8();
    char option[] = "--instance-name";
    char name[] = "sioyek-mru-tabs-regression";
    char* args[] = {argv[0], first.data(), option, name};
    sioyek_application_main(4, args);
    return result;
}
