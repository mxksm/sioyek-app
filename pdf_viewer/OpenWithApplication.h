#ifndef OPEN_WITH_APP_H
#define OPEN_WITH_APP_H

#include <QApplication>
#include <QFileOpenEvent>
#include <QPointer>
#include <QTimer>
#include <QWidget>
#include <vector>

class OpenWithApplication : public QApplication
{
    Q_OBJECT
public:
    OpenWithApplication(int& argc, char** argv);
    ~OpenWithApplication() override;
#ifdef Q_OS_MACOS
    void setDisplaySleeping(bool sleeping);
    bool renderingSuspended() const { return rendering_suspended_; }
    bool notify(QObject* receiver, QEvent* event) override;
private:
    bool rendering_suspended_ = false;
    QTimer resume_rendering_;
    std::vector<QPointer<QWidget>> suspended_windows_;
    void suspendWindow(QWidget* window);
#endif
signals:
    void file_ready(const QString& file_name);

protected:
    bool event(QEvent* event) override;
};

#endif // OPEN_WITH_APP_H
