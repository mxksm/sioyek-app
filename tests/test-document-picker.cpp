#define main sioyek_application_main
#include "../pdf_viewer/main.cpp"
#undef main
#include <QTemporaryDir>
#include <QPdfWriter>
#include <QEventLoop>
#include <QElapsedTimer>
#include <QScrollBar>
#include <QClipboard>
#include <QMimeData>
#include <QFontInfo>
#include <stdexcept>
#include <array>

extern bool FUZZY_SEARCHING;
extern std::wstring PDF_LIBRARY_PATH;
extern float UI_BACKGROUND_COLOR[3], UI_TEXT_COLOR[3], UI_SELECTED_BACKGROUND_COLOR[3], UI_SELECTED_TEXT_COLOR[3];
static QString folder;
static int result = 1;
static void check(bool yes, const char* message) { if (!yes) throw std::runtime_error(message); }
static void settle(int ms = 100) { QEventLoop loop; QTimer::singleShot(ms, &loop, &QEventLoop::quit); loop.exec(); }
static void key(int code, Qt::KeyboardModifiers modifiers = Qt::NoModifier, const QString& text = {}) {
    auto target = QApplication::focusWidget(); check(target, "keyboard focus exists");
    QKeyEvent press(QEvent::KeyPress, code, modifiers, text); QApplication::sendEvent(target, &press);
    QKeyEvent release(QEvent::KeyRelease, code, modifiers, text);
    if (QApplication::focusWidget()) QApplication::sendEvent(QApplication::focusWidget(), &release);
    settle();
}
static void dismiss_picker() {
    auto edit = qobject_cast<QLineEdit*>(QApplication::focusWidget());
    if (edit && !edit->isReadOnly()) key(Qt::Key_Escape);
    key(Qt::Key_Escape);
}
static void type_keys(const QString& text) {
    for (QChar ch : text) key(ch.toUpper().unicode(), ch.isUpper() ? Qt::ShiftModifier : Qt::NoModifier, QString(ch));
}
// Native clipboard operations are part of the feature, but test yanks should
// not replace the user's clipboard after the test exits.
struct ClipboardRestore {
    QMimeData* saved = new QMimeData;
    ClipboardRestore() {
        const auto current = QApplication::clipboard()->mimeData();
        if (current) for (const auto& format : current->formats()) saved->setData(format, current->data(format));
    }
    ~ClipboardRestore() { QApplication::clipboard()->setMimeData(saved); }
};
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
    check(widget->findChild<QLineEdit*>()->isReadOnly() &&
        widget->findChild<QLabel*>("documentPickerMode")->text() == "NORMAL", "picker opens in normal mode");
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
    for (int row = 0; row < widget->get_view()->model()->rowCount(); ++row)
        qWarning() << "result" << row << widget->get_view()->model()->index(row,0).data(Qt::ToolTipRole);
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
static void test_vim(DocumentPicker* widget, MainWidget* pane) {
    auto edit = widget->findChild<QLineEdit*>();
    auto mode = widget->findChild<QLabel*>("documentPickerMode");
    check(edit->isReadOnly() && mode->text() == "NORMAL", "picker starts in normal mode");
    type_keys("jkyy");
    check(edit->text().isEmpty() && QApplication::clipboard()->text() ==
        QString::fromStdWString(widget->get_selected_text()), "navigation and path yanking work immediately on opening");
    type_keys("i");
    check(!edit->isReadOnly() && mode->text() == "INSERT", "i starts query editing");
    edit->clear(); type_keys("jkyxgG");
    check(edit->text() == "jkyxgG", "insert mode types Vim command letters normally");
    edit->setText("alpha beta gamma");
    key(Qt::Key_Escape);
    check(widget->isVisible() && edit->isReadOnly() && mode->text() == "NORMAL", "Escape enters normal mode without closing");
    check(QFontInfo(edit->font()).family() == "JetBrains Mono", "search uses the bundled WezTerm font");
    check(edit->fontMetrics().horizontalAdvance('i') == edit->fontMetrics().horizontalAdvance('W'),
        "search letters occupy equal-width cells");
    edit->setText(QString::fromUtf8("iWm .😀"));
    QSize cursor_size;
    for (int pos : {0, 1, 2, 3, 4, 5, 7}) {
        edit->setCursorPosition(pos);
        edit->setProperty("vimNormalCursor", false);
        const auto without = edit->grab().toImage();
        edit->setProperty("vimNormalCursor", true);
        const auto with = edit->grab().toImage();
        QRect changed;
        for (int y = 0; y < with.height(); ++y) for (int x = 0; x < with.width(); ++x)
            if (with.pixel(x,y) != without.pixel(x,y)) changed = changed.united(QRect(x,y,1,1));
        check(!changed.isEmpty(), "normal-mode cursor is visible");
        if (cursor_size.isEmpty()) cursor_size = changed.size();
        check(changed.size() == cursor_size, "rendered cursor size is constant over letters, spaces, emoji and end of query");
        if (pos < 5) {
            edit->setProperty("vimNormalCursor", false);
            edit->setSelection(pos, 1);
            const auto selected = edit->grab().toImage();
            QRect selection;
            for (int y = 0; y < selected.height(); ++y) for (int x = 0; x < selected.width(); ++x)
                if (selected.pixel(x,y) != without.pixel(x,y)) selection = selection.united(QRect(x,y,1,1));
            check(std::abs(selection.left() - changed.left()) <= selected.devicePixelRatio(),
                "normal cursor aligns with the selected character cell");
            edit->deselect(); edit->setCursorPosition(pos);
            edit->setProperty("vimNormalCursor", true);
        }
    }
    edit->setCursorPosition(1);
    if (qEnvironmentVariableIsSet("SIOYEK_PICKER_SCREENSHOT")) edit->grab().save(qEnvironmentVariable("SIOYEK_PICKER_SCREENSHOT") + ".cursor.png");
    edit->setText("alpha beta gamma");
    type_keys("Vy");
    check(QApplication::clipboard()->text() == "alpha beta gamma", "visual y still copies search text to the clipboard");
    type_keys("0ve");
    check(edit->selectedText() == "alpha" && mode->text() == "VISUAL", "visual word motion selects a word");
    type_keys("x"); check(edit->text() == " beta gamma", "x deletes the visual selection");
    type_keys("u"); check(edit->text() == "alpha beta gamma", "u undoes search edits");
    key(Qt::Key_R, Qt::MetaModifier);
    check(edit->text() == " beta gamma", "physical Ctrl+R redoes search edits on macOS");
    type_keys("u0wcw");
    check(!edit->isReadOnly() && edit->text() == "alpha  gamma", "cw changes a word and enters insert mode");
    type_keys("delta"); key(Qt::Key_Escape);
    check(edit->text() == "alpha delta gamma", "insert after cw replaces the word");
    type_keys("0dw"); check(edit->text() == "delta gamma", "dw removes a word and its following space");
    type_keys("dd"); check(edit->text().isEmpty(), "dd clears the query");
    type_keys("p"); check(edit->text() == "delta gamma", "p pastes the yanked query");
    type_keys("0v2lx"); check(edit->text() == "ta gamma", "counted visual motions delete just the selected characters");
    type_keys("uA"); type_keys(" z"); key(Qt::Key_Escape);
    check(edit->text() == "delta gamma z", "A appends at the end");
    type_keys("I"); type_keys("pre "); key(Qt::Key_Escape);
    check(edit->text() == "pre delta gamma z", "I inserts at the beginning");
    type_keys("V");
    check(edit->selectedText() == edit->text(), "V selects the complete query");
    key(Qt::Key_Escape);
    check(widget->isVisible() && !edit->hasSelectedText() && mode->text() == "NORMAL", "Escape leaves visual mode without closing");
    edit->setText(QString::fromUtf8("a😀éz"));
    type_keys("0lx"); check(edit->text() == QString::fromUtf8("aéz"), "x does not split surrogate pairs");
    type_keys("u0llx"); check(edit->text() == QString::fromUtf8("a😀z"), "x does not split combining characters");
    edit->setText("many-"); settle();
    auto view = widget->get_view();
    view->setCurrentIndex(view->model()->index(0, 0));
    const float left_y = pane->tab_owner()->main_document_view->get_offset_y();
    const float pane_y = pane->main_document_view->get_offset_y();
    type_keys("j2jk"); check(view->currentIndex().row() == 2, "j/k and counts navigate results");
    type_keys("999k"); check(view->currentIndex().row() == 0, "k stops at the first result");
    type_keys("999j"); check(view->currentIndex().row() == view->model()->rowCount() - 1, "j stops at the last result");
    type_keys("g");
    check(view->currentIndex().row() == view->model()->rowCount() - 1 && mode->text() == "NORMAL  g",
        "first g waits for a second key without moving the result");
    key(Qt::Key_Escape);
    check(widget->isVisible() && mode->text() == "NORMAL", "Escape cancels pending g without closing");
    type_keys("gg"); check(view->currentIndex().row() == 0, "gg selects the first filtered result");
    type_keys("gg"); check(view->currentIndex().row() == 0, "gg stays at the first result");
    type_keys("G"); check(view->currentIndex().row() == view->model()->rowCount() - 1, "G selects the last filtered result");
    type_keys("G"); check(view->currentIndex().row() == view->model()->rowCount() - 1, "G stays at the last result");
    check(edit->text() == "many-" && pane->main_document_view->get_offset_y() == pane_y &&
        pane->tab_owner()->main_document_view->get_offset_y() == left_y, "normal navigation changes neither query nor PDFs");
    ConfigFileChangeListener::notify_config_file_changed(pane->config_manager); settle();
    check(mode->text() == "NORMAL" && edit->isReadOnly(), "theme changes retain Vim mode");
    if (qEnvironmentVariableIsSet("SIOYEK_PICKER_SCREENSHOT")) widget->grab().save(qEnvironmentVariable("SIOYEK_PICKER_SCREENSHOT") + ".normal.png");
    const QString selected_path = QString::fromStdWString(widget->get_selected_text());
    const int cursor = edit->cursorPosition();
    type_keys("yy");
    check(QApplication::clipboard()->text() == selected_path, "normal yy copies the selected PDF path");
    check(edit->text() == "many-" && edit->cursorPosition() == cursor &&
        QString::fromStdWString(widget->get_selected_text()) == selected_path && mode->text() == "NORMAL",
        "path yanking preserves query, cursor, selected result and mode");
    type_keys("kyy");
    check(QApplication::clipboard()->text() == QString::fromStdWString(widget->get_selected_text()) &&
        QApplication::clipboard()->text() != selected_path, "yy follows the currently selected result after navigation");
    if (dynamic_cast<FileSelector*>(widget)) {
        search(widget, "research notes", folder + "/research notes");
        type_keys("yy");
        check(QApplication::clipboard()->text() == folder + "/research notes", "yy copies folder paths with spaces without quotes");
    }
    const auto clipboard_before = QApplication::clipboard()->text();
    edit->setText("zzzzzzzzzzzzzzzz"); settle(); type_keys("yy");
    check(QApplication::clipboard()->text() == clipboard_before && mode->text() == "NORMAL",
        "yy with no results preserves the clipboard and finishes the command");
    type_keys("ggG");
    check(!view->currentIndex().isValid() && edit->text() == "zzzzzzzzzzzzzzzz" && mode->text() == "NORMAL",
        "gg and G with no results are harmless");
    key(Qt::Key_Escape);
    check(pane->current_widget_stack.empty() && pane->hasFocus(), "Escape from normal mode closes to the originating pane");
}

static void test_library(MainWidget* pane) {
    const std::wstring original_root = PDF_LIBRARY_PATH;
    struct RestoreRoot { std::wstring value; ~RestoreRoot() { PDF_LIBRARY_PATH = value; } } restore{original_root};
    const QString root = folder + "/PDF library";
    QDir().mkpath(root + "/books/algebra"); QDir().mkpath(root + "/books/cs/deep");
    const QString algebra = root + "/books/algebra/report.pdf";
    const QString graphs = root + "/books/cs/deep/notes.PDF";
    const QString duplicate = root + "/books/cs/report.pdf";
    pdf(algebra, "Abstract Algebra and Galois Theory", "Galois extensions");
    pdf(graphs, "Graph Algorithms", "Graph algorithms");
    pdf(duplicate, "Compilers", "Compilers and interpreters");
    pdf(root + "/ignored.epub", "Not a PDF extension", "Ignored");
    // A symlink cycle must never turn a recursive scan into an unbounded walk.
    check(QFile::link(root, root + "/loop"), "create library symlink cycle fixture");
    check(pane->config_manager->deserialize_config("pdf_library_path", root.toStdWString()), "library folder is configurable");
    auto open = [&](MainWidget* target) {
        target->setFocus();
        key(Qt::Key_O, Qt::ShiftModifier, "O");
        check(!target->current_widget_stack.empty(), "Shift+O opens library search");
        auto library = dynamic_cast<LibrarySelector*>(target->current_widget_stack.back());
        check(library && library->isVisible(), "library command creates its picker");
        check(library->findChild<QLineEdit*>()->isReadOnly(), "library starts in normal mode");
        check(library->parentWidget() == target->tab_owner(), "library floats over the whole split window");
        return library;
    };
    auto library = open(pane);
    search(library, "galois theory", algebra);
    search(library, "graph algorithms", graphs);
    search(library, "algebra report", algebra);
    search(library, "cs report", duplicate);
    auto edit = library->findChild<QLineEdit*>();
    edit->clear(); settle();
    check(library->get_view()->model()->rowCount() == 3, "recursive scan includes nested and uppercase PDFs only, without symlink duplicates");
    const auto first = library->get_view()->model()->index(0,0);
    check(first.data(Qt::UserRole + 21).toString().contains("books/algebra"), "library rows show the containing folder");
    type_keys("G"); check(library->get_view()->currentIndex().row() == 2, "G selects the last library result");
    type_keys("ggyy"); check(QApplication::clipboard()->text() == algebra, "gg and yy select and copy the first library path");
    check_navigation(library, pane);
    std::vector<std::pair<std::wstring, std::wstring>> history;
    pane->db_manager->select_opened_books_path_and_doc_names(history);
    for (const auto& item : history) {
        const auto path = pane->checksummer->get_path(utf8_encode(item.first));
        check(!path || QString::fromStdWString(*path).startsWith(root) == false, "library indexing does not add unopened PDFs to recents");
    }
    auto refresh = [&]() {
        bool completed = false;
        auto connection = QObject::connect(PdfLibraryIndex::instance(), &PdfLibraryIndex::scanned, library,
            [&](const QString& scanned_root, const QStringList&, const QString&, const QString&) { if (scanned_root == root) completed = true; });
        library->refresh();
        QElapsedTimer timer; timer.start();
        while (!completed && timer.elapsed() < 5000) settle(30);
        QObject::disconnect(connection);
        check(completed, "library refresh finishes in background");
    };
    search(library, "algebra report", algebra);
    const QString added = root + "/books/algebra/new.pdf";
    pdf(added, "Commutative Rings", "Commutative rings");
    refresh();
    check(edit->text() == "algebra report" && QString::fromStdWString(library->get_selected_text()) == algebra && edit->isReadOnly(),
        "refresh preserves query, mode and selected result");
    search(library, "commutative rings", added);
    check(QFile::remove(duplicate), "remove library fixture");
    const QString moved = root + "/books/algebra/moved.PDF";
    check(QFile::rename(graphs, moved), "move library fixture");
    refresh();
    search(library, "graph algorithms", moved);
    edit->clear(); settle(); check(library->get_view()->model()->rowCount() == 3, "refresh drops removed and old renamed paths");
    pdf(algebra, "Revised Differential Geometry", "Revised Differential Geometry");
    refresh(); search(library, "revised differential", algebra);
    const auto cached = PdfLibraryIndex::instance()->cached(root);
    check(cached.paths.size() == 3 && !cached.version.isEmpty(), "scan results are cached for reopening");
    if (qEnvironmentVariableIsSet("SIOYEK_PICKER_SCREENSHOT")) {
        edit->clear(); settle(); library->grab().save(qEnvironmentVariable("SIOYEK_PICKER_SCREENSHOT") + ".library.png");
    }
    dismiss_picker();
    auto right = pane->split_pane_.data(); check(right, "library test has a second pane");
    const auto left_path = pane->doc()->get_path();
    library = open(right);
    search(library, "commutative rings", added); key(Qt::Key_Return);
    check(right->doc()->get_path() == added.toStdWString() && pane->doc()->get_path() == left_path,
        "library opens the selected PDF only in the originating pane");
    right->pop_all_widgets();
    // A bad setting should be visible and dismissible, with no stale cached rows.
    PDF_LIBRARY_PATH = (root + "/missing folder").toStdWString();
    library = open(pane);
    QElapsedTimer timer; timer.start();
    while (!library->findChild<QLabel*>("documentPickerDetail")->text().contains("unavailable") && timer.elapsed() < 5000) settle(30);
    check(library->get_view()->model()->rowCount() == 0 &&
        library->findChild<QLabel*>("documentPickerDetail")->toolTip().contains("Library folder unavailable"), "missing library root has a clear empty state");
    dismiss_picker();
    qInfo("PASS: recursive library shortcut, titles, folders, cache, additions, moves, deletion, changed PDFs and split-pane ownership");
}

static void run() {
    ClipboardRestore clipboard_restore;
    try {
        FUZZY_SEARCHING = true;
        auto pane = windows.front();
        QApplication::setActiveWindow(pane);
        const QString a = folder + "/2401.00001.pdf", b = folder + "/download.pdf", c = folder + "/random.pdf";
        QDir().mkpath(folder + "/subfolder");
        QDir().mkpath(folder + "/research notes");
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
        type_keys("i"); edit->clear(); key(Qt::Key_Backspace); search(files, "quantum", a);
        edit->setText(folder + "/subfolder/nested.pdf"); key(Qt::Key_Return);
        check(pane->doc()->get_path() == (folder + "/subfolder/nested.pdf").toStdWString(), "explicit file path opens");
        pane->pop_all_widgets();
        pane->open_document(a.toStdWString()); settle();
        // A second picker must invalidate a title cached before a rewrite.
        pdf(b, "Revised Geometry and Topology", "Revised Geometry and Topology");
        files = picker(pane, true); search(files, "revised topology", b);
        dismiss_picker(); check(pane->hasFocus(), "Escape restores PDF focus");
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
        dismiss_picker();
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
        dismiss_picker();
        pane->toggle_split(); settle();
        files = picker(pane, true); search(files, "revised", b); dismiss_picker();
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
                type_keys("A"); key(Qt::Key_Backspace);
                check(search_box->text() == "many", "ordinary Backspace still edits the query");
                search_box->clear(); settle();
                const int unfiltered_count = view->model()->rowCount();
                key(Qt::Key_Backspace, Qt::ControlModifier);
                check(view->model()->rowCount() == unfiltered_count - 1, "Cmd+Delete still removes unfiltered results once");
            }
            if (qEnvironmentVariableIsSet("SIOYEK_PICKER_SCREENSHOT")) large->grab().save(qEnvironmentVariable("SIOYEK_PICKER_SCREENSHOT") + ".large.png");
            dismiss_picker();
        }
        qInfo("PASS: large libraries expand both pickers and remain scrollable");
        pane->toggle_split(); settle();
        auto vim_pane = pane->split_pane_.data();
        vim_pane->open_document((folder + "/many-29.pdf").toStdWString());
        for (const bool filesystem : {false, true}) {
            test_vim(picker(vim_pane, filesystem), vim_pane);
            auto cancel = picker(vim_pane, filesystem);
            type_keys("i");
            key(Qt::Key_C, Qt::MetaModifier);
            check(vim_pane->current_widget_stack.empty() && vim_pane->hasFocus(), "Ctrl+C closes immediately from insert mode");
            auto confirm = picker(vim_pane, filesystem);
            search(confirm, "many-7.pdf", folder + "/many-7.pdf");
            key(Qt::Key_Return);
            check(vim_pane->doc()->get_path() == (folder + "/many-7.pdf").toStdWString(), "Enter opens the selected file in normal mode");
            vim_pane->pop_all_widgets();
            vim_pane->open_document((folder + "/many-29.pdf").toStdWString()); settle();
        }
        qInfo("PASS: both pickers support modal search editing, clipboard, Unicode, undo and result navigation");
        test_library(pane);
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
