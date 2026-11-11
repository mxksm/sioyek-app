#include "qt_compat.h"
#include "blank_canvas.h"
#include "ui.h"
#include <qfiledialog.h>

#include <QItemSelectionModel>
#include <QTapGesture>
#include <main_widget.h>
#include "document_view.h"

#include <QQuickView>
#include <QQmlComponent>
#include <QQuickItem>
#include <QQmlEngine>
#include <QQuickWidget>
#include <QFile>
#include <QStyledItemDelegate>
#include <QSignalBlocker>
#include <QShowEvent>
#include <QClipboard>
#include <QTextBoundaryFinder>
#include <QTextBlock>
#include <QDirIterator>
#include <QCryptographicHash>
#include <QStandardPaths>
#include "touchui/TouchSlider.h"
#include "touchui/TouchConfigMenu.h"
#include "touchui/TouchSettings.h"

extern std::wstring DEFAULT_OPEN_FILE_PATH;
extern float DARK_MODE_CONTRAST;
extern float BACKGROUND_COLOR[3];
extern bool RULER_MODE;
extern float VISUAL_MARK_NEXT_PAGE_FRACTION;
extern float VISUAL_MARK_NEXT_PAGE_THRESHOLD;
extern float HIGHLIGHT_COLORS[26 * 3];
extern float TTS_RATE;
extern float MENU_SCREEN_WDITH_RATIO;
extern bool SHOW_MOST_RECENT_COMMANDS_FIRST;

std::wstring select_command_file_name(std::string command_name, std::optional<QString> root_dir) {
    if (command_name == "open_document" || command_name == "open_document_in_directory") {
        return select_document_file_name(root_dir);
    }
    else if (command_name == "source_config") {
        return select_any_file_name(root_dir);
    }
    else {
        return select_any_file_name(root_dir);
    }
}

std::wstring select_command_folder_name(std::optional<QString> root_dir) {
    QString dir_name = QFileDialog::getExistingDirectory(nullptr, "Select Folder", root_dir.value_or(QString()));
    return dir_name.toStdWString();
}

std::wstring select_document_file_name(std::optional<QString> root_dir) {
    if (DEFAULT_OPEN_FILE_PATH.size() == 0) {

        QString file_name = QFileDialog::getOpenFileName(nullptr, "Select Document", root_dir.value_or(""), "Documents (*.pdf *.epub *.cbz)");
        return file_name.toStdWString();
    }
    else {

        QFileDialog fd = QFileDialog(nullptr, "Select Document", root_dir.value_or(""), "Documents (*.pdf *.epub *.cbz)");
        fd.setDirectory(QString::fromStdWString(DEFAULT_OPEN_FILE_PATH));
        if (fd.exec()) {

            QString file_name = fd.selectedFiles().first();
            return file_name.toStdWString();
        }
        else {
            return L"";
        }
    }

}

std::wstring select_json_file_name(std::optional<QString> root_dir) {
    QString file_name = QFileDialog::getSaveFileName(nullptr, "Select Document", root_dir.value_or(""), "Documents (*.json )");
    return file_name.toStdWString();
}

std::wstring select_any_file_name(std::optional<QString> root_dir) {
    QString file_name = QFileDialog::getSaveFileName(nullptr, "Select File", root_dir.value_or(""), "Any (*)");
    return file_name.toStdWString();
}

std::wstring select_new_json_file_name(std::optional<QString> root_dir) {
    QString file_name = QFileDialog::getSaveFileName(nullptr, "Select Document", root_dir.value_or(""), "Documents (*.json )");
    return file_name.toStdWString();
}

std::wstring select_new_pdf_file_name(std::optional<QString> root_dir) {
    QString file_name = QFileDialog::getSaveFileName(nullptr, "Select Document", root_dir.value_or(""), "Documents (*.pdf )");
    return file_name.toStdWString();
}


std::vector<ConfigFileChangeListener*> ConfigFileChangeListener::registered_listeners;

ConfigFileChangeListener::ConfigFileChangeListener() {
    registered_listeners.push_back(this);
}

ConfigFileChangeListener::~ConfigFileChangeListener() {
    registered_listeners.erase(std::find(registered_listeners.begin(), registered_listeners.end(), this));
}

void ConfigFileChangeListener::notify_config_file_changed(ConfigManager* new_config_manager) {
    for (auto* it : ConfigFileChangeListener::registered_listeners) {
        it->on_config_file_changed(new_config_manager);
    }
}

bool HierarchialSortFilterProxyModel::filterAcceptsRow(int source_row, const QModelIndex& source_parent) const
{
    // custom behaviour :
#ifdef SIOYEK_QT6
    if (filterRegularExpression().pattern().size() == 0)
#else
    if (filterRegExp().isEmpty() == false)
#endif
    {
        // get source-model index for current row
        QModelIndex source_index = sourceModel()->index(source_row, this->filterKeyColumn(), source_parent);
        if (source_index.isValid())
        {
            // check current index itself :
            QString key = sourceModel()->data(source_index, filterRole()).toString();

#ifdef SIOYEK_QT6
            bool parent_contains = key.contains(filterRegularExpression());
#else
            bool parent_contains = key.contains(filterRegExp());
#endif

            if (parent_contains) return true;

            // if any of children matches the filter, then current index matches the filter as well
            int i, nb = sourceModel()->rowCount(source_index);
            for (i = 0; i < nb; ++i)
            {
                if (filterAcceptsRow(i, source_index))
                {
                    return true;
                }
            }
            return false;
        }
    }
    // parent call for initial behaviour
    return QSortFilterProxyModel::filterAcceptsRow(source_row, source_parent);
}

AndroidSelector::AndroidSelector(QWidget* parent) : QWidget(parent) {
    //     layout = new QVBoxLayout();

    main_widget = dynamic_cast<MainWidget*>(parent);
    int current_colorscheme_index = main_widget->get_current_colorscheme_index();
    bool horizontal_locked = main_widget->horizontal_scroll_locked;
    bool fullscreen = main_widget->isFullScreen();
    bool ruler = main_widget->is_visual_mark_mode();
    bool speaking = main_widget->is_reading;
    bool portaling = main_widget->is_pending_link_source_filled();
    bool fit_mode = main_widget->last_smart_fit_page.has_value();

    main_menu = new TouchMainMenu(fit_mode, portaling, fullscreen, ruler, speaking, horizontal_locked, current_colorscheme_index, this);


    //    set_rect_config_button = new QPushButton("Rect Config", this);
    //    goto_page_button = new QPushButton("Goto Page", this);
    //    fullscreen_button = new QPushButton("Fullscreen", this);
    //    select_text_button = new QPushButton("Select Text", this);
    //    open_document_button = new QPushButton("Open New Document", this);
    //    open_prev_document_button = new QPushButton("Open Previous Document", this);
    //    command_button = new QPushButton("Command", this);
    //    visual_mode_button = new QPushButton("Visual Mark Mode", this);
    //    search_button = new QPushButton("Search", this);
    //    set_background_color = new QPushButton("Background Color", this);
    //    set_dark_mode_contrast = new QPushButton("Dark Mode Contrast", this);
    //    set_ruler_mode = new QPushButton("Ruler Mode", this);
    //    restore_default_config_button = new QPushButton("Restore Default Config", this);
    //    toggle_dark_mode_button = new QPushButton("Toggle Dark Mode", this);
    //    ruler_mode_bounds_config_button = new QPushButton("Configure Ruler Mode", this);
    //    test_rectangle_select_ui = new QPushButton("Rectangle Select", this);

    //    layout->addWidget(set_rect_config_button);
    //    layout->addWidget(goto_page_button);
    //    layout->addWidget(fullscreen_button);
    //    layout->addWidget(select_text_button);
    //    layout->addWidget(open_document_button);
    //    layout->addWidget(open_prev_document_button);
    //    layout->addWidget(command_button);
    //    layout->addWidget(visual_mode_button);
    //    layout->addWidget(search_button);
    //    layout->addWidget(set_background_color);
    //    layout->addWidget(set_dark_mode_contrast);
    //    layout->addWidget(set_ruler_mode);
    //    layout->addWidget(restore_default_config_button);
    //    layout->addWidget(toggle_dark_mode_button);
    //    layout->addWidget(ruler_mode_bounds_config_button);
    //    layout->addWidget(test_rectangle_select_ui);


    QObject::connect(main_menu, &TouchMainMenu::fullscreenClicked, [&]() {
        //main_widget->current_widget = {};
        assert(main_widget->current_widget_stack.back() == this);
        main_widget->pop_current_widget();
        //deleteLater();
        main_widget->toggle_fullscreen();
        });

    QObject::connect(main_menu, &TouchMainMenu::selectTextClicked, [&]() {
        //main_widget->current_widget = {};
        //deleteLater();
        assert(main_widget->current_widget_stack.back() == this);
        main_widget->pop_current_widget();
        main_widget->handle_mobile_selection();
        main_widget->invalidate_render();
        });

    QObject::connect(main_menu, &TouchMainMenu::openNewDocClicked, [&]() {
        //main_widget->current_widget = {};
        //deleteLater();
        assert(main_widget->current_widget_stack.back() == this);
        main_widget->pop_current_widget();
        main_widget->run_command_with_name("open_document");
        });

    QObject::connect(main_menu, &TouchMainMenu::openPrevDocClicked, [&]() {
        main_widget->run_command_with_name("open_prev_doc", true);
        });

    QObject::connect(main_menu, &TouchMainMenu::highlightsClicked, [&]() {
        main_widget->run_command_with_name("goto_highlight", true);
        });

    QObject::connect(main_menu, &TouchMainMenu::tocClicked, [&]() {
        main_widget->run_command_with_name("goto_toc", true);
        });

    QObject::connect(main_menu, &TouchMainMenu::bookmarksClicked, [&]() {
        main_widget->run_command_with_name("goto_bookmark", true);
        });

    QObject::connect(main_menu, &TouchMainMenu::hintClicked, [&]() {
        //main_widget->run_command_with_name("toggle_rect_hints", true);

        main_widget->toggle_scratchpad_mode();
        main_widget->pop_current_widget();

        main_widget->invalidate_render();

        });

    QObject::connect(main_menu, &TouchMainMenu::commandsClicked, [&]() {
        main_widget->run_command_with_name("command", true);
        });

    QObject::connect(main_menu, &TouchMainMenu::drawingModeClicked, [&]() {
        main_widget->run_command_with_name("toggle_freehand_drawing_mode", true);
        });

    QObject::connect(main_menu, &TouchMainMenu::settingsClicked, [&]() {
        //TouchConfigMenu* config_menu = new TouchConfigMenu(main_widget);
        main_widget->show_touch_settings_menu();

        //TouchSettings* config_menu = new TouchSettings(main_widget);
        //assert(main_widget->current_widget_stack.back() == this);
        //main_widget->pop_current_widget();

        //main_widget->set_current_widget(config_menu);
        //main_widget->show_current_widget();

        });

    QObject::connect(main_menu, &TouchMainMenu::rulerModeClicked, [&]() {
        main_widget->android_handle_visual_mode();
        //main_widget->current_widget = {};
        //deleteLater();
        assert(main_widget->current_widget_stack.back() == this);
        //main_widget->pop_current_widget();
        main_widget->invalidate_render();
        });

    QObject::connect(main_menu, &TouchMainMenu::searchClicked, [&]() {
        main_widget->run_command_with_name("search", true);
        });

    QObject::connect(main_menu, &TouchMainMenu::addBookmarkClicked, [&]() {
        main_widget->run_command_with_name("add_bookmark", true);
        });

    QObject::connect(main_menu, &TouchMainMenu::portalClicked, [&]() {
        main_widget->run_command_with_name("portal", true);
        });

    QObject::connect(main_menu, &TouchMainMenu::deletePortalClicked, [&]() {
        main_widget->run_command_with_name("delete_portal", true);
        });

    QObject::connect(main_menu, &TouchMainMenu::globalBookmarksClicked, [&]() {
        main_widget->run_command_with_name("goto_bookmark_g", true);
        });

    QObject::connect(main_menu, &TouchMainMenu::globalHighlightsClicked, [&]() {
        main_widget->run_command_with_name("goto_highlight_g", true);
        });

    QObject::connect(main_menu, &TouchMainMenu::ttsClicked, [&]() {
        main_widget->run_command_with_name("start_reading", true);
        });

    QObject::connect(main_menu, &TouchMainMenu::horizontalLockClicked, [&]() {
        main_widget->run_command_with_name("toggle_horizontal_scroll_lock");
        });

    //    QObject::connect(set_background_color, &QPushButton::pressed, [&](){

    //        auto command = main_widget->command_manager->get_command_with_name("setconfig_background_color");
    //        main_widget->current_widget = {};
    //        deleteLater();
    //        main_widget->handle_command_types(std::move(command), 0);
    //    });

    //    QObject::connect(set_rect_config_button, &QPushButton::pressed, [&](){

    //        auto command = main_widget->command_manager->get_command_with_name("setconfig_portrait_back_ui_rect");
    //        main_widget->current_widget = {};
    //        deleteLater();
    //        main_widget->handle_command_types(std::move(command), 0);
    //    });

    //    QObject::connect(set_dark_mode_contrast, &QPushButton::pressed, [&](){

    //        main_widget->current_widget = {};
    //        deleteLater();

    //        main_widget->current_widget = new FloatConfigUI(main_widget, &DARK_MODE_CONTRAST, 0.0f, 1.0f);
    //        main_widget->current_widget->show();
    //    });

    //    QObject::connect(set_ruler_mode, &QPushButton::pressed, [&](){

    //        auto command = main_widget->command_manager->get_command_with_name("setconfig_ruler_mode");
    //        main_widget->current_widget = {};
    //        deleteLater();
    //        main_widget->handle_command_types(std::move(command), 0);

    //    });

    //    QObject::connect(restore_default_config_button, &QPushButton::pressed, [&](){

    //        main_widget->current_widget = {};
    //        deleteLater();
    //        main_widget->restore_default_config();

    //    });

    QObject::connect(main_menu, &TouchMainMenu::darkColorschemeClicked, [&]() {
        main_widget->set_dark_mode();
        //main_widget->current_widget = {};
        //deleteLater();
        assert(main_widget->current_widget_stack.back() == this);
        //main_widget->pop_current_widget();
        main_widget->invalidate_render();
        });

    QObject::connect(main_menu, &TouchMainMenu::lightColorschemeClicked, [&]() {
        main_widget->set_light_mode();
        //main_widget->current_widget = {};
        //deleteLater();
        assert(main_widget->current_widget_stack.back() == this);
        //main_widget->pop_current_widget();
        main_widget->invalidate_render();
        });

    QObject::connect(main_menu, &TouchMainMenu::customColorschemeClicked, [&]() {
        main_widget->set_custom_color_mode();
        //main_widget->current_widget = {};
        //deleteLater();
        assert(main_widget->current_widget_stack.back() == this);
        //main_widget->pop_current_widget();
        main_widget->invalidate_render();
        });

    QObject::connect(main_menu, &TouchMainMenu::downloadPaperClicked, [&]() {
        main_widget->pop_current_widget();
        main_widget->download_paper_under_cursor(true);
        main_widget->invalidate_render();
        });


    QObject::connect(main_menu, &TouchMainMenu::fitToPageWidthClicked, [&]() {

        if (main_widget->last_smart_fit_page) {
            main_widget->last_smart_fit_page = {};
            //main_widget->pop_current_widget();
            main_widget->invalidate_render();
        }
        else {
            //main_widget->main_document_view->fit_to_page_width(true);
            main_widget->handle_fit_to_page_width(true);
            int current_page = main_widget->get_current_page_number();
            main_widget->last_smart_fit_page = current_page;

            //main_widget->pop_current_widget();
            main_widget->invalidate_render();
        }
        });
    //    QObject::connect(ruler_mode_bounds_config_button, &QPushButton::pressed, [&](){
    ////        auto command = main_widget->command_manager->get_command_with_name("toggle_dark_mode");
    //        main_widget->current_widget = nullptr;
    //        deleteLater();
    //        RangeConfigUI* config_ui = new RangeConfigUI(main_widget, &VISUAL_MARK_NEXT_PAGE_FRACTION, &VISUAL_MARK_NEXT_PAGE_THRESHOLD);
    //        main_widget->current_widget = config_ui;
    //        main_widget->current_widget->show();
    ////        main_widget->handle_command_types(std::move(command), 0);
    //    });

    QObject::connect(main_menu, &TouchMainMenu::gotoPageClicked, [&]() {
        //main_widget->current_widget = nullptr;
        //deleteLater();
        assert(main_widget->current_widget_stack.back() == this);
        main_widget->pop_current_widget();
        PageSelectorUI* page_ui = new PageSelectorUI(main_widget,
            main_widget->get_current_page_number(),
            main_widget->current_document_page_count());

        main_widget->set_current_widget(page_ui);
        main_widget->show_current_widget();
        //main_widget->current_widget = page_ui;
        //main_widget->current_widget->show();
        });

    //    QObject::connect(test_rectangle_select_ui, &QPushButton::pressed, [&](){
    ////        auto command = main_widget->command_manager->get_command_with_name("toggle_dark_mode");
    ////        main_widget->current_widget = {};
    //        RectangleConfigUI* configui = new RectangleConfigUI(main_widget, &testrect);
    //        main_widget->current_widget = configui;
    //        configui->show();

    //        deleteLater();
    ////        main_widget->handle_command_types(std::move(command), 0);
    //    });

    //     layout->insertStretch(-1, 1);

    //     this->setLayout(layout);
}

void AndroidSelector::resizeEvent(QResizeEvent* resize_event) {
    QWidget::resizeEvent(resize_event);
    int parent_width = parentWidget()->width();
    int parent_height = parentWidget()->height();

    //float parent_width_in_centimeters = static_cast<float>(parent_width) / logicalDpiX() * 2.54f;
    //float parent_height_in_centimeters = static_cast<float>(parent_height) / logicalDpiY() * 2.54f;
    int ten_cm = static_cast<int>(12 * logicalDpiX() / 2.54f);

    int w = static_cast<int>(parent_width * 0.9f);
    int h = parent_height;

    w = std::min(w, ten_cm);
    h = std::min(h, ten_cm);

    main_menu->resize(w, h);
    setFixedSize(w, h);

    //    list_view->setFixedSize(parent_width * 0.9f, parent_height);
    move((parent_width - w) / 2, (parent_height - h) / 2);

}

//TextSelectionButtons::TextSelectionButtons(MainWidget* parent) : QWidget(parent) {

//    QHBoxLayout* layout = new QHBoxLayout();

//    main_widget = parent;
//    copy_button = new QPushButton("Copy");
//    search_in_scholar_button = new QPushButton("Search Scholar");
//    search_in_google_button = new QPushButton("Search Google");
//    highlight_button = new QPushButton("Highlight");

//    QObject::connect(copy_button, &QPushButton::clicked, [&](){
//        copy_to_clipboard(main_widget->selected_text);
//    });

//    QObject::connect(search_in_scholar_button, &QPushButton::clicked, [&](){
//        search_custom_engine(main_widget->selected_text, L"https://scholar.google.com/scholar?&q=");
//    });

//    QObject::connect(search_in_google_button, &QPushButton::clicked, [&](){
//        search_custom_engine(main_widget->selected_text, L"https://www.google.com/search?q=");
//    });

//    QObject::connect(highlight_button, &QPushButton::clicked, [&](){
//        main_widget->handle_touch_highlight();
//    });

//    layout->addWidget(copy_button);
//    layout->addWidget(search_in_scholar_button);
//    layout->addWidget(search_in_google_button);
//    layout->addWidget(highlight_button);

//    this->setLayout(layout);
//}

//void TextSelectionButtons::resizeEvent(QResizeEvent* resize_event) {
//    QWidget::resizeEvent(resize_event);
//    int parent_width = parentWidget()->width();
//    int parent_height = parentWidget()->height();

//    setFixedSize(parent_width, parent_height / 5);
////    list_view->setFixedSize(parent_width * 0.9f, parent_height);
//    move(0, 0);

//}

TouchTextSelectionButtons::TouchTextSelectionButtons(MainWidget* parent) : QWidget(parent) {
    main_widget = parent;
    buttons_ui = new TouchCopyOptions(this);
    this->setAttribute(Qt::WA_NoMousePropagation);

    QObject::connect(buttons_ui, &TouchCopyOptions::copyClicked, [&]() {
        copy_to_clipboard(main_widget->get_selected_text());
        main_widget->clear_selection_indicators();
        });

    QObject::connect(buttons_ui, &TouchCopyOptions::searchClicked, [&]() {
        main_widget->perform_search(main_widget->get_selected_text(), false);
        main_widget->show_search_buttons();
        main_widget->clear_selection_indicators();
        });

    QObject::connect(buttons_ui, &TouchCopyOptions::scholarClicked, [&]() {
        search_custom_engine(main_widget->get_selected_text(), L"https://scholar.google.com/scholar?&q=");
        main_widget->clear_selection_indicators();
        });

    QObject::connect(buttons_ui, &TouchCopyOptions::googleClicked, [&]() {
        search_custom_engine(main_widget->get_selected_text(), L"https://www.google.com/search?q=");
        main_widget->clear_selection_indicators();
        });

    QObject::connect(buttons_ui, &TouchCopyOptions::highlightClicked, [&]() {
        main_widget->handle_touch_highlight();
        main_widget->clear_selection_indicators();
        });

    QObject::connect(buttons_ui, &TouchCopyOptions::downloadClicked, [&]() {
        main_widget->download_selected_text();
        main_widget->clear_selection_indicators();
        });

    QObject::connect(buttons_ui, &TouchCopyOptions::smartJumpClicked, [&]() {
        main_widget->smart_jump_to_selected_text();
        main_widget->clear_selection_indicators();
        });
}

DrawControlsUI::DrawControlsUI(MainWidget* parent) : QWidget(parent) {
    main_widget = parent;
    controls_ui = new TouchDrawControls(parent->freehand_thickness, parent->get_current_freehand_type(), this);
    this->setAttribute(Qt::WA_NoMousePropagation);

    QObject::connect(controls_ui, &TouchDrawControls::exitDrawModePressed, [&]() {
        main_widget->exit_freehand_drawing_mode();
        });

    QObject::connect(controls_ui, &TouchDrawControls::changeColorPressed, [&](int color_index) {
        main_widget->current_freehand_type = 'a' + color_index;
        main_widget->update_drawing_cursor();
        });

    QObject::connect(controls_ui, &TouchDrawControls::enablePenDrawModePressed, [&]() {
        main_widget->set_pen_drawing_mode(true);
        });

    QObject::connect(controls_ui, &TouchDrawControls::screenshotPressed, [&]() {
        if (main_widget->is_scratchpad_mode()) {
            main_widget->run_command_with_name("copy_drawings_from_scratchpad");
        }
        else {
            main_widget->run_command_with_name("copy_screenshot_to_scratchpad");
        }
        });

    QObject::connect(controls_ui, &TouchDrawControls::saveScratchpadPressed, [&]() {
        main_widget->run_command_with_name("save_scratchpad");
        main_widget->invalidate_render();
        });

    QObject::connect(controls_ui, &TouchDrawControls::loadScratchpadPressed, [&]() {
        main_widget->run_command_with_name("load_scratchpad");
        main_widget->invalidate_render();
        });

    QObject::connect(controls_ui, &TouchDrawControls::movePressed, [&]() {
        main_widget->run_command_with_name("select_freehand_drawings");
        main_widget->invalidate_render();
        });

    QObject::connect(controls_ui, &TouchDrawControls::toggleScratchpadPressed, [&]() {
        main_widget->run_command_with_name("toggle_scratchpad_mode");
        main_widget->invalidate_render();
        });

    QObject::connect(controls_ui, &TouchDrawControls::disablePenDrawModePressed, [&]() {
        main_widget->set_hand_drawing_mode(true);
        });

    QObject::connect(controls_ui, &TouchDrawControls::eraserPressed, [&]() {
        main_widget->run_command_with_name("delete_freehand_drawings");
        });

    QObject::connect(controls_ui, &TouchDrawControls::penSizeChanged, [&](qreal val) {
        main_widget->set_freehand_thickness(val);
        });


}
void DrawControlsUI::resizeEvent(QResizeEvent* resize_event) {
    QWidget::resizeEvent(resize_event);

    int pwidth = parentWidget()->width();
    int width = parentWidget()->width() * 3 / 4;
    int height = std::max(parentWidget()->height() / 16, 50);

    controls_ui->move(0, 0);
    controls_ui->resize(width, height);
    move((pwidth - width) / 2, height);
    setFixedSize(width, height);

}

void TouchTextSelectionButtons::resizeEvent(QResizeEvent* resize_event) {
    QWidget::resizeEvent(resize_event);

    int pwidth = parentWidget()->width();
    int width = parentWidget()->width() * 3 / 4;
    int height = parentWidget()->height() / 16;

    buttons_ui->move(0, 0);
    buttons_ui->resize(width, height);
    move((pwidth - width) / 2, height);
    //    setFixedSize(0, 0);
    setFixedSize(width, height);

}

HighlightButtons::HighlightButtons(MainWidget* parent) : QWidget(parent) {
    main_widget = parent;
    //layout = new QHBoxLayout();

    //delete_highlight_button = new QPushButton("Delete");
    //buttons_widget = new 
    highlight_buttons = new TouchHighlightButtons(main_widget->get_current_selected_highlight_type(), this);

    QObject::connect(highlight_buttons, &TouchHighlightButtons::deletePressed, [&]() {
        main_widget->handle_delete_selected_highlight();
        hide();
        });

    QObject::connect(highlight_buttons, &TouchHighlightButtons::editPressed, [&]() {
        main_widget->run_command_with_name("edit_selected_highlight");
        //main_widget->handle_delete_selected_highlight();
        hide();
        });

    QObject::connect(highlight_buttons, &TouchHighlightButtons::changeColorPressed, [&](int index) {
        //float* color = &HIGHLIGHT_COLORS[3 * index];

        //main_widget->handle_delete_selected_highlight();
        if (index < 26) {
            main_widget->change_selected_highlight_type('a' + index);
        }
        else {
            main_widget->change_selected_highlight_type('A' + index - 26);
        }
        hide();
        main_widget->invalidate_render();
        //main_widget->highlight_buttons = nullptr;
        //deleteLater();
        });

    //layout->addWidget(delete_highlight_button);
    //this->setLayout(layout);
}

void HighlightButtons::resizeEvent(QResizeEvent* resize_event) {

    QWidget::resizeEvent(resize_event);

    int parent_width = parentWidget()->width();
    int parent_height = parentWidget()->height();

    int dpi = physicalDpiY();

    //int w = static_cast<int>(parent_width / 5);
    int w = parent_width;
    int h = static_cast<int>(static_cast<float>(dpi) / 2.54f);
    w = std::max(w, h * 6);

    setFixedSize(w, h);
    highlight_buttons->resize(w, h);
    move((parent_width - w) / 2, parent_height / 5);
}


SearchButtons::SearchButtons(MainWidget* parent) : QWidget(parent) {
    main_widget = parent;
    //layout = new QHBoxLayout(this);

//    delete_highlight_button = new QPushButton("Delete");
//    QObject::connect(delete_highlight_button, &QPushButton::clicked, [&](){
//        main_widget->handle_delete_selected_highlight();
//        hide();
//        main_widget->highlight_buttons = nullptr;
//        deleteLater();
//    });
    //next_match_button = new QPushButton("next");
    //prev_match_button = new QPushButton("prev");
    //goto_initial_location_button = new QPushButton("initial");
    buttons_widget = new TouchSearchButtons(this);

    QObject::connect(buttons_widget, &TouchSearchButtons::nextPressed, [&]() {
        //main_widget->opengl_widget->goto_search_result(1);
        main_widget->run_command_with_name("next_item");
        main_widget->validate_render();
        });

    QObject::connect(buttons_widget, &TouchSearchButtons::previousPressed, [&]() {
        main_widget->run_command_with_name("previous_item");
        //main_widget->opengl_widget->goto_search_result(-1);
        main_widget->validate_render();
        });

    QObject::connect(buttons_widget, &TouchSearchButtons::initialPressed, [&]() {
        main_widget->goto_mark('/');
        main_widget->invalidate_render();
        });


    //layout->addWidget(next_match_button);
    //layout->addWidget(goto_initial_location_button);
    //layout->addWidget(prev_match_button);

    //this->setLayout(layout);
}

void SearchButtons::resizeEvent(QResizeEvent* resize_event) {

    QWidget::resizeEvent(resize_event);
    int parent_width = parentWidget()->width();
    int parent_height = parentWidget()->height();

    int width = 2 * parentWidget()->width() / 3;
    int height = parentWidget()->height() / 12;


    buttons_widget->resize(width, height);
    setFixedSize(width, height);
    //layout->update();
    move((parent_width - width) / 2, parent_height - 3 * height / 2);
}

//ConfigUI::ConfigUI(MainWidget* parent) : QQuickWidget(parent){
ConfigUI::ConfigUI(std::string name, MainWidget* parent) : QWidget(parent) {
    main_widget = parent;
    config_name = name;
}

void ConfigUI::on_change() {
    main_widget->on_config_changed(config_name);
}

void ConfigUI::set_should_persist(bool val) {
    this->should_persist = val;
}

void ConfigUI::resizeEvent(QResizeEvent* resize_event) {
    QWidget::resizeEvent(resize_event);
    int parent_width = parentWidget()->width();
    int parent_height = parentWidget()->height();

    setFixedSize(2 * parent_width / 3, parent_height / 2);
    move(parent_width / 6, parent_height / 4);
}

Color3ConfigUI::Color3ConfigUI(std::string name, MainWidget* parent, float* config_location_) : ConfigUI(name, parent) {
    color_location = config_location_;
    color_picker = new QColorDialog(this);
    color_picker->show();

    connect(color_picker, &QColorDialog::colorSelected, [&](const QColor& color) {
        convert_qcolor_to_float3(color, color_location);
        main_widget->invalidate_render();
        on_change();

        if (should_persist) {
            main_widget->persist_config();
        }
        });

    //    QQmlEngine* engine = new QQmlEngine();
    //    QQmlComponent* component = new QQmlComponent(engine, QUrl("qrc:/pdf_viewer/qml/MyColorPicker.qml"), this);
    //    QObject *object = component->create();
    //    QQuickItem *item = qobject_cast<QQuickItem*>(object);

    //    QFile source_file("qrc:/pdf_viewer/qml/MyColorPicker.qml");
    //    QString source = source_file.readAll();

    //    QQuickWidget* quick_widget(source, this);
    //    QUrl url("qrc:/pdf_viewer/qml/MyColorPicker.qml");
    //    color_picker = new QQuickWidget(url, this);
    //    color_picker->setSizePolicy(QSizePolicy::Policy::Maximum, QSizePolicy::Policy::Maximum);
    //    color_picker->show();
    //    TouchSlider* slider = new TouchSlider(0, 100, 25, this);
    //    QObject::connect(slider, &TouchSlider::itemSelected, [&](int selected_value){
    //        qDebug() << "selected " << selected_value << "\n";

    //    });


    //    QObject::connect(color_picker, SIGNAL()

    //    QQuickWidget
    //    QQuickView* view = new QQuickView();
    //    view->setSource(QUrl("qrc:/pdf_viewer/qml/MyColorPicker.qml"));
    //    view->show();


}

void Color3ConfigUI::resizeEvent(QResizeEvent* resize_event) {
    QWidget::resizeEvent(resize_event);
    int parent_width = parentWidget()->width();
    int parent_height = parentWidget()->height();

    setFixedSize(2 * parent_width / 3, parent_height / 2);
    color_picker->resize(width(), height());

    move(parent_width / 6, parent_height / 4);
}

Color4ConfigUI::Color4ConfigUI(std::string name, MainWidget* parent, float* config_location_) : ConfigUI(name, parent) {
    color_location = config_location_;
    color_picker = new QColorDialog(this);
    color_picker->show();

    connect(color_picker, &QColorDialog::colorSelected, [&](const QColor& color) {
        convert_qcolor_to_float4(color, color_location);
        main_widget->invalidate_render();
        on_change();
        if (should_persist) {
            main_widget->persist_config();
        }
        });
}

MacroConfigUI::MacroConfigUI(std::string name, MainWidget* parent, std::wstring* config_location, std::wstring initial_macro) : ConfigUI(name, parent) {

    macro_editor = new TouchMacroEditor(utf8_encode(initial_macro), this, parent);

    connect(macro_editor, &TouchMacroEditor::macroConfirmed, [&, config_location](std::string macro) {
        //convert_qcolor_to_float4(color, color_location);
        (*config_location) = utf8_decode(macro);
        on_change();
        if (should_persist) {
            main_widget->persist_config();
        }
        main_widget->pop_current_widget();
        });

    QObject::connect(macro_editor, &TouchMacroEditor::canceled, [&]() {
        main_widget->pop_current_widget();
        });


}

FloatConfigUI::FloatConfigUI(std::string name, MainWidget* parent, float* config_location, float min_value_, float max_value_) : ConfigUI(name, parent) {

    min_value = min_value_;
    max_value = max_value_;
    float_location = config_location;

    int current_value = static_cast<int>((*config_location - min_value) / (max_value - min_value) * 100);
    slider = new TouchSlider(0, 100, current_value, this);
    QObject::connect(slider, &TouchSlider::itemSelected, [&](int val) {
        float value = min_value + (static_cast<float>(val) / 100.0f) * (max_value - min_value);
        *float_location = value;
        on_change();
        main_widget->invalidate_render();
        main_widget->pop_current_widget();
        });

    QObject::connect(slider, &TouchSlider::canceled, [&]() {
        main_widget->pop_current_widget();
        });

}


IntConfigUI::IntConfigUI(std::string name, MainWidget* parent, int* config_location, int min_value_, int max_value_) : ConfigUI(name, parent) {

    min_value = min_value_;
    max_value = max_value_;
    int_location = config_location;

    int current_value = static_cast<int>((*config_location - min_value) / (max_value - min_value) * 100);
    slider = new TouchSlider(min_value, max_value, current_value, this);
    QObject::connect(slider, &TouchSlider::itemSelected, [&](int val) {
        *int_location = val;
        on_change();
        main_widget->invalidate_render();
        main_widget->pop_current_widget();
        });

    QObject::connect(slider, &TouchSlider::canceled, [&]() {
        main_widget->pop_current_widget();
        });

}


PageSelectorUI::PageSelectorUI(MainWidget* parent, int current, int num_pages) : ConfigUI("", parent) {

    page_selector = new TouchPageSelector(0, num_pages - 1, current, this);

    QObject::connect(page_selector, &TouchPageSelector::pageSelected, [&](int val) {
        main_widget->goto_page_with_page_number(val);
        main_widget->invalidate_render();
        });

}

void PageSelectorUI::resizeEvent(QResizeEvent* resize_event) {
    QWidget::resizeEvent(resize_event);
    int parent_width = parentWidget()->width();
    int parent_height = parentWidget()->height();

    int w = 2 * parent_width / 3;
    int h = parent_height / 6;
    page_selector->resize(w, h);

    setFixedSize(w, h);
    move((parent_width - w) / 2, parent_height - 2 * h);
}

AudioUI::AudioUI(MainWidget* parent) : ConfigUI("", parent) {

    buttons = new TouchAudioButtons(this);
    buttons->set_rate(TTS_RATE);

    QObject::connect(buttons, &TouchAudioButtons::playPressed, [&]() {
        //main_widget->main_document_view->goto_page(val);
        //main_widget->invalidate_render();
        main_widget->handle_play();
        });

    QObject::connect(buttons, &TouchAudioButtons::stopPressed, [&]() {
        //main_widget->main_document_view->goto_page(val);
        //main_widget->invalidate_render();
        main_widget->handle_stop_reading();
        });

    QObject::connect(buttons, &TouchAudioButtons::pausePressed, [&]() {
        //main_widget->main_document_view->goto_page(val);
        //main_widget->invalidate_render();
        main_widget->handle_pause();
        });


    QObject::connect(buttons, &TouchAudioButtons::rateChanged, [&](qreal rate) {
        TTS_RATE = rate;
        buttons->set_rate(TTS_RATE);
        main_widget->handle_pause();
        main_widget->handle_play();
        });
    //QObject::connect(buttons, &TouchAudioButtons::speedIncreasePressed, [&](){
    //    TTS_RATE += 0.1;
    //    if (TTS_RATE > 1.0f) {
    //        TTS_RATE = 1.0f;
    //    }
    //    buttons->set_rate(TTS_RATE);
    //});

    //QObject::connect(buttons, &TouchAudioButtons::speedDecreasePressed, [&](){
    //    TTS_RATE -= 0.1;
    //    if (TTS_RATE < -1.0f) {
    //        TTS_RATE = -1.0f;
    //    }
    //    buttons->set_rate(TTS_RATE);
    //});
}

void AudioUI::resizeEvent(QResizeEvent* resize_event) {
    QWidget::resizeEvent(resize_event);
    int parent_width = parentWidget()->width();
    int parent_height = parentWidget()->height();

    int w = parent_width;
    int h = parent_height / 6;
    buttons->resize(w, h);

    setFixedSize(w, h);
    move((parent_width - w) / 2, parent_height - h);
}



BoolConfigUI::BoolConfigUI(std::string name_, MainWidget* parent, bool* config_location, QString qname) : ConfigUI(name_, parent) {
    bool_location = config_location;

    checkbox = new TouchCheckbox(qname, *config_location, this);
    QObject::connect(checkbox, &TouchCheckbox::itemSelected, [&](bool new_state) {
        *bool_location = static_cast<bool>(new_state);
        main_widget->invalidate_render();
        on_change();
        if (should_persist) {
            main_widget->persist_config();
        }

        main_widget->pop_current_widget();
        });

    QObject::connect(checkbox, &TouchCheckbox::canceled, [&]() {
        main_widget->pop_current_widget();
        });
}

void BoolConfigUI::resizeEvent(QResizeEvent* resize_event) {
    QWidget::resizeEvent(resize_event);
    int parent_width = parentWidget()->width();
    int parent_height = parentWidget()->height();

    int five_cm = static_cast<int>(12 * logicalDpiX() / 2.54f);

    int w = 2 * parent_width / 3;
    int h = parent_height / 2;

    w = std::min(w, five_cm);
    h = std::min(h, five_cm);

    checkbox->resize(w, h);

    setFixedSize(w, h);
    move((parent_width - w) / 2, (parent_height - h) / 2);
}

void MacroConfigUI::resizeEvent(QResizeEvent* resize_event) {
    QWidget::resizeEvent(resize_event);
    int parent_width = parentWidget()->width();
    int parent_height = parentWidget()->height();

    int w = 2 * parent_width / 3;
    int h = parent_height / 2;
    macro_editor->resize(w, h);

    setFixedSize(w, h);
    move(parent_width / 6, parent_height / 4);
}

void FloatConfigUI::resizeEvent(QResizeEvent* resize_event) {
    QWidget::resizeEvent(resize_event);
    int parent_width = parentWidget()->width();
    int parent_height = parentWidget()->height();

    int w = 2 * parent_width / 3;
    int h = parent_height / 2;
    slider->resize(w, h);

    setFixedSize(w, h);
    move(parent_width / 6, parent_height / 4);
}

void IntConfigUI::resizeEvent(QResizeEvent* resize_event) {
    QWidget::resizeEvent(resize_event);
    int parent_width = parentWidget()->width();
    int parent_height = parentWidget()->height();

    int w = 2 * parent_width / 3;
    int h = parent_height / 2;
    slider->resize(w, h);

    setFixedSize(w, h);
    move(parent_width / 6, parent_height / 4);
}

//QWidget* color3_configurator_ui(MainWidget* main_widget, void* location){
//    return new Color3ConfigUI(main_widget, (float*)location);
//}

//QWidget* color4_configurator_ui(MainWidget* main_widget, void* location){
//    return new Color4ConfigUI(main_widget, (float*)location);
//}


TouchCommandSelector::TouchCommandSelector(bool is_fuzzy, const QStringList& commands, MainWidget* mw) : QWidget(mw) {
    main_widget = mw;
    QStringList touch_commands;
    for (auto com : commands) {
        if (!(com.startsWith("toggleconfig_") || com.startsWith("setconfig_") || com.startsWith("saveconfig_") || com.startsWith("deleteconfig_") || com.startsWith("setsaveconfig_"))) {
            touch_commands.push_back(com);
        }
    }
    list_view = new TouchListView(is_fuzzy, touch_commands, -1, this);

    QObject::connect(list_view, &TouchListView::itemSelected, [&](QString val, int /*index*/) {
        //main_widget->current_widget = nullptr;
        main_widget->pop_current_widget();
        main_widget->on_command_done(val.toStdString(), val.toStdString());
        //deleteLater();
        });
}

void TouchCommandSelector::resizeEvent(QResizeEvent* resize_event) {
    QWidget::resizeEvent(resize_event);

    int parent_width = parentWidget()->size().width();
    int parent_height = parentWidget()->size().height();

    resize(parent_width * 0.9f, parent_height);
    move(parent_width * 0.05f, 0);
    list_view->resize(parent_width * 0.9f, parent_height);
}


RectangleConfigUI::RectangleConfigUI(std::string name, MainWidget* parent, UIRect* config_location) : ConfigUI(name, parent) {

    rect_location = config_location;

    bool current_enabled = config_location->enabled;

    float current_left = config_location->left;
    float current_right = config_location->right;
    float current_top = config_location->top;
    float current_bottom = config_location->bottom;

    //    layout = new QVBoxLayout();

    rectangle_select_ui = new TouchRectangleSelectUI(current_enabled,
        current_left,
        current_top,
        (current_right - current_left) / 2.0f,
        (current_bottom - current_top) / 2.0f,
        this);


    //    int current_value = static_cast<int>((*config_location - min_value) / (max_value - min_value) * 100);
    //    slider = new TouchSlider(0, 100, current_value, this);
    QObject::connect(rectangle_select_ui, &TouchRectangleSelectUI::rectangleSelected, [&](bool enabled, qreal left, qreal right, qreal top, qreal bottom) {

        rect_location->enabled = enabled;
        rect_location->left = left;
        rect_location->right = right;
        rect_location->top = top;
        rect_location->bottom = bottom;

        on_change();
        if (should_persist) {
            main_widget->persist_config();
        }
        main_widget->invalidate_render();
        //main_widget->current_widget = nullptr;
        //deleteLater();
        main_widget->pop_current_widget();
        });

}

//bool RectangleConfigUI::eventFilter(QObject *obj, QEvent *event){
//    if ((obj == parentWidget()) && (event->type() == QEvent::Type::Resize)){

//        QResizeEvent* resize_event = static_cast<QResizeEvent*>(event);
//        int parent_width = resize_event->size().width();
//        int parent_height = resize_event->size().height();

//        bool res = QObject::eventFilter(obj, event);

//        qDebug() << "resizing to " << parent_width << " " << parent_height;
//        rectangle_select_ui->resize(parent_width, parent_height);
//        setFixedSize(parent_width, parent_height);

////        setFixedSize(parent_width, parent_height);
//        move(0, 0);
//        return res;
//    }
//    else{
//        return QObject::eventFilter(obj, event);
//    }
//}

void RectangleConfigUI::resizeEvent(QResizeEvent* resize_event) {
    QWidget::resizeEvent(resize_event);
    move(0, 0);
    //    rectangle_select_ui->resize(resize_event->size().width(), resize_event->size().height());
    setFixedSize(parentWidget()->size());
    rectangle_select_ui->resize(parentWidget()->size());

}

RangeConfigUI::RangeConfigUI(std::string name, MainWidget* parent, float* top_config_location, float* bottom_config_location) : ConfigUI(name, parent) {

    //    range_location = config_location;
    top_location = top_config_location;
    bottom_location = bottom_config_location;

    float current_top = -(*top_location);
    float current_bottom = -(*bottom_location) + 1;


    range_select_ui = new TouchRangeSelectUI(current_top, current_bottom, this);

    // force a resize event in order to have correct sizes
    resize(parent->width(), parent->height());


    QObject::connect(range_select_ui, &TouchRangeSelectUI::rangeSelected, [&](qreal top, qreal bottom) {

        *top_location = -top;
        *bottom_location = -bottom + 1;

        on_change();
        if (should_persist) {
            main_widget->persist_config();
        }
        main_widget->invalidate_render();
        //main_widget->current_widget = nullptr;
        //deleteLater();
        main_widget->pop_current_widget();
        });

    QObject::connect(range_select_ui, &TouchRangeSelectUI::rangeCanceled, [&]() {
        main_widget->invalidate_render();
        //main_widget->current_widget = nullptr;
        //deleteLater();
        main_widget->pop_current_widget();
        });

}

void RangeConfigUI::resizeEvent(QResizeEvent* resize_event) {
    QWidget::resizeEvent(resize_event);
    move(0, 0);
    range_select_ui->resize(resize_event->size().width(), resize_event->size().height());

}
// A single-line Vim editor for document queries. Result navigation remains the
// picker's responsibility; edits use QLineEdit's undo history and system clipboard.
class PickerVimEditor {
public:
    enum class Mode { Insert, Normal, Visual };
    enum class Result { Pass, Handled, Close, YankPath };
    explicit PickerVimEditor(QLineEdit* edit, std::function<void(int)> move_results,
        std::function<void(bool)> jump_results, bool yank_path = true, Mode initial_mode = Mode::Normal) :
        edit_(edit), move_results_(std::move(move_results)), jump_results_(std::move(jump_results)), yank_path_(yank_path) { mode_=initial_mode; refresh_mode(); }
    bool inserting() const { return mode_ == Mode::Insert; }
    QString label() const {
        QString name = inserting() ? "INSERT" : mode_ == Mode::Visual ? "VISUAL" : "NORMAL";
        if (pending_) name += "  " + QString(QChar(pending_));
        if (count_) name += QString::number(count_);
        return name;
    }
    void mouse_pressed() {
        if (!inserting()) set_mode(Mode::Normal, edit_->cursorPosition());
    }
    Result key(QKeyEvent* event) {
        const int code = event->key();
        const bool control = is_platform_control_pressed(event);
        if (control && code == Qt::Key_C) return Result::Close;
        if (code == Qt::Key_Escape || (control && code == Qt::Key_BracketLeft)) {
            if (event->isAutoRepeat()) return Result::Handled;
            if (inserting()) set_mode(Mode::Normal, previous(edit_->cursorPosition()));
            else if (mode_ == Mode::Visual || pending_ || count_) set_mode(Mode::Normal, position());
            else return Result::Close;
            return Result::Handled;
        }
        if (inserting()) return Result::Pass;
        if (control && code == Qt::Key_R) {
            edit_->setReadOnly(false); edit_->redo();
            set_mode(Mode::Normal, edit_->cursorPosition());
            return Result::Handled;
        }
        // Preserve native copy/select shortcuts and folder navigation. Read-only
        // mode prevents native text edits from bypassing Vim's editing commands.
        if (event->modifiers() & (Qt::ControlModifier | Qt::MetaModifier | Qt::AltModifier)) return Result::Pass;
        if (code == Qt::Key_Return || code == Qt::Key_Enter || code == Qt::Key_Up ||
            code == Qt::Key_Down || code == Qt::Key_PageUp || code == Qt::Key_PageDown ||
            code == Qt::Key_Tab || code == Qt::Key_Backtab) {
            reset_pending(); return Result::Pass;
        }
        QChar command = event->text().isEmpty() ? QChar() : event->text().front();
        if (code == Qt::Key_Left || code == Qt::Key_Backspace) command = 'h';
        if (code == Qt::Key_Right) command = 'l';
        if (code == Qt::Key_Home) command = '0';
        if (code == Qt::Key_End) command = '$';
        if (command >= '0' && command <= '9' && (command != '0' || count_)) {
            count_ = std::min(999, count_ * 10 + command.digitValue());
            return Result::Handled;
        }
        const int count = std::max(1, count_); count_ = 0;
        const int start = position();
        if (pending_) {
            const char op = pending_;
            const int repeats = std::min(999, operator_count_ * count);
            reset_pending();
            if (op == 'g') {
                if (command == 'g') jump_results_(false);
                return Result::Handled;
            }
            if (yank_path_ && op == 'y' && command == 'y' && mode_ == Mode::Normal) return Result::YankPath;
            if (command == QChar(op)) operate(op, 0, length());
            else if (is_motion(command)) {
                // Vim's cw changes the current word without consuming its trailing space.
                const QChar motion_key = op == 'c' && (command == 'w' || command == 'W') && kind(start, false) != 0 ?
                    (command == 'w' ? QChar('e') : QChar('E')) : command;
                int target = motion(motion_key, repeats);
                const bool inclusive = motion_key == 'e' || motion_key == 'E' || motion_key == '$';
                operate(op, std::min(start, target), inclusive ? next(std::max(start, target)) : std::max(start, target));
            }
            return Result::Handled;
        }
        if (mode_ == Mode::Normal && command == 'g') {
            pending_ = 'g';
        } else if (mode_ == Mode::Normal && command == 'G') {
            jump_results_(true);
        } else if (command == 'j' || command == 'k') {
            move_results_(command == 'j' ? count : -count);
        } else if (is_motion(command)) {
            move_cursor(motion(command, count));
        } else if (command == 'i' || command == 'a' || command == 'I' || command == 'A') {
            int pos = start;
            if (command == 'a') pos = next(start);
            if (command == 'I') pos = motion('^', 1);
            if (command == 'A') pos = length();
            set_mode(Mode::Insert, pos);
        } else if (command == 'v' || command == 'V') {
            if (mode_ == Mode::Visual && command == 'v') set_mode(Mode::Normal, start);
            else {
                mode_ = Mode::Visual; anchor_ = command == 'V' ? 0 : start;
                move_cursor(command == 'V' ? previous(length()) : start);
            }
        } else if (command == 'x' || command == 'd' || command == 'c' || command == 'y') {
            if (edit_->hasSelectedText() && (command != 'y' || mode_ == Mode::Visual)) {
                operate(command == 'x' ? 'd' : command.toLatin1(), edit_->selectionStart(),
                    edit_->selectionStart() + edit_->selectedText().size());
            } else if (command == 'x') {
                int end = start; for (int i = 0; i < count; ++i) end = next(end);
                operate('d', start, end);
            } else { pending_ = command.toLatin1(); operator_count_ = count; }
        } else if (command == 'D' || command == 'C') {
            operate(command == 'D' ? 'd' : 'c', start, length());
        } else if (command == 'p' || command == 'P') {
            QString text = QApplication::clipboard()->text();
            text.replace(QRegularExpression("[\\r\\n]+"), " ");
            if (!text.isEmpty()) {
                const int pos = edit_->hasSelectedText() ? edit_->selectionStart() : command == 'p' ? next(start) : start;
                if (!edit_->hasSelectedText()) edit_->setCursorPosition(pos);
                edit_->setReadOnly(false); edit_->insert(text);
                set_mode(Mode::Normal, previous(edit_->cursorPosition()));
            }
        } else if (command == 'u') {
            edit_->setReadOnly(false); edit_->undo();
            set_mode(Mode::Normal, edit_->cursorPosition());
        } else if (command == 'q') return Result::Close;
        // Unrecognized normal-mode keys never reach PDF shortcuts or insert text.
        return Result::Handled;
    }
private:
    QLineEdit* edit_;
    std::function<void(int)> move_results_;
    std::function<void(bool)> jump_results_;
    bool yank_path_;
    Mode mode_ = Mode::Normal;
    int anchor_ = 0, visual_cursor_ = 0, count_ = 0, operator_count_ = 1;
    char pending_ = 0;
    int length() const { return int(edit_->text().size()); }
    int previous(int pos) const {
        QTextBoundaryFinder finder(QTextBoundaryFinder::Grapheme, edit_->text());
        finder.setPosition(std::clamp(pos, 0, length()));
        return std::max(0, int(finder.toPreviousBoundary()));
    }
    int next(int pos) const {
        QTextBoundaryFinder finder(QTextBoundaryFinder::Grapheme, edit_->text());
        finder.setPosition(std::clamp(pos, 0, length()));
        const int end = finder.toNextBoundary(); return end < 0 ? length() : end;
    }
    int position() const {
        return std::clamp(mode_ == Mode::Visual ? visual_cursor_ : edit_->cursorPosition(), 0, previous(length()));
    }
    int kind(int pos, bool big_word) const {
        if (pos >= length() || edit_->text()[pos].isSpace()) return 0;
        const QChar ch = edit_->text()[pos];
        return big_word || ch.isLetterOrNumber() || ch == '_' ? 1 : 2;
    }
    static bool is_motion(QChar key) { return QString("hlwbeWBE0^$").contains(key) && !key.isNull(); }
    int motion(QChar key, int count) const {
        int pos = position();
        const bool big = key.isUpper();
        if (key == '0') return 0;
        if (key == '^') { pos = 0; while (pos < length() && !kind(pos, false)) pos = next(pos); return pos; }
        if (key == '$') return previous(length());
        for (int i = 0; i < count; ++i) {
            if (key == 'h') pos = previous(pos);
            else if (key == 'l') pos = next(pos);
            else if (key.toLower() == 'w') {
                const int type = kind(pos, big);
                while (pos < length() && kind(pos, big) == type) pos = next(pos);
                while (pos < length() && !kind(pos, big)) pos = next(pos);
            } else if (key.toLower() == 'b') {
                pos = previous(pos);
                while (pos > 0 && !kind(pos, big)) pos = previous(pos);
                const int type = kind(pos, big);
                while (pos > 0 && kind(previous(pos), big) == type) pos = previous(pos);
            } else {
                pos = next(pos);
                while (pos < length() && !kind(pos, big)) pos = next(pos);
                const int type = kind(pos, big);
                while (next(pos) < length() && kind(next(pos), big) == type) pos = next(pos);
            }
        }
        return pos;
    }
    void reset_pending() { pending_ = 0; count_ = 0; operator_count_ = 1; }
    void refresh_mode() {
        edit_->setReadOnly(!inserting());
        edit_->setProperty("vimNormalCursor", mode_ == Mode::Normal);
        edit_->update();
    }
    void set_mode(Mode mode, int pos) {
        mode_ = mode; reset_pending();
        edit_->deselect();
        edit_->setCursorPosition(std::clamp(pos, 0, inserting() ? length() : previous(length())));
        refresh_mode();
    }
    void move_cursor(int target) {
        target = std::clamp(target, 0, previous(length()));
        if (mode_ == Mode::Visual) {
            anchor_ = std::clamp(anchor_, 0, previous(length()));
            visual_cursor_ = target;
            const int first = std::min(anchor_, target), last = next(std::max(anchor_, target));
            if (target < anchor_) edit_->setSelection(last, first - last);
            else edit_->setSelection(first, last - first);
        } else edit_->setCursorPosition(target);
        refresh_mode();
    }
    void operate(char op, int first, int last) {
        first = std::clamp(first, 0, length()); last = std::clamp(last, first, length());
        if (last > first) {
            QApplication::clipboard()->setText(edit_->text().mid(first, last - first));
            if (op != 'y') {
                edit_->setReadOnly(false);
                edit_->setSelection(first, last - first); edit_->insert("");
            }
        }
        set_mode(op == 'c' ? Mode::Insert : Mode::Normal, first);
    }
};

namespace {
QString floating_selector_stylesheet(const QString& panel, const QString& view) {
    return QString(
        "QWidget#%1, QWidget#%1 QLabel, %2#%1List { %3 border-radius: 0; }"
        "QLineEdit#%1Search { %3 padding: 6px 8px; border: 1px solid palette(mid); border-radius: 0; }"
        "%2#%1List { outline: none; }"
        "%2#%1List::item:selected { %4 }")
        .arg(panel, view, get_ui_stylesheet(true), get_selected_stylesheet(true));
}
class CheatsheetSearchEdit : public QLineEdit {
public:
    using QLineEdit::QLineEdit;
protected:
    void keyPressEvent(QKeyEvent* event) override {
        QLineEdit::keyPressEvent(event);
        event->accept(); // Unhandled arrows must not reach the PDF under the sheet.
    }
};
class CommandPickerTable : public QTableView {
    void keyPressEvent(QKeyEvent* event) override {
        QTableView::keyPressEvent(event);
        event->accept(); // Never send boundary navigation through to the PDF.
    }
};
}

CommandCheatsheet::CommandCheatsheet(MainWidget* pane) : QWidget(pane->tab_owner()), pane_(pane) {
    setObjectName("cheatsheet"); setAttribute(Qt::WA_StyledBackground);
    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 12, 16, 12); layout->setSpacing(8);
    auto title = new QLabel("Shortcuts & commands", this);
    auto bold = title->font(); bold.setBold(true); title->setFont(bold); layout->addWidget(title);
    text_ = new QPlainTextEdit(this); text_->setObjectName("cheatsheetList");
    text_->setReadOnly(true); text_->setUndoRedoEnabled(false);
    text_->setLineWrapMode(QPlainTextEdit::NoWrap); text_->setFrameShape(QFrame::NoFrame);
    text_->setCursorWidth(0);
    QFont mono("JetBrains Mono", 12); mono.setStyleHint(QFont::Monospace); mono.setFixedPitch(true); mono.setKerning(false);
    text_->setFont(mono); layout->addWidget(text_, 1);
    search_ = new CheatsheetSearchEdit(this); search_->setObjectName("cheatsheetSearch"); search_->setFont(mono);
    search_->setPlaceholderText("/ Search commands and shortcuts"); layout->addWidget(search_); search_->hide();
    status_ = new QLabel(this); layout->addWidget(status_);

    const auto mapping = pane->input_handler->get_command_key_mappings();
    auto names = pane->command_manager->get_all_command_names(); names.removeDuplicates(); names.sort(Qt::CaseInsensitive);
    QStringList lines;
    auto append = [&](QString text, QString command = {}) { lines.append(text); commands_.append(command); };
    for (bool bound : {true, false}) {
        append(bound ? "KEYBOARD SHORTCUTS" : "OTHER COMMANDS  ·  Enter to run"); append("");
        for (const auto& name : names) {
            const auto binding = mapping.find(name.toStdString());
            const bool has_keys = binding != mapping.end() && !binding->second.empty();
            if (has_keys != bound) continue;
            // Thousands of generated config setters remain available through :.
            const auto prefix = pane->command_manager->command_required_prefixes.find(name);
            if (!bound && prefix != pane->command_manager->command_required_prefixes.end() &&
                !prefix->second.isEmpty() && prefix->second != "_") continue;
            QStringList keys;
            if (has_keys) for (const auto& key : binding->second) {
                QString label = QString::fromStdString(key);
                label.replace(" ", "Space ");
                // Replace modifier tokens only: a literal capital C, D or S is a key.
                for (const auto& token : {std::pair<QString, QString>{"D", "Cmd"},
                        {"C", "Ctrl"}, {"A", "Alt"}, {"S", "Shift"}}) {
                    label.replace("<" + token.first + "-", "<" + token.second + "+");
                    label.replace("-" + token.first + "-", "-" + token.second + "+");
                    label.replace("+" + token.first + "-", "+" + token.second + "+");
                }
                // In Sioyek a capital letter implies Shift, including after Cmd/Ctrl.
                if (!label.contains("Shift+"))
                    label.replace(QRegularExpression("([+])([A-Z])(?=>)"), "\\1Shift+\\2");
                label.replace(QRegularExpression("<([^<>]+)>"), "\\1");
                label.replace(QRegularExpression("<([^<>]+)>"), "\\1");
                if (label.size() == 1 && label.front().isUpper()) label.prepend("Shift+");
                keys.append(label);
            }
            append(name.leftJustified(43, ' ') + keys.join("  ·  "), name);
            const auto description = pane->command_manager->command_human_readable_names.find(name.toStdString());
            if (description != pane->command_manager->command_human_readable_names.end() && !description->second.empty())
                append("    " + QString::fromStdString(description->second).simplified(), name);
        }
        append("");
    }
    text_->setPlainText(lines.join('\n'));
    text_->installEventFilter(this); search_->installEventFilter(this); parentWidget()->installEventFilter(this);
    connect(text_, &QPlainTextEdit::cursorPositionChanged, this, [this]() { decorate(); });
    connect(search_, &QLineEdit::textChanged, this, [this](const QString& query) {
        query_ = query; find_match(reverse_, true);
    });
    on_config_file_changed();
}

void CommandCheatsheet::update_geometry() {
    if (!parentWidget()) return;
    const int w = std::max(1, std::min(1050, parentWidget()->width() - 48));
    const int h = std::max(1, std::min(850, parentWidget()->height() - 64));
    setGeometry((parentWidget()->width() - w) / 2, (parentWidget()->height() - h) / 2, w, h);
}
void CommandCheatsheet::showEvent(QShowEvent* event) {
    QWidget::showEvent(event); update_geometry(); raise(); text_->setFocus(); decorate();
}
void CommandCheatsheet::on_config_file_changed() {
    setStyleSheet(floating_selector_stylesheet("cheatsheet", "QPlainTextEdit"));
    update_geometry(); decorate();
}
void CommandCheatsheet::decorate() {
    extern float UI_SELECTED_BACKGROUND_COLOR[3], UI_SELECTED_TEXT_COLOR[3];
    const auto bg = QColor::fromRgbF(UI_SELECTED_BACKGROUND_COLOR[0], UI_SELECTED_BACKGROUND_COLOR[1], UI_SELECTED_BACKGROUND_COLOR[2]);
    const auto fg = QColor::fromRgbF(UI_SELECTED_TEXT_COLOR[0], UI_SELECTED_TEXT_COLOR[1], UI_SELECTED_TEXT_COLOR[2]);
    QList<QTextEdit::ExtraSelection> selections;
    QTextEdit::ExtraSelection line;
    line.cursor = text_->textCursor(); line.cursor.clearSelection();
    line.format.setBackground(bg); line.format.setForeground(fg);
    line.format.setProperty(QTextFormat::FullWidthSelection, true); selections.append(line);
    if (!query_.isEmpty()) {
        QTextCursor found(text_->document());
        const auto flags = query_ == query_.toLower() ? QTextDocument::FindFlags() : QTextDocument::FindCaseSensitively;
        while (!(found = text_->document()->find(query_, found, flags)).isNull()) {
            QTextEdit::ExtraSelection hit; hit.cursor = found;
            hit.format.setFontUnderline(true); hit.format.setFontWeight(QFont::Bold); selections.append(hit);
        }
    }
    auto cursor = text_->textCursor(); cursor.clearSelection();
    if (!cursor.atBlockEnd()) {
        cursor.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor);
        QTextEdit::ExtraSelection block; block.cursor = cursor;
        block.format.setBackground(fg); block.format.setForeground(bg); selections.append(block);
    }
    text_->setExtraSelections(selections);
    status_->setText(QString("%1  ·  %2/%3    hjkl  / search  n/N  Enter run  Esc close")
        .arg(visual_ ? "VISUAL" : "NORMAL").arg(text_->textCursor().blockNumber() + 1).arg(commands_.size()));
}
void CommandCheatsheet::find_match(bool backwards, bool from_origin) {
    if (query_.isEmpty()) { if (from_origin) text_->setTextCursor(search_origin_); decorate(); return; }
    auto cursor = from_origin ? search_origin_ : text_->textCursor(); cursor.clearSelection();
    if (!backwards && !from_origin) cursor.movePosition(QTextCursor::NextCharacter);
    auto flags = backwards ? QTextDocument::FindBackward : QTextDocument::FindFlags();
    if (query_ != query_.toLower()) flags |= QTextDocument::FindCaseSensitively;
    auto found = text_->document()->find(query_, cursor, flags);
    if (found.isNull()) {
        cursor.movePosition(backwards ? QTextCursor::End : QTextCursor::Start);
        found = text_->document()->find(query_, cursor, flags);
    }
    if (!found.isNull()) {
        found.setPosition(found.selectionStart()); text_->setTextCursor(found); text_->ensureCursorVisible();
    }
    decorate();
    if (found.isNull()) status_->setText("Pattern not found: " + query_);
}
void CommandCheatsheet::dismiss() {
    if (pane_) { pane_->pop_current_widget(); pane_->setFocus(); }
}
void CommandCheatsheet::execute() {
    const int row = text_->textCursor().blockNumber();
    if (!pane_ || row < 0 || row >= commands_.size() || commands_[row].isEmpty()) return;
    const auto name = commands_[row].toStdString();
    const auto callback = pane_->on_command_done;
    dismiss(); callback(name, "");
}
bool CommandCheatsheet::eventFilter(QObject* object, QEvent* event) {
    if (object == parentWidget() && event->type() == QEvent::Resize) update_geometry();
    if (object != text_ && object != search_) return QWidget::eventFilter(object, event);
    if (event->type() == QEvent::ShortcutOverride) { event->accept(); return true; }
    if (event->type() == QEvent::KeyRelease) return true;
    if (event->type() != QEvent::KeyPress) return QWidget::eventFilter(object, event);
    auto key = static_cast<QKeyEvent*>(event);
    if (object == search_) {
        if (key->key() == Qt::Key_Escape || key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter) {
            if (key->key() == Qt::Key_Escape) text_->setTextCursor(search_origin_);
            search_->hide(); text_->setFocus(); decorate(); return true;
        }
        return false;
    }
    const auto ch = key->text().isEmpty() ? QChar() : key->text().front();
#ifdef Q_OS_MACOS
    const bool ctrl = key->modifiers().testFlag(Qt::MetaModifier);
#else
    const bool ctrl = key->modifiers().testFlag(Qt::ControlModifier);
#endif
    if (key->key() == Qt::Key_Escape) {
        if (visual_) { visual_ = false; auto c = text_->textCursor(); c.clearSelection(); text_->setTextCursor(c); decorate(); }
        else dismiss();
        return true;
    }
    if (key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter) { execute(); return true; }
    if (!ctrl && ch.isDigit() && (count_ || ch != '0')) { count_ = std::min(9999, count_ * 10 + ch.digitValue()); return true; }
    int count = std::max(1, count_); const bool counted = count_ > 0; count_ = 0;
    auto cursor = text_->textCursor();
    auto mode = visual_ ? QTextCursor::KeepAnchor : QTextCursor::MoveAnchor;
    if (!visual_) cursor.clearSelection();
    const QChar pending = pending_; pending_ = {};
    if (!ctrl && (ch == '/' || ch == '?')) {
        visual_ = false; reverse_ = ch == '?'; search_origin_ = cursor;
        search_->setPlaceholderText(reverse_ ? "? Search backwards" : "/ Search commands and shortcuts");
        search_->show(); search_->setFocus(); search_->selectAll(); return true;
    }
    if (!ctrl && (ch == 'n' || ch == 'N')) {
        visual_ = false; for (int i = 0; i < count; ++i) find_match(ch == 'n' ? reverse_ : !reverse_); return true;
    }
    if (!ctrl && ch == 'q') { dismiss(); return true; }
    if (!ctrl && ch == 'v') { visual_ = !visual_; if (!visual_) cursor.clearSelection(); }
    else if (!ctrl && ch == 'y') {
        if (visual_ || cursor.hasSelection()) {
            QApplication::clipboard()->setText(cursor.selectedText().replace(QChar::ParagraphSeparator, '\n'));
            visual_ = false; cursor.clearSelection();
        } else if (pending == 'y') {
            auto line = cursor; line.select(QTextCursor::LineUnderCursor);
            QApplication::clipboard()->setText(line.selectedText());
        } else pending_ = 'y';
    }
    else if (!ctrl && ch == 'g') {
        if (pending == 'g') { cursor.movePosition(QTextCursor::Start, mode); cursor.movePosition(QTextCursor::Down, mode, count - 1); }
        else { pending_ = 'g'; count_ = counted ? count : 0; }
    }
    else if (!ctrl && ch == 'G') {
        cursor.movePosition(counted ? QTextCursor::Start : QTextCursor::End, mode);
        if (counted) cursor.movePosition(QTextCursor::Down, mode, count - 1);
        cursor.movePosition(QTextCursor::StartOfBlock, mode);
    }
    else if (ctrl && (key->key() == Qt::Key_D || key->key() == Qt::Key_U || key->key() == Qt::Key_F || key->key() == Qt::Key_B)) {
        const bool half = key->key() == Qt::Key_D || key->key() == Qt::Key_U;
        int rows = std::max(1, text_->viewport()->height() / text_->fontMetrics().lineSpacing() / (half ? 2 : 1));
        cursor.movePosition(key->key() == Qt::Key_D || key->key() == Qt::Key_F ? QTextCursor::Down : QTextCursor::Up, mode, rows * count);
    }
    else if (!ctrl) {
        QTextCursor::MoveOperation op = QTextCursor::NoMove;
        if (ch == 'h' || key->key() == Qt::Key_Left) { op = QTextCursor::PreviousCharacter; count = std::min(count, cursor.positionInBlock()); }
        if (ch == 'l' || key->key() == Qt::Key_Right) { op = QTextCursor::NextCharacter; count = std::min(count, std::max(0, cursor.block().length() - 2 - cursor.positionInBlock())); }
        if (ch == 'j' || key->key() == Qt::Key_Down) op = QTextCursor::Down;
        if (ch == 'k' || key->key() == Qt::Key_Up) op = QTextCursor::Up;
        if (ch == 'w') op = QTextCursor::NextWord;
        if (ch == 'b') op = QTextCursor::PreviousWord;
        if (ch == 'e') {
            for (int i = 0; i < count; ++i) {
                cursor.movePosition(QTextCursor::NextCharacter, mode);
                cursor.movePosition(QTextCursor::EndOfWord, mode);
                if (!cursor.atBlockStart()) cursor.movePosition(QTextCursor::PreviousCharacter, mode);
            }
            count = 0;
        }
        if (ch == '0' || key->key() == Qt::Key_Home) op = QTextCursor::StartOfBlock;
        if (ch == '$' || key->key() == Qt::Key_End) op = QTextCursor::EndOfBlock;
        if (key->key() == Qt::Key_PageDown || key->key() == Qt::Key_PageUp) {
            op = key->key() == Qt::Key_PageDown ? QTextCursor::Down : QTextCursor::Up;
            count *= std::max(1, text_->viewport()->height() / text_->fontMetrics().lineSpacing());
        }
        if (count) cursor.movePosition(op, mode, count);
        if (ch == '^') { cursor.movePosition(QTextCursor::StartOfBlock, mode); while (cursor.positionInBlock() < cursor.block().text().size() && cursor.block().text()[cursor.positionInBlock()].isSpace()) cursor.movePosition(QTextCursor::NextCharacter, mode); }
    }
    text_->setTextCursor(cursor); text_->ensureCursorVisible(); decorate();
    return true; // All navigation, including at document boundaries, stays here.
}

QList<QStandardItem*> CommandSelector::get_item(std::string command_name) {

    std::string command_key = "";

    if (key_map.find(command_name) != key_map.end()) {
        const std::vector<std::string>& command_keys = key_map[command_name];
        for (size_t i = 0; i < command_keys.size(); i++) {
            const std::string& ck = command_keys[i];
            if (i > 0) {
                command_key += " | ";
            }
            QString label = QString::fromStdString(ck);
            label.replace(" ", "Space ");
            for (const auto& token : {std::pair<QString, QString>{"D", "Cmd"},
                    {"C", "Ctrl"}, {"A", "Alt"}, {"S", "Shift"}}) {
                label.replace("<" + token.first + "-", "<" + token.second + "+");
                label.replace("-" + token.first + "-", "-" + token.second + "+");
                label.replace("+" + token.first + "-", "+" + token.second + "+");
            }
            if (!label.contains("Shift+"))
                label.replace(QRegularExpression("([+])([A-Z])(?=>)"), "\\1Shift+\\2");
            label.replace(QRegularExpression("<([^<>]+)>"), "\\1");
            if (label.size() == 1 && label.front().isUpper()) label.prepend("Shift+");
            command_key += label.toStdString();
        }

    }
    QStandardItem* name_item = new QStandardItem(QString::fromStdString(command_name));
    QStandardItem* key_item = new QStandardItem(QString::fromStdString(command_key));
    key_item->setTextAlignment(Qt::AlignVCenter | Qt::AlignLeft);
    return (QList<QStandardItem*>() << name_item << key_item);
}

QStandardItemModel* CommandSelector::get_standard_item_model(std::vector<std::string> command_names) {

    QStandardItemModel* res = new QStandardItemModel(this);
    res->setColumnCount(2);

    for (size_t i = 0; i < command_names.size(); i++) {
        res->appendRow(get_item(command_names[i]));
    }
    return res;
}

QStandardItemModel* CommandSelector::get_standard_item_model(QStringList command_names) {

    std::vector<std::string> std_command_names;

    for (int i = 0; i < command_names.size(); i++) {
        std_command_names.push_back(command_names.at(i).toStdString());
    }
    return get_standard_item_model(std_command_names);
}


QString CommandSelector::get_view_stylesheet_type_name() {
    return "QTableView";
}

void CommandSelector::on_select(const QModelIndex& index) {
    bool numeric = false;
    line_edit->text().toInt(&numeric);
    if (!numeric && !index.isValid()) return;
    const auto query = line_edit->text().toStdString();
    const auto name = numeric ? query : index.siblingAtColumn(0).data().toString().toStdString();
    const auto callback = *on_done;
    if (main_widget) {
        main_widget->pop_current_widget();
        main_widget->setFocus();
    }
    callback(name, query);
}

CommandSelector::~CommandSelector() {
    fzf_free_slab(slab);
}

QStringList CommandSelector::get_elements_matching_prefix(QString text) {
    QStringList res;

    for (int i = 0; i < string_elements.size(); i++) {
        QString required_prefix = prefixes[string_elements[i]];
        bool show_because_command_was_recent = SHOW_MOST_RECENT_COMMANDS_FIRST ? i < 10 : false;
        if (show_because_command_was_recent || required_prefix.size() == 0 || text.startsWith(required_prefix)) {
            res.push_back(string_elements[i]);
        }
    }
    return res;
}

CommandSelector::CommandSelector(bool is_fuzzy, std::function<void(std::string, std::string)>* on_done,
    MainWidget* parent,
    QStringList elements,
    const std::unordered_map<QString, QString>& required_prefixed,
    std::unordered_map<std::string, std::vector<std::string>> key_map
) : BaseSelectorWidget(new CommandPickerTable(), is_fuzzy, nullptr, parent),
    main_widget(parent),
    key_map(key_map),
    prefixes(required_prefixed),
    on_done(on_done)
{
    slab = fzf_make_default_slab();
    string_elements = elements;
    setParent(parent->tab_owner());
    setObjectName("commandPicker");
    setAttribute(Qt::WA_StyledBackground);
    QFont text_font("JetBrains Mono", 12); text_font.setStyleHint(QFont::Monospace);
    text_font.setFixedPitch(true); text_font.setKerning(false); setFont(text_font);
    QFont search_font("JetBrains Mono", 12); search_font.setStyleHint(QFont::Monospace);
    search_font.setFixedPitch(true); search_font.setKerning(false);
    line_edit->setFont(search_font);
    line_edit->setObjectName("commandPickerSearch");
    line_edit->setPlaceholderText("Search commands or enter a page number…");
    line_edit->setClearButtonEnabled(true); line_edit->setMinimumHeight(34);
    auto layout = static_cast<QVBoxLayout*>(this->layout());
    layout->setContentsMargins(14, 12, 14, 12); layout->setSpacing(8);
    heading_ = new QLabel("Shortcuts & commands", this);
    QFont bold = text_font; bold.setBold(true); heading_->setFont(bold);
    auto header = new QWidget(this);
    auto header_layout = new QHBoxLayout(header);
    header_layout->setContentsMargins(0, 0, 0, 0);
    mode_label_ = new QLabel(this);
    header_layout->addWidget(heading_); header_layout->addStretch(); header_layout->addWidget(mode_label_);
    layout->insertWidget(0, header);
    detail_ = new QLabel(this); detail_->setTextFormat(Qt::PlainText);
    detail_->setMinimumWidth(0); layout->addWidget(detail_);
    proxy_model->setParent(this);
    standard_item_model = get_standard_item_model(get_elements_matching_prefix(""));
    auto table = static_cast<QTableView*>(get_view());
    auto old_selection = table->selectionModel();
    table->setModel(standard_item_model);
    delete old_selection;
    table->setObjectName("commandPickerList"); table->setFont(text_font);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setFrameShape(QFrame::NoFrame); table->setShowGrid(false);
    table->setFocusPolicy(Qt::NoFocus);
    table->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    table->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    table->horizontalHeader()->setStretchLastSection(false);
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
    table->horizontalHeader()->hide(); table->verticalHeader()->hide();
    table->verticalHeader()->setDefaultSectionSize(30);
    table->setCurrentIndex(standard_item_model->index(0, 0));
    connect(table->selectionModel(), &QItemSelectionModel::currentChanged, this, [this]() { update_detail(); });
    vim_editor_ = std::make_unique<PickerVimEditor>(line_edit, [this](int delta) {
        const int count = standard_item_model->rowCount();
        if (!count) return;
        auto index = standard_item_model->index(std::clamp(std::max(0, get_view()->currentIndex().row()) + delta, 0, count - 1), 0);
        get_view()->setCurrentIndex(index); get_view()->scrollTo(index);
    }, [this](bool last) {
        const int count = standard_item_model->rowCount();
        if (!count) return;
        auto index = standard_item_model->index(last ? count - 1 : 0, 0);
        get_view()->setCurrentIndex(index); get_view()->scrollTo(index);
    });
    mode_label_->setText(vim_editor_->label());
    parentWidget()->installEventFilter(this);
    on_config_file_changed();

}

bool CommandSelector::on_text_change(const QString& text) {

    std::vector<std::string> matching_element_names;
    std::vector<int> scores;
    std::string search_text_string = text.toStdString();
    std::vector<std::pair<std::string, int>> match_score_pairs;


    QString actual_text = text;

    if (actual_text.startsWith("+=")) {
        actual_text = "setsaveconfig_" + actual_text.right(actual_text.size() - 2);
        search_text_string = actual_text.toStdString();
    }

    if (actual_text.startsWith("!")) {
        actual_text = "toggleconfig_" + actual_text.right(actual_text.size() - 1);
        search_text_string = actual_text.toStdString();
    }

    if (actual_text.startsWith("=")) {
        actual_text = "setconfig_" + actual_text.right(actual_text.size() - 1);
        search_text_string = actual_text.toStdString();
    }

    if (actual_text.startsWith("+")) {
        actual_text = "saveconfig_" + actual_text.right(actual_text.size() - 1);
        search_text_string = actual_text.toStdString();
    }


    if (actual_text.startsWith("-")) {
        actual_text = "deleteconfig_" + actual_text.right(actual_text.size() - 1);
        search_text_string = actual_text.toStdString();
    }

    if (actual_text.endsWith("?")){
        actual_text = actual_text.left(actual_text.size()-1);
        search_text_string = actual_text.toStdString();
    }
    if (actual_text.startsWith("?")){
        actual_text = actual_text.right(actual_text.size()-1);
        search_text_string = actual_text.toStdString();
    }

    std::string pattern_str = actual_text.toStdString();
    fzf_pattern_t* pattern = fzf_parse_pattern(CaseSmart, false, (char*)pattern_str.c_str(), true);

    QStringList elements_matching_prefix = get_elements_matching_prefix(actual_text);

    for (int i = 0; i < elements_matching_prefix.size(); i++) {
        std::string encoded = utf8_encode(elements_matching_prefix.at(i).toStdWString());
        int score = 0;
        if (is_fuzzy) {
            //score = static_cast<int>(rapidfuzz::fuzz::partial_ratio(search_text_string, encoded));
            score = fzf_get_score(encoded.c_str(), pattern, slab);
        }
        else {
            fts::fuzzy_match(search_text_string.c_str(), encoded.c_str(), score);
        }
        match_score_pairs.push_back(std::make_pair(encoded, score));
    }
    std::sort(match_score_pairs.begin(), match_score_pairs.end(), [](std::pair<std::string, int> lhs, std::pair<std::string, int> rhs) {
        return lhs.second > rhs.second;
        });

    for (int i = 0; i < elements_matching_prefix.size(); i++) {
        if (elements_matching_prefix.at(i).startsWith(actual_text)) {
            matching_element_names.push_back(elements_matching_prefix.at(i).toStdString());
        }
    }

    //if (matching_element_names.size() == 0) {
    for (auto [command, score] : match_score_pairs) {
        if (score > 60 && (!QString::fromStdString(command).startsWith(actual_text))) {
            matching_element_names.push_back(command);
        }
    }
    //}

    fzf_free_pattern(pattern);

    QStandardItemModel* new_standard_item_model = get_standard_item_model(matching_element_names);
    auto old_selection = get_view()->selectionModel();
    get_view()->setModel(new_standard_item_model);
    delete old_selection;
    delete standard_item_model;
    standard_item_model = new_standard_item_model;
    get_view()->setCurrentIndex(standard_item_model->index(0, 0));
    connect(get_view()->selectionModel(), &QItemSelectionModel::currentChanged, this, [this]() { update_detail(); });
    update_geometry(); update_detail();

    return true;
}

void CommandSelector::update_detail() {
    if (!detail_) return;
    const auto index = get_view()->currentIndex();
    const auto name = index.siblingAtColumn(0).data().toString();
    QString text;
    bool numeric = false; const int page = line_edit->text().toInt(&numeric);
    if (numeric) text = QString("Go to page %1").arg(page);
    else if (index.isValid() && main_widget) {
        const auto& names = main_widget->command_manager->command_human_readable_names;
        const auto it = names.find(name.toStdString());
        text = it == names.end() || it->second.empty() ? name : QString::fromStdString(it->second);
    } else text = "No matching commands";
    detail_->setToolTip(text);
    detail_->setText(detail_->fontMetrics().elidedText(text, Qt::ElideRight, std::max(0, width() - 28)));
}
void CommandSelector::update_geometry() {
    if (!parentWidget() || !heading_ || !standard_item_model) return;
    const int width = std::max(1, std::min(1050, parentWidget()->width() - 48));
    const int rows = std::max(1, standard_item_model->rowCount());
    const int height = std::max(1, std::min(24 + 24 + heading_->sizeHint().height() + 34 +
        detail_->sizeHint().height() + rows * 30, std::min({720, std::max(220, width * 3 / 4), parentWidget()->height() - 64})));
    setGeometry((parentWidget()->width()-width)/2, (parentWidget()->height()-height)/2, width, height);
    static_cast<QTableView*>(get_view())->setColumnWidth(1, std::min(360, width * 2 / 5));
}
void CommandSelector::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event); update_geometry(); update_detail();
}
void CommandSelector::showEvent(QShowEvent* event) {
    QWidget::showEvent(event); update_geometry(); update_detail(); raise(); line_edit->setFocus();
}
void CommandSelector::on_config_file_changed() {
    setStyleSheet(floating_selector_stylesheet("commandPicker", "QTableView"));
    update_geometry(); update_detail();
}
bool CommandSelector::eventFilter(QObject* object, QEvent* event) {
    if (object == parentWidget() && event->type() == QEvent::Resize) update_geometry();
    if (object == line_edit && vim_editor_) {
        if (event->type() == QEvent::MouseButtonPress) {
            vim_editor_->mouse_pressed(); mode_label_->setText(vim_editor_->label());
        }
        if (event->type() == QEvent::ShortcutOverride && !vim_editor_->inserting()) {
            event->accept(); return true;
        }
        if (event->type() == QEvent::KeyPress) {
            const auto result = vim_editor_->key(static_cast<QKeyEvent*>(event));
            mode_label_->setText(vim_editor_->label());
            if (result == PickerVimEditor::Result::YankPath) {
                const auto index = get_view()->currentIndex().siblingAtColumn(0);
                if (index.isValid()) QApplication::clipboard()->setText(index.data().toString());
                return true;
            }
            if (result == PickerVimEditor::Result::Close) {
                if (main_widget) { main_widget->pop_current_widget(); main_widget->setFocus(); }
                return true;
            }
            if (result == PickerVimEditor::Result::Handled) return true;
        }
        if (event->type() == QEvent::KeyRelease && !vim_editor_->inserting()) return true;
    }
    if (object == line_edit && event->type() == QEvent::KeyPress) {
        auto key = static_cast<QKeyEvent*>(event);
        if (key->key() == Qt::Key_Up || key->key() == Qt::Key_Down) {
            QKeyEvent navigation(QEvent::KeyPress, key->key(), Qt::NoModifier);
            QCoreApplication::sendEvent(get_view(), &navigation); return true;
        }
        if (key->key() == Qt::Key_Escape && main_widget) {
            main_widget->pop_current_widget(); main_widget->setFocus(); return true;
        }
    }
    if (BaseSelectorWidget::eventFilter(object, event)) return true;
    if (object == line_edit && event->type() == QEvent::KeyPress && main_widget) {
        auto key = static_cast<QKeyEvent*>(event);
        auto command = main_widget->input_handler->get_menu_command(main_widget, key,
            key->modifiers() & Qt::ShiftModifier, is_platform_control_pressed(key),
            is_platform_meta_pressed(key), key->modifiers() & Qt::AltModifier);
        if (command && command->is_menu_command()) {
            main_widget->key_event(false, key, key->isAutoRepeat()); return true;
        }
    }
    return false;
}

BaseSelectorWidget::BaseSelectorWidget(QAbstractItemView* item_view, bool fuzzy, QStandardItemModel* item_model, MainWidget* parent) : QWidget(parent) {

    bool is_tree = dynamic_cast<QTreeView*>(item_view) != nullptr;
    is_fuzzy = fuzzy;
    proxy_model = new MySortFilterProxyModel(fuzzy, is_tree);
    proxy_model->setFilterCaseSensitivity(Qt::CaseSensitivity::CaseInsensitive);

    if (item_model) {
        proxy_model->setSourceModel(item_model);
    }

    resize(300, 800);
    QVBoxLayout* layout = new QVBoxLayout;
    setLayout(layout);

    line_edit = new MyLineEdit(parent);
    abstract_item_view = item_view;
    abstract_item_view->setParent(this);
    abstract_item_view->setModel(proxy_model);
    abstract_item_view->setEditTriggers(QAbstractItemView::NoEditTriggers);

    if (TOUCH_MODE) {
        QScroller::grabGesture(abstract_item_view->viewport(), QScroller::TouchGesture);
        abstract_item_view->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
        QObject::connect(abstract_item_view, &QListView::pressed, [&](const QModelIndex& index) {
            pressed_row = index.row();
            pressed_pos = QCursor::pos();
            });

        QObject::connect(abstract_item_view, &QListView::clicked, [&](const QModelIndex& index) {
            QPoint current_pos = QCursor::pos();
            if (index.row() == pressed_row) {
                if ((current_pos - pressed_pos).manhattanLength() < 10) {
                    on_select(index);
                }
            }
            });
    }

    QTreeView* tree_view = dynamic_cast<QTreeView*>(abstract_item_view);

    if (tree_view) {
        tree_view->expandAll();
        tree_view->setHeaderHidden(true);
        tree_view->resizeColumnToContents(0);
        tree_view->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    }
    if (proxy_model) {
        proxy_model->setRecursiveFilteringEnabled(true);
    }

    layout->addWidget(line_edit);
    layout->addWidget(abstract_item_view);

    line_edit->installEventFilter(this);
    line_edit->setFocus();

    if (!TOUCH_MODE) {
        QObject::connect(abstract_item_view, &QAbstractItemView::activated, [&](const QModelIndex& index) {
            on_select(index);
            });
    }

    QObject::connect(line_edit, &QLineEdit::textChanged, [&](const QString& text) {
        on_text_changed(text);
        });

    if (TOUCH_MODE) {
        QScroller::grabGesture(abstract_item_view, QScroller::TouchGesture);
        abstract_item_view->setHorizontalScrollMode(QAbstractItemView::ScrollMode::ScrollPerPixel);
        abstract_item_view->setVerticalScrollMode(QAbstractItemView::ScrollMode::ScrollPerPixel);
    }

    QString font_face = get_ui_font_face_name();
    if (font_face.size() > 0) {
        abstract_item_view->setFont(font_face);
        line_edit->setFont(font_face);
    }
}

void BaseSelectorWidget::on_text_changed(const QString& text) {
    if (!on_text_change(text)) {
        // generic text change handling when we don't explicitly handle text change events
        //proxy_model->setFilterFixedString(text);
        proxy_model->setFilterCustom(text);
        QTreeView* t_view = dynamic_cast<QTreeView*>(get_view());
        if (t_view) {
            t_view->expandAll();
        }
        else {
            get_view()->setCurrentIndex(get_view()->model()->index(0, 0));
        }
    }
}

QAbstractItemView* BaseSelectorWidget::get_view() {
    return abstract_item_view;
}
void BaseSelectorWidget::on_delete(const QModelIndex& /*source_index*/, const QModelIndex& /*selected_index*/) {}
void BaseSelectorWidget::on_edit(const QModelIndex& /*source_index*/, const QModelIndex& /*selected_index*/) {}

void BaseSelectorWidget::on_return_no_select(const QString& text) {
    bool is_numeric = false;
    text.toInt(&is_numeric);

    if (is_numeric){
        auto invalid_index = get_view()->model()->index(-1, 0);
        on_select(invalid_index);
    }
    else if (get_view()->model()->hasIndex(0, 0)) {
        on_select(get_view()->model()->index(0, 0));
    }
}

bool BaseSelectorWidget::on_text_change(const QString& /*text*/) {
    return false;
}

void BaseSelectorWidget::set_filter_column_index(int index) {
    proxy_model->setFilterKeyColumn(index);
}

std::optional<QModelIndex> BaseSelectorWidget::get_selected_index() {
    QModelIndexList selected_index_list = get_view()->selectionModel()->selectedIndexes();

    if (selected_index_list.size() > 0) {
        QModelIndex selected_index = selected_index_list.at(0);
        return selected_index;
    }
    return {};
}

std::wstring BaseSelectorWidget::get_selected_text() {
    return L"";
}

bool BaseSelectorWidget::eventFilter(QObject* obj, QEvent* event) {
    if (obj == line_edit) {
#ifdef SIOYEK_QT6
        if (event->type() == QEvent::KeyRelease) {
            QKeyEvent* key_event = static_cast<QKeyEvent*>(event);
            if (should_trigger_delete(key_event)) {
                handle_delete();
            }
            else if (key_event->key() == Qt::Key_Insert) {
                handle_edit();
            }
        }
#endif
        if (event->type() == QEvent::InputMethod) {
            if (TOUCH_MODE) {
                QInputMethodEvent* input_event = static_cast<QInputMethodEvent*>(event);
                QString text = input_event->preeditString();
                if (input_event->commitString().size() > 0) {
                    text = input_event->commitString();
                }
                if (text.size() > 0) {
                    on_text_changed(text);
                }
            }
        }
        if ((event->type() == QEvent::KeyPress)) {
            QKeyEvent* key_event = static_cast<QKeyEvent*>(event);
            bool is_control_pressed = key_event->modifiers().testFlag(Qt::ControlModifier) || key_event->modifiers().testFlag(Qt::MetaModifier);

            if (TOUCH_MODE) {
                if (key_event->key() == Qt::Key_Back) {
                    return false;
                }
            }
            if (key_event->key() == Qt::Key_Down ||
                key_event->key() == Qt::Key_Up ||
                key_event->key() == Qt::Key_Left ||
                key_event->key() == Qt::Key_Right
                ) {
                if (!line_edit->hasSelectedText()){
                    bool is_tree = get_view_stylesheet_type_name() == "QTreeView";
                    if (is_tree || (key_event->key() == Qt::Key_Down) || (key_event->key() == Qt::Key_Up)){
#ifdef SIOYEK_QT6
                        QKeyEvent* newEvent = key_event->clone();
#else
                        QKeyEvent* newEvent = new QKeyEvent(*key_event);
#endif
                        QCoreApplication::postEvent(get_view(), newEvent);
                        //QCoreApplication::postEvent(tree_view, key_event);
                        return true;
                    }
                }
            }
            if (key_event->key() == Qt::Key_Tab) {
                QKeyEvent* new_key_event = new QKeyEvent(key_event->type(), Qt::Key_Down, key_event->modifiers());
                QCoreApplication::postEvent(get_view(), new_key_event);
                return true;
            }
            if ((key_event->key() == Qt::Key_PageDown)) {
                QKeyEvent* new_key_event = new QKeyEvent(key_event->type(), Qt::Key_PageDown, key_event->modifiers());
                QCoreApplication::postEvent(get_view(), new_key_event);
                return true;
            }
            if ((key_event->key() == Qt::Key_PageUp)) {
                QKeyEvent* new_key_event = new QKeyEvent(key_event->type(), Qt::Key_PageUp, key_event->modifiers());
                QCoreApplication::postEvent(get_view(), new_key_event);
                return true;
            }
            if (key_event->key() == Qt::Key_Backtab) {
                QKeyEvent* new_key_event = new QKeyEvent(key_event->type(), Qt::Key_Up, key_event->modifiers());
                QCoreApplication::postEvent(get_view(), new_key_event);
                return true;
            }
            if (((key_event->key() == Qt::Key_C) && is_control_pressed)) {
                if (!line_edit->hasSelectedText()){
                    std::wstring text = get_selected_text();
                    if (text.size() > 0) {
                        copy_to_clipboard(text);
                    }
                    return true;
                }
            }
            if (key_event->key() == Qt::Key_Return || key_event->key() == Qt::Key_Enter) {
                std::optional<QModelIndex> selected_index = get_selected_index();
                if (selected_index) {
                    on_select(selected_index.value());
                }
                else {
                    on_return_no_select(line_edit->text());
                }
                return true;
            }

        }
    }
    return false;
}

void BaseSelectorWidget::simulate_end() {
    get_view()->scrollToBottom();
    int last_index = get_view()->model()->rowCount() - 1;
    get_view()->setCurrentIndex(get_view()->model()->index(last_index, 0));
}

void BaseSelectorWidget::simulate_home() {
    get_view()->scrollToTop();
    get_view()->setCurrentIndex(get_view()->model()->index(0, 0));

}
void BaseSelectorWidget::simulate_page_down() {
    QKeyEvent* new_key_event = new QKeyEvent(QEvent::Type::KeyPress, Qt::Key_PageDown, Qt::KeyboardModifier::NoModifier);
    QCoreApplication::postEvent(get_view(), new_key_event);
}
bool is_tree_view_index_last(const QModelIndex& index, QTreeView* view, bool include_chilren){
    bool index_has_children = view->model()->rowCount(index) > 0;
    if (include_chilren && index_has_children) return false;
    if (index.parent().isValid()){
        int parent_row_count = view->model()->rowCount(index.parent());
        if (index.row() == (parent_row_count - 1)){
            return is_tree_view_index_last(index.parent(), view, false);
        }
        else{
            return false;
        }
    }
    else{
        return index.row() == view->model()->rowCount() - 1;
    }
}

bool is_tree_view_index_first(const QModelIndex& index, QTreeView* /*view*/){

    if (index.parent().isValid()){
        return false;
    }
    return index.row() == 0;
}

void BaseSelectorWidget::simulate_page_up() {
    QKeyEvent* new_key_event = new QKeyEvent(QEvent::Type::KeyPress, Qt::Key_PageUp, Qt::KeyboardModifier::NoModifier);
    QCoreApplication::postEvent(get_view(), new_key_event);
}


void BaseSelectorWidget::simulate_move_down() {
    bool is_last = false;
    if (dynamic_cast<QTreeView*>(get_view())){
        QTreeView* tree_view = dynamic_cast<QTreeView*>(get_view());
        const QModelIndex& tree_index = tree_view->currentIndex();
        is_last = is_tree_view_index_last(tree_index, tree_view, true);
    }
    else{
        is_last = get_view()->currentIndex().row() == get_view()->model()->rowCount() - 1;
    }

    if (is_last){
        get_view()->setCurrentIndex(get_view()->model()->index(0, 0));
    }
    else{
        QKeyEvent* move_down_event = new QKeyEvent(QEvent::Type::KeyPress, Qt::Key_Down, Qt::KeyboardModifier::NoModifier);
        QCoreApplication::postEvent(get_view(), move_down_event);
    }
}

QModelIndex get_last_tree_index(QAbstractItemModel* model, QModelIndex parent_index){
    int row_count = model->rowCount(parent_index);
    if (row_count == 0){
        return parent_index;
    }
    else{
        QModelIndex last_child = model->index(row_count - 1, 0, parent_index);
        return get_last_tree_index(model, last_child);
    }
}

void BaseSelectorWidget::simulate_move_up() {
    bool is_first = false;
    if (dynamic_cast<QTreeView*>(get_view())){
        QTreeView* tree_view = dynamic_cast<QTreeView*>(get_view());
        const QModelIndex& tree_index = tree_view->currentIndex();
        is_first = is_tree_view_index_first(tree_index, tree_view);
    }
    else{
        is_first = get_view()->currentIndex().row() == 0;
    }

    if (is_first) {
        get_view()->setCurrentIndex(get_last_tree_index(get_view()->model(), QModelIndex()));
    }
    else{
        QKeyEvent* move_up_event = new QKeyEvent(QEvent::Type::KeyPress, Qt::Key_Up, Qt::KeyboardModifier::NoModifier);
        QCoreApplication::postEvent(get_view(), move_up_event);
    }}

void BaseSelectorWidget::simulate_move_left() {
    QKeyEvent* move_left_event = new QKeyEvent(QEvent::Type::KeyPress, Qt::Key_Left, Qt::KeyboardModifier::NoModifier);
    QCoreApplication::postEvent(get_view(), move_left_event);
}

void BaseSelectorWidget::simulate_move_right() {
    QKeyEvent* move_right_event = new QKeyEvent(QEvent::Type::KeyPress, Qt::Key_Right, Qt::KeyboardModifier::NoModifier);
    QCoreApplication::postEvent(get_view(), move_right_event);
}

QString BaseSelectorWidget::get_selected_item() {
    if (get_selected_index()) {
        return get_view()->model()->data(get_selected_index().value()).toString();
    }
    return "";
}

void BaseSelectorWidget::simulate_select() {
    std::optional<QModelIndex> selected_index = get_selected_index();
    if (selected_index) {
        on_select(selected_index.value());
    }
    else {
        on_return_no_select(line_edit->text());
    }
}

void BaseSelectorWidget::handle_delete() {
    QModelIndexList selected_index_list = get_view()->selectionModel()->selectedIndexes();
    if (selected_index_list.size() > 0) {
        QModelIndex selected_index = selected_index_list.at(0);
        if (proxy_model->hasIndex(selected_index.row(), selected_index.column())) {
            QModelIndex source_index = proxy_model->mapToSource(selected_index);
            on_delete(source_index, selected_index);
        }
    }
}

void BaseSelectorWidget::handle_edit() {
    QModelIndexList selected_index_list = get_view()->selectionModel()->selectedIndexes();
    if (selected_index_list.size() > 0) {
        QModelIndex selected_index = selected_index_list.at(0);
        if (proxy_model->hasIndex(selected_index.row(), selected_index.column())) {
            QModelIndex source_index = proxy_model->mapToSource(selected_index);
            on_edit(source_index, selected_index);
        }
    }
}

#ifndef SIOYEK_QT6
    void BaseSelectorWidget::keyReleaseEvent(QKeyEvent* event) {
		if (should_trigger_delete(event)) {
            handle_delete();
        }
        QWidget::keyReleaseEvent(event);
    }
#endif

void BaseSelectorWidget::on_config_file_changed() {
    QString font_size_stylesheet = "";
    if (FONT_SIZE > 0) {
        font_size_stylesheet = QString("font-size: %1px").arg(FONT_SIZE);
    }

    setStyleSheet(get_ui_stylesheet(true) + font_size_stylesheet);
    get_view()->setStyleSheet(get_view_stylesheet_type_name() + "::item::selected{" + get_selected_stylesheet() + "}");
}

void BaseSelectorWidget::resizeEvent(QResizeEvent* resize_event) {
    QWidget::resizeEvent(resize_event);
    QWidget* viewport = parentWidget();
#ifndef SIOYEK_ANDROID
    // The left MainWidget owns the entire split window; its canvas is the
    // actual pane. Keep the MainWidget parent for command/focus ownership.
    viewport = static_cast<MainWidget*>(parentWidget())->central_widget;
#endif
    int parent_width = viewport->width();
    int parent_height = viewport->height();
    setFixedSize(parent_width * MENU_SCREEN_WDITH_RATIO, parent_height);
    move(viewport->mapTo(parentWidget(), QPoint(parent_width * (1 - MENU_SCREEN_WDITH_RATIO) / 2, 0)));
    on_config_file_changed();
}


MyLineEdit::MyLineEdit(MainWidget* parent) : QLineEdit(parent) {
    main_widget = parent;
}

int MyLineEdit::get_next_word_position() {
    int current_position = cursorPosition();

    int whitespace_position = text().indexOf(QRegularExpression("\\s"), current_position);
    if (whitespace_position == -1) {
        return text().size();
    }
    int next_word_position = text().indexOf(QRegularExpression("\\S"), whitespace_position);

    if (next_word_position == -1) {
        return text().size();
    }
    return next_word_position;
}

int MyLineEdit::get_prev_word_position() {
    int current_position = qMax(cursorPosition() -1, 0);

    int whitespace_position = text().lastIndexOf(QRegularExpression("\\s"), current_position);
    if (whitespace_position == -1) {
        return 0;
    }
    int prev_word_position = text().lastIndexOf(QRegularExpression("\\S"), whitespace_position);

    if (prev_word_position == -1) {
        return 0;
    }

    return qMin(prev_word_position + 1, text().size());
}

void MyLineEdit::keyPressEvent(QKeyEvent* event) {

    bool is_alt_pressed = event->modifiers() & Qt::AltModifier;
    bool is_control_pressed = is_platform_control_pressed(event);
    bool is_shift_pressed = event->modifiers() & Qt::ShiftModifier;
    bool is_meta_pressed = is_platform_meta_pressed(event);
    bool is_invisible = event->text().size() == 0;
    if (is_invisible || is_alt_pressed || is_control_pressed) {
        std::unique_ptr<Command> command = main_widget->input_handler->get_menu_command(main_widget, event, is_shift_pressed, is_control_pressed, is_meta_pressed, is_alt_pressed);

        if (command && command->is_menu_command()) {
            // this command will be handled later by our command manager so we ignore it here.
            event->ignore();
            return;
        }
    }

    return QLineEdit::keyPressEvent(event);

}

void MyLineEdit::paintEvent(QPaintEvent* event) {
    QLineEdit::paintEvent(event);
    if (property("vimNormalCursor").toBool() && hasFocus() && !hasSelectedText()) {
        QRect cursor = cursorRect();
        // Qt's line-edit cursor rectangle includes five pixels of repaint
        // padding to the left of the actual text insertion position.
        cursor.translate(5, 0);
        // Keep one character cell even over narrow letters, spaces or fallback glyphs.
        cursor.setWidth(std::max(2, fontMetrics().horizontalAdvance(QLatin1Char('M'))));
        QColor color = palette().color(QPalette::Text); color.setAlpha(90);
        QPainter painter(this);
        painter.setClipRect(contentsRect().adjusted(2, 2, -2, -2));
        painter.fillRect(cursor, color);
    }
}

namespace {
constexpr int PickerEntryRole = Qt::UserRole + 20;
constexpr int PickerTitleRole = Qt::UserRole + 21;

QString search_key(QString text) {
    text = text.normalized(QString::NormalizationForm_KD).toCaseFolded();
    text.remove(QRegularExpression("[\\p{M}]"));
    text.replace(QRegularExpression("[^\\p{L}\\p{N}]+"), " ");
    return text.simplified();
}

int document_match(const DocumentPickerEntry& entry, const QString& query, bool fuzzy) {
    if (query.isEmpty()) return 1;
    const QString& name = entry.search_name;
    const QString& title = entry.search_title;
    if (name == query || title == query) return 1200;
    if (name.contains(query) || title.contains(query)) return 1000;
    // Match words in any order, including a mixture of filename and title.
    const QString primary = name + " " + title;
    const QString& other = entry.search_other;
    int score = 0;
    const auto words = query.split(' ', Qt::SkipEmptyParts);
    const auto candidates = primary.split(' ', Qt::SkipEmptyParts);
    for (const auto& word : words) {
        if (primary.contains(word)) { score += 90; continue; }
        if (other.contains(word)) { score += 65; continue; }
        int best = 0;
        if (fuzzy && word.size() >= 4) {
            for (const auto& candidate : candidates)
                best = std::max(best, int(rapidfuzz::fuzz::ratio(word.toStdWString(), candidate.toStdWString())));
        }
        if (best < 75) return 0;
        score += best / 2;
    }
    return score / std::max(1, int(words.size()));
}

class DocumentPickerList : public QListView {
protected:
    void keyPressEvent(QKeyEvent* event) override {
        QListView::keyPressEvent(event);
        // Qt ignores navigation keys when there is nowhere left to move. Keep
        // those events inside the floating picker instead of passing to the PDF.
        event->accept();
    }
};

class DocumentPickerDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    QSize sizeHint(const QStyleOptionViewItem&, const QModelIndex&) const override { return QSize(0, 54); }
    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        QStyleOptionViewItem opt(option);
        initStyleOption(&opt, index);
        const QString filename = opt.text;
        const QString title = index.data(PickerTitleRole).toString();
        opt.text.clear();
        opt.state &= ~QStyle::State_HasFocus;
        const auto style = opt.widget ? opt.widget->style() : QApplication::style();
        style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, opt.widget);
        painter->save();
        const bool selected = opt.state & QStyle::State_Selected;
        const QColor color = opt.palette.color(selected ? QPalette::HighlightedText : QPalette::Text);
        painter->setPen(color);
        painter->setFont(opt.font);
        const QRect box = opt.rect.adjusted(10, 4, -10, -4);
        if (title.isEmpty()) {
            painter->drawText(box, Qt::AlignVCenter, opt.fontMetrics.elidedText(filename, Qt::ElideMiddle, box.width()));
        } else {
            painter->drawText(box.adjusted(0, 0, 0, -22), Qt::AlignVCenter,
                opt.fontMetrics.elidedText(filename, Qt::ElideMiddle, box.width()));
            QFont small = opt.font;
            small.setPointSizeF(std::max(9.0, small.pointSizeF() - 1));
            painter->setFont(small);
            QColor secondary = color;
            if (!selected) secondary.setAlphaF(0.78);
            painter->setPen(secondary);
            painter->drawText(box.adjusted(0, 24, 0, 0), Qt::AlignVCenter,
                QFontMetrics(small).elidedText(title, Qt::ElideRight, box.width()));
        }
        painter->restore();
    }
};
}

DocumentTitleIndex::DocumentTitleIndex(QObject* parent) : QObject(parent), worker_([this]() { run(); }) {}
DocumentTitleIndex::~DocumentTitleIndex() {
    { std::lock_guard<std::mutex> lock(mutex_); stopping_ = true; requests_.clear(); }
    ready_.notify_one();
    worker_.join();
}
DocumentTitleIndex* DocumentTitleIndex::instance() {
    static QPointer<DocumentTitleIndex> index;
    if (!index) index = new DocumentTitleIndex(qApp);
    return index;
}
void DocumentTitleIndex::request(const QString& path, const std::shared_ptr<int>& lifetime) {
    if (QFileInfo(path).suffix().compare("pdf", Qt::CaseInsensitive) != 0) return;
    { std::lock_guard<std::mutex> lock(mutex_); requests_.push_back({path, lifetime}); }
    ready_.notify_one();
}
void DocumentTitleIndex::run() {
    // This context and every document it opens belong exclusively to this thread.
    fz_context* context = fz_new_context(nullptr, nullptr, 32 << 20);
    if (!context) return;
    fz_register_document_handlers(context);
    struct Cached { qint64 size; QDateTime modified; QString title; QString words; };
    QHash<QString, Cached> cache;
    while (true) {
        Request request;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            ready_.wait(lock, [&]() { return stopping_ || !requests_.empty(); });
            if (stopping_) break;
            request = requests_.front(); requests_.pop_front();
        }
        if (request.lifetime.expired()) continue;
        const QFileInfo info(request.path);
        if (!info.isFile()) continue;
        auto cached = cache.constFind(request.path);
        if (cached != cache.cend() && cached->size == info.size() && cached->modified == info.lastModified()) {
            emit title_ready(request.path, cached->title, cached->words);
            continue;
        }
        QString title, words, detected;
        fz_document* document = nullptr;
        fz_stext_page* page = nullptr;
        bool success = false;
        fz_var(document); fz_var(page); fz_var(success);
        const QByteArray filename = request.path.toUtf8();
        fz_try(context) {
            document = fz_open_document(context, filename.constData());
            if (!fz_needs_password(context, document)) {
                char metadata[2048] = {};
                if (fz_lookup_metadata(context, document, FZ_META_INFO_TITLE, metadata, sizeof(metadata)) > 0)
                    title = QString::fromUtf8(metadata).simplified();
                if (fz_count_pages(context, document) > 0) {
                    page = fz_new_stext_page_from_page_number(context, document, 0, nullptr);
                    float largest = 0;
                    for (auto block = page->first_block; block && words.size() < 12000; block = block->next) {
                        if (block->type != FZ_STEXT_BLOCK_TEXT) continue;
                        QString text;
                        float sizes = 0; int count = 0;
                        for (auto line = block->u.t.first_line; line; line = line->next) {
                            for (auto ch = line->first_char; ch && text.size() < 12000; ch = ch->next) {
                                const char32_t codepoint = ch->c;
                                text += QString::fromUcs4(&codepoint, 1);
                                sizes += ch->size; ++count;
                            }
                            text += ' ';
                        }
                        text = text.simplified();
                        words += text + ' ';
                        if (text.size() >= 8 && text.size() <= 300 && count && sizes / count > largest) {
                            largest = sizes / count; detected = text;
                        }
                    }
                }
                words = (title + ' ' + words).left(14000);
                if (title.isEmpty() || title.compare("untitled", Qt::CaseInsensitive) == 0 ||
                    title == info.fileName() || title == info.completeBaseName()) title = detected;
                success = true;
            }
        }
        fz_always(context) { fz_drop_stext_page(context, page); fz_drop_document(context, document); }
        fz_catch(context) { /* An incomplete, encrypted or missing PDF remains searchable by filename. */ }
        if (success && QFileInfo(request.path).lastModified() == info.lastModified()) {
            if (cache.size() >= 8192 && !cache.contains(request.path)) cache.erase(cache.begin());
            cache.insert(request.path, {info.size(), info.lastModified(), title, words});
            if (!request.lifetime.expired()) emit title_ready(request.path, title, words);
        }
    }
    fz_drop_context(context);
}

FloatingTextPrompt::FloatingTextPrompt(MainWidget* pane) : QWidget(pane), pane_(pane) {
    setObjectName("textPrompt");
    setAttribute(Qt::WA_StyledBackground);
    QFont text_font = font(); text_font.setPointSize(12); setFont(text_font);
    auto box = new QVBoxLayout(this);
    box->setContentsMargins(14, 12, 14, 12); box->setSpacing(8);
    heading = new QLabel(this);
    QFont title_font = text_font; title_font.setBold(true); heading->setFont(title_font);
    heading->setTextFormat(Qt::PlainText);
    edit = new MyLineEdit(pane);
    edit->setObjectName("textPromptSearch");
    QFont mono("JetBrains Mono", 12);
    mono.setStyleHint(QFont::Monospace); mono.setFixedPitch(true); mono.setKerning(false);
    edit->setFont(mono); edit->setMinimumHeight(34);
    mode_label_ = new QLabel(this);
    box->addWidget(heading); box->addWidget(edit); box->addWidget(mode_label_);
    edit->installEventFilter(this);
    on_config_file_changed(nullptr);
    hide();
}
FloatingTextPrompt::~FloatingTextPrompt() = default;
void FloatingTextPrompt::begin(const QString& title, const QString& value, bool password, bool insert_mode) {
    if (!pane_) return;
    if (parentWidget()) parentWidget()->removeEventFilter(this);
    setParent(pane_->tab_owner());
    parentWidget()->installEventFilter(this);
    heading->setText(title);
    editor_.reset();
    edit->setReadOnly(false); edit->setProperty("vimNormalCursor", false);
    edit->setEchoMode(password ? QLineEdit::Password : QLineEdit::Normal);
    edit->setText(value); edit->setCursorPosition(insert_mode ? int(value.size()) : 0);
    // Password input remains ordinary masked input; it must not yank secrets.
    if (!password) {
        editor_ = std::make_unique<PickerVimEditor>(edit, [](int) {}, [this](bool last) {
            edit->setCursorPosition(last ? std::max(0, int(edit->text().size()) - 1) : 0);
        }, false, insert_mode ? PickerVimEditor::Mode::Insert : PickerVimEditor::Mode::Normal);
    }
    mode_label_->setText(editor_ ? editor_->label() : "PASSWORD");
    update_geometry(); show(); raise(); edit->setFocus();
}
void FloatingTextPrompt::update_geometry() {
    if (!parentWidget()) return;
    const int w = std::max(1, std::min(560, parentWidget()->width() - 48));
    const int h = std::max(1, std::min(sizeHint().height(), parentWidget()->height() - 48));
    setGeometry((parentWidget()->width()-w)/2, (parentWidget()->height()-h)/2, w, h);
}
void FloatingTextPrompt::on_config_file_changed(ConfigManager*) {
    setStyleSheet(floating_selector_stylesheet("textPrompt", "QListView"));
    update_geometry();
}
bool FloatingTextPrompt::eventFilter(QObject* object, QEvent* event) {
    if (object == parentWidget() && event->type() == QEvent::Resize) update_geometry();
    if (object != edit || !pane_) return false;
    if (event->type() == QEvent::ShortcutOverride) {
        event->accept(); return true;
    }
    if (event->type() == QEvent::MouseButtonPress && editor_) {
        editor_->mouse_pressed(); mode_label_->setText(editor_->label());
    }
    if (event->type() != QEvent::KeyPress) return false;
    auto key = static_cast<QKeyEvent*>(event);
    if (editor_) {
        const auto result = editor_->key(key);
        mode_label_->setText(editor_->label());
        if (result == PickerVimEditor::Result::Close) {
            pane_->hide_command_line_edit(); return true;
        }
        if (result == PickerVimEditor::Result::Handled) return true;
    }
    if (key->key() == Qt::Key_Escape) {
        pane_->hide_command_line_edit(); return true;
    }
    if (key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter) {
        const auto value = edit->text().toStdWString();
        hide(); pane_->setFocus(); pane_->handle_pending_text_command(value);
        return true;
    }
    // Retain command-specific completion/history keys, owned by the invoking pane.
    auto command = pane_->input_handler->get_menu_command(pane_, key,
        key->modifiers() & Qt::ShiftModifier, is_platform_control_pressed(key),
        is_platform_meta_pressed(key), key->modifiers() & Qt::AltModifier);
    if (command && command->is_menu_command()) {
        pane_->key_event(false, key, key->isAutoRepeat()); return true;
    }
    // Call QLineEdit directly so ignored keys cannot bubble through to PDF actions.
    edit->native_key_press(key);
    return true;
}

DocumentPicker::DocumentPicker(QString heading, bool fuzzy, MainWidget* pane,
    std::vector<DocumentPickerEntry> entries, std::function<void(QString)> on_done,
    std::function<bool(QString)> on_delete, bool document_entries) :
    BaseSelectorWidget(new DocumentPickerList(), false, nullptr, pane),
    on_done_(std::move(on_done)), on_delete_(std::move(on_delete)), pane_(pane), fuzzy_(fuzzy), document_entries_(document_entries) {
    // Render over the whole window, but keep command/keyboard ownership in the
    // originating pane. In particular a right-pane picker must not open on the left.
    setParent(pane->tab_owner());
    setObjectName("documentPicker");
    setAttribute(Qt::WA_StyledBackground);
    QFont text_font = font(); text_font.setPointSize(12); setFont(text_font);
    // This font is bundled with Sioyek, so Vim's cursor and character positions
    // use equal-width cells without requiring a separately installed font.
    QFont search_font("JetBrains Mono", 12);
    search_font.setStyleHint(QFont::Monospace);
    search_font.setFixedPitch(true);
    search_font.setKerning(false);
    line_edit->setFont(search_font);
    line_edit->setObjectName("documentPickerSearch");
    line_edit->setPlaceholderText(document_entries_ ? "Search filenames or PDF titles…" : "Search workspaces…");
    if (!document_entries_) empty_message_ = "No workspaces yet — save one with Cmd+Shift+S";
    line_edit->setClearButtonEnabled(true);
    line_edit->setMinimumHeight(34);
    vim_editor_ = std::make_unique<PickerVimEditor>(line_edit, [this](int delta) {
        const int count = proxy_model->rowCount();
        if (!count) return;
        const int current = std::max(0, abstract_item_view->currentIndex().row());
        const auto target = proxy_model->index(std::clamp(current + delta, 0, count - 1), 0);
        abstract_item_view->setCurrentIndex(target);
        abstract_item_view->scrollTo(target);
    }, [this](bool last) {
        const int count = proxy_model->rowCount();
        if (!count) return;
        const auto target = proxy_model->index(last ? count - 1 : 0, 0);
        abstract_item_view->setCurrentIndex(target);
        abstract_item_view->scrollTo(target);
    });
    auto layout = static_cast<QVBoxLayout*>(this->layout());
    layout->setContentsMargins(14, 12, 14, 12); layout->setSpacing(8);
    heading_ = new QLabel(heading, this);
    QFont bold = text_font; bold.setBold(true); heading_->setFont(bold);
    mode_label_ = new QLabel(vim_editor_->label(), this);
    mode_label_->setObjectName("documentPickerMode");
    mode_label_->setAccessibleName("Search editing mode");
    QFont mode_font = text_font; mode_font.setPointSize(10); mode_label_->setFont(mode_font);
    auto heading_row = new QHBoxLayout();
    heading_row->addWidget(heading_); heading_row->addStretch(); heading_row->addWidget(mode_label_);
    layout->insertLayout(0, heading_row);
    detail_ = new QLabel(this); detail_->setObjectName("documentPickerDetail"); detail_->setTextFormat(Qt::PlainText); detail_->setMinimumWidth(0);
    layout->addWidget(detail_);
    if (document_entries_) {
        layout->removeWidget(abstract_item_view);
        auto results = new QHBoxLayout;
        results->setSpacing(12);
        results->addWidget(abstract_item_view, 1);
        preview_ = new PdfPickerPreview(this, pane->opengl_widget);
        results->addWidget(preview_, 1);
        layout->insertLayout(2, results, 1);
    }
    model_ = new QStandardItemModel(this);
    proxy_model->setParent(this);
    proxy_model->setSourceModel(model_);
    abstract_item_view->setFont(text_font);
    abstract_item_view->setObjectName("documentPickerList");
    abstract_item_view->setItemDelegate(new DocumentPickerDelegate(this));
    abstract_item_view->setFrameShape(QFrame::NoFrame);
    abstract_item_view->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    abstract_item_view->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    abstract_item_view->setFocusPolicy(Qt::NoFocus);
    static_cast<QListView*>(abstract_item_view)->setUniformItemSizes(true);
    connect(abstract_item_view->selectionModel(), &QItemSelectionModel::currentChanged,
        this, [this]() { update_detail(); });
    refresh_timer_.setSingleShot(true); refresh_timer_.setInterval(40);
    connect(&refresh_timer_, &QTimer::timeout, this, [this]() { rebuild_results(true); });
    connect(DocumentTitleIndex::instance(), &DocumentTitleIndex::title_ready, this,
        [this](const QString& path, const QString& title, const QString& words) {
            if (!document_entries_ || content_results_) return;
            bool changed = false;
            for (auto& entry : entries_) if (entry.path == path) {
                if (!title.isEmpty()) entry.title = title;
                entry.words = words;
                entry.search_title = search_key(entry.title);
                entry.search_other = search_key(entry.path + " " + words);
                changed = true;
            }
            if (changed && !refresh_timer_.isActive()) refresh_timer_.start();
        });
    parentWidget()->installEventFilter(this);
    set_entries(std::move(entries));
    on_config_file_changed();
}
DocumentPicker::~DocumentPicker() { indexing_lifetime_.reset(); }
bool DocumentPicker::search_insert_mode() const { return vim_editor_->inserting(); }
void DocumentPicker::set_entries(std::vector<DocumentPickerEntry> entries, const QString& selected) {
    indexing_lifetime_ = std::make_shared<int>(0);
    entries_ = std::move(entries);
    for (auto& entry : entries_) {
        entry.search_name = search_key(document_entries_ ? QFileInfo(entry.path).fileName() : entry.path);
        entry.search_title = search_key(entry.title);
        entry.search_other = search_key(entry.path + " " + entry.words);
    }
    rebuild_results(false,selected);
    for (const auto& entry : entries_) if (document_entries_ && !content_results_ && !entry.directory)
        DocumentTitleIndex::instance()->request(entry.path, indexing_lifetime_);
}
void DocumentPicker::rebuild_results(bool preserve_selection, QString selected) {
    QSignalBlocker block_selection(abstract_item_view->selectionModel());
    auto current = abstract_item_view->currentIndex();
    if (preserve_selection && current.isValid()) selected = entries_[current.data(PickerEntryRole).toInt()].value;
    const QString query = search_key(line_edit->text());
    std::vector<std::pair<int, int>> matches;
    for (int i = 0; i < int(entries_.size()); ++i) {
        const int score = content_results_ ? 1 : document_match(entries_[i], query, fuzzy_);
        if (score) matches.push_back({i, score});
    }
    std::stable_sort(matches.begin(), matches.end(), [](auto a, auto b) { return a.second > b.second; });
    model_->clear();
    int selection = 0;
    for (const auto& match : matches) {
        const auto& entry = entries_[match.first];
        QString name = document_entries_ ? QFileInfo(entry.path).fileName() : entry.path;
        if (name.isEmpty()) name = entry.path;
        if (entry.directory) name += '/';
        if (entry.page >= 0 && !content_results_) name += QString("  ·  p. %1").arg(entry.page+1);
        auto item = new QStandardItem(name);
        item->setData(match.first, PickerEntryRole);
        QString subtitle = entry.title.simplified();
        if (show_folders_) {
            QString folder = QFileInfo(entry.path).absolutePath();
            if (!location_.isEmpty()) folder = QDir(location_).relativeFilePath(folder);
            else if (folder.startsWith(QDir::homePath() + '/')) folder.replace(0, QDir::homePath().size(), "~");
            subtitle = folder + (subtitle.isEmpty() ? QString() : "  ·  " + subtitle);
        }
        if (content_results_) subtitle += QString("  ·  %1 matching page%2").arg(entry.matching_pages.size()).arg(entry.matching_pages.size()==1 ? "" : "s");
        else if (!entry.snippet.isEmpty()) subtitle = entry.snippet;
        item->setData(subtitle, PickerTitleRole);
        item->setToolTip(entry.path);
        if (!selected.isEmpty() && entry.value == selected) selection = model_->rowCount();
        model_->appendRow(item);
    }
    abstract_item_view->setCurrentIndex(proxy_model->index(selection, 0));
    abstract_item_view->scrollTo(abstract_item_view->currentIndex());
    update_geometry(); update_detail();
}
bool DocumentPicker::on_text_change(const QString&) { rebuild_results(); return true; }
void DocumentPicker::on_select(const QModelIndex& index) {
    if (!index.isValid()) return;
    const int row = index.data(PickerEntryRole).toInt();
    if (row >= 0 && row < int(entries_.size())) activate(entries_[row]);
}
void DocumentPicker::activate(const DocumentPickerEntry& entry) {
    const QString value = entry.value;
    hide();
    if (pane_) pane_->setFocus();
    if (on_done_) on_done_(value);
}
void DocumentPicker::on_delete(const QModelIndex& source, const QModelIndex& selected) {
    if (!on_delete_ || !source.isValid()) return;
    const int row = source.data(PickerEntryRole).toInt();
    const QString value = entries_[row].value;
    // Keep the entry visible if persistence fails.
    if (!on_delete_(value)) return;
    // Use the visible result order, including searches, rather than history order.
    const int selected_row = selected.row();
    entries_.erase(entries_.begin() + row);
    rebuild_results();
    // The next result moves into the deleted row; at the end, use the new last row.
    const auto next = proxy_model->index(std::min(selected_row, proxy_model->rowCount() - 1), 0);
    abstract_item_view->setCurrentIndex(next);
    abstract_item_view->scrollTo(next);
}
std::wstring DocumentPicker::get_selected_text() {
    auto index = abstract_item_view->currentIndex();
    return index.isValid() ? entries_[index.data(PickerEntryRole).toInt()].path.toStdWString() : L"";
}
void DocumentPicker::update_detail() {
    const auto index = abstract_item_view->currentIndex();
    const QString text = index.isValid() ? entries_[index.data(PickerEntryRole).toInt()].path + (search_status_.isEmpty() ? QString() : "  ·  " + search_status_) :
        (content_results_ || line_edit->text().isEmpty() ? empty_message_ : document_entries_ ? "No matching documents" : "No matching workspaces");
    detail_->setText(detail_->fontMetrics().elidedText(text, Qt::ElideMiddle, std::max(1, width() - 28)));
    detail_->setToolTip(text);
    if (preview_) {
        if (index.isValid()) {
            const auto& entry = entries_[index.data(PickerEntryRole).toInt()];
            preview_->select(entry.directory ? QString() : entry.path, std::max(0,entry.page), line_edit->text(),entry.matching_pages);
            QList<PdfPreviewTarget> nearby;
            for(int distance=1;distance<=2;++distance) for(int direction:{1,-1}) {
                const auto neighbor=proxy_model->index(index.row()+direction*distance,0);
                if(!neighbor.isValid()) continue;
                const auto& candidate=entries_[neighbor.data(PickerEntryRole).toInt()];
                if(!candidate.directory) nearby.append({candidate.path,std::max(0,candidate.page)});
            }
            preview_->prefetch(std::move(nearby));
        } else preview_->select({},0,{});
    }
}
void DocumentPicker::update_geometry() {
    if (!parentWidget()) return;
    const int width = std::max(1, std::min(document_entries_ ? 1200 : 560, parentWidget()->width() - 48));
    // Like the tab switcher, leave no empty rows between results and the path.
    const int rows = std::max(1, model_->rowCount());
    const int height = std::max(1, std::min(24 + 24 + heading_->sizeHint().height() + 34 +
        detail_->sizeHint().height() + (document_entries_ ? 720 : rows * 54), parentWidget()->height() - 48));
    setGeometry((parentWidget()->width() - width) / 2, (parentWidget()->height() - height) / 2, width, height);
}
void DocumentPicker::resizeEvent(QResizeEvent* event) { QWidget::resizeEvent(event); update_geometry(); update_detail(); }
void DocumentPicker::showEvent(QShowEvent* event) {
    QWidget::showEvent(event); update_geometry(); raise(); line_edit->setFocus();
}
bool DocumentPicker::eventFilter(QObject* object, QEvent* event) {
    if (object == parentWidget() && event->type() == QEvent::Resize) update_geometry();
    if (object == line_edit && preview_ && event->type() == QEvent::KeyPress) {
        const auto key = static_cast<QKeyEvent*>(event);
        if (is_platform_control_pressed(key) && (key->key()==Qt::Key_D || key->key()==Qt::Key_L || key->key()==Qt::Key_U)) {
            preview_->change_page(key->key()==Qt::Key_D ? 1 : -1); return true;
        }
        if(content_results_ && !search_insert_mode() && key->modifiers()==Qt::NoModifier &&
            (key->key()==Qt::Key_BracketLeft || key->key()==Qt::Key_BracketRight)) {
            preview_->change_match(key->key()==Qt::Key_BracketRight ? 1 : -1);return true;
        }
    }
    if (object == line_edit && vim_editor_) {
        if (event->type() == QEvent::MouseButtonPress) {
            vim_editor_->mouse_pressed(); mode_label_->setText(vim_editor_->label());
        }
        if (event->type() == QEvent::ShortcutOverride) {
            auto key = static_cast<QKeyEvent*>(event);
            if (!search_insert_mode() || key->key() == Qt::Key_Escape ||
                (is_platform_control_pressed(key) && (key->key() == Qt::Key_C || key->key() == Qt::Key_BracketLeft))) {
                event->accept(); return true;
            }
        }
    }
    if (object == line_edit && on_delete_ &&
        (event->type() == QEvent::ShortcutOverride || event->type() == QEvent::KeyPress ||
         event->type() == QEvent::KeyRelease)) {
        auto key = static_cast<QKeyEvent*>(event);
        if (should_trigger_delete(key)) {
            // Claim history deletion before QLineEdit can alter the query and
            // selection. Swallow release as well so the base handler cannot
            // delete a second result; holding the shortcut deletes only once.
            if (event->type() == QEvent::KeyPress && !key->isAutoRepeat()) handle_delete();
            event->accept();
            return true;
        }
    }
    if (object == line_edit && event->type() == QEvent::KeyPress) {
        auto key = static_cast<QKeyEvent*>(event);
        const auto result = vim_editor_->key(key);
        mode_label_->setText(vim_editor_->label());
        if (result == PickerVimEditor::Result::YankPath) {
            const auto index = abstract_item_view->currentIndex();
            if (index.isValid()) {
                const QString text = document_entries_
                    ? QDir::cleanPath(QFileInfo(QString::fromStdWString(get_selected_text())).absoluteFilePath())
                    : index.data(Qt::DisplayRole).toString();
                if (!text.isEmpty()) QApplication::clipboard()->setText(text);
            }
            return true;
        }
        if (result == PickerVimEditor::Result::Close) {
            if (pane_) {
                QKeyEvent escape(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
                pane_->key_event(false, &escape, false);
            }
            return true;
        }
        if (result == PickerVimEditor::Result::Handled) return true;
        // Up/down navigate results even when the search text is selected.
        if (key->key() == Qt::Key_Up || key->key() == Qt::Key_Down) {
            QKeyEvent navigation(QEvent::KeyPress, key->key(), Qt::NoModifier,
                QString(), key->isAutoRepeat(), key->count());
            QCoreApplication::sendEvent(get_view(), &navigation);
            return true;
        }
    }
    if (object == line_edit && event->type() == QEvent::KeyRelease && !search_insert_mode()) return true;
    if (BaseSelectorWidget::eventFilter(object, event)) return true;
    if (object == line_edit && event->type() == QEvent::KeyPress && pane_) {
        auto key = static_cast<QKeyEvent*>(event);
        // The floating panel is parented to the outer window. Route menu keys
        // explicitly so a right-pane search does not send them to the left pane.
        auto command = pane_->input_handler->get_menu_command(pane_, key,
            key->modifiers() & Qt::ShiftModifier, is_platform_control_pressed(key),
            is_platform_meta_pressed(key), key->modifiers() & Qt::AltModifier);
        if (key->key() == Qt::Key_Escape || (command && command->is_menu_command())) {
            pane_->key_event(false, key, key->isAutoRepeat());
            return true;
        }
    }
    return false;
}
void DocumentPicker::on_config_file_changed() {
    setStyleSheet(floating_selector_stylesheet("documentPicker", "QListView"));
    if (preview_) {
        preview_->setStyleSheet(get_ui_stylesheet(true));
        preview_->update();
    }
    update_geometry();
}

PdfLibraryIndex::PdfLibraryIndex(QObject* parent) : QObject(parent), worker_([this]() { run(); }) {
    connect(this, &PdfLibraryIndex::scanned, this,
        [this](const QString& root, const QStringList& paths, const QString& version, const QString& error) {
            pending_.remove(root);
            cache_[root] = {paths, version, error};
        });
}
PdfLibraryIndex::~PdfLibraryIndex() {
    { std::lock_guard<std::mutex> lock(mutex_); stopping_ = true; }
    ready_.notify_one();
    worker_.join();
}
PdfLibraryIndex* PdfLibraryIndex::instance() {
    static QPointer<PdfLibraryIndex> index;
    if (!index) index = new PdfLibraryIndex(qApp);
    return index;
}
void PdfLibraryIndex::request(const QString& root) {
    if (pending_.contains(root)) return;
    pending_.insert(root);
    { std::lock_guard<std::mutex> lock(mutex_); requests_.push_back(root); }
    ready_.notify_one();
}
void PdfLibraryIndex::run() {
    while (true) {
        QString root;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            ready_.wait(lock, [&]() { return stopping_ || !requests_.empty(); });
            if (stopping_) return;
            root = requests_.front(); requests_.pop_front();
        }
        QStringList paths;
        QString error;
        const QFileInfo root_info(root);
        if (!root_info.isDir() || !root_info.isReadable()) {
            error = "Library folder unavailable: " + root;
        } else {
            // No FollowSymlinks: avoid directory cycles and scans outside the root.
            QDirIterator files(root, QDir::Files | QDir::Readable | QDir::NoSymLinks,
                QDirIterator::Subdirectories);
            while (files.hasNext()) {
                if (stopping_) return;
                files.next();
                if (files.fileInfo().suffix().compare("pdf", Qt::CaseInsensitive) == 0 &&
                    !is_blank_canvas(files.filePath().toStdWString()))
                    paths.append(files.filePath());
            }
        }
        paths.sort(Qt::CaseInsensitive);
        QCryptographicHash hash(QCryptographicHash::Sha256);
        for (const auto& path : paths) {
            if (stopping_) return;
            const QFileInfo info(path);
            hash.addData(path.toUtf8()); hash_add_data(hash, "\0", 1);
            hash.addData(QByteArray::number(info.size())); hash_add_data(hash, "\0", 1);
            hash.addData(QByteArray::number(info.lastModified().toMSecsSinceEpoch()));
        }
        hash.addData(error.toUtf8());
        emit scanned(root, paths, QString::fromLatin1(hash.result().toHex()), error);
    }
}

LibrarySelector::LibrarySelector(bool fuzzy, std::function<void(QString)> on_done, MainWidget* pane, QString root, bool contents) :
    DocumentPicker(contents ? "Search PDF contents" : "PDF library", fuzzy, pane, {}, std::move(on_done)), contents_(contents) {
    content_results_ = contents;
    search_timer_.setSingleShot(true); search_timer_.setInterval(220);
    connect(&search_timer_, &QTimer::timeout, this, [this]{ search_contents(); });
    content_worker_.search_ready = [this](QList<PdfContentHit> hits, int done, int total, bool finished, int failures) {
        const auto current = abstract_item_view->currentIndex();
        const QString selected = current.isValid() ? entries_[current.data(PickerEntryRole).toInt()].value : QString();
        for (const auto& hit : hits) {
            DocumentPickerEntry entry{hit.path, {}, {}, hit.path, false};
            entry.page = hit.page; entry.matching_pages=hit.matching_pages; entries_.push_back(std::move(entry));
        }
        empty_message_ = finished ? "No matching text (scanned images need OCR)" : "Searching PDF text…";
        search_status_ = QString("%1/%2 files · %3 matching PDFs%4%5")
            .arg(done).arg(total).arg(entries_.size()).arg(finished ? "" : " · searching…")
            .arg(entries_.size()>=2000 ? " · limit 2000; refine search" : "");
        auto updated = std::move(entries_); set_entries(std::move(updated), selected);
        line_edit->setToolTip(QString("%1 / %2 files searched · %3 matching PDFs%4%5")
            .arg(done).arg(total).arg(entries_.size()).arg(failures ? QString(" · %1 unavailable PDFs").arg(failures) : QString())
            .arg(entries_.size()>=2000 ? " · first 2000 results; refine your search" : ""));
    };
    for (QString path : root.split(';', Qt::SkipEmptyParts)) {
        path = path.trimmed();
        if (path.isEmpty()) continue;
        if (path == "~" || path.startsWith("~/")) path.replace(0, 1, QDir::homePath());
        const QFileInfo info(path);
        const QString canonical = info.canonicalFilePath();
        path = canonical.isEmpty() ? QDir::cleanPath(info.absoluteFilePath()) : canonical;
        if (!roots_.contains(path)) roots_.append(path);
    }
    if (roots_.isEmpty()) roots_.append(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation));
    // Multiple roots need unambiguous folder labels rather than unrelated relative paths.
    location_ = roots_.size() == 1 ? roots_.front() : QString();
    show_folders_ = true;
    line_edit->setPlaceholderText(contents_ ? "Search text inside library PDFs…" : "Search library filenames, titles or folders…");
    auto index = PdfLibraryIndex::instance();
    connect(index, &PdfLibraryIndex::scanned, this,
        [this](const QString& root, const QStringList& paths, const QString& version, const QString& error) {
            if (!roots_.contains(root) || snapshots_.value(root).version == version) return;
            snapshots_[root] = {paths, version, error};
            apply_snapshots();
        });
    for (const auto& path : roots_) {
        const auto cached = index->cached(path);
        if (!cached.version.isEmpty()) snapshots_[path] = cached;
    }
    apply_snapshots();
    rescan_timer_.setInterval(30000);
    connect(&rescan_timer_, &QTimer::timeout, this, [this]() { if (isVisible()) refresh(); });
    rescan_timer_.start();
    refresh();
}
void LibrarySelector::refresh() {
    for (const auto& root : roots_) PdfLibraryIndex::instance()->request(root);
}
void LibrarySelector::apply_snapshots() {
    QSet<QString> unique_paths;
    QStringList errors;
    for (const auto& root : roots_) {
        const auto snapshot = snapshots_.value(root);
        for (const auto& path : snapshot.paths) unique_paths.insert(path);
        if (!snapshot.error.isEmpty()) errors.append(snapshot.error);
    }
    QStringList paths(unique_paths.begin(), unique_paths.end());
    paths.sort(Qt::CaseInsensitive);
    if (snapshots_.size() < roots_.size()) empty_message_ = "Scanning library folders…";
    else empty_message_ = errors.isEmpty() ? "No PDFs in " + roots_.join("; ") : errors.join("; ");
    line_edit->setToolTip(roots_.join("\n") + (errors.isEmpty() ? QString() : "\n\n" + errors.join("\n")));
    all_paths_ = paths;
    if (contents_) { search_contents(); return; }
    const QString selected = QString::fromStdWString(get_selected_text());
    QHash<QString, DocumentPickerEntry> previous;
    for (auto& entry : entries_) {
        const QString path = entry.path;
        previous.insert(path, std::move(entry));
    }
    std::vector<DocumentPickerEntry> entries;
    entries.reserve(paths.size());
    for (const auto& path : paths) {
        if (previous.contains(path)) entries.push_back(previous.take(path));
        else entries.push_back({path, {}, {}, path, false});
    }
    set_entries(std::move(entries), selected);
}

bool LibrarySelector::on_text_change(const QString& text) {
    if (!contents_) return DocumentPicker::on_text_change(text);
    content_worker_.cancel();
    empty_message_ = text.trimmed().isEmpty() ? "Type a phrase to search PDF contents" : "Searching PDF text…";
    set_entries({}); search_timer_.start(); return true;
}
void LibrarySelector::search_contents() {
    if (!contents_) return;
    set_entries({});
    empty_message_ = line_edit->text().trimmed().isEmpty() ? "Type a phrase to search PDF contents" : "Searching PDF text…";
    rebuild_results();
    content_worker_.search(all_paths_, line_edit->text());
}
void LibrarySelector::activate(const DocumentPickerEntry& entry) {
    const auto preview=findChild<QWidget*>("pdfPickerPreview");
    const int page = contents_ && preview ? static_cast<PdfPickerPreview*>(preview)->current_page() : entry.page;
    const auto pane = pane_;
    auto chosen = entry; chosen.value = entry.path;
    DocumentPicker::activate(chosen);
    if (pane && page >= 0 && pane->doc()) {
        pane->main_document_view->goto_page(page); pane->invalidate_render();
    }
}

FileSelector::FileSelector(bool fuzzy, std::function<void(std::wstring)> on_done, MainWidget* pane, QString last_path) :
    DocumentPicker("Open document", fuzzy, pane, {},
        [on_done](QString path) { on_done(path.toStdWString()); }) {
    const QFileInfo last(expand_home_dir(last_path));
    const QString root = last_path.isEmpty() ? QDir::homePath() : (last.isDir() ? last.absoluteFilePath() : last.absolutePath());
    open_directory(root, last.isFile() ? last.absoluteFilePath() : QString());
}
void FileSelector::open_directory(const QString& path, const QString& selected) {
    QDir dir(expand_home_dir(path));
    if (!dir.exists()) return;
    location_ = dir.absolutePath();
    QSignalBlocker block(line_edit);
    line_edit->clear();
    line_edit->setPlaceholderText("Search this folder by filename or PDF title…");
    line_edit->setToolTip(location_);
    std::vector<DocumentPickerEntry> entries;
    if (!dir.isRoot()) entries.push_back({QFileInfo(dir.absoluteFilePath("..")).absoluteFilePath(), "Parent folder", {}, dir.absoluteFilePath(".."), true});
    for (const auto& info : dir.entryInfoList(QDir::NoDotAndDotDot | QDir::Dirs | QDir::Files | QDir::Readable, QDir::DirsFirst | QDir::Name | QDir::IgnoreCase)) {
        entries.push_back({info.absoluteFilePath(), {}, {}, info.absoluteFilePath(), info.isDir()});
    }
    set_entries(std::move(entries), selected);
}
bool FileSelector::on_text_change(const QString& text) {
    // An explicit path still navigates folders; ordinary typing searches titles
    // without requiring users to preserve a directory prefix in the search box.
    QString path = expand_home_dir(text);
    if (text.endsWith('/') && !QDir::isAbsolutePath(path)) path = QDir(location_).filePath(path);
    if (text.endsWith('/') && QDir(path).exists()) { open_directory(path); return true; }
    return DocumentPicker::on_text_change(text);
}
void FileSelector::activate(const DocumentPickerEntry& entry) {
    const QString path = entry.path;
    if (entry.directory) open_directory(path);
    else DocumentPicker::activate(entry);
}
bool FileSelector::eventFilter(QObject* object, QEvent* event) {
    if (object == line_edit && event->type() == QEvent::KeyPress) {
        auto key = static_cast<QKeyEvent*>(event);
        if ((key->key() == Qt::Key_Backspace && line_edit->text().isEmpty() && search_insert_mode() && key->modifiers() == Qt::NoModifier) ||
            (key->key() == Qt::Key_Up && key->modifiers() == Qt::AltModifier)) {
            open_directory(QDir(location_).absoluteFilePath("..")); return true;
        }
        if (key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter) {
            const QString text = expand_home_dir(line_edit->text());
            const QFileInfo explicit_path(QDir::isAbsolutePath(text) ? text : QDir(location_).filePath(text));
            if (!text.isEmpty() && explicit_path.exists()) {
                if (explicit_path.isDir()) open_directory(explicit_path.absoluteFilePath());
                else activate({explicit_path.absoluteFilePath(), {}, {}, explicit_path.absoluteFilePath(), false});
                return true;
            }
        }
    }
    return DocumentPicker::eventFilter(object, event);
}
