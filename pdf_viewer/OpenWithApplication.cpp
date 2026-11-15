#include <OpenWithApplication.h>
#include <functional>
#include <QCursor>

#ifdef Q_OS_MACOS
void setDisplaySleepHandler(std::function<void(bool)> handler);
#endif

OpenWithApplication::OpenWithApplication(int& argc, char** argv) : QApplication(argc, argv) {
    pointer_idle_timer_.setSingleShot(true);
    pointer_idle_timer_.setInterval(3000);
    connect(&pointer_idle_timer_, &QTimer::timeout, this, [this] {
        if (applicationState() != Qt::ApplicationActive || !activeWindow()) return;
        // A stationary mouse can still be holding a pen stroke or selection.
        if (mouseButtons() != Qt::NoButton) {
            pointer_idle_timer_.start();
            return;
        }
        if (!pointer_hidden_ && !overrideCursor()) {
            setOverrideCursor(Qt::BlankCursor);
            pointer_hidden_ = true;
        }
    });
    installEventFilter(this);
    connect(this, &QGuiApplication::applicationStateChanged, this, [this](Qt::ApplicationState state) {
        showPointer();
        if (state == Qt::ApplicationActive) restartPointerTimer();
    });
#ifdef Q_OS_MACOS
    resume_rendering_.setSingleShot(true);
    resume_rendering_.setInterval(500);
    connect(&resume_rendering_, &QTimer::timeout, this, [this] {
        rendering_suspended_ = false;
        auto windows = std::move(suspended_windows_);
        suspended_windows_.clear();
        for (auto window : windows) if (window) {
            window->setUpdatesEnabled(true);
            window->update();
        }
    });
    setDisplaySleepHandler([this](bool sleeping) { setDisplaySleeping(sleeping); });
#endif
}

OpenWithApplication::~OpenWithApplication() {
    removeEventFilter(this);
    showPointer();
#ifdef Q_OS_MACOS
    setDisplaySleepHandler({});
#endif
}

void OpenWithApplication::showPointer() {
    pointer_idle_timer_.stop();
    if (pointer_hidden_) {
        restoreOverrideCursor();
        pointer_hidden_ = false;
    }
}

void OpenWithApplication::restartPointerTimer() {
    showPointer();
    if (applicationState() == Qt::ApplicationActive) pointer_idle_timer_.start();
}

bool OpenWithApplication::eventFilter(QObject* receiver, QEvent* event) {
    switch (event->type()) {
    case QEvent::MouseMove:
    case QEvent::MouseButtonPress:
    case QEvent::MouseButtonRelease:
    case QEvent::MouseButtonDblClick:
    case QEvent::Wheel:
    case QEvent::TabletMove:
    case QEvent::TabletPress:
    case QEvent::TabletRelease:
    case QEvent::TouchBegin:
    case QEvent::TouchUpdate:
    case QEvent::TouchEnd:
    case QEvent::Enter:
    case QEvent::WindowActivate:
        restartPointerTimer();
        break;
    case QEvent::Leave:
    case QEvent::WindowDeactivate:
    case QEvent::Hide:
        if (auto widget = qobject_cast<QWidget*>(receiver); widget && widget->isWindow())
            showPointer();
        break;
    default:
        break;
    }
    return QApplication::eventFilter(receiver, event);
}

#ifdef Q_OS_MACOS
void OpenWithApplication::suspendWindow(QWidget* window) {
    if (!window || !window->isWindow() || !window->updatesEnabled()) return;
    suspended_windows_.push_back(window);
    window->setUpdatesEnabled(false);
}

void OpenWithApplication::setDisplaySleeping(bool sleeping) {
    // Stop widget composition, not just paintGL: Qt can present an already
    // rendered frame from its backing store while the display is unavailable.
    resume_rendering_.stop();
    rendering_suspended_ = true;
    for (auto window : topLevelWidgets()) suspendWindow(window);
    // Let Cocoa finish restoring its drawable before processing queued paints.
    if (!sleeping) resume_rendering_.start();
}

bool OpenWithApplication::notify(QObject* receiver, QEvent* event) {
    if (rendering_suspended_) {
        if (auto widget = qobject_cast<QWidget*>(receiver)) {
            if (event->type() == QEvent::Show) suspendWindow(widget->window());
            // An UpdateRequest may already have been posted before sleep.
            if (event->type() == QEvent::UpdateRequest) return true;
        }
    }
    return QApplication::notify(receiver, event);
}
#endif

bool OpenWithApplication::event(QEvent* event) {
    if (event->type() == QEvent::FileOpen) {
        QFileOpenEvent* openEvent = static_cast<QFileOpenEvent*>(event);
        emit file_ready(openEvent->file());
    }

    return QApplication::event(event);
}
