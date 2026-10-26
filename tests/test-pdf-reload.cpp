// Exercise replacement of the same path with the real document and renderer.
#define main sioyek_application_main
#include "../pdf_viewer/main.cpp"
#undef main
#include <QTemporaryDir>
#include <QPdfWriter>
#include <QEventLoop>
#include <QSaveFile>
#include <QBuffer>
#include <stdexcept>

static QString test_directory;
static int test_result = 1;
static void check(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
static void settle(int ms = 100) {
    QEventLoop loop;
    QTimer::singleShot(ms, &loop, &QEventLoop::quit);
    loop.exec();
}
static QByteArray pdf_bytes(int pages, QColor color, int size = 600) {
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    {
        QPdfWriter writer(&buffer);
        writer.setResolution(72);
        writer.setPageSize(QPageSize(QSizeF(size, size), QPageSize::Point));
        writer.setPageMargins(QMarginsF());
        QPainter painter(&writer);
        for (int i = 0; i < pages; ++i) {
            if (i) writer.newPage();
            painter.fillRect(0, 0, size, size, color);
        }
    }
    return bytes;
}
static void replace_pdf(const QString& path, const QByteArray& bytes, bool atomic) {
    if (atomic) {
        QSaveFile file(path);
        check(file.open(QIODevice::WriteOnly), "open atomic replacement");
        check(file.write(bytes) == bytes.size() && file.commit(), "commit replacement");
    } else {
        QFile file(path);
        check(file.open(QIODevice::WriteOnly | QIODevice::Truncate), "open overwrite");
        check(file.write(bytes) == bytes.size(), "write replacement");
    }
}
static void expect_page(MainWidget* widget, QColor color, int pages, int size = 600) {
    bool matched = false;
    for (int i = 0; i < 50 && !matched; ++i) {
        settle();
        auto image = widget->opengl_widget->grabFramebuffer();
        if (image.isNull()) continue;
        auto pixel = image.pixelColor(image.width() / 2, image.height() / 2);
        int matching_samples = 0;
        for (int y = 10; y < image.height(); y += 20) {
            for (int x = 10; x < image.width(); x += 20) {
                const auto sample = image.pixelColor(x, y);
                if (std::abs(sample.red() - color.red()) < 12 &&
                    std::abs(sample.green() - color.green()) < 12 &&
                    std::abs(sample.blue() - color.blue()) < 12) ++matching_samples;
            }
        }
        if (i == 49) {
            qWarning() << "Expected" << color << pages << size << "got"
                << pixel << widget->doc()->num_pages() << widget->doc()->get_page_width(0)
                << "view" << widget->main_document_view->get_offset_x()
                << widget->main_document_view->get_offset_y()
                << widget->main_document_view->get_zoom_level()
                << widget->main_document_view->get_view_height();
            image.save("/tmp/sioyek-reload-failure.png");
        }
        matched = widget->doc()->num_pages() == pages &&
            std::abs(widget->doc()->get_page_width(0) - size) < 2 &&
            matching_samples > 50;
    }
    check(matched, "replacement must render at the existing zoom and position");
}
static void test_worker_recovery(MainWidget* widget) {
    // This renderer has no UI timer to rescue a failed request. Only its own
    // render_advance signal can request another frame after the initial failure.
    const auto path = test_directory + "/worker.pdf";
    replace_pdf(path, pdf_bytes(1, Qt::red), false);
    bool quit = false;
    PdfRenderer renderer(4, &quit, widget->mupdf_context);
    renderer.start_threads();
    bool rendered = false;
    bool expect_blue = true;
    auto request = [&] {
        widget->opengl_widget->makeCurrent();
        int width = 0, height = 0;
        auto texture = renderer.find_rendered_page(path.toStdWString(), 1, true,
            -1, 1, 1, 1, 1, &width, &height);
        if (texture && width > 0 && height > 0) {
            std::vector<unsigned char> pixels(width * height * 3);
            glBindTexture(GL_TEXTURE_2D, texture);
            glPixelStorei(GL_PACK_ALIGNMENT, 1);
            glGetTexImage(GL_TEXTURE_2D, 0, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
            glPixelStorei(GL_PACK_ALIGNMENT, 4);
            rendered = expect_blue ?
                pixels[0] < 12 && pixels[1] < 12 && pixels[2] > 240 :
                pixels[0] > 240 && pixels[1] < 12 && pixels[2] < 12;
        }
        widget->opengl_widget->doneCurrent();
    };
    auto connection = QObject::connect(&renderer, &PdfRenderer::render_advance, &renderer, request);
    request(); // page 2 does not exist yet
    settle(500);
    replace_pdf(path, pdf_bytes(2, Qt::blue), false);
    for (int i = 0; i < 50 && !rendered; ++i) settle();
    const bool recovered = rendered;
    // Replace again with old work queued/in flight, including an invalidation
    // during a resize. Results from the previous generation must be discarded.
    for (int i = 0; i < 20; ++i) {
        renderer.add_request(path.toStdWString(), i % 2, true, 2 + i * 0.1f, 1, -1, 1, 1);
    }
    sleep_ms(20);
    replace_pdf(path, pdf_bytes(2, Qt::red), true);
    renderer.no_rerender = true;
    renderer.clear_cache();
    renderer.no_rerender = false;
    expect_blue = false;
    rendered = false;
    request();
    for (int i = 0; i < 50 && !rendered; ++i) settle();
    QObject::disconnect(connection);
    quit = true;
    renderer.join_threads();
    widget->opengl_widget->makeCurrent();
    renderer.free_all_resources_for_document(path.toStdWString());
    widget->opengl_widget->doneCurrent();
    check(recovered, "failed render must retry and reopen its handle without a user event");
    check(rendered, "in-flight results and resize must not defeat document invalidation");
    qInfo("PASS: failed worker request recovers using its own repaint signal");
    qInfo("PASS: invalidation during rendering and resize shows the new generation");
}
static void test_reload() {
    try {
        auto widget = windows.front();
        auto view = widget->main_document_view;
        widget->resize(700, 600);
        widget->opengl_widget->set_custom_color_mode(false);
        widget->opengl_widget->set_dark_mode(false);
        view->set_zoom_level(1, true);
        view->goto_page(0);
        widget->invalidate_render();
        const auto path = test_directory + "/generated.pdf";
        expect_page(widget, Qt::red, 3);
        qInfo("PASS: initial fixture rendered");

        // Reopening an already-cached document used to reset its reload timestamp,
        // permanently hiding an overwrite that happened while another PDF was open.
        widget->open_document((test_directory + "/other.pdf").toStdWString());
        settle();
        replace_pdf(path, pdf_bytes(2, Qt::blue), true);
        widget->open_document(path.toStdWString());
        expect_page(widget, Qt::blue, 2);
        qInfo("PASS: reopening a replaced cached document");

        for (int i = 0; i < 8; ++i) {
            auto color = i % 2 ? QColor(Qt::blue) : QColor(Qt::red);
            replace_pdf(path, pdf_bytes(2, color), i % 2);
            expect_page(widget, color, 2);
        }
        qInfo("PASS: repeated in-place writes and atomic replacements without zooming");

        // A compiler may truncate the output before writing the finished PDF.
        replace_pdf(path, QByteArray("%PDF-1.7\n"), false);
        settle(600);
        widget->reload(false);
        check(widget->doc()->num_pages() == 2, "failed reload preserves the current document");
        replace_pdf(path, pdf_bytes(2, Qt::green), false);
        expect_page(widget, Qt::green, 2);
        qInfo("PASS: recovery after incomplete output");

        view->goto_page(1);
        widget->invalidate_render();
        settle();
        replace_pdf(path, pdf_bytes(1, Qt::red, 400), true);
        expect_page(widget, Qt::red, 1, 400);
        qInfo("PASS: smaller page count and dimensions keep a visible page");

        widget->toggle_split();
        settle();
        auto right = widget->split_pane_.data();
        check(right != nullptr, "split created");
        right->open_document(path.toStdWString());
        right->opengl_widget->set_custom_color_mode(false);
        right->opengl_widget->set_dark_mode(false);
        replace_pdf(path, pdf_bytes(2, Qt::blue), true);
        expect_page(widget, Qt::blue, 2);
        expect_page(right, Qt::blue, 2);
        qInfo("PASS: replacement refreshes both split panes");
        widget->toggle_split();
        settle();
        test_worker_recovery(widget);
        test_result = 0;
        QCoreApplication::exit(0);
    } catch (const std::exception& error) {
        qCritical("FAIL: %s", error.what());
        QCoreApplication::exit(1);
    }
}
static void schedule_reload_test() { QTimer::singleShot(1200, qApp, test_reload); }
Q_COREAPP_STARTUP_FUNCTION(schedule_reload_test)
int main(int argc, char** argv) {
    QStandardPaths::setTestModeEnabled(true);
    QTemporaryDir directory;
    test_directory = directory.path();
    replace_pdf(test_directory + "/generated.pdf", pdf_bytes(3, Qt::red), false);
    replace_pdf(test_directory + "/other.pdf", pdf_bytes(1, Qt::green), false);
    QByteArray first = (test_directory + "/generated.pdf").toUtf8();
    char instance[] = "--instance-name";
    char name[] = "sioyek-pdf-reload-regression";
    char* args[] = {argv[0], first.data(), instance, name};
    sioyek_application_main(4, args);
    return test_result;
}
