#include "../pdf_viewer/OpenWithApplication.h"
#include <QOpenGLWidget>
#include <QEventLoop>
#include <QSurfaceFormat>
#include <QTimer>
#include <QCursor>
#include <QKeyEvent>
#include <QMouseEvent>
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
        [NSApp activateIgnoringOtherApps:YES];
        window.raise(); window.activateWindow();
        settle(700); // Allow the window manager to finish tiling the test window.
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
    } catch (const std::exception& e) { qCritical("FAIL: %s", e.what()); return 1; }
    return 0;
}
