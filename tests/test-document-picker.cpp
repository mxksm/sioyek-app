#define main sioyek_application_main
#include "../pdf_viewer/main.cpp"
#undef main
#include <QTemporaryDir>
#include <QPdfWriter>
#include <QEventLoop>
#include <QElapsedTimer>
#include <QScrollBar>
#include <stdexcept>
#include <array>

extern bool FUZZY_SEARCHING;
extern float UI_BACKGROUND_COLOR[3], UI_TEXT_COLOR[3], UI_SELECTED_BACKGROUND_COLOR[3], UI_SELECTED_TEXT_COLOR[3];
static QString folder;
static int result = 1;
static void check(bool yes, const char* message) { if (!yes) throw std::runtime_error(message); }
static void settle(int ms = 100) { QEventLoop loop; QTimer::singleShot(ms, &loop, &QEventLoop::quit); loop.exec(); }
static void key(int code, Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
    auto target = QApplication::focusWidget(); check(target, "keyboard focus exists");
    QKeyEvent press(QEvent::KeyPress, code, modifiers); QApplication::sendEvent(target, &press);
    QKeyEvent release(QEvent::KeyRelease, code, modifiers);
    if (QApplication::focusWidget()) QApplication::sendEvent(QApplication::focusWidget(), &release);
    settle();
}
static void pdf(const QString& name, const QString& metadata, const QString& heading) {
    // A small, explicit PDF avoids platform PDF-writer metadata differences.
    QByteArray bytes("%PDF-1.4\n");
    std::vector<int> offsets{0};
    auto object = [&](QByteArray body) {
        offsets.push_back(bytes.size());
        bytes += QByteArray::number(int(offsets.size())-1) + " 0 obj\n" + body + "\nendobj\n";
    };
    object("<< /Type /Catalog /Pages 2 0 R >>");
    object("<< /Type /Pages /Kids [3 0 R] /Count 1 >>");
    object("<< /Type /Page /Parent 2 0 R /MediaBox [0 0 600 800] /Resources << /Font << /F1 4 0 R >> >> /Contents 5 0 R >>");
    object("<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>");
    QByteArray drawing = "BT /F1 22 Tf 30 730 Td (" + heading.toUtf8() + ") Tj ET\n";
    object("<< /Length " + QByteArray::number(drawing.size()) + " >>\nstream\n" + drawing + "endstream");
    QByteArray title("feff");
    for (QChar ch : metadata) title += QByteArray::number(ch.unicode(),16).rightJustified(4,'0');
    object("<< /Title <" + title + "> >>");
    const int xref = bytes.size();
    bytes += "xref\n0 7\n0000000000 65535 f \n";
    for (size_t i = 1; i < offsets.size(); ++i) bytes += QByteArray::number(offsets[i]).rightJustified(10,'0') + " 00000 n \n";
    bytes += "trailer\n<< /Size 7 /Root 1 0 R /Info 6 0 R >>\nstartxref\n" + QByteArray::number(xref) + "\n%%EOF\n";
    QFile file(name); check(file.open(QIODevice::WriteOnly), "write PDF fixture");
    check(file.write(bytes) == bytes.size(), "complete PDF fixture");

}
static DocumentPicker* picker(MainWidget* pane, bool filesystem) {
    qInfo() << "PICKER" << filesystem << "right" << bool(pane->split_owner_);
    pane->setFocus();
    pane->execute_macro_if_enabled(filesystem ? L"open_document_embedded_from_current_path" : L"open_prev_doc");
    settle();
    check(!pane->current_widget_stack.empty(), "picker opens");
    auto widget = dynamic_cast<DocumentPicker*>(pane->current_widget_stack.back());
    check(widget && widget->isVisible(), "floating document picker is visible");
    if (!widget->findChild<QLineEdit*>()->hasFocus()) qInfo() << "FOCUS" << QApplication::focusWidget() << "active" << QApplication::activeWindow() << "pane" << pane;
    check(widget->findChild<QLineEdit*>()->hasFocus(), "search has focus");
    const auto host = pane->tab_owner();
    check(widget->parentWidget() == host && host->rect().contains(widget->geometry()) &&
        (widget->geometry().center() - host->rect().center()).manhattanLength() <= 2, "picker centered over whole window");
    return widget;
}
static void search(DocumentPicker* widget, const QString& query, const QString& expected) {
    widget->findChild<QLineEdit*>()->setText(query);
    QElapsedTimer clock; clock.start();
    while (clock.elapsed() < 5000) {
        if (QString::fromStdWString(widget->get_selected_text()) == expected) return;
        settle(30);
    }
    qWarning() << "query" << query << "got" << QString::fromStdWString(widget->get_selected_text()) << "expected" << expected;
    check(false, "title/filename search ranks the expected file first");
}
static void check_navigation(DocumentPicker* widget, MainWidget* pane) {
    auto view = widget->get_view();
    const float x = pane->main_document_view->get_offset_x();
    const float y = pane->main_document_view->get_offset_y();
    const auto owner = pane->tab_owner();
    const float owner_x = owner->main_document_view->get_offset_x();
    const float owner_y = owner->main_document_view->get_offset_y();
    for (const auto code : {Qt::Key_Up, Qt::Key_Down, Qt::Key_PageUp, Qt::Key_PageDown}) {
        const int row = (code == Qt::Key_Up || code == Qt::Key_PageUp) ? 0 : view->model()->rowCount() - 1;
        view->setCurrentIndex(view->model()->index(row, 0));
        for (int i = 0; i < 3; ++i) key(code);
        check(pane->main_document_view->get_offset_x() == x &&
            pane->main_document_view->get_offset_y() == y &&
            owner->main_document_view->get_offset_x() == owner_x &&
            owner->main_document_view->get_offset_y() == owner_y, "list boundary keys never scroll either PDF");
    }
}
static void run() {
    try {
        FUZZY_SEARCHING = true;
        auto pane = windows.front();
        const QString a = folder + "/2401.00001.pdf", b = folder + "/download.pdf", c = folder + "/random.pdf";
        QDir().mkpath(folder + "/subfolder");
        pdf(a, "Quantum Entanglement and Information", "Entangled particles");
        pdf(b, "", "Stochastic Differential Equations");
        pdf(c, "Über die Wahrscheinlichkeit", "A probability study");
        pdf(folder + "/subfolder/nested.pdf", "Nested Algebra", "Nested Algebra");
        for (const auto& path : {a,b,c}) { pane->open_document(path.toStdWString()); settle(200); }
        auto recent = picker(pane, false);
        check_navigation(recent, pane);
        search(recent, "QUANTUM information", a);
        search(recent, "information quantum", a);
        search(recent, "quntum entanglement", a);
        search(recent, "stochastic differential", b);
        search(recent, "2401.00001", a);
        recent->findChild<QLineEdit*>()->setText("zzzzzzzzzzzzzzzz"); settle();
        check(recent->get_view()->model()->rowCount() == 0, "no matches is an empty list");
        check_navigation(recent, pane);
        key(Qt::Key_Return); check(recent->isVisible(), "Enter with no matches is harmless");
        recent->findChild<QLineEdit*>()->setText("123456789"); key(Qt::Key_Return);
        check(recent->isVisible(), "numeric no-match query does not select an invalid row");
        search(recent, "quantum", a);
        recent->findChild<QLineEdit*>()->selectAll();
        check_navigation(recent, pane);
        if (qEnvironmentVariableIsSet("SIOYEK_PICKER_SCREENSHOT")) recent->grab().save(qEnvironmentVariable("SIOYEK_PICKER_SCREENSHOT"));
        const std::array<float*,4> colors{UI_BACKGROUND_COLOR, UI_TEXT_COLOR, UI_SELECTED_BACKGROUND_COLOR, UI_SELECTED_TEXT_COLOR};
        std::array<std::array<float,3>,4> saved;
        const float light[4][3] = {{0.94f,0.94f,0.92f}, {0.12f,0.12f,0.12f}, {0.25f,0.45f,0.65f}, {1,1,1}};
        for (int i=0;i<4;++i) for (int j=0;j<3;++j) { saved[i][j]=colors[i][j]; colors[i][j]=light[i][j]; }
        ConfigFileChangeListener::notify_config_file_changed(pane->config_manager); settle();
        check(recent->findChild<QLineEdit*>()->text() == "quantum", "live theme change preserves search");
        const auto image = recent->grab().toImage();
        check(image.pixelColor(0,0) == QColor(get_color_qml_string(light[0][0],light[0][1],light[0][2])), "floating background follows the light theme");
        const auto row = recent->get_view()->visualRect(recent->get_view()->currentIndex());
        const auto spot = recent->get_view()->viewport()->mapTo(recent,QPoint(row.left()+3,row.center().y()));
        const auto scale = image.devicePixelRatio();
        check(image.pixelColor(int(spot.x()*scale),int(spot.y()*scale)) == QColor(get_color_qml_string(light[2][0],light[2][1],light[2][2])), "selection follows the light theme");
        if (qEnvironmentVariableIsSet("SIOYEK_PICKER_SCREENSHOT")) image.save(qEnvironmentVariable("SIOYEK_PICKER_SCREENSHOT") + ".light.png");
        for (int i=0;i<4;++i) for (int j=0;j<3;++j) colors[i][j]=saved[i][j];
        ConfigFileChangeListener::notify_config_file_changed(pane->config_manager);
        key(Qt::Key_Return); check(pane->doc()->get_path() == a.toStdWString(), "Enter opens searched recent document");
        pane->pop_all_widgets();
        qInfo("PASS: recent search finds metadata and first-page titles, reordered words, typos and numeric filenames");

        auto files = picker(pane, true);
        check_navigation(files, pane);
        search(files, "uber wahrscheinlichkeit", c);
        search(files, "stochastic", b);
        auto edit = files->findChild<QLineEdit*>();
        edit->setText("subfolder/"); settle(); search(files, "nested algebra", folder + "/subfolder/nested.pdf");
        edit->clear(); key(Qt::Key_Backspace); search(files, "quantum", a);
        edit->setText(folder + "/subfolder/nested.pdf"); key(Qt::Key_Return);
        check(pane->doc()->get_path() == (folder + "/subfolder/nested.pdf").toStdWString(), "explicit file path opens");
        pane->pop_all_widgets();
        pane->open_document(a.toStdWString()); settle();
        // A second picker must invalidate a title cached before a rewrite.
        pdf(b, "Revised Geometry and Topology", "Revised Geometry and Topology");
        files = picker(pane, true); search(files, "revised topology", b);
        key(Qt::Key_Escape); check(pane->hasFocus(), "Escape restores PDF focus");
        qInfo("PASS: folder search, Unicode, directory traversal, explicit paths and replaced PDF title cache");

        pane->toggle_split(); settle();
        auto right = pane->split_pane_.data(); check(right, "split exists");
        right->open_document(a.toStdWString()); pane->focus_split_pane(1); settle();
        files = picker(right, true);
        check_navigation(files, right);
        auto right_edit = files->findChild<QLineEdit*>();
        right_edit->setText("revised");
        key(Qt::Key_A, Qt::ControlModifier);
        check(right_edit->selectedText() == "revised", "Cmd+A selects right-pane search text");
        right_edit->clear(); key(Qt::Key_Down);
        check(files->get_view()->currentIndex().row() == 1, "arrow navigation targets right-pane picker");
        key(Qt::Key_Escape);
        check(right->hasFocus() && right->current_widget_stack.empty(), "Escape returns focus to right pane");
        files = picker(right, true); search(files, "revised topology", b);
        const auto left_path = pane->doc()->get_path();
        pane->resize(960,700); settle();
        check(pane->rect().contains(files->geometry()) &&
            (files->geometry().center()-pane->rect().center()).manhattanLength() <= 2, "floating picker recenters after resize");
        key(Qt::Key_Return);
        check(right->doc()->get_path() == b.toStdWString() && pane->doc()->get_path() == left_path, "right picker opens only in right pane");
        right->pop_all_widgets();
        recent = picker(right, false); search(recent, "quantum", a);
        recent->findChild<QLineEdit*>()->selectAll();
        key(Qt::Key_Backspace, Qt::ControlModifier);
        check(recent->findChild<QLineEdit*>()->text() == "quantum", "Cmd+Delete preserves selected search text");
        check(QFileInfo::exists(a) && recent->get_view()->model()->rowCount() == 0, "delete removes history result without deleting PDF");
        key(Qt::Key_Backspace, Qt::ControlModifier);
        check(recent->findChild<QLineEdit*>()->text() == "quantum", "Cmd+Delete with no matches preserves search");
        key(Qt::Key_Escape);
        pane->toggle_split(); settle();
        files = picker(pane, true); search(files, "revised", b); key(Qt::Key_Escape);
        qInfo("PASS: floating pickers preserve split focus, resize, history deletion and collapse behavior");
        for (int i = 0; i < 30; ++i) {
            const QString path = folder + QString("/many-%1.pdf").arg(i);
            pdf(path, "", QString("Large library fixture %1").arg(i));
            pane->open_document(path.toStdWString()); settle(30);
        }
        pane->resize(1000, 1000); settle();
        for (const bool filesystem : {false, true}) {
            auto large = picker(pane, filesystem);
            settle(300);
            check(large->height() == pane->height() - 48, "large lists use available window height");
            auto view = large->get_view();
            check(view->verticalScrollBar()->maximum() > 0, "large lists retain overflow scrolling");
            view->scrollToTop();
            const QPointF pos = view->viewport()->rect().center();
            QWheelEvent wheel(pos, view->viewport()->mapToGlobal(pos.toPoint()), QPoint(), QPoint(0,-120),
                Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
            QApplication::sendEvent(view->viewport(), &wheel); settle();
            check(view->verticalScrollBar()->value() > 0, "mouse wheel scrolls beyond visible results");
            if (!filesystem) {
                auto search_box = large->findChild<QLineEdit*>();
                search_box->setText("many-"); settle();
                const int count = view->model()->rowCount();
                const auto removed_path = QString::fromStdWString(large->get_selected_text());
                key(Qt::Key_Backspace, Qt::ControlModifier);
                check(search_box->text() == "many-" && view->model()->rowCount() == count - 1,
                    "Cmd+Delete removes exactly one filtered result without editing the query");
                check(QFileInfo::exists(removed_path), "Cmd+Delete leaves the PDF on disk");
                key(Qt::Key_Backspace);
                check(search_box->text() == "many", "ordinary Backspace still edits the query");
                search_box->clear(); settle();
                const int unfiltered_count = view->model()->rowCount();
                key(Qt::Key_Backspace, Qt::ControlModifier);
                check(view->model()->rowCount() == unfiltered_count - 1, "Cmd+Delete still removes unfiltered results once");
            }
            if (qEnvironmentVariableIsSet("SIOYEK_PICKER_SCREENSHOT")) large->grab().save(qEnvironmentVariable("SIOYEK_PICKER_SCREENSHOT") + ".large.png");
            key(Qt::Key_Escape);
        }
        qInfo("PASS: large libraries expand both pickers and remain scrollable");
        result = 0; QCoreApplication::exit(0);
    } catch (const std::exception& e) { qCritical("FAIL: %s", e.what()); QCoreApplication::exit(1); }
}
static void schedule() { QTimer::singleShot(1200, qApp, run); }
Q_COREAPP_STARTUP_FUNCTION(schedule)
int main(int argc, char** argv) {
    QStandardPaths::setTestModeEnabled(true); QTemporaryDir temp; folder = temp.path();
    char option[] = "--instance-name", name[] = "sioyek-document-picker-test";
    char* args[] = {argv[0], argv[1], option, name};
    sioyek_application_main(4,args); return result;
}
