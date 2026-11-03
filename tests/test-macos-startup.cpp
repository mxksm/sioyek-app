// Exercise production startup with two restored windows and resize-hook commands.
#define main sioyek_application_main
#include "../pdf_viewer/main.cpp"
#undef main

void installStartupProbe();
void checkStartupPolicy(bool primary);
static QString readyPath;
static bool primary = false;

static void scheduleStartupCheck() {
    if (!primary || !qobject_cast<QApplication*>(QCoreApplication::instance())) return;
    QTimer::singleShot(500, qApp, [] {
        if (windows.empty() || !windows[0]->doc()) {
            scheduleStartupCheck();
            return;
        }
        WindowState state;
        state.tabs.push_back(windows[0]->doc()->get_path());
        MainWidget::create_restored_window(windows[0], state);
        if (windows.size() != 2) {
            qFatal("Expected two windows, got %zu", windows.size());
        }
        checkStartupPolicy(true);
        QFile ready(readyPath);
        if (!ready.open(QIODevice::WriteOnly)) std::abort();
        ready.write(QByteArray::number(QCoreApplication::applicationPid()));
    });
    // Safety exit if the driver fails before sending the quit command.
    QTimer::singleShot(15000, qApp, &QCoreApplication::quit);
}
Q_COREAPP_STARTUP_FUNCTION(scheduleStartupCheck)

int main(int argc, char** argv) {
    if (argc == 2 && QByteArray(argv[1]) == "ipc-delay") {
        QCoreApplication app(argc, argv);
        QString instance = "sy-ipc-" + QString::number(app.applicationPid());
        RunGuard guard(instance, true);
        if (!guard.isPrimary()) return 1;
        bool received = false;
        QObject::connect(&guard, &RunGuard::messageReceived, [&](const QByteArray& data, QLocalSocket*) {
            received = deserialize_string_array(data).contains("fit_to_page_width");
            app.quit();
        });
        QProcess client;
        client.start(app.applicationFilePath(), {"secondary", "fit_to_page_width", "unused", instance});
        QTimer::singleShot(300, &app, [&] { if (!guard.startServer()) app.exit(1); });
        QTimer::singleShot(5000, &app, &QCoreApplication::quit);
        app.exec();
        if (!client.waitForFinished(5000) || client.exitCode() != 0 || !received) return 1;
        std::fprintf(stderr, "PASS: command survives deferred listener startup\n");
        return 0;
    }
    if (argc != 5) return 2;
    QStandardPaths::setTestModeEnabled(true);
    primary = QByteArray(argv[1]) == "primary";
    readyPath = QString::fromLocal8Bit(argv[3]);
    installStartupProbe();
    QList<QByteArray> values{argv[0], "--instance-name", argv[4]};
    if (primary) values.append(argv[2]);
    else values.append({"--nofocus", "--execute-command", argv[2]});
    std::vector<char*> args;
    for (auto& value : values) args.push_back(value.data());
    int count = args.size();
    args.push_back(nullptr);
    int result = sioyek_application_main(count, args.data());
    if (!primary) checkStartupPolicy(false);
    return result;
}
