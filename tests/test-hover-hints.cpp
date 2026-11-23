#define main sioyek_application_main
#include "../pdf_viewer/main.cpp"
#undef main
#include <QHelpEvent>
#include <QTabBar>
#include <QToolButton>
#include <QListWidget>
#include <QTemporaryDir>
#include <stdexcept>

static int result = 1;
static void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
static void settle(int ms = 80) {
    QEventLoop loop; QTimer::singleShot(ms, &loop, &QEventLoop::quit); loop.exec();
}
static QLabel* hover(QWidget* target, QPoint point, QWidget* owner) {
    const auto focus = QApplication::focusWidget();
    const auto windows = QApplication::topLevelWidgets().size();
    QHelpEvent event(QEvent::ToolTip, point, target->mapToGlobal(point));
    QApplication::sendEvent(target, &event);
    auto hint = owner->findChild<QLabel*>("inWindowHoverHint", Qt::FindDirectChildrenOnly);
    check(hint && hint->isVisible(), "hover shows an in-window hint");
    check(!hint->isWindow() && hint->window() == owner, "hint is a child, never a native window");
    check(QApplication::topLevelWidgets().size() == windows, "hover creates no top-level tooltip");
    check(QApplication::focusWidget() == focus, "hover never steals keyboard focus");
    check(hint->testAttribute(Qt::WA_TransparentForMouseEvents), "hint cannot intercept PDF input");
    check(owner->rect().adjusted(8,8,-8,-8).contains(hint->geometry()), "hint fits inside owner");
    check(hint->width() <= 480 && hint->height() <= 5*hint->fontMetrics().lineSpacing()+16,
        "long hints stay compact");
    return hint;
}
static void test() {
    try {
        auto pane = windows.front();
        pane->resize(900,700); pane->show(); pane->activateWindow(); pane->setFocus(); settle();
        auto tabs = pane->findChild<QTabBar*>("floatingDocumentTabs");
        check(tabs && tabs->count(), "document tabs exist");
        tabs->show();
        auto hint = hover(tabs, tabs->tabRect(0).center(), pane);
        check(hint->text().contains("tutorial"), "tab hint contains document path");
        QKeyEvent key(QEvent::KeyPress, Qt::Key_Shift, Qt::ShiftModifier);
        QApplication::sendEvent(pane, &key);
        check(!hint->isVisible(), "keyboard input dismisses hint");

        // Exercise long paths, literal markup, edge placement and source deletion.
        auto source = new QLabel(pane);
        source->setGeometry(pane->width()-25, pane->height()-25, 20,20);
        source->setToolTip("<file>&" + QString(600, 'w') + "\n" + QString("excerpt ").repeated(200));
        source->show();
        hint = hover(source, QPoint(10,10), pane);
        check(hint->textFormat() == Qt::PlainText && hint->text().contains("<file>&"), "paths are plain text");
        check(hint->text().endsWith(QChar(0x2026)), "long excerpts end in an ellipsis");
        delete source;
        check(!hint->isVisible(), "deleting hovered source dismisses hint");

        pane->restore_navigation_history({}); pane->save_navigation_state();
        pane->history.back().snippet = "A useful saved excerpt";
        ++pane->navigation_history_revision_;
        pane->navigation_sidebar_visibility_override_ = 1;
        pane->update_navigation_sidebar(); settle();
        auto list = pane->findChild<QListWidget*>("navigationHistoryStops");
        check(list && list->isVisible(), "history sidebar visible");
        hint = hover(list->viewport(), list->visualItemRect(list->item(0)).center(), pane);
        check(hint->text().contains("Page") && hint->text().contains("saved excerpt"), "history hint includes location and excerpt");
        if (qEnvironmentVariableIsSet("SIOYEK_HOVER_SCREENSHOT")) pane->grab().save(qEnvironmentVariable("SIOYEK_HOVER_SCREENSHOT"));
        const QPoint away(-10,-10);
        QMouseEvent motion(QEvent::MouseMove, away, list->viewport()->mapToGlobal(away),
            Qt::NoButton, Qt::NoButton, Qt::NoModifier);
        QApplication::sendEvent(list->viewport(), &motion);
        check(!hint->isVisible(), "moving away from the hovered row dismisses hint");
        auto expand = pane->findChild<QToolButton*>("expandNavigationHistory");
        hint = hover(expand, expand->rect().center(), pane);
        check(hint->text().contains("Expand"), "history button uses the same hint");
        QEvent deactivate(QEvent::WindowDeactivate); QApplication::sendEvent(pane, &deactivate);
        check(!hint->isVisible(), "switching apps dismisses hint");

        pane->toggle_split(); settle();
        auto right = pane->split_pane_.data(); check(right, "split pane exists");
        right->save_navigation_state(); right->navigation_sidebar_visibility_override_ = 1;
        right->update_navigation_sidebar(); settle();
        list = right->findChild<QListWidget*>("navigationHistoryStops");
        check(list && list->isVisible(), "right history visible");
        hint = hover(list->viewport(), list->visualItemRect(list->item(0)).center(), pane);
        check(hint->parentWidget() == pane, "split hints belong to the shared outer window");
        ConfigFileChangeListener::notify_config_file_changed(pane->config_manager);
        check(!hint->isVisible(), "theme reload dismisses stale colors");
        hover(list->viewport(), list->visualItemRect(list->item(0)).center(), pane);
        pane->resize(500,600); settle();
        check(!hint->isVisible(), "resizing dismisses the old hint");
        hover(list->viewport(), list->visualItemRect(list->item(0)).center(), pane);
        settle(5200);
        check(!hint->isVisible(), "hover hint expires");
        qInfo("PASS: child-only tab/history hints, wrapping, bounds, focus, dismissal, split ownership and expiry");
        result = 0;
    } catch (const std::exception& e) { qCritical("FAIL: %s", e.what()); }
    QCoreApplication::exit(result);
}
static void schedule() { QTimer::singleShot(1000, qApp, test); }
Q_COREAPP_STARTUP_FUNCTION(schedule)
int main(int argc, char** argv) {
    Q_UNUSED(argc);
    QStandardPaths::setTestModeEnabled(true);
    char option[] = "--instance-name", name[] = "sioyek-hover-hints-test";
    char* args[] = {argv[0], argv[1], option, name};
    sioyek_application_main(4,args); return result;
}
