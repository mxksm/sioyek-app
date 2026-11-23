#include "../pdf_viewer/OpenWithApplication.h"
#include <QOpenGLWidget>
#include <QEventLoop>
#include <QSurfaceFormat>
#include <QTimer>
#include <QCursor>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QWindow>
#include <QScreen>
#include <QVBoxLayout>
#import <AppKit/AppKit.h>
#include <stdexcept>

static void check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
static void settle(int ms = 80) { QEventLoop loop; QTimer::singleShot(ms, &loop, &QEventLoop::quit); loop.exec(); }
static void post(NSString* name) {
    [NSWorkspace.sharedWorkspace.notificationCenter postNotificationName:name object:nil];
}
class PaintWindow : public QOpenGLWidget {
public:
    int paints = 0;
    void paintGL() override { ++paints; }
};
int main(int argc, char** argv) {
    QSurfaceFormat format; format.setVersion(3,3); format.setProfile(QSurfaceFormat::CoreProfile);
    QSurfaceFormat::setDefaultFormat(format);
    OpenWithApplication app(argc, argv);
    try {
        PaintWindow window; window.resize(320,240); window.show(); settle(300);
        int swaps = 0;
        QObject::connect(&window, &QOpenGLWidget::frameSwapped, &window, [&] { ++swaps; });
        [NSApp activateIgnoringOtherApps:YES];
        window.raise(); window.activateWindow();
        settle(700); // Allow the window manager to finish tiling the test window.
        if (!qEnvironmentVariableIsSet("SIOYEK_DISPLAY_TEST_ONLY")) {
            window.setCursor(Qt::CrossCursor);
            QKeyEvent key(QEvent::KeyPress, Qt::Key_Shift, Qt::ShiftModifier);
            QMouseEvent mouse(QEvent::MouseMove, QPointF(20, 20), QPointF(window.mapToGlobal(QPoint(20, 20))),
                Qt::NoButton, Qt::NoButton, Qt::NoModifier);
            QApplication::sendEvent(&window, &mouse);
            for (int i = 0; i < 40; ++i) {
                QApplication::sendEvent(&window, &key);
                settle(100);
            }
            check(QApplication::overrideCursor() && QApplication::overrideCursor()->shape() == Qt::BlankCursor,
                  "idle pointer hides after three seconds");
            QApplication::sendEvent(&window, &key);
            check(QApplication::overrideCursor() && QApplication::overrideCursor()->shape() == Qt::BlankCursor,
                  "keyboard input keeps the pointer hidden");
            QApplication::sendEvent(&window, &mouse);
            check(!QApplication::overrideCursor() && window.cursor().shape() == Qt::CrossCursor,
                  "mouse movement restores the original tool cursor");
            settle(3500);
            check(QApplication::overrideCursor(), "pointer hides again after mouse activity");
            QEvent leave(QEvent::Leave); QApplication::sendEvent(&window, &leave);
            check(!QApplication::overrideCursor(), "leaving the window restores the pointer");
            qInfo("PASS: idle pointer hides, keyboard leaves pointer hidden, mouse restores tool cursor, window exit restores visibility");
        }
        QWidget disabled; disabled.setUpdatesEnabled(false);
        check(window.paints > 0, "initial OpenGL frame renders");
        post(NSWorkspaceWillSleepNotification);
        post(NSWorkspaceScreensDidSleepNotification);
        check(app.renderingSuspended() && !window.updatesEnabled(), "sleep disables composition");
        int before = window.paints;
        QEvent queued(QEvent::UpdateRequest); QApplication::sendEvent(&window, &queued);
        window.update(); settle(); check(window.paints == before, "queued paints do not render during sleep");
        auto transient = new PaintWindow; transient->show(); settle();
        check(!transient->updatesEnabled(), "new windows also suspend"); delete transient;
        post(NSWorkspaceDidWakeNotification); settle(650);
        check(app.renderingSuspended(), "system wake waits for display wake");
        post(NSWorkspaceScreensDidWakeNotification); settle(100);
        check(app.renderingSuspended(), "wake defers painting until Cocoa settles");
        post(NSWorkspaceScreensDidSleepNotification); settle(650);
        check(app.renderingSuspended(), "sleep cancels pending resume");
        post(NSWorkspaceScreensDidWakeNotification); settle(750);
        check(!app.renderingSuspended() && window.updatesEnabled() && window.paints > before,
              "wake resumes OpenGL rendering");
        check(!disabled.updatesEnabled(), "previously disabled windows stay disabled");
        post(NSWorkspaceWillSleepNotification); post(NSWorkspaceDidWakeNotification); settle(750);
        check(!app.renderingSuspended(), "system-only sleep resumes");
        post(NSWorkspaceScreensDidSleepNotification); post(NSWorkspaceScreensDidWakeNotification); settle(750);
        check(!app.renderingSuspended(), "display-only sleep resumes");
        qInfo("PASS: native sleep/wake notifications, queued paints, repeated sleep, deleted/new windows and OpenGL resume");

        // Exercise Qt's actual screenChanged -> handleScreenChange -> immediate
        // repaint path from both crash reports, without unplugging this Mac.
        const auto changeScreen = [&] {
            window.windowHandle()->screenChanged(window.screen());
        };
        before = swaps;
        changeScreen();
        check(app.renderingSuspended() && !window.updatesEnabled(), "screen change suspends before Qt's synchronous repaint");
        check(swaps == before, "screen change cannot immediately swap the old drawable");
        QApplication::sendEvent(&window, &queued);
        window.repaint(); settle(100);
        check(swaps == before, "queued and explicit repaints cannot present during reconfiguration");
        for (int i = 0; i < 4; ++i) { changeScreen(); settle(150); }
        settle(150);
        check(app.renderingSuspended() && swaps == before, "a burst of display changes restarts the settle delay");
        settle(650);
        check(!app.renderingSuspended() && swaps > before, "composition resumes after final display change");
        check(!disabled.updatesEnabled(), "display changes preserve preexisting disabled state");
#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
        before = swaps;
        QEvent dpi(QEvent::DevicePixelRatioChange);
        QApplication::sendEvent(window.windowHandle(), &dpi);
        check(app.renderingSuspended() && swaps == before, "Retina/external DPI change blocks immediate composition");
        settle(750);
        check(!app.renderingSuspended() && swaps > before, "DPI change resumes with a new frame");
#endif
        qInfo("PASS: actual Qt screenChanged/DPI repaint paths, frame swaps and multi-monitor event bursts");

        app.setDisplayReconfiguring(true);
        post(NSWorkspaceWillSleepNotification);
        app.setDisplayReconfiguring(false); settle(750);
        check(app.renderingSuspended(), "display reconfiguration completion cannot override system sleep");
        app.setDisplayReconfiguring(true);
        post(NSWorkspaceDidWakeNotification); settle(750);
        check(app.renderingSuspended(), "system wake cannot override unfinished display reconfiguration");
        app.setDisplayReconfiguring(false); settle(750);
        check(!app.renderingSuspended(), "both blockers must clear before rendering resumes");
        // A later monitor-removal event is still guarded after the wake delay.
        before = swaps; changeScreen();
        check(app.renderingSuspended() && swaps == before, "late topology changes suspend again after wake");
        post(NSWorkspaceScreensDidSleepNotification); settle(750);
        check(app.renderingSuspended(), "sleep cancels a pending topology resume");
        post(NSWorkspaceScreensDidWakeNotification); settle(750);
        check(!app.renderingSuspended() && swaps > before, "interleaved sleep and topology changes recover");
        qInfo("PASS: independent sleep/reconfiguration blockers and late monitor removal after wake");

        // Match Sioyek's QWidget shell with child GL panes, not only a top-level
        // QOpenGLWidget. All windows must be guarded together.
        QWidget shell;
        auto layout = new QVBoxLayout(&shell);
        auto pane = new PaintWindow;
        layout->addWidget(pane);
        shell.resize(420, 300); shell.show(); settle(800);
        int child_swaps = 0;
        QObject::connect(pane, &QOpenGLWidget::frameSwapped, &shell, [&] { ++child_swaps; });
        before = swaps;
        shell.windowHandle()->screenChanged(shell.screen());
        check(app.renderingSuspended() && !shell.updatesEnabled() && !window.updatesEnabled(), "all windows suspend before a child-pane shell repaints");
        QApplication::sendEvent(&shell, &queued); settle(150);
        check(child_swaps == 0 && swaps == before, "neither top-level nor child GL panes swap while suspended");
        settle(650);
        check(!app.renderingSuspended() && shell.updatesEnabled() && window.updatesEnabled() && child_swaps > 0,
              "multiwindow composition resumes after settling");
        qInfo("PASS: multiple windows and nested GL-pane composition");

        // A window sent to another Space can remain QWidget-visible even while
        // its native drawable is unavailable. Exercise native ordering without
        // changing the user's desktop or physical monitor configuration.
        NSWindow* nativeShell = [(NSView*)shell.winId() window];
        [nativeShell orderOut:nil];
        settle(750);
        check(shell.isVisible() && !nativeShell.visible, "native ordering leaves Qt widget logically visible");
        check(!shell.updatesEnabled(), "unpresentable native window stays suspended after the settle timer");
        before = child_swaps;
        QApplication::sendEvent(&shell, &queued);
        QExposeEvent expose(QRegion(shell.rect()));
        QApplication::sendEvent(shell.windowHandle(), &expose);
        shell.repaint(); settle(750);
        check(child_swaps == before, "queued updates and exposes cannot present an unavailable window");
        window.raise(); settle(750);
        const int visible_before = swaps;
        window.update(); settle();
        check(!app.renderingSuspended() && swaps > visible_before,
              "a window on an unavailable Space does not stop visible windows rendering");
        [nativeShell orderFront:nil];
        for (int i = 0; i < 50 && (!shell.updatesEnabled() || child_swaps == before); ++i) settle(100);
        check(shell.updatesEnabled() && child_swaps > before,
              "native re-exposure resumes the deferred window and repaints its contents");

        before = child_swaps;
        post(NSWorkspaceActiveSpaceDidChangeNotification);
        check(app.renderingSuspended(), "native Space transitions suspend without a Qt screen-change event");
        QApplication::sendEvent(&shell, &queued);
        check(child_swaps == before, "Space transfer guards the posted repaint path from the crash");
        settle(750);
        check(!app.renderingSuspended() && shell.updatesEnabled() && child_swaps > before,
              "Space transition resumes once the window is exposed and settled");
        qInfo("PASS: native occlusion, queued/expose suppression, independent visible windows and Space-change recovery");

    } catch (const std::exception& e) { qCritical("FAIL: %s", e.what()); return 1; }
    return 0;
}
