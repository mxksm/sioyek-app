#define main sioyek_application_main
#include "../pdf_viewer/main.cpp"
#undef main
#include <QTemporaryDir>
#include <QEventLoop>
#include <QTreeView>
#include <QClipboard>
#include <stdexcept>

extern bool FLAT_TABLE_OF_CONTENTS;
static int result = 1;
static void check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
static void settle(int ms = 60) { QEventLoop loop; QTimer::singleShot(ms, &loop, &QEventLoop::quit); loop.exec(); }
static void key(int code, QString text = {}) {
    auto target = windows.front()->focusWidget(); check(target, "focus exists");
    QKeyEvent press(QEvent::KeyPress, code, Qt::NoModifier, text); QApplication::sendEvent(target, &press);
    if (auto focus = windows.front()->focusWidget()) {
        QKeyEvent release(QEvent::KeyRelease, code, Qt::NoModifier, text); QApplication::sendEvent(focus, &release);
    }
    settle();
}
static void type(QString text) { for (auto ch : text) key(ch.toUpper().unicode(), QString(ch)); }
static TocSelector* open_toc(MainWidget* pane) {
    pane->setFocus(); type("t");
    check(!pane->current_widget_stack.empty(), "t opens TOC");
    auto picker = dynamic_cast<TocSelector*>(pane->current_widget_stack.back());
    check(picker, "t uses floating TOC");
    auto owner = pane->tab_owner();
    check(picker->parentWidget() == owner && owner->rect().contains(picker->geometry()) &&
        (picker->geometry().center()-owner->rect().center()).manhattanLength() <= 2, "TOC floats centered over the whole window");
    check(picker->width() == std::min(1050, owner->width()-48) && picker->height() < owner->height(), "TOC is wide with margins");
    auto edit = picker->findChild<QLineEdit*>();
    check(edit && owner->focusWidget() == edit && edit->isReadOnly(), "TOC starts in normal mode");
    check(owner->childAt(edit->mapTo(owner, edit->rect().center())) == edit, "TOC search is above PDF surfaces");
    return picker;
}
static void test_pane(MainWidget* pane, bool flat) {
    FLAT_TABLE_OF_CONTENTS = flat;
    auto picker = open_toc(pane);
    auto tree = static_cast<QTreeView*>(picker->get_view());
    const auto y = pane->main_document_view->get_offset_y();
    type("gg");
    for (int i=0; i<5; ++i) key(Qt::Key_Up);
    check(pane->main_document_view->get_offset_y() == y, "top boundary does not scroll underlying PDF");
    type("G");
    check(tree->currentIndex().data().toString() == "Appendix", "G selects last chapter");
    for (int i=0; i<5; ++i) key(Qt::Key_Down);
    check(pane->main_document_view->get_offset_y() == y, "bottom boundary does not scroll underlying PDF");
    type("ggj");
    check(tree->currentIndex().data().toString() == "Nested section", "j selects nested section in document order");
    type("yy"); check(QApplication::clipboard()->text() == "Nested section", "yy copies selected section title");
    if (!flat) {
        type("gg"); key(Qt::Key_Left);
        check(!tree->isExpanded(tree->currentIndex()), "left arrow collapses chapter");
        key(Qt::Key_Right); check(tree->isExpanded(tree->currentIndex()), "right arrow expands chapter");
    }
    type("iAppendix");
    auto edit = picker->findChild<QLineEdit*>();
    check(!edit->isReadOnly() && tree->currentIndex().data().toString() == "Appendix", "insert mode searches chapters");
    key(Qt::Key_Escape); check(picker->isVisible() && edit->isReadOnly(), "Escape returns search to normal mode");
    key(Qt::Key_Return);
    check(pane->current_widget_stack.empty() && pane->main_document_view->get_center_page_number() == 2,
        "Enter closes picker and jumps in invoking pane");
    picker = open_toc(pane); tree = static_cast<QTreeView*>(picker->get_view());
    check(tree->currentIndex().data().toString() == "Appendix", "reopen selects current chapter");
    type("iNested");
    check(tree->model()->rowCount() > 0, "search preserves ancestors of matching subsection");
    key(Qt::Key_Escape); type("G"); key(Qt::Key_Return);
    check(pane->main_document_view->get_center_page_number() == 1, "filtered nested section maps to correct page");
    picker = open_toc(pane);
    if (!flat && pane == pane->tab_owner()) picker->grab().save("/tmp/sioyek-toc-picker.png");
    key(Qt::Key_Escape); check(pane->current_widget_stack.empty(), "normal Escape closes TOC");
}
static void run() {
    try {
        auto owner=windows.front(); owner->resize(1200,900); owner->activateWindow(); settle();
        test_pane(owner, false); test_pane(owner, true);
        FLAT_TABLE_OF_CONTENTS = false;
        owner->toggle_split(); settle(); auto right=owner->split_pane_.data(); check(right, "split exists");
        auto left_y=owner->main_document_view->get_offset_y(); test_pane(right, false);
        check(owner->main_document_view->get_offset_y()==left_y, "right TOC leaves left PDF unchanged");
        auto picker=open_toc(right); owner->resize(820,700); settle();
        check((picker->geometry().center()-owner->rect().center()).manhattanLength()<=2 && picker->width()==std::min(1050, owner->width()-48),
            "TOC stays wide and centered after resize");
        picker->on_config_file_changed();
        key(Qt::Key_Escape);
        qInfo("PASS: wide floating TOC, Vim search/navigation, nested and flat outlines, split dispatch, resize, boundary keys");
        result=0; QCoreApplication::exit(0);
    } catch(const std::exception& e) { qCritical("FAIL: %s", e.what()); QCoreApplication::exit(1); }
}
static QByteArray toc_pdf() {
    QByteArray bytes("%PDF-1.4\n"); std::vector<int> offsets{0};
    auto object=[&](QByteArray body) { offsets.push_back(bytes.size()); bytes+=QByteArray::number(offsets.size()-1)+" 0 obj\n"+body+"\nendobj\n"; };
    object("<< /Type /Catalog /Pages 2 0 R /Outlines 6 0 R >>");
    object("<< /Type /Pages /Kids [3 0 R 4 0 R 5 0 R] /Count 3 >>");
    for (int i=0;i<3;++i) object("<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Resources << >> >>");
    object("<< /Type /Outlines /First 7 0 R /Last 9 0 R /Count 3 >>");
    object("<< /Title (Introduction) /Parent 6 0 R /First 8 0 R /Last 8 0 R /Count 1 /Next 9 0 R /Dest [3 0 R /Fit] >>");
    object("<< /Title (Nested section) /Parent 7 0 R /Dest [4 0 R /Fit] >>");
    object("<< /Title (Appendix) /Parent 6 0 R /Prev 7 0 R /Dest [5 0 R /Fit] >>");
    const int xref=bytes.size(); bytes+="xref\n0 10\n0000000000 65535 f \n";
    for (size_t i=1;i<offsets.size();++i) bytes+=QByteArray::number(offsets[i]).rightJustified(10,'0')+" 00000 n \n";
    bytes+="trailer\n<< /Size 10 /Root 1 0 R >>\nstartxref\n"+QByteArray::number(xref)+"\n%%EOF\n"; return bytes;
}
static void schedule() { QTimer::singleShot(1200,qApp,run); }
Q_COREAPP_STARTUP_FUNCTION(schedule)
int main(int, char** argv) {
    QStandardPaths::setTestModeEnabled(true); QTemporaryDir dir;
    const auto path=dir.path()+"/outline.pdf"; QFile file(path); file.open(QIODevice::WriteOnly);file.write(toc_pdf());file.close();
    QByteArray first=path.toUtf8();char option[]="--instance-name",name[]="sioyek-toc-picker-test";
    char* args[]={argv[0],first.data(),option,name};sioyek_application_main(4,args);return result;
}
