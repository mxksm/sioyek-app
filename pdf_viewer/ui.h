#pragma once
#include <iterator>
#include <string>
#include <vector>
#include <iostream>
#include <functional>
#include <optional>
#include <unordered_map>
#include <condition_variable>
#include <thread>
#include <mutex>
#include <deque>
#include <atomic>
#include <QSet>
#include <QPointer>

#include <QPlainTextEdit>
#include <QListWidget>
#include <QScroller>
#include <qsizepolicy.h>
#include <qapplication.h>
#include <qqmlengine.h>
#include <qpushbutton.h>
#include <qopenglwidget.h>
#include <qopenglextrafunctions.h>
#include <qopenglfunctions.h>
#include <qopengl.h>
#include <qwindow.h>
#include <QKeyEvent>
#include <qinputmethod.h>
#include <qlineedit.h>
#include <qtreeview.h>
#include <qsortfilterproxymodel.h>
#include <qabstractitemmodel.h>
#include <qopenglshaderprogram.h>
#include <qtimer.h>
#include <qdatetime.h>
#include <qstackedwidget.h>
#include <qboxlayout.h>
#include <qlistview.h>
#include <qtableview.h>
#include <qstringlistmodel.h>
#include <qpalette.h>
#include <qstandarditemmodel.h>
#include <qfilesystemmodel.h>
#include <qheaderview.h>
#include <qcolordialog.h>
#include <qslider.h>
#include <qlabel.h>
#include <qcheckbox.h>
#include <QQuickWidget>
#include "touchui/TouchSlider.h"
#include "touchui/TouchCheckbox.h"
#include "touchui/TouchListView.h"
#include "touchui/TouchCopyOptions.h"
#include "touchui/TouchRectangleSelectUI.h"
#include "touchui/TouchRangeSelectUI.h"
#include "touchui/TouchPageSelector.h"
#include "touchui/TouchMainMenu.h"
#include "touchui/TouchTextEdit.h"
#include "touchui/TouchSearchButtons.h"
#include "touchui/TouchDeleteButton.h"
#include "touchui/TouchHighlightButtons.h"
#include "touchui/TouchAudioButtons.h"
#include "touchui/TouchDrawControls.h"
#include "touchui/TouchMacroEditor.h"
#include "touchui/TouchGenericButtons.h"

#include "mysortfilterproxymodel.h"
#include "rapidfuzz_amalgamated.hpp"

#define FTS_FUZZY_MATCH_IMPLEMENTATION
#include "fts_fuzzy_match.h"
#undef FTS_FUZZY_MATCH_IMPLEMENTATION


#include "utils.h"
#include "config.h"
#include "pdf_picker.h"

extern "C" {
    #include <fzf/fzf.h>
}

class MainWidget;
extern std::wstring UI_FONT_FACE_NAME;
extern int FONT_SIZE;
const int max_select_size = 100;
extern bool SMALL_TOC;
extern bool MULTILINE_MENUS;
extern bool TOUCH_MODE;


class HierarchialSortFilterProxyModel : public QSortFilterProxyModel {
protected:
    bool filterAcceptsRow(int source_row, const QModelIndex& source_parent) const;
    //public:
    //	mutable int count = 0;
};

class ConfigFileChangeListener {
    static std::vector<ConfigFileChangeListener*> registered_listeners;


public:
    ConfigFileChangeListener();
    ~ConfigFileChangeListener();
    virtual void on_config_file_changed(ConfigManager* new_config_manager) = 0;
    static void notify_config_file_changed(ConfigManager* new_config_manager);
};


class MyLineEdit: public QLineEdit {
    Q_OBJECT

public:
    MainWidget* main_widget;
    MyLineEdit(MainWidget* parent);
    void native_key_press(QKeyEvent* event) { QLineEdit::keyPressEvent(event); }

    void keyPressEvent(QKeyEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    int get_next_word_position();
    int get_prev_word_position();
};

class PickerVimEditor;

// Shared command input, with a multiline editor for text annotations.
class FloatingTextPrompt : public QWidget, public ConfigFileChangeListener {
    QPointer<MainWidget> pane_;
    std::unique_ptr<PickerVimEditor> editor_;
    QLabel* mode_label_;
    QPlainTextEdit* note_edit_ = nullptr;
    QWidget* confirmation_ = nullptr;
    bool note_mode_ = false;
    bool moved_ = false;
    std::optional<QPoint> drag_offset_;
    QString initial_value_;
    void cancel();
    void save();
    void request_close();
public:
    MyLineEdit* edit;
    QLabel* heading;
    explicit FloatingTextPrompt(MainWidget* pane);
    ~FloatingTextPrompt() override;
    bool close_note_if_visible();
    void begin(const QString& title, const QString& value, bool password, bool insert_mode = false);
    void update_geometry();
    void on_config_file_changed(ConfigManager*) override;
    bool eventFilter(QObject*, QEvent*) override;
};

class BaseSelectorWidget : public QWidget {

protected:
    BaseSelectorWidget(QAbstractItemView* item_view, bool fuzzy, QStandardItemModel* item_model, MainWidget* parent);

    void on_text_changed(const QString& text);




    QLineEdit* line_edit = nullptr;
    //QSortFilterProxyModel* proxy_model = nullptr;
    MySortFilterProxyModel* proxy_model = nullptr;
    bool is_fuzzy = false;
    int pressed_row = -1;
    QPoint pressed_pos;

    QAbstractItemView* abstract_item_view;

    virtual void on_select(const QModelIndex& value) = 0;
    virtual void on_delete(const QModelIndex& source_index, const QModelIndex& selected_index);
    virtual void on_edit(const QModelIndex& source_index, const QModelIndex& selected_index);

    virtual void on_return_no_select(const QString& text);

    // should return true when we want to manually handle text change events
    virtual bool on_text_change(const QString& text);

    virtual QString get_view_stylesheet_type_name() = 0;

public:


    virtual QAbstractItemView* get_view();
    void set_filter_column_index(int index);
    std::optional<QModelIndex> get_selected_index();
    virtual std::wstring get_selected_text();
    bool eventFilter(QObject* obj, QEvent* event) override;
    void simulate_move_down();
    void simulate_move_up();
    void simulate_move_left();
    void simulate_move_right();
    void simulate_page_down();
    void simulate_page_up();
    void simulate_end();
    void simulate_home();
    void simulate_select();
    void handle_delete();
    void handle_edit();
    QString get_selected_item();


#ifndef SIOYEK_QT6
    void keyReleaseEvent(QKeyEvent* event) override;
#endif

    virtual void on_config_file_changed();
    void resizeEvent(QResizeEvent* resize_event) override;

};

template<typename T>
class FilteredTreeSelect : public BaseSelectorWidget {
private:
    std::function<void(const std::vector<int>&)> on_done;

protected:

public:

    QString get_view_stylesheet_type_name() {
        return "QTreeView";
    }

    FilteredTreeSelect(bool fuzzy, QStandardItemModel* item_model,
        std::function<void(const std::vector<int>&)> on_done,
        MainWidget* parent,
        std::vector<int> selected_index, QTreeView* view = nullptr) : BaseSelectorWidget(view ? view : new QTreeView(), fuzzy, item_model, parent),
        on_done(on_done)
    {
        auto index = QModelIndex();
        for (auto i : selected_index) {
            index = this->proxy_model->index(i, 0, index);
        }

        QTreeView* tree_view = dynamic_cast<QTreeView*>(this->get_view());
        if (SMALL_TOC) {
            tree_view->collapseAll();
            tree_view->expand(index);
        }

        tree_view->header()->setStretchLastSection(false);
        tree_view->header()->setSectionResizeMode(0, QHeaderView::Stretch);
        tree_view->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
        tree_view->setCurrentIndex(index);

    }

    void on_select(const QModelIndex& index) {
        this->hide();
        this->parentWidget()->setFocus();
        auto source_index = this->proxy_model->mapToSource(index);
        std::vector<int> indices;
        while (source_index != QModelIndex()) {
            indices.push_back(source_index.row());
            source_index = source_index.parent();
        }
        on_done(indices);
    }
};

// Wide floating outline picker; commands still belong to the invoking PDF pane.
class TocSelector : public FilteredTreeSelect<int>, public ConfigFileChangeListener {
    int preferred_height_ = 220;
    QPointer<MainWidget> pane_;
    std::unique_ptr<PickerVimEditor> vim_editor_;
    QLabel* mode_label_ = nullptr;
    void navigate(int key);
public:
    TocSelector(bool fuzzy, QStandardItemModel* model,
        std::function<void(const std::vector<int>&)> on_done, MainWidget* pane,
        std::vector<int> selected_index);
    ~TocSelector() override;
    void update_geometry();
    void on_config_file_changed() override;
    void on_config_file_changed(ConfigManager*) override { on_config_file_changed(); }
    bool on_text_change(const QString& text) override;
    void on_select(const QModelIndex& index) override;
    bool eventFilter(QObject*, QEvent*) override;
    void resizeEvent(QResizeEvent*) override;
    void showEvent(QShowEvent*) override;
};

// Common floating presentation for the remaining list/table selectors.
class FloatingListSelector : public BaseSelectorWidget, public ConfigFileChangeListener {
    QPointer<MainWidget> pane_;
    std::unique_ptr<PickerVimEditor> editor_;
    QLabel* heading_ = nullptr;
    QLabel* mode_label_ = nullptr;
    int preferred_height_ = 220;
    void move_selection(int delta);
protected:
    FloatingListSelector(QAbstractItemView* view, bool fuzzy, MainWidget* pane);
    void initialize_floating(int rows);
    void finish_selection();
public:
    ~FloatingListSelector() override;
    void set_heading(const QString& title);
    void update_geometry();
    void on_config_file_changed() override;
    void on_config_file_changed(ConfigManager*) override { on_config_file_changed(); }
    bool eventFilter(QObject*, QEvent*) override;
    void resizeEvent(QResizeEvent*) override;
    void showEvent(QShowEvent*) override;
};

template<typename T>
class FilteredSelectTableWindowClass : public FloatingListSelector {
private:

    QStringListModel* string_list_model = nullptr;
    std::vector<T> values;
    std::vector<std::wstring> item_strings;
    std::function<void(T*)> on_done = nullptr;
    std::function<void(T*)> on_delete_function = nullptr;
    std::function<void(T*)> on_edit_function = nullptr;
    int n_cols = 1;
    bool last_column_fixed = false;

protected:

public:

    QString get_view_stylesheet_type_name() override {
        return "QTableView";
    }

    FilteredSelectTableWindowClass(
        bool fuzzy,
        bool multiline,
        const std::vector<std::vector<std::wstring>>& table,
        std::vector<T> values,
        int selected_index,
        std::function<void(T*)> on_done,
        MainWidget* parent,
        std::function<void(T*)> on_delete_function = nullptr,
        bool last_column_fixed = false) : FloatingListSelector(new QTableView(), fuzzy, parent),
        values(values),
        on_done(on_done),
        on_delete_function(on_delete_function),
        last_column_fixed(last_column_fixed)
    {
        item_strings = table[0];
        QVector<QString> q_string_list;
        for (const auto& s : table[0]) {
            q_string_list.push_back(QString::fromStdWString(s));

        }


        QStandardItemModel* model = create_table_model(table);
        //model->setItem(selected_index, 0, new QStandardItem(QString::fromStdWString(std_string_list[selected_index])));
        //QStandardItemModel* model = new QStandardItemModel();

        //for (size_t i = 0; i < std_string_list.size(); i++) {
        //	QStandardItem* name_item = new QStandardItem(QString::fromStdWString(std_string_list[i]));
        //	QStandardItem* key_item = new QStandardItem(QString::fromStdWString(std_string_list_right[i]));
        //	key_item->setTextAlignment(Qt::AlignVCenter | Qt::AlignRight);
        //	model->appendRow(QList<QStandardItem*>() << name_item << key_item);
        //}

        model->setParent(this);
        this->proxy_model->setSourceModel(model);

        QTableView* table_view = dynamic_cast<QTableView*>(this->get_view());
        //        QScroller::grabGesture(table_view, QScroller::LeftMouseButtonGesture);
        //        table_view->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
        //        table_view->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);


        if (selected_index != -1) {
            table_view->selectionModel()->setCurrentIndex(this->proxy_model->mapFromSource(model->index(selected_index, 0)), QItemSelectionModel::Rows | QItemSelectionModel::SelectCurrent);
        }
        else {
            table_view->selectionModel()->setCurrentIndex(this->proxy_model->index(0, 0), QItemSelectionModel::Rows | QItemSelectionModel::SelectCurrent);
        }

        table_view->setSelectionMode(QAbstractItemView::SingleSelection);
        table_view->setSelectionBehavior(QAbstractItemView::SelectRows);
        table_view->setEditTriggers(QAbstractItemView::NoEditTriggers);

        if (table[0].size() > 0) {
            n_cols = table.size();
            table_view->horizontalHeader()->setStretchLastSection(false);
            for (int i = 0; i < std::ssize(table) - 1; i++) {
                table_view->horizontalHeader()->setSectionResizeMode(i, QHeaderView::Stretch);
            }

            if (last_column_fixed) {
                table_view->horizontalHeader()->setSectionResizeMode(table.size() - 1, QHeaderView::Fixed);
            }
            else {
                table_view->horizontalHeader()->setSectionResizeMode(table.size() - 1, QHeaderView::ResizeToContents);
            }
        }

        table_view->horizontalHeader()->hide();
        table_view->verticalHeader()->hide();

        if (multiline) {
            table_view->setWordWrap(true);
            table_view->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
        }

        initialize_floating(int(values.size()));
        if (selected_index != -1) {
            table_view->scrollTo(table_view->currentIndex(), QAbstractItemView::EnsureVisible);
        }
    }

    void set_equal_columns() {
        if (item_strings.size() > 0) {
            QTableView* table_view = dynamic_cast<QTableView*>(get_view());
            for (int i = 0; i < n_cols; i++) {
                table_view->horizontalHeader()->setSectionResizeMode(i, QHeaderView::Stretch);
            }
        }
    }

    void set_on_edit_function(std::function<void(T*)> edit_func) {
        on_edit_function = edit_func;
    }

    void set_value_second_item(T value, QString str) {

        for (int i = 0; i < std::ssize(values); i++) {
            if (values[i] == value) {
                auto source_model = this->proxy_model->sourceModel();
                source_model->setData(source_model->index(i, 1), str);
                return;
            }
        }

    }

    virtual std::wstring get_selected_text() override {
        auto index = this->get_selected_index();

        if (index) {

            auto source_index = this->proxy_model->mapToSource(index.value());
            return item_strings[source_index.row()];
        }

        return L"";
    }

    virtual void on_delete(const QModelIndex& source_index, const QModelIndex& selected_index) override {
        if (on_delete_function) {
            on_delete_function(&values[source_index.row()]);
            this->proxy_model->removeRow(selected_index.row());
            values.erase(values.begin() + source_index.row());
            item_strings.erase(item_strings.begin() + source_index.row());
        }
    }

    virtual void on_edit(const QModelIndex& source_index, const QModelIndex& /*selected_index*/) override {
        if (on_edit_function) {
            on_edit_function(&values[source_index.row()]);
        }
    }

    void on_select(const QModelIndex& index) override {
        auto source_index = this->proxy_model->mapToSource(index);
        if (!source_index.isValid() || source_index.row() >= std::ssize(values)) return;
        auto value = values[source_index.row()];
        auto callback = on_done;
        finish_selection();
        callback(&value);
    }
};

template <typename T>
class TouchFilteredSelectWidget : public QWidget {
private:
    //    QStringListModel string_list_model;
    //    MySortFilterProxyModel proxy_model;
    TouchListView* list_view = nullptr;
    QWidget* parent_widget;
    std::vector<T> values;
    std::function<void(T*)> on_done;
    std::function<void(T*)> on_delete_function = nullptr;

public:
    //	void set_selected_index(int index) {
    //		list_view->set_selected_index(index);
    //	}
    void initialize() {

        QObject::connect(list_view, &TouchListView::itemSelected, [&](QString /*name*/, int index) {
            on_done(&values[index]);
            deleteLater();
            });

        QObject::connect(list_view, &TouchListView::itemDeleted, [&](QString /*name*/, int index) {
            on_delete_function(&values[index]);
            //deleteLater();
            });
    }
    TouchFilteredSelectWidget(bool is_fuzzy, std::vector<std::wstring> std_string_list,
        std::vector<T> values_,
        int selected_index,
        std::function<void(T*)> on_done_,
        std::function<void(T*)> on_delete,
        QWidget* parent) :
        QWidget(parent),
        values(values_),
        on_done(on_done_),
        on_delete_function(on_delete) {

        parent_widget = parent;
        QStringList string_list;
        for (auto s : std_string_list) {
            string_list.append(QString::fromStdWString(s));
        }
        //        string_list_model.setStringList(string_list);
        //        proxy_model.setSourceModel(string_list_model);

        list_view = new TouchListView(is_fuzzy, string_list, selected_index, this, true);
        initialize();
    }

    TouchFilteredSelectWidget(bool is_fuzzy, QAbstractItemModel* model,
        int selected_index,
        std::function<void(T*)> on_done_,
        QWidget* parent) :
        QWidget(parent),
        on_done(on_done_) {
        parent_widget = parent;
        list_view = new TouchListView(is_fuzzy, model, selected_index, this, false, false, true);

        QObject::connect(list_view, &TouchListView::itemSelected, [&](QString /*name*/, int index) {
            on_done(&values[index]);
            deleteLater();
            });
    }

    TouchFilteredSelectWidget(bool is_fuzzy, QAbstractItemModel* model,
        std::vector<T> values_,
        int selected_index,
        std::function<void(T*)> on_done_,
        std::function<void(T*)> on_delete,
        QWidget* parent) :
        QWidget(parent),
        values(values_),
        on_done(on_done_),
        on_delete_function(on_delete) {

        parent_widget = parent;
        //        string_list_model.setStringList(string_list);
        //        proxy_model.setSourceModel(string_list_model);

        list_view = new TouchListView(is_fuzzy, model, selected_index, this, true);
        initialize();
    }

    void resizeEvent(QResizeEvent* resize_event) override {
        QWidget::resizeEvent(resize_event);
        int parent_width = parentWidget()->size().width();
        int parent_height = parentWidget()->size().height();
        //        setFixedSize(parent_width * 0.9f, parent_height);
        list_view->resize(parent_width * 0.9f, parent_height);
        move(parent_width * 0.05f, 0);
        resize(parent_width * 0.9f, parent_height);
    }

    void set_filter_column_index(int index) {
        list_view->proxy_model->setFilterKeyColumn(index);
    }

    void set_value_second_item(T value, QString str) {

        auto source_model = list_view->proxy_model->sourceModel();

        if (source_model->columnCount() < 2) return;

        for (int i = 0; i < std::ssize(values); i++) {
            if (values[i] == value) {
                source_model->setData(source_model->index(i, 1), str);
                list_view->update_model();
                return;
            }
        }

    }
};


template<typename T>
class FilteredSelectWindowClass : public FloatingListSelector {
private:

    QStringListModel* string_list_model = nullptr;
    std::vector<T> values;
    std::function<void(T*)> on_done = nullptr;
    std::function<void(T*)> on_delete_function = nullptr;

protected:

public:

    QString get_view_stylesheet_type_name() override {
        return "QListView";
    }


    FilteredSelectWindowClass(bool fuzzy, std::vector<std::wstring> std_string_list,
        std::vector<T> values,
        std::function<void(T*)> on_done,
        MainWidget* parent,
        std::function<void(T*)> on_delete_function = nullptr, int selected_index = -1) : FloatingListSelector(new QListView(), fuzzy, parent),
        values(values),
        on_done(on_done),
        on_delete_function(on_delete_function)
    {
        QVector<QString> q_string_list;
        for (const auto& s : std_string_list) {
            q_string_list.push_back(QString::fromStdWString(s));

        }
        QStringList string_list = QStringList::fromVector(q_string_list);


        string_list_model = new QStringListModel(string_list, this);
        this->proxy_model->setSourceModel(string_list_model);

        if (selected_index != -1) {
            dynamic_cast<QListView*>(this->get_view())->selectionModel()->setCurrentIndex(this->proxy_model->index(selected_index, 0), QItemSelectionModel::Rows | QItemSelectionModel::SelectCurrent);
        }
        else {
            dynamic_cast<QListView*>(this->get_view())->selectionModel()->setCurrentIndex(this->proxy_model->index(0, 0), QItemSelectionModel::Rows | QItemSelectionModel::SelectCurrent);
        }

        initialize_floating(int(values.size()));
    }

    std::wstring get_selected_text() override {
        return get_view()->currentIndex().data().toString().toStdWString();
    }

    virtual void on_delete(const QModelIndex& source_index, const QModelIndex& selected_index) override {
        if (on_delete_function) {
            on_delete_function(&values[source_index.row()]);
            this->proxy_model->removeRow(selected_index.row());
            values.erase(values.begin() + source_index.row());
        }
    }

    void on_select(const QModelIndex& index) override {
        auto source_index = this->proxy_model->mapToSource(index);
        if (!source_index.isValid() || source_index.row() >= std::ssize(values)) return;
        auto value = values[source_index.row()];
        auto callback = on_done;
        finish_selection();
        callback(&value);
    }
};

class TouchCommandSelector : public QWidget {
public:
    TouchCommandSelector(bool is_fuzzy, const QStringList& commands, MainWidget* mw);
    void resizeEvent(QResizeEvent* resize_event) override;
    //    void keyReleaseEvent(QKeyEvent* key_event) override;

private:
    MainWidget* main_widget;
    TouchListView* list_view;

};

// A read-only Vim document, with executable command rows and in-place search.
class CommandCheatsheet : public QWidget, public ConfigFileChangeListener {
    QPointer<MainWidget> pane_;
    QPlainTextEdit* text_;
    QLineEdit* search_;
    QLabel* status_;
    QStringList commands_;
    QString query_;
    QTextCursor search_origin_;
    int count_ = 0;
    QChar pending_;
    bool reverse_ = false;
    bool visual_ = false;
    void update_geometry();
    void decorate();
    void find_match(bool backwards, bool from_origin = false);
    void dismiss();
    void execute();
public:
    explicit CommandCheatsheet(MainWidget* pane);
    bool eventFilter(QObject*, QEvent*) override;
    void showEvent(QShowEvent*) override;
    void on_config_file_changed();
    void on_config_file_changed(ConfigManager*) override { on_config_file_changed(); }
};

class CommandSelector : public BaseSelectorWidget, public ConfigFileChangeListener {
private:
    QStringList string_elements;
    QPointer<MainWidget> main_widget;
    QLabel* heading_ = nullptr;
    std::unique_ptr<PickerVimEditor> vim_editor_;
    QLabel* mode_label_ = nullptr;
    QLabel* detail_ = nullptr;
    void update_geometry();
    void update_detail();
    QStandardItemModel* standard_item_model = nullptr;
    std::unordered_map<std::string, std::vector<std::string>> key_map;
    std::unordered_map<QString, QString> prefixes;
    std::function<void(std::string, std::string)>* on_done = nullptr;
    fzf_slab_t* slab;

    QList<QStandardItem*> get_item(std::string command_name);
    QStandardItemModel* get_standard_item_model(std::vector<std::string> command_names);
    QStandardItemModel* get_standard_item_model(QStringList command_names);
    QStringList get_elements_matching_prefix(QString prefix);

protected:
public:

    QString get_view_stylesheet_type_name() override;

    void on_select(const QModelIndex& index) override;

    CommandSelector(bool is_fuzzy, std::function<void(std::string, std::string)>* on_done,
        MainWidget* parent,
        QStringList elements,
        const std::unordered_map<QString, QString>& required_prefixed,
        std::unordered_map<std::string, std::vector<std::string>> key_map);
    ~CommandSelector();
    bool eventFilter(QObject*, QEvent*) override;
    void resizeEvent(QResizeEvent*) override;
    void showEvent(QShowEvent*) override;
    void on_config_file_changed() override;
    void on_config_file_changed(ConfigManager*) override { on_config_file_changed(); }

    virtual bool on_text_change(const QString& text) override;

};

// A shared worker keeps disk reads and first-page extraction off the UI thread.
// Requests expire with their picker; cached titles are invalidated by file changes.
class DocumentTitleIndex : public QObject {
    Q_OBJECT
    struct Request { QString path; std::weak_ptr<int> lifetime; };
    std::mutex mutex_;
    std::condition_variable ready_;
    std::deque<Request> requests_;
    bool stopping_ = false;
    std::thread worker_;
    void run();
public:
    explicit DocumentTitleIndex(QObject* parent);
    ~DocumentTitleIndex();
    static DocumentTitleIndex* instance();
    void request(const QString& path, const std::shared_ptr<int>& lifetime);
signals:
    void title_ready(QString path, QString title, QString words);
};

// Shared recursive scans run off the UI thread. Cached snapshots are available
// immediately on reopening; each request rechecks additions, removals and changes.
class PdfLibraryIndex : public QObject {
    Q_OBJECT
    std::mutex mutex_;
    std::condition_variable ready_;
    std::deque<QString> requests_;
    std::atomic<bool> stopping_{false};
    std::thread worker_;
    QSet<QString> pending_;
    void run();
public:
    struct Snapshot { QStringList paths; QString version; QString error; };
private:
    QHash<QString, Snapshot> cache_;
public:
    explicit PdfLibraryIndex(QObject* parent);
    ~PdfLibraryIndex();
    static PdfLibraryIndex* instance();
    Snapshot cached(const QString& root) const { return cache_.value(root); }
    void request(const QString& root);
signals:
    void scanned(QString root, QStringList paths, QString version, QString error);
};

struct DocumentPickerEntry {
    QString path;
    QString title;
    QString words;
    QString value; // history checksum, or a filesystem path
    bool directory = false;
    QString search_name = {}, search_title = {}, search_other = {};
    int page = -1;
    QString snippet = {};
    QList<int> matching_pages = {};
};

class PickerVimEditor;

class DocumentPicker : public BaseSelectorWidget, public ConfigFileChangeListener {
    QStandardItemModel* model_;
    PdfPickerPreview* preview_ = nullptr;
    QLabel* heading_;
    QLabel* mode_label_;
    QLabel* detail_;
    std::unique_ptr<PickerVimEditor> vim_editor_;
    QTimer refresh_timer_;
    std::shared_ptr<int> indexing_lifetime_;
    std::function<void(QString)> on_done_;
    std::function<bool(QString)> on_delete_;
    std::function<void(QString)> on_preview_;
    std::function<void()> on_cancel_preview_;
    QString preview_value_;
    bool preview_finished_ = false;
    void update_detail();
protected:
    QPointer<MainWidget> pane_;
    std::vector<DocumentPickerEntry> entries_;
    QString location_;
    bool fuzzy_;
    bool show_folders_ = false;
    bool document_entries_ = true;
    QString non_document_kind_;
    bool content_results_ = false;
    QString empty_message_ = "No documents";
    QString search_status_;
    bool search_insert_mode() const;
    void set_entries(std::vector<DocumentPickerEntry> entries, const QString& selected = {});
    void rebuild_results(bool preserve_selection = false, QString selected = {});
    void update_geometry();
    bool on_text_change(const QString& text) override;
    void on_select(const QModelIndex& index) override;
    void on_delete(const QModelIndex& source, const QModelIndex& selected) override;
    void on_return_no_select(const QString&) override {};
    virtual void activate(const DocumentPickerEntry& entry);
    QString get_view_stylesheet_type_name() override { return "QListView"; }
public:
    DocumentPicker(QString heading, bool fuzzy, MainWidget* pane,
        std::vector<DocumentPickerEntry> entries, std::function<void(QString)> on_done,
        std::function<bool(QString)> on_delete = nullptr, bool document_entries = true,
        QString non_document_kind = "workspaces");
    ~DocumentPicker();
    void set_live_preview(const QString& selected, std::function<void(QString)> preview,
        std::function<void()> cancel);
    void hideEvent(QHideEvent* event) override;
    bool eventFilter(QObject* object, QEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void on_config_file_changed() override;
    void on_config_file_changed(ConfigManager*) override { on_config_file_changed(); }
    std::wstring get_selected_text() override;
};

class LibrarySelector : public DocumentPicker {
    QStringList roots_;
    QHash<QString, PdfLibraryIndex::Snapshot> snapshots_;
    QTimer rescan_timer_;
    bool contents_ = false;
    QStringList all_paths_;
    PdfPickerWorker content_worker_;
    QTimer search_timer_;
    void apply_snapshots();
    void search_contents();
    bool on_text_change(const QString& text) override;
    void activate(const DocumentPickerEntry& entry) override;
public:
    LibrarySelector(bool fuzzy, std::function<void(QString)> on_done, MainWidget* pane, QString root, bool contents = false);
    void refresh();
};

class FileSelector : public DocumentPicker {
    void open_directory(const QString& path, const QString& selected = {});
    bool on_text_change(const QString& text) override;
    void activate(const DocumentPickerEntry& entry) override;
public:
    FileSelector(bool fuzzy, std::function<void(std::wstring)> on_done,
        MainWidget* parent, QString last_path);
    bool eventFilter(QObject* object, QEvent* event) override;
};

class AndroidSelector : public QWidget {
public:

    AndroidSelector(QWidget* parent);
    void resizeEvent(QResizeEvent* resize_event) override;
    //    bool event(QEvent *event) override;
private:
    //    QVBoxLayout* layout;
    //    QPushButton* fullscreen_button;
    //    QPushButton* select_text_button;
    //    QPushButton* open_document_button;
    //    QPushButton* open_prev_document_button;
    //    QPushButton* command_button;
    //    QPushButton* visual_mode_button;
    //    QPushButton* search_button;
    //    QPushButton* set_background_color;
    //    QPushButton* set_dark_mode_contrast;
    //    QPushButton* set_ruler_mode;
    //    QPushButton* restore_default_config_button;
    //    QPushButton* toggle_dark_mode_button;
    //    QPushButton* ruler_mode_bounds_config_button;
    //    QPushButton* goto_page_button;
    //    QPushButton* set_rect_config_button;
    //    QPushButton* test_rectangle_select_ui;
    TouchMainMenu* main_menu;


    MainWidget* main_widget;

};

//class TextSelectionButtons : public QWidget{
//public:
//    TextSelectionButtons(MainWidget* parent);
//    void resizeEvent(QResizeEvent* resize_event) override;
//private:

//    MainWidget* main_widget;
//    QHBoxLayout* layout;
//    QPushButton* copy_button;
//    QPushButton* search_in_scholar_button;
//    QPushButton* search_in_google_button;
//    QPushButton* highlight_button;

//};


class DrawControlsUI : public QWidget {
public:
    DrawControlsUI(MainWidget* parent);
    void resizeEvent(QResizeEvent* resize_event) override;
    TouchDrawControls* controls_ui;
private:
    MainWidget* main_widget;

};

class TouchTextSelectionButtons : public QWidget {
public:
    TouchTextSelectionButtons(MainWidget* parent);
    void resizeEvent(QResizeEvent* resize_event) override;
private:
    MainWidget* main_widget;

    TouchCopyOptions* buttons_ui;
};

class HighlightButtons : public QWidget {

public:
    HighlightButtons(MainWidget* parent);
    void resizeEvent(QResizeEvent* resize_event) override;
    TouchHighlightButtons* highlight_buttons;
private:

    MainWidget* main_widget;
    //TouchDeleteButton* delete_button;
    //QQuickWidget* buttons_widget;
    //QHBoxLayout* layout;
    //QPushButton* delete_highlight_button;
};

class SearchButtons : public QWidget {

public:
    SearchButtons(MainWidget* parent);
    void resizeEvent(QResizeEvent* resize_event) override;
private:

    MainWidget* main_widget;
    TouchSearchButtons* buttons_widget;
    //QHBoxLayout* layout;
    //QPushButton* prev_match_button;
    //QPushButton* next_match_button;
    //QPushButton* goto_initial_location_button;
};

class ConfigUI : public QWidget {
    //class ConfigUI : public QQuickWidget{
public:
    ConfigUI(std::string name, MainWidget* parent);
    void resizeEvent(QResizeEvent* resize_event) override;
    void set_should_persist(bool val);
    void on_change();

protected:
    MainWidget* main_widget;
    std::string config_name;
    bool should_persist = true;
};

class Color3ConfigUI : public ConfigUI {
public:
    Color3ConfigUI(std::string name, MainWidget* parent, float* config_location_);
    void resizeEvent(QResizeEvent* resize_event) override;

private:
    float* color_location;
    QColorDialog* color_picker;
    //    QQuickWidget* color_picker;

};

class Color4ConfigUI : public ConfigUI {
public:
    Color4ConfigUI(std::string name, MainWidget* parent, float* config_location_);

private:
    float* color_location;
    QColorDialog* color_picker;

};

class BoolConfigUI : public ConfigUI {
public:
    BoolConfigUI(std::string name, MainWidget* parent, bool* config_location, QString name_);
    void resizeEvent(QResizeEvent* resize_event) override;
private:

    bool* bool_location;
    TouchCheckbox* checkbox;
    //   	TouchCh
    //    QHBoxLayout* layout;
    //    QCheckBox* checkbox;
    //    QLabel* label;

};


//class TextConfigUI : public ConfigUI{
//public:
//	TextConfigUI(MainWidget* parent, std::wstring* config_location);
//    void resizeEvent(QResizeEvent* resize_event) override;
//private:
//    std::wstring* float_location;
//    TouchTextEdit* text_edit = nullptr;
//};

class FloatConfigUI : public ConfigUI {
public:
    FloatConfigUI(std::string name, MainWidget* parent, float* config_location, float min_value, float max_value);
    void resizeEvent(QResizeEvent* resize_event) override;
private:
    float* float_location;
    float min_value;
    float max_value;
    TouchSlider* slider = nullptr;
};

class MacroConfigUI : public ConfigUI {
public:
    MacroConfigUI(std::string name, MainWidget* parent, std::wstring* config_location, std::wstring initial_macro);
    void resizeEvent(QResizeEvent* resize_event) override;
private:
    TouchMacroEditor* macro_editor = nullptr;
};

class IntConfigUI : public ConfigUI {
public:
    IntConfigUI(std::string name, MainWidget* parent, int* config_location, int min_value, int max_value);
    void resizeEvent(QResizeEvent* resize_event) override;
private:
    int* int_location;
    int min_value;
    int max_value;
    TouchSlider* slider = nullptr;
};

class PageSelectorUI : public ConfigUI {
public:
    PageSelectorUI(MainWidget* parent, int current, int num_pages);
    void resizeEvent(QResizeEvent* resize_event) override;
private:
    TouchPageSelector* page_selector = nullptr;
};

class AudioUI : public ConfigUI {
public:
    AudioUI(MainWidget* parent);
    void resizeEvent(QResizeEvent* resize_event) override;
    TouchAudioButtons* buttons = nullptr;
};

class RectangleConfigUI : public ConfigUI {
public:
    RectangleConfigUI(std::string name, MainWidget* parent, UIRect* config_location);

    void resizeEvent(QResizeEvent* resize_event) override;
private:
    UIRect* rect_location;

    TouchRectangleSelectUI* rectangle_select_ui = nullptr;
};

class RangeConfigUI : public ConfigUI {
public:
    RangeConfigUI(std::string name, MainWidget* parent, float* top_location, float* bottom_location);

    void resizeEvent(QResizeEvent* resize_event) override;
private:
    float* top_location;
    float* bottom_location;

    TouchRangeSelectUI* range_select_ui = nullptr;
};


std::wstring select_document_file_name(std::optional<QString> root_dir={});
std::wstring select_json_file_name(std::optional<QString> root_dir={});
std::wstring select_any_file_name(std::optional<QString> root_dir={});
std::wstring select_command_file_name(std::string command_name, std::optional<QString> root_dir={});
std::wstring select_new_json_file_name(std::optional<QString> root_dir={});
std::wstring select_new_pdf_file_name(std::optional<QString> root_dir={});
std::wstring select_command_folder_name(std::optional<QString> root_dir={});

//QWidget* color3_configurator_ui(MainWidget* main_widget, void* location);
//QWidget* color4_configurator_ui(MainWidget* main_widget, void* location);

//template<float min_value, float max_value>
//QWidget* float_configurator_ui(MainWidget* main_widget, void* location){
//    return new FloatConfigUI(main_widget, (float*)location, min_value, max_value);
//}

//template<QString name>
//QWidget* bool_configurator_ui(MainWidget* main_widget, void* location){
//    return new BoolConfigUI(main_widget, (float*)location, name);
//}
