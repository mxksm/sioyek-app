#ifndef OPEN_WITH_APP_H
#define OPEN_WITH_APP_H

#include <QApplication>
#include <QFileOpenEvent>
#include <QPointer>
#include <QTimer>
#include <QWidget>
#include <QWindow>
#include <vector>

class OpenWithApplication : public QApplication
{
    Q_OBJECT
public:
    OpenWithApplication(int& argc, char** argv);
    ~OpenWithApplication() override;
#ifdef Q_OS_MACOS
    void setDisplaySleeping(bool sleeping);
    void setDisplayReconfiguring(bool reconfiguring);
    bool renderingSuspended() const { return rendering_suspended_; }
    bool notify(QObject* receiver, QEvent* event) override;
private:
    bool rendering_suspended_ = false;
    bool display_sleeping_ = false;
    bool display_reconfiguring_ = false;
    QTimer resume_rendering_;
    std::vector<QPointer<QWidget>> suspended_windows_;
    std::vector<QPointer<QWindow>> exposed_windows_;
    void suspendWindow(QWidget* window);
    bool windowSuspended(QWidget* window) const;
    bool windowReadyForPresentation(QWidget* window) const;
    void refreshWindowPresentation();
    void suspendRendering();
    void scheduleRenderingResume();
    void displayConfigurationChanged();
#endif
signals:
    void file_ready(const QString& file_name);

private:
    QTimer pointer_idle_timer_;
    bool pointer_hidden_ = false;
    void showPointer();
    void restartPointerTimer();

protected:
    bool eventFilter(QObject* receiver, QEvent* event) override;
    bool event(QEvent* event) override;
};

#endif // OPEN_WITH_APP_H
