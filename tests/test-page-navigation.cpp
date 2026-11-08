// Exercise the real commands and DocumentView against isolated PDF fixtures.
#define main sioyek_application_main
#include "../pdf_viewer/main.cpp"
#undef main
#include <QTemporaryDir>
#include <QPdfWriter>
#include <QEventLoop>
#include <QElapsedTimer>
#include <stdexcept>

static QString test_directory;
static int test_result = 1;
static void check(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
static void settle() {
    QEventLoop loop;
    QTimer::singleShot(120, &loop, &QEventLoop::quit);
    loop.exec();
}
static void same_position(DocumentView* view, float expected, const char* message) {
    check(std::abs(view->get_offset_y() - expected) < 0.01f, message);
}
// Keep these checks independent of PDF complexity: a tiny render measures queue
// wake-up, and suspending maintenance verifies that ready frames drive repaint.
static void test_render_latency(MainWidget* widget) {
    bool quit = false;
    PdfRenderer renderer(1, &quit, widget->mupdf_context);
    renderer.start_threads();
    std::vector<qint64> timings;
    bool completed = false;
    auto connection = QObject::connect(&renderer, &PdfRenderer::render_advance,
        &renderer, [&]() { completed = true; });
    for (int i = 0; i < 12; ++i) {
        QEventLoop idle;
        QTimer::singleShot(113 + i * 7, &idle, &QEventLoop::quit);
        idle.exec();
        completed = false;
        QElapsedTimer elapsed;
        elapsed.start();
        renderer.add_request((test_directory + "/pages.pdf").toStdWString(),
            i % 3, true, 0.1f + i * 0.01f, 1, -1, 1, 1);
        while (!completed && elapsed.elapsed() < 1000) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
        }
        timings.push_back(elapsed.elapsed());
    }
    QObject::disconnect(connection);
    quit = true;
    renderer.join_threads();
    renderer.free_all_resources_for_document((test_directory + "/pages.pdf").toStdWString());
    std::sort(timings.begin(), timings.end());
    qInfo("Render queue latency: median %lld ms, worst %lld ms", timings[6], timings.back());

    widget->validation_interval_timer->stop();
    widget->validate_render();
    bool painted = false;
    auto frame = QObject::connect(widget->opengl_widget, &QOpenGLWidget::frameSwapped,
        widget, [&]() { if (!widget->is_render_invalidated) painted = true; });
    QElapsedTimer elapsed;
    elapsed.start();
    // A burst of ready slices must produce a queued repaint without the 200ms
    // maintenance timer or another key/mouse event rescuing it.
    for (int i = 0; i < 20; ++i) widget->invalidate_render();
    while (!painted && elapsed.elapsed() < 150) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
    }
    qInfo("Repaint without maintenance timer: %s (%lld ms)", painted ? "ready" : "stalled", elapsed.elapsed());
    QObject::disconnect(frame);
    widget->validation_interval_timer->start();
    if (!qEnvironmentVariableIsSet("SIOYEK_LATENCY_BASELINE")) {
        check(timings[6] < 40, "idle render worker must wake immediately for a new page");
        check(painted, "invalidated frame must repaint without waiting for maintenance");
    }
}
static void test_navigation() {
    try {
        auto widget = windows.front();
        auto view = widget->main_document_view;
        widget->resize(900, 700);
        settle();
        const int last = view->get_document()->num_pages() - 1;
        check(last == 2, "three-page fixture loaded");
        test_render_latency(widget);
        for (float zoom : {1.0f, 2.0f}) {
            view->set_zoom_level(zoom, true);
            view->goto_page(0);
            const float first_position = view->get_offset_y();
            widget->execute_macro_if_enabled(L"previous_page");
            same_position(view, first_position, "previous at first page is a no-op");
            widget->execute_macro_if_enabled(L"next_page");
            widget->execute_macro_if_enabled(L"previous_page");
            same_position(view, first_position, "next/previous keeps first-page alignment");

            view->goto_page(last);
            const float last_position = view->get_offset_y();
            for (int i = 0; i < 4; ++i) widget->execute_macro_if_enabled(L"next_page");
            same_position(view, last_position, "repeated next at last page is a no-op");
            widget->execute_macro_if_enabled(L"previous_page");
            widget->execute_macro_if_enabled(L"next_page");
            same_position(view, last_position, "previous/next keeps last-page alignment");

            widget->execute_macro_if_enabled(L"goto_beginning");
            same_position(view, first_position, "gg aligns like a normal first-page jump");
            widget->execute_macro_if_enabled(L"goto_end");
            same_position(view, last_position, "G aligns like a normal last-page jump");

            view->goto_page(1);
            view->move_pages(100);
            same_position(view, last_position, "large forward count stops on the last page");
            view->move_pages(-100);
            same_position(view, first_position, "large backward count stops on the first page");

            // Reaching an edge must not reset a manually scrolled position.
            view->goto_page(last);
            view->set_offset_y(view->get_offset_y() + 75);
            const float scrolled = view->get_offset_y();
            widget->execute_macro_if_enabled(L"next_page");
            same_position(view, scrolled, "last-page no-op preserves manual scrolling");
        }
        view->set_zoom_level(1.0f, true);
        widget->execute_macro_if_enabled(L"toggle_two_page_mode");
        settle();
        view->goto_page(0);
        const float first_spread = view->get_offset_y();
        widget->execute_macro_if_enabled(L"previous_page");
        same_position(view, first_spread, "previous at first spread is a no-op");
        view->goto_page(last);
        const float last_spread = view->get_offset_y();
        widget->execute_macro_if_enabled(L"next_page");
        same_position(view, last_spread, "next at last spread is a no-op");
        widget->execute_macro_if_enabled(L"toggle_two_page_mode");
        settle();

        // A fresh report retains auto-fit until its first window resize, even
        // after keyboard page navigation. Enlarging it must keep the table in view.
        widget->resize(550, 400); settle();
        widget->open_document((test_directory + "/resize.pdf").toStdWString()); settle();
        check(view->get_is_auto_resize_mode(), "fresh report starts in automatic fit mode");
        for (int i = 0; i < 4; ++i) widget->execute_macro_if_enabled(L"next_page");
        const int report_page = view->get_center_page_number();
        const float report_offset = view->get_offset_y();
        check(report_page >= 3 && view->get_is_auto_resize_mode(), "keyboard navigation retains fresh-report auto fit");
        widget->resize(1100, 800); settle();
        check(view->get_center_page_number() == report_page, "first enlargement preserves report page");
        same_position(view, report_offset, "first enlargement preserves table position");
        widget->resize(550, 400); settle();
        same_position(view, report_offset, "shrinking the report preserves its position");
        widget->resize(1100, 800); settle();
        same_position(view, report_offset, "subsequent enlargement preserves its position");

        widget->open_document((test_directory + "/single.pdf").toStdWString());
        settle();
        view->goto_page(0);
        const float only = view->get_offset_y();
        widget->execute_macro_if_enabled(L"next_page");
        widget->execute_macro_if_enabled(L"previous_page");
        widget->execute_macro_if_enabled(L"goto_beginning");
        widget->execute_macro_if_enabled(L"goto_end");
        same_position(view, only, "all four commands agree on a single-page PDF");
        qInfo("PASS: first/last page boundaries, return steps, counts, zoom, gg/G and single-page PDFs");
        test_result = 0;
        QCoreApplication::exit(0);
    } catch (const std::exception& error) {
        qCritical("FAIL: %s", error.what());
        QCoreApplication::exit(1);
    }
}
static void schedule_navigation_test() { QTimer::singleShot(1200, qApp, test_navigation); }
Q_COREAPP_STARTUP_FUNCTION(schedule_navigation_test)
int main(int argc, char** argv) {
    QStandardPaths::setTestModeEnabled(true);
    QTemporaryDir directory;
    test_directory = directory.path();
    for (int count : {1, 3, 8}) {
        QPdfWriter writer(test_directory + (count == 1 ? "/single.pdf" : count == 3 ? "/pages.pdf" : "/resize.pdf"));
        writer.setResolution(72);
        QPainter painter(&writer);
        for (int page = 0; page < count; ++page) {
            if (page) writer.newPage();
            painter.drawRect(20, 20, 100, 100);
        }
    }
    QByteArray first = (test_directory + "/pages.pdf").toUtf8();
    char instance[] = "--instance-name";
    char name[] = "sioyek-page-navigation-regression";
    char* args[] = {argv[0], first.data(), instance, name};
    sioyek_application_main(4, args);
    return test_result;
}
