#define main sioyek_application_main
#include "../pdf_viewer/main.cpp"
#undef main
#include "../pdf_viewer/workspace_store.h"
#include <QTemporaryDir>
#include <QEventLoop>
#include <QLineEdit>
#include <QMessageBox>
#include <QListView>
#include <stdexcept>
#include <cmath>

static QString directory;
static int result = 1;
static void check(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
static void settle(int ms = 150) { QEventLoop loop; QTimer::singleShot(ms, &loop, &QEventLoop::quit); loop.exec(); }
static void write(const QString& path, const QByteArray& bytes) {
    QFile file(path); check(file.open(QIODevice::WriteOnly), "open fixture"); check(file.write(bytes) == bytes.size(), "write fixture");
}
static QByteArray read(const QString& path) { QFile file(path); check(file.open(QIODevice::ReadOnly), "read fixture"); return file.readAll(); }
static void set_view(MainWidget* pane, float zoom, float x, float y) {
    pane->main_document_view->set_book_state(OpenedBookState{zoom,x,y}); pane->invalidate_render();
}
static void expect_view(MainWidget* pane, float zoom, float x, float y) {
    const auto v = pane->main_document_view->get_state().book_state;
    if (!(std::abs(v.zoom_level-zoom)<0.01 && std::abs(v.offset_x-x)<0.1 && std::abs(v.offset_y-y)<0.1))
        qInfo() << "VIEW" << v.zoom_level << v.offset_x << v.offset_y << "expected" << zoom << x << y;
    check(std::abs(v.zoom_level-zoom)<0.01 && std::abs(v.offset_x-x)<0.1 && std::abs(v.offset_y-y)<0.1,
        "exact saved zoom and offsets restored");
}
static void key(int code, Qt::KeyboardModifiers mods = Qt::NoModifier, QString text = {}) {
    auto target = QApplication::focusWidget();
    if (!target) qInfo() << "NO FOCUS FOR KEY" << code;
    check(target, "key target exists");
    QKeyEvent press(QEvent::KeyPress,code,mods,text); QApplication::sendEvent(target,&press);
    QKeyEvent release(QEvent::KeyRelease,code,mods,text);
    if (QApplication::focusWidget()) QApplication::sendEvent(QApplication::focusWidget(),&release);
    settle();
}
static void run() {
    try {
        standard_data_path = Path(directory.toStdWString());
        last_opened_file_address_path = standard_data_path.slash(L"session.txt");
        auto owner = windows.front(); owner->window_tabs = {};
        const auto keys = directory + "/keys.config";
        write(keys, "save_workspace <D-S>\nopen_workspace <D-P>\n");
        owner->input_handler->reload_config_files(default_keys_path, {Path(keys.toStdWString())});
        const auto a = (directory + "/a.pdf").toStdWString(), b = (directory + "/b.pdf").toStdWString();
        write(QString::fromStdWString(a), blank_canvas_pdf(10));
        write(QString::fromStdWString(b), blank_canvas_pdf(12));
        owner->open_document(a); set_view(owner,1.5,10,1200);
        owner->open_document(b); set_view(owner,1.2,20,1800);
        owner->toggle_split(); settle();
        owner->focus_split_pane(0); owner->select_tab(0);
        set_view(owner,1.5,10,1200);
        owner->focus_split_pane(1); auto right = owner->split_pane_.data(); right->select_tab(1);
        owner->pdf_splitter_->setSizes({600,300}); settle();
        set_view(owner,1.5,10,1200); set_view(right,1.2,20,1800);
        QString error;
        check(right->save_workspace("Topology / α",error), "save named workspace from right pane");
        const auto original = owner->capture_workspace();
        const auto inactive_b = owner->window_tabs.state(b).value();
        check(original["tabs"].toArray().size()==2 && original["panes"].toArray().size()==2 && original["focus"].toInt()==1,
            "snapshot captures entire window from either pane");
        owner->toggle_split(Qt::Vertical); settle();
        owner->pdf_splitter_->setSizes({200,400}); settle();
        set_view(owner,1.5,0,1200); set_view(owner->split_pane_,1.2,0,1800);
        const auto stacked = owner->capture_workspace();
        check(stacked["split_layout"].toString()=="stacked", "workspace stores stacked orientation");
        check(owner->apply_workspace(original,error), "switch from stacked workspace to side-by-side"); settle();
        check(owner->apply_workspace(stacked,error), "restore stacked workspace"); settle();
        check(owner->pdf_splitter_->orientation()==Qt::Vertical && owner->active_pane()==owner->split_pane_,
            "stacked workspace restores orientation and focus");
        const auto heights = owner->pdf_splitter_->sizes();
        check(std::abs(double(heights[0])/(heights[0]+heights[1])-stacked["split_fraction"].toDouble())<0.02,
            "stacked workspace restores height ratio");
        expect_view(owner,1.5,0,1200); expect_view(owner->split_pane_,1.2,0,1800);
        auto legacy = original; legacy.remove("split_layout");
        check(owner->apply_workspace(legacy,error), "older workspaces still load"); settle();
        check(owner->pdf_splitter_->orientation()==Qt::Horizontal, "old workspace defaults to side-by-side");
        qInfo("PASS: stacked workspace orientation, proportions, focus, positions and legacy compatibility");
        owner->focus_split_pane(0); owner->toggle_split(); owner->open_blank_canvas(); owner->window_tabs = {};
        const auto before_switch = owner->capture_workspace();
        check(owner->load_workspace("Topology / α",error), "restore named workspace"); settle();
        check(owner->window_tabs.paths() == std::vector<std::wstring>{a,b}, "tab order restored");
        check(owner->doc()->get_path()==a && owner->split_pane_ && owner->split_pane_->doc()->get_path()==b &&
            owner->active_pane()==owner->split_pane_, "split documents and focus restored");
        expect_view(owner,1.5,10,1200); expect_view(owner->split_pane_,1.2,20,1800);
        const auto sizes=owner->pdf_splitter_->sizes();
        qInfo() << "SPLIT" << sizes << "expected fraction" << original["split_fraction"].toDouble();
        check(std::abs(double(sizes[0])/(sizes[0]+sizes[1])-original["split_fraction"].toDouble())<0.02, "split ratio restored");
        check(owner->load_workspace("",error), "previous session is recoverable"); settle();
        check(!owner->split_pane_ && owner->window_tabs.paths().empty() && is_blank_canvas(owner->doc()->get_path()),
            "previous scratch-only session restored without a numbered canvas tab");
        check(owner->load_workspace("Topology / α",error), "repeat workspace switch"); settle();
        owner->focus_split_pane(0); owner->select_tab(1); expect_view(owner,inactive_b.zoom_level,inactive_b.offset_x,inactive_b.offset_y);
        owner->select_tab(0); expect_view(owner,1.5,10,1200);
        owner->focus_split_pane(1); owner->split_pane_->select_tab(0); set_view(owner->split_pane_,2.0,40,2500);
        check(owner->save_workspace("Same PDF",error), "save independent views of one PDF");
        owner->open_blank_canvas();
        check(owner->load_workspace("Same PDF",error), "restore two views of the same PDF"); settle();
        check(owner->doc()==owner->split_pane_->doc(), "shared document object preserved");
        expect_view(owner,1.5,10,1200); expect_view(owner->split_pane_,2.0,40,2500);
        owner->split_pane_->open_blank_canvas();
        check(owner->save_workspace("Study + scratch",error), "save PDF with hidden scratch pane");
        check(owner->load_workspace("Topology / α",error) && owner->load_workspace("Study + scratch",error), "restore scratch split"); settle();
        check(owner->window_tabs.paths().size()==2 && is_blank_canvas(owner->split_pane_->doc()->get_path()), "canvas remains hidden after workspace restore");
        // A second window can read the on-disk store; loading it leaves the first window untouched.
        auto sibling = owner->handle_new_window(); settle();
        check(sibling->load_workspace("Topology / α",error), "workspaces are shared across windows"); settle();
        check(is_blank_canvas(owner->split_pane_->doc()->get_path()), "loading affects only the invoking window");
        sibling->close(); settle();
        // Missing files and broken stores fail before changing the current layout.
        const auto stable = owner->capture_workspace();
        check(QFile::rename(QString::fromStdWString(b),QString::fromStdWString(b)+".away"), "hide fixture PDF");
        check(!owner->load_workspace("Topology / α",error) && error.contains("unavailable"), "missing PDF reported");
        check(owner->capture_workspace()==stable, "missing PDF preserves current layout");
        check(QFile::rename(QString::fromStdWString(b)+".away",QString::fromStdWString(b)), "restore fixture PDF");
        const auto store_bytes=read(workspace_store_path());
        write(workspace_store_path(),"broken json");
        check(!owner->save_workspace("New",error) && read(workspace_store_path())=="broken json", "corrupt store is not overwritten");
        write(workspace_store_path(),store_bytes);
        check(!owner->save_workspace("  ",error), "empty workspace name rejected");
        // Exercise the actual picker and deferred command from the disposable right pane.
        owner->focus_split_pane(1); QApplication::setActiveWindow(owner); owner->split_pane_->setFocus();
        key(Qt::Key_P, Qt::ControlModifier | Qt::ShiftModifier, "P");
        auto picker=dynamic_cast<DocumentPicker*>(owner->split_pane_->current_widget_stack.back()); check(picker,"workspace picker exists");
        auto search=picker->findChild<QLineEdit*>(); auto list=picker->findChild<QListView*>();
        check(search->isReadOnly() && picker->parentWidget()==owner,"workspace picker uses Vim normal mode and full-window overlay");
        search->setText("Topology / α"); settle();
        check(list->model()->rowCount()==1 && list->model()->index(0,0).data().toString()=="Topology / α",
            "workspace names retain slashes and Unicode and are searchable");
        key(Qt::Key_Return); settle();
        check(owner->split_pane_ && owner->split_pane_->doc()->get_path()==b,"Enter switches workspace safely from right pane");
        key(Qt::Key_S, Qt::ControlModifier | Qt::ShiftModifier, "S");
        auto edit=qobject_cast<QLineEdit*>(QApplication::focusWidget());
        qInfo() << "SAVE PROMPT" << QApplication::focusWidget() << (edit ? edit->text() : "no edit");
        check(edit && edit->text()=="Topology / α", "save prompt prefills the last workspace name");
        key(Qt::Key_Escape);
        // Deletion uses the visible selection, persists immediately, and never closes PDFs.
        const auto layout_before_delete = owner->capture_workspace();
        check(owner->save_workspace("Delete A",error) && owner->save_workspace("Delete B",error), "save deletion fixtures");
        QApplication::setActiveWindow(owner); owner->active_pane()->setFocus(); settle();
        key(Qt::Key_P, Qt::ControlModifier | Qt::ShiftModifier, "P");
        picker=dynamic_cast<DocumentPicker*>(owner->active_pane()->current_widget_stack.back());
        check(picker,"deletion picker exists");
        search=picker->findChild<QLineEdit*>(); list=picker->findChild<QListView*>();
        search->setText("Delete"); settle();
        check(list->model()->rowCount()==2,"filtered deletion fixtures");
        key(Qt::Key_Backspace,Qt::ControlModifier);
        QJsonObject saved;
        check(read_workspace_store(saved,error) && !saved["workspaces"].toObject().contains("Delete A") &&
            saved["workspaces"].toObject().contains("Delete B"),"normal-mode deletion persists only selected workspace");
        check(list->currentIndex().data().toString()=="Delete B" && search->text()=="Delete","next result selected and query preserved");
        key(Qt::Key_I,Qt::NoModifier,"i");
        key(Qt::Key_Backspace,Qt::ControlModifier);
        check(list->model()->rowCount()==0 && search->text()=="Delete","insert-mode deletion preserves query");
        check(read_workspace_store(saved,error) && !saved["workspaces"].toObject().contains("Delete B"),"insert-mode deletion persisted");
        key(Qt::Key_Backspace,Qt::ControlModifier); // No selection: harmless.
        search->setText("Previous session"); settle();
        key(Qt::Key_Backspace,Qt::ControlModifier);
        check(read_workspace_store(saved,error) && !saved.contains("previous") && !saved.contains("previous_name"),"automatic recovery entry can be deleted");
        search->setText("Topology / α"); settle();
        const auto valid_store=read(workspace_store_path());
        write(workspace_store_path(),"broken json");
        QTimer::singleShot(100, [] {
            if (auto dialog = qobject_cast<QMessageBox*>(QApplication::activeModalWidget())) dialog->accept();
        });
        key(Qt::Key_Backspace,Qt::ControlModifier);
        check(list->model()->rowCount()==1 && read(workspace_store_path())=="broken json","failed deletion retains row and storage");
        write(workspace_store_path(),valid_store);
        QApplication::setActiveWindow(owner); search->setFocus(); settle();
        key(Qt::Key_Escape); key(Qt::Key_Escape);
        check(owner->capture_workspace()==layout_before_delete && QFileInfo::exists(QString::fromStdWString(a)) &&
            QFileInfo::exists(QString::fromStdWString(b)),"deletion leaves open layout and PDF files untouched");
        qInfo("PASS: named workspace persistence, split state, independent tab views, scratch canvas, previous session, validation and Vim picker");
        result=0; QCoreApplication::exit(0);
    } catch(const std::exception& e) { qCritical("FAIL: %s",e.what()); QCoreApplication::exit(1); }
}
static void schedule() { QTimer::singleShot(1200,qApp,run); }
Q_COREAPP_STARTUP_FUNCTION(schedule)
int main(int argc,char** argv) {
    QStandardPaths::setTestModeEnabled(true); QTemporaryDir temp; directory=temp.path();
    char option[]="--instance-name",name[]="sioyek-workspaces-test";
    char* args[]={argv[0],argv[1],option,name}; sioyek_application_main(4,args); return result;
}
