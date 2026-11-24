#include <OpenWithApplication.h>
#include <functional>
#include <QCursor>
#include <QWindow>
#include <algorithm>

#ifdef Q_OS_MACOS
void setDisplaySleepHandler(std::function<void(bool)> handler);
void setDisplayReconfigurationHandler(std::function<void(bool)> handler);
void setWindowPresentationHandler(std::function<void()> handler);
bool isWindowReadyForPresentation(WId winId);
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
        if (display_sleeping_ || display_reconfiguring_ || screens().isEmpty()) return;
        rendering_suspended_ = false;
        refreshWindowPresentation();
    });
    setDisplaySleepHandler([this](bool sleeping) { setDisplaySleeping(sleeping); });
    setDisplayReconfigurationHandler([this](bool changing) { setDisplayReconfiguring(changing); });
    setWindowPresentationHandler([this] { refreshWindowPresentation(); });
    connect(this, &QGuiApplication::screenAdded, this, [this] { displayConfigurationChanged(); });
    connect(this, &QGuiApplication::screenRemoved, this, [this] { displayConfigurationChanged(); });
#endif
}

OpenWithApplication::~OpenWithApplication() {
    removeEventFilter(this);
    showPointer();
#ifdef Q_OS_MACOS
    setDisplaySleepHandler({});
    setDisplayReconfigurationHandler({});
    setWindowPresentationHandler({});
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
bool OpenWithApplication::windowSuspended(QWidget* window) const {
    return std::any_of(suspended_windows_.begin(), suspended_windows_.end(),
        [window](const auto& suspended) { return suspended == window; });
}

bool OpenWithApplication::windowReadyForPresentation(QWidget* window) const {
    const auto handle = window ? window->windowHandle() : nullptr;
    return handle && handle->isExposed() && isWindowReadyForPresentation(window->internalWinId());
}

void OpenWithApplication::suspendWindow(QWidget* window) {
    if (!window || !window->isWindow() || !window->updatesEnabled()) return;
    suspended_windows_.push_back(window);
    window->setUpdatesEnabled(false);
}

void OpenWithApplication::refreshWindowPresentation() {
    // Exposure changes also happen on an ordinary open/raise. They are not a
    // monitor reconfiguration and must not start or extend the settle timer.
    for (auto window : topLevelWidgets()) {
        if (!windowReadyForPresentation(window)) suspendWindow(window);
    }
    if (rendering_suspended_) return;
    auto windows = std::move(suspended_windows_);
    suspended_windows_.clear();
    for (auto window : windows) if (window) {
        if (!windowReadyForPresentation(window)) {
            suspended_windows_.push_back(window);
            continue;
        }
        window->setUpdatesEnabled(true);
        window->update();
    }
}

void OpenWithApplication::suspendRendering() {
    // Stop widget composition, not just paintGL: Qt can present an already
    // rendered frame from its backing store while the display is unavailable.
    resume_rendering_.stop();
    rendering_suspended_ = true;
    for (auto window : topLevelWidgets()) suspendWindow(window);
}

void OpenWithApplication::scheduleRenderingResume() {
    // Debounce the entire transition, not just the wake notification. Monitor
    // removal/DPI changes can follow wake by seconds and arrive in bursts.
    if (!display_sleeping_ && !display_reconfiguring_ && !screens().isEmpty())
        resume_rendering_.start();
}

void OpenWithApplication::setDisplaySleeping(bool sleeping) {
    display_sleeping_ = sleeping;
    suspendRendering();
    scheduleRenderingResume();
}

void OpenWithApplication::setDisplayReconfiguring(bool reconfiguring) {
    display_reconfiguring_ = reconfiguring;
    suspendRendering();
    scheduleRenderingResume();
}

void OpenWithApplication::displayConfigurationChanged() {
    suspendRendering();
    scheduleRenderingResume();
}

bool OpenWithApplication::notify(QObject* receiver, QEvent* event) {
    if (event->type() == QEvent::ScreenChangeInternal
#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
        || event->type() == QEvent::DevicePixelRatioChange
#endif
    ) {
        // Qt 6.8's QWidgetWindow sends this BEFORE synchronously repainting its
        // backing store. A screenChanged signal connection runs too late:
        // Qt's own handler was connected first. Disable composition here, then
        // still deliver the event so Qt can update screen/DPI bookkeeping.
        auto handle = qobject_cast<QWindow*>(receiver);
        if (auto widget = qobject_cast<QWidget*>(receiver)) handle = widget->window()->windowHandle();
        // Qt also sends these events when assigning a screen to a NEW window.
        // There is no old presented framebuffer to retire in that case.
        if (handle && std::any_of(exposed_windows_.begin(), exposed_windows_.end(),
                [handle](const auto& exposed) { return exposed == handle; }))
            displayConfigurationChanged();
    }
    const QPointer<QWindow> expose_handle = event->type() == QEvent::Expose
        ? qobject_cast<QWindow*>(receiver) : nullptr;
    if (expose_handle) {
        // Guard Qt's synchronous expose flush, but resume ready windows without
        // a delay unless an actual screen/sleep transition is still settling.
        refreshWindowPresentation();
    }
    if (auto widget = qobject_cast<QWidget*>(receiver)) {
        auto window = widget->window();
        if (rendering_suspended_ && event->type() == QEvent::Show) suspendWindow(window);
        if (event->type() == QEvent::UpdateRequest) {
            // This is the queued QWidgetRepaintManager::sync() path. Qt checks
            // isVisible/WA_Mapped here, not whether Cocoa can actually present
            // the window on the active Space. Check before entering the driver.
            if (!windowReadyForPresentation(window)) suspendWindow(window);
            if (rendering_suspended_ || windowSuspended(window)) return true;
        }
    }
    const bool handled = QApplication::notify(receiver, event);
    if (expose_handle) {
        if (auto handle = expose_handle.data(); handle->isExposed()) {
            exposed_windows_.erase(std::remove_if(exposed_windows_.begin(), exposed_windows_.end(),
                [handle](const auto& exposed) { return !exposed || exposed == handle; }), exposed_windows_.end());
            exposed_windows_.push_back(handle);
        }
        refreshWindowPresentation();
    }
    return handled;
}
#endif

bool OpenWithApplication::event(QEvent* event) {
    if (event->type() == QEvent::FileOpen) {
        QFileOpenEvent* openEvent = static_cast<QFileOpenEvent*>(event);
        emit file_ready(openEvent->file());
    }

    return QApplication::event(event);
}
