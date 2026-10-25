// Exercise the real commands and DocumentView against isolated PDF fixtures.
#define main sioyek_application_main
#include "../pdf_viewer/main.cpp"
#undef main
#include <QTemporaryDir>
#include <QPdfWriter>
#include <QEventLoop>
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
static void test_navigation() {
    try {
        auto widget = windows.front();
        auto view = widget->main_document_view;
        widget->resize(900, 700);
        settle();
        const int last = view->get_document()->num_pages() - 1;
        check(last == 2, "three-page fixture loaded");
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
    for (int count : {1, 3}) {
        QPdfWriter writer(test_directory + (count == 1 ? "/single.pdf" : "/pages.pdf"));
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
