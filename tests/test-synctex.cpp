// Run against the real widgets and a PDF compiled with SyncTeX by the runner.
#define main sioyek_application_main
#include "../pdf_viewer/main.cpp"
#undef main
#include <QTemporaryDir>
#include <QEventLoop>
#include <QJsonDocument>
#include <QJsonArray>
#include "../pdf_viewer/synctex/synctex_parser.h"
#include <stdexcept>

static int result = 1;
static void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
static void settle(int ms = 100) {
    QEventLoop loop; QTimer::singleShot(ms, &loop, &QEventLoop::quit); loop.exec();
}
static QList<QByteArray> calls() {
    QFile file(qEnvironmentVariable("SIOYEK_TEST_OUTPUT"));
    if (!file.open(QIODevice::ReadOnly)) return {};
    auto lines = file.readAll().split('\n'); lines.removeAll({}); return lines;
}
static void wait_for_call(int count) {
    for (int i = 0; i < 40 && calls().size() < count; ++i) settle();
    settle(150);
    check(calls().size() == count, "one source jump per gesture");
    auto args = QJsonDocument::fromJson(calls().back()).array();
    check(args.size() == 3 && QFileInfo(args[0].toString()).canonicalFilePath() == QFileInfo(qEnvironmentVariable("SIOYEK_TEST_SOURCE")).canonicalFilePath(),
        "source path with spaces/apostrophe reaches the editor unchanged");
    check(args[1].toString().toInt() == 4 && args[2].toString().toInt() >= 0,
        "source line and column retain their numbered placeholders");
}
static void run() {
    try {
        auto owner = windows.front();
        const auto source = qEnvironmentVariable("SIOYEK_TEST_SOURCE");
        const auto pdf = owner->doc()->get_path();
        auto math_page = owner->doc()->get_stext_with_page_number(owner->doc()->num_pages() - 1);
        auto visual_lines = get_source_line_rects(math_page);
        qInfo("Math fixture has %zu visual lines", visual_lines.size());
        check(visual_lines.size() == 9,
            "one hint per visual line for headings, inline/display math and short lines; columns remain separate");
        auto scanner = synctex_scanner_new_with_output_file(utf8_encode(pdf).c_str(), nullptr, 1);
        check(scanner, "fixture has SyncTeX data");
        check(synctex_display_query(scanner, "./notes.tex", 4, 0, 0) > 0, "locate source marker");
        auto node = synctex_scanner_next_result(scanner);
        check(node, "source marker has a PDF box");
        DocumentPos location{synctex_node_page(node) - 1,
            synctex_node_box_visible_h(node) + 10,
            synctex_node_box_visible_v(node) - synctex_node_box_visible_height(node) / 2};
        synctex_scanner_free(scanner);
        QString command = "\"" + qEnvironmentVariable("SIOYEK_TEST_PYTHON") + "\" \"" +
            qEnvironmentVariable("SIOYEK_TEST_CAPTURE") + "\" \"" +
            qEnvironmentVariable("SIOYEK_TEST_OUTPUT") + "\" \"%1\" %2 %3";
        owner->set_inverse_search_command(command.toStdWString());
        owner->resize(1000, 800);
        int count = 0;
        bool tested_multi_letter = false;
        for (auto orientation : {Qt::Horizontal, Qt::Vertical}) {
            owner->toggle_split(orientation); settle();
            for (auto pane : {owner, owner->split_pane_.data()}) {
                pane->do_synctex_forward_search(Path(pdf), Path(source.toStdWString()), 4, 0);
                pane->dv()->set_zoom_level(0.35f, true);
                pane->dv()->set_offset_x(0);
                pane->dv()->set_offset_y(pane->doc()->document_to_absolute_y(location.page, location.y));
                pane->setFocus(); settle();
                auto position = pane->dv()->document_to_window_pos_in_pixels_uncentered(location);
                QPoint point(position.x, position.y);
                check(pane->rect().contains(point), "source marker is visible in each pane");
                if (pane->freehand_drawing_mode == DrawingMode::None) pane->toggle_freehand_drawing_mode();
                int ink = pane->doc()->num_freehand_drawings();
                auto mouse = [&](QEvent::Type type, Qt::MouseButton button, Qt::MouseButtons buttons) {
                    QMouseEvent event(type, point, pane->mapToGlobal(point), button, buttons, Qt::ControlModifier);
                    QApplication::sendEvent(pane, &event);
                };
                mouse(QEvent::MouseButtonPress, Qt::LeftButton, Qt::LeftButton);
                mouse(QEvent::MouseMove, Qt::NoButton, Qt::LeftButton);
                mouse(QEvent::MouseButtonRelease, Qt::LeftButton, Qt::NoButton);
                wait_for_call(++count);
                check(pane->doc()->num_freehand_drawings() == ink && !pane->is_drawing,
                    "Cmd+click does not draw or leave a drawing gesture active");
                auto key = [&](int code, Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
                    QKeyEvent event(QEvent::KeyPress, code, modifiers,
                        code >= Qt::Key_A && code <= Qt::Key_Z ? QString(QChar(code + (modifiers.testFlag(Qt::ShiftModifier) ? 0 : 32))) : QString());
                    QApplication::sendEvent(pane, &event);
                };
                key(Qt::Key_S); key(Qt::Key_D);
                check(pane->pending_command_instance && pane->pending_command_instance->get_name() == "synctex_select_line",
                    "sd waits for a line label instead of using the mouse");
                auto lines = pane->opengl_widget->get_highlight_word_rects();
                check(!lines.empty(), "visible text has line labels");
                tested_multi_letter |= lines.size() > 26;
                int target = -1;
                for (size_t i = 0; i < lines.size(); ++i) {
                    auto rect = lines[i].to_window(pane->dv());
                    check(rect.x0 >= -1 && rect.y0 >= -1 && rect.x1 <= pane->dv()->get_view_width()+1 &&
                        rect.y1 <= pane->dv()->get_view_height()+1, "line labels remain inside the focused pane");
                    if (point.x() >= rect.x0 && point.x() <= rect.x1 && point.y() >= rect.y0 && point.y() <= rect.y1)
                        target = static_cast<int>(i);
                }
                check(target >= 0, "source marker has one line label");
                key(Qt::Key_Z, Qt::ShiftModifier);
                check(calls().size() == count && pane->pending_command_instance, "invalid label does not navigate or jump");
                key(Qt::Key_Escape);
                check(!pane->pending_command_instance && calls().size() == count, "Escape cancels without launching the editor");
                key(Qt::Key_S); key(Qt::Key_D);
                const auto label = get_tags(lines.size())[target];
                for (char c : label) key(Qt::Key_A + c - 'a');
                wait_for_call(++count);
                check(!pane->pending_command_instance, "label selection finishes the command");

            }
        }
        check(tested_multi_letter, "fixture exercises multi-letter line labels");
        // A missing map produces useful feedback, not a null-scanner crash.
        QFile::remove(QString::fromStdWString(pdf).replace(".pdf", ".synctex.gz"));
        auto pos = owner->dv()->document_to_window_pos_in_pixels_uncentered(location);
        check(owner->synctex_under_pos(pos).empty(), "missing SyncTeX does not jump");
        settle();
        auto overlay=owner->findChild<QWidget*>("floatingErrorOverlay");
        bool explained=false;
        if(overlay && overlay->isVisible()) for(auto label:overlay->findChildren<QLabel*>())
            explained |= label->text().contains("-synctex=1");
        check(explained, "missing SyncTeX is explained inside the window");
        qInfo("PASS: Cmd+click and sd in both split orientations, line labels, cancellation, no accidental ink, one callback, source arguments, missing SyncTeX");
        result = 0;
    } catch (const std::exception& error) { qCritical("FAIL: %s", error.what()); }
    QCoreApplication::exit(result);
}
static void schedule() { QTimer::singleShot(1200, qApp, run); }
Q_COREAPP_STARTUP_FUNCTION(schedule)
int main(int argc, char** argv) {
    QStandardPaths::setTestModeEnabled(true);
    QTemporaryDir temp;
    qputenv("XDG_DATA_HOME", temp.path().toUtf8());
    char option[] = "--instance-name", name[] = "sioyek-synctex-test";
    char* args[] = {argv[0], argv[1], option, name};
    sioyek_application_main(4, args); return result;
}
