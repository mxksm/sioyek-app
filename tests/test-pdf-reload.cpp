// Exercise replacement of the same path with the real document and renderer.
#define main sioyek_application_main
#include "../pdf_viewer/main.cpp"
#undef main
#include <QTemporaryDir>
#include <QPdfWriter>
#include <QEventLoop>
#include <QSaveFile>
#include <QBuffer>
#include <QElapsedTimer>
#include <stdexcept>
#include <malloc/malloc.h>

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
// TeX commonly writes fractional point dimensions; QPdfWriter's fixtures above
// use whole points and would hide precision loss during reload.
static QByteArray fractional_pdf_bytes(bool mixed_sizes, bool outline = false) {
    QByteArray bytes("%PDF-1.4\n");
    std::vector<int> offsets = {0};
    auto object = [&](const QByteArray& body) {
        offsets.push_back(bytes.size());
        bytes += QByteArray::number(int(offsets.size()) - 1) + " 0 obj\n" + body + "\nendobj\n";
    };
    object(outline ? "<< /Type /Catalog /Pages 2 0 R /Outlines 8 0 R >>" : "<< /Type /Catalog /Pages 2 0 R >>");
    object("<< /Type /Pages /Count 4 /Kids [3 0 R 4 0 R 5 0 R 6 0 R] >>");
    for (int i = 0; i < 4; ++i) {
        const QByteArray size = mixed_sizes && i % 2 ? "612.5 792.25" : "595.2756 841.8898";
        object("<< /Type /Page /Parent 2 0 R /MediaBox [0 0 " + size + "] /Resources << >> /Contents 7 0 R >>");
    }
    const QByteArray drawing("0.2 0.4 0.8 rg 50 50 400 600 re f\n");
    object("<< /Length " + QByteArray::number(drawing.size()) + " >>\nstream\n" + drawing + "endstream");
    if (outline) {
        object("<< /Type /Outlines /First 9 0 R /Last 9 0 R /Count 1 >>");
        object("<< /Title (Chapter) /Parent 8 0 R /Dest [3 0 R /Fit] >>");
    }
    const int xref = bytes.size();
    bytes += "xref\n0 " + QByteArray::number(int(offsets.size())) + "\n0000000000 65535 f \n";
    for (size_t i = 1; i < offsets.size(); ++i)
        bytes += QByteArray::number(offsets[i]).rightJustified(10, '0') + " 00000 n \n";
    bytes += "trailer\n<< /Size " + QByteArray::number(int(offsets.size())) + " /Root 1 0 R >>\nstartxref\n"
        + QByteArray::number(xref) + "\n%%EOF\n";
    return bytes;
}
static void test_reload_position(MainWidget* widget, const QString& path) {
    auto view = widget->main_document_view;
    for (bool mixed : {false, true}) {
        const auto bytes = fractional_pdf_bytes(mixed);
        replace_pdf(path, bytes, true);
        widget->reload(false);
        settle(300);
        view->set_zoom_level(1.37f, true);
        view->goto_offset_within_page(2, 320.375f);
        const auto before = view->get_offsets();
        auto right = widget->split_pane_.data();
        AbsoluteDocumentPos right_before;
        if (right) {
            right->main_document_view->set_zoom_level(1.61f, true);
            right->main_document_view->goto_offset_within_page(1, 400.125f);
            right_before = right->main_document_view->get_offsets();
        }
        for (int i = 0; i < 12; ++i) {
            QElapsedTimer elapsed;
            elapsed.start();
            replace_pdf(path, bytes, i % 2);
            if (i % 3 == 0) {
                // Exercise the save/compile watcher too, not only manual reload.
                bool reloaded = false;
                for (int j = 0; j < 60 && !reloaded; ++j) {
                    settle(50);
                    reloaded = widget->doc()->get_milies_since_last_document_update_time() < elapsed.elapsed();
                }
                check(reloaded, "file watcher must reload overwritten PDF");
            } else {
                widget->reload(false);
            }
            settle(100);
            const auto after = view->get_offsets();
            if (std::abs(after.y - before.y) > 0.001f)
                qWarning() << "Reload drift, mixed sizes:" << mixed << "iteration:" << i
                    << "before:" << before.y << "after:" << after.y;
            check(std::abs(after.y - before.y) < 0.001f && std::abs(after.x - before.x) < 0.001f,
                "unchanged fractional PDF must not drift after reload");
            check(view->get_zoom_level() == 1.37f, "reload preserves zoom");
            if (right) {
                auto other = right->main_document_view->get_offsets();
                check(std::abs(other.y - right_before.y) < 0.001f && std::abs(other.x - right_before.x) < 0.001f,
                    "reload preserves the other pane's independent position");
                check(right->main_document_view->get_zoom_level() == 1.61f, "reload preserves the other pane's zoom");
            }
        }
    }
    qInfo("PASS: repeated fractional and mixed-size PDF reloads preserve exact view position");
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
static void test_texture_cleanup(MainWidget* widget) {
    const auto path = test_directory + "/texture-cleanup.pdf";
    replace_pdf(path, pdf_bytes(1, Qt::green), false);
    bool quit = false;
    PdfRenderer renderer(1, &quit, widget->mupdf_context);
    renderer.start_threads();
    GLuint texture = 0;
    for (int i = 0; i < 50 && !texture; ++i) {
        widget->opengl_widget->makeCurrent();
        texture = renderer.find_rendered_page(path.toStdWString(), 0, true, -1, 1, 1, 2, 1, nullptr, nullptr);
        widget->opengl_widget->doneCurrent();
        if (!texture) settle(20);
    }
    check(texture != 0, "cleanup fixture uploaded");
    // The housekeeping timer runs outside paintGL, with no context current.
    if (auto context = QOpenGLContext::currentContext()) context->doneCurrent();
    renderer.delete_old_pages(true);
    check(!QOpenGLContext::currentContext(), "cleanup restores the caller's context state");
    widget->opengl_widget->makeCurrent();
    const bool deleted = !glIsTexture(texture);
    widget->opengl_widget->doneCurrent();
    quit = true; renderer.join_threads();
    renderer.free_all_resources_for_document(path.toStdWString());
    check(deleted, "cache eviction actually releases GPU textures without a current context");
    qInfo("PASS: evicted GPU textures are released outside paintGL");
}
// Count actual MuPDF allocations, including cloned worker contexts and page
// handles. This catches retention even when macOS keeps freed malloc arenas.
struct TrackedMuPDF {
    std::atomic<long long> bytes{0};
    std::mutex locks[FZ_LOCK_MAX];
    static void* allocate(void* user, size_t size) {
        void* ptr = std::malloc(size);
        if (ptr) static_cast<TrackedMuPDF*>(user)->bytes += malloc_size(ptr);
        return ptr;
    }
    static void release(void* user, void* ptr) {
        if (ptr) static_cast<TrackedMuPDF*>(user)->bytes -= malloc_size(ptr);
        std::free(ptr);
    }
    static void* resize(void* user, void* ptr, size_t size) {
        if (!ptr) return allocate(user, size);
        if (!size) { release(user, ptr); return nullptr; }
        const auto before = malloc_size(ptr);
        void* next = std::realloc(ptr, size);
        if (next) static_cast<TrackedMuPDF*>(user)->bytes += static_cast<long long>(malloc_size(next)) - before;
        return next;
    }
};
static void test_search_resources(MainWidget* widget) {
    const auto path = test_directory + "/search-memory.pdf";
    {
        QPdfWriter writer(path); writer.setResolution(72);
        QPainter painter(&writer);
        for (int i=0; i<10; ++i) {
            if (i) writer.newPage();
            painter.drawText(40, 80, "needle");
        }
    }
    TrackedMuPDF tracking;
    fz_alloc_context alloc{&tracking, TrackedMuPDF::allocate, TrackedMuPDF::resize, TrackedMuPDF::release};
    fz_locks_context locks{&tracking,
        [](void* user, int index) { static_cast<TrackedMuPDF*>(user)->locks[index].lock(); },
        [](void* user, int index) { static_cast<TrackedMuPDF*>(user)->locks[index].unlock(); }};
    auto context = fz_new_context(&alloc, &locks, FZ_STORE_DEFAULT);
    check(context, "tracked context created"); fz_register_document_handlers(context);
    bool quit = false, all_matched = true;
    std::vector<SearchResult> results; std::mutex results_mutex;
    float progress = 0; bool searching = false;
    {
        PdfRenderer renderer(1, &quit, context); renderer.start_threads();
        for (int run=0; run<8; ++run) {
            renderer.add_request(path.toStdWString(),0,L"needle",false,&results,&progress,&searching,&results_mutex);
            QElapsedTimer timer; timer.start();
            while (renderer.is_search_busy() && timer.elapsed()<5000) settle(5);
            if (renderer.is_search_busy()) { all_matched=false; break; }
            std::lock_guard<std::mutex> lock(results_mutex);
            all_matched = all_matched && results.size()==10;
        }
        // With workers otherwise idle, upload a fresh image and ensure its
        // CPU backing buffer is reclaimed without submitting another render.
        bool ready=false;
        auto connection=QObject::connect(&renderer,&PdfRenderer::render_advance,&renderer,[&] { ready=true; });
        renderer.add_request(path.toStdWString(),0,true,2,1,-1,1,1);
        QElapsedTimer render_timer; render_timer.start();
        while (!ready && render_timer.elapsed()<5000) settle(5);
        const auto before_upload=tracking.bytes.load();
        int width=0,height=0;
        widget->opengl_widget->makeCurrent();
        const auto texture=renderer.find_rendered_page(path.toStdWString(),0,true,-1,1,1,2,1,&width,&height);
        widget->opengl_widget->doneCurrent();
        QElapsedTimer cleanup_timer; cleanup_timer.start();
        const auto pixel_bytes=static_cast<long long>(width)*height*3;
        while (tracking.bytes.load()>before_upload-pixel_bytes && cleanup_timer.elapsed()<1000) settle(1);
        const bool reclaimed=texture && pixel_bytes>0 && tracking.bytes.load()<=before_upload-pixel_bytes;
        qInfo("Idle worker reclaimed %lld pixel bytes in %lld ms", pixel_bytes, cleanup_timer.elapsed());
        QObject::disconnect(connection);
        quit=true; renderer.join_threads();
        check(reclaimed,"idle render worker wakes to release the uploaded pixmap");
    }
    fz_drop_context(context);
    qInfo("MuPDF allocations retained after repeated searches and renderer destruction: %lld bytes", tracking.bytes.load());
    check(all_matched,"event-driven searches return every match on repeated requests");
    check(tracking.bytes==0,"search pages, document handles and cloned contexts are all released");
    qInfo("PASS: repeated searches release all MuPDF allocations");
}
static void test_word_and_outline_boundaries(MainWidget* widget) {
    bool words_ok = true;
    for (const std::wstring text : {L"", L"x", L"alpha", L"alpha beta gamma"}) {
        std::vector<fz_stext_char> chars(text.size());
        std::vector<fz_stext_char*> flat;
        for (size_t i = 0; i < text.size(); ++i) {
            chars[i].c = text[i];
            chars[i].quad = fz_quad_from_rect(fz_make_rect(i * 10, 0, (i + 1) * 10, 12));
            flat.push_back(&chars[i]);
        }
        std::vector<PagelessDocumentRect> bounds;
        std::vector<std::vector<PagelessDocumentRect>> character_bounds, reading_bounds;
        std::vector<std::wstring> words;
        get_flat_words_from_flat_chars(flat, bounds, &character_bounds);
        get_word_rect_list_from_flat_chars(flat, words, reading_bounds);
        const size_t expected = text.empty() ? 0 : text == L"alpha beta gamma" ? 3 : 1;
        std::wstring reconstructed;
        for (const auto& word : words) reconstructed += word;
        words_ok &= bounds.size() == expected && character_bounds.size() == expected
            && reading_bounds.size() == expected && reconstructed == text;
    }
    qInfo("Word boundary regression: %s", words_ok ? "PASS" : "FAIL");
    const auto path = test_directory + "/outline-reload.pdf";
    replace_pdf(path, fractional_pdf_bytes(false, true), true);
    bool invalid = false;
    auto document = widget->document_manager->get_document(path.toStdWString());
    check(document->open(&invalid, true), "outline fixture opens");
    bool outlines_ok = document->get_flat_toc_names().size() == 1;
    for (int i = 0; i < 3; ++i) {
        check(document->reload(), "outline fixture reloads");
        outlines_ok &= document->get_flat_toc_names().size() == 1
            && document->get_flat_toc_pages().size() == 1;
    }
    replace_pdf(path, fractional_pdf_bytes(false), true);
    check(document->reload(), "outline-free replacement reloads");
    outlines_ok &= document->get_flat_toc_names().empty() && document->get_flat_toc_pages().empty();
    widget->document_manager->free_document(document);
    qInfo("Outline reload regression: %s", outlines_ok ? "PASS" : "FAIL");
    check(words_ok && outlines_ok, "word boundaries and outline reload preserve correct contents");
}
static void test_document_cache_cleanup(MainWidget* widget) {
    const auto path = test_directory + "/word-cache.pdf";
    auto write_words = [&](const QString& text) {
        QByteArray bytes; QBuffer buffer(&bytes); buffer.open(QIODevice::WriteOnly);
        {
            QPdfWriter writer(&buffer); writer.setResolution(72);
            QPainter painter(&writer); painter.drawText(40,80,text);
        }
        replace_pdf(path, bytes, true);
    };
    write_words("alpha beta gamma");
    bool invalid = false;
    QPointer<QStandardItemModel> toc_model;
    {
        auto document_ptr = widget->document_manager->get_document(path.toStdWString());
        auto& document = *document_ptr;
        check(document.open(&invalid,true), "word-cache fixture opened");
        check(!document.get_page_flat_words(0).empty(), "word geometry initially cached");
        check(!document.get_page_flat_word_chars(0).empty(), "character rectangles initially cached");
        write_words("");
        check(document.reload(), "word-cache fixture reloaded");
        check(document.get_page_flat_word_chars(0).empty(), "reload discards stale character geometry");
        check(document.get_page_flat_words(0).empty() && document.get_page_flat_word_chars(0).empty(),
            "replacement rebuilds word and character geometry from new content");
        toc_model=document.get_toc_model();
        check(toc_model,"TOC model created");
        widget->document_manager->free_document(document_ptr);
    }
    check(toc_model.isNull(),"document destruction releases its TOC model");
    qInfo("PASS: reload discards word geometry and document destruction releases the TOC model");
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

        test_reload_position(widget, path);
        replace_pdf(path, pdf_bytes(2, Qt::blue), true);
        widget->reload(false);
        view->set_zoom_level(1, true);
        view->goto_page(0);
        expect_page(widget, Qt::blue, 2);

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
        test_reload_position(widget, path);
        widget->toggle_split();
        settle();
        test_worker_recovery(widget);
        test_texture_cleanup(widget);
        test_search_resources(widget);
        test_document_cache_cleanup(widget);
        test_word_and_outline_boundaries(widget);
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
