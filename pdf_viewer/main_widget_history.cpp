#include "main_widget.h"
#include "document.h"
#include "document_view.h"
#include "database.h"
#include "pdf_view_opengl_widget.h"
#include "ui.h"
#include "utils.h"
#include "workspace_store.h"
#include <QListWidget>
#include <QToolButton>
#include <QVBoxLayout>
#include <QFileInfo>
#include <QDir>
#include <QSignalBlocker>
#include <QTimer>
#include <cmath>

namespace {
constexpr int history_limit = 200;
bool same_location(const DocumentViewState& a, const DocumentViewState& b) {
    return a.document_path == b.document_path &&
        std::abs(a.book_state.offset_x - b.book_state.offset_x) < 0.5f &&
        std::abs(a.book_state.offset_y - b.book_state.offset_y) < 0.5f &&
        std::abs(a.book_state.zoom_level - b.book_state.zoom_level) < 0.0001f &&
        a.book_state.presentation_page == b.book_state.presentation_page;
}
QString history_path(const std::wstring& path) {
    if (is_blank_canvas(path)) return "@canvas";
    QString result = QString::fromStdWString(path);
    if (result.startsWith(QDir::homePath() + '/')) result.replace(0, QDir::homePath().size(), "~");
    return result;
}
QString point_label(const MainWidget::NavigationPoint& point, int index) {
    const QString name = is_blank_canvas(point.document_path) ? "Canvas" :
        QFileInfo(QString::fromStdWString(point.document_path)).completeBaseName();
    return QString("%1: %2 | Page %3").arg(index + 1).arg(name).arg(point.page + 1);
}
}

// A small edge overlay leaves PDF geometry and zoom unchanged when toggled.
// No preview renderer exists until the user explicitly expands the history.
class NavigationHistorySidebar : public QWidget {
    QListWidget* list_;
    QToolButton* expand_;
    int selected_ = -1;
    unsigned int revision_ = ~0u;
public:
    explicit NavigationHistorySidebar(MainWidget* pane) : QWidget(pane->central_widget) {
        setObjectName("navigationHistorySidebar");
        setAttribute(Qt::WA_StyledBackground);
        setFocusPolicy(Qt::NoFocus);
        auto layout = new QVBoxLayout(this);
        layout->setContentsMargins(2, 2, 2, 2); layout->setSpacing(2);
        expand_ = new QToolButton(this);
        expand_->setObjectName("expandNavigationHistory");
        expand_->setText(QString::fromUtf8("↗"));
        expand_->setToolTip("Expand navigation history (Space H F)");
        expand_->setAccessibleName("Expand navigation history");
        expand_->setFocusPolicy(Qt::NoFocus);
        layout->addWidget(expand_);
        list_ = new QListWidget(this);
        list_->setObjectName("navigationHistoryStops");
        list_->setAccessibleName("Navigation history stops");
        list_->setFrameShape(QFrame::NoFrame);
        list_->setFocusPolicy(Qt::NoFocus);
        list_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        list_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        list_->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
        list_->setUniformItemSizes(true);
        list_->setFont(QFont("JetBrains Mono", 11));
        layout->addWidget(list_);
        connect(list_, &QListWidget::itemClicked, this, [pane, this](QListWidgetItem* item) {
            pane->tab_owner()->focus_split_pane(pane->split_owner_ ? 1 : 0);
            pane->goto_navigation_history(list_->row(item));
            pane->setFocus();
        });
        connect(expand_, &QToolButton::clicked, pane, &MainWidget::show_navigation_history);
        connect(list_, &QListWidget::itemDoubleClicked, this, [pane](QListWidgetItem*) { pane->show_navigation_history(); });
    }
    void refresh(MainWidget* pane) {
        const bool visible = pane->navigation_sidebar_visible_ && pane->history.size() > 1 && pane->doc();
        if (!visible) { hide(); return; }
        const bool scroll = !isVisible() || selected_ != pane->current_history_index || list_->count() != int(pane->history.size());
        const QString style = QString(
            "QWidget#navigationHistorySidebar { %1 border: 0; border-radius: 0; }"
            "QListWidget { %1 border: 0; outline: 0; }"
            "QListWidget::item { padding: 3px 0; border: 0; }"
            "QListWidget::item:selected { %2 font-weight: bold; }"
            "QToolButton { %1 border: 0; border-radius: 0; padding: 2px; }"
            "QToolButton:hover { %2 }").arg(get_ui_stylesheet(true), get_selected_stylesheet(true));
        if (styleSheet() != style) setStyleSheet(style);
        if (revision_ != pane->navigation_history_revision_) {
            QSignalBlocker block(list_);
            while (list_->count() > int(pane->history.size())) delete list_->takeItem(list_->count() - 1);
            for (int i = 0; i < int(pane->history.size()); ++i) {
                if (i >= list_->count()) list_->addItem(QString::number(i + 1));
                auto item = list_->item(i);
                item->setTextAlignment(Qt::AlignCenter);
                item->setToolTip(point_label(pane->history[i], i) + "\n" + pane->history[i].snippet);
            }
            revision_ = pane->navigation_history_revision_;
        }
        selected_ = pane->current_history_index;
        if (list_->currentRow() != selected_) list_->setCurrentRow(selected_);
        const int width = std::max(34, list_->fontMetrics().horizontalAdvance(QString::number(pane->history.size())) + 14);
        const int height = std::min(std::max(70, parentWidget()->height() - 112), int(pane->history.size()) * 26 + 30);
        setGeometry(8, std::max(52, (parentWidget()->height() - height) / 2), width, height);
        expand_->setFixedWidth(width - 4);
        show(); raise();
        if (scroll && list_->currentItem()) list_->scrollToItem(list_->currentItem(), QAbstractItemView::EnsureVisible);
    }
};

MainWidget::NavigationPoint MainWidget::navigation_point() {
    NavigationPoint result;
    if (!doc()) return result;
    static_cast<DocumentViewState&>(result) = dv()->get_state();
    result.page = dv()->get_center_page_number();
    return result;
}

void MainWidget::update_current_history_index() {
    if (!doc() || restoring_navigation_history_) return;
    auto point = navigation_point();
    if (current_history_index < 0 || current_history_index >= int(history.size())) {
        history = {point}; current_history_index = 0;
    } else {
        auto& old = history[current_history_index];
        if (same_location(old, point) && old.page == point.page) return;
        old = std::move(point);
    }
    ++navigation_history_revision_;
}

void MainWidget::push_state(bool /*update*/) {
    if (restoring_navigation_history_ || navigation_jump_pending_) return;
    update_current_history_index();
    navigation_jump_pending_ = true;
    // Coalesce a navigation macro (e.g. goto_end;fit;center) into one stop.
    QTimer::singleShot(0, this, [this] { finish_navigation_jump(); });
}

void MainWidget::finish_navigation_jump() {
    if (!navigation_jump_pending_ || restoring_navigation_history_) return;
    navigation_jump_pending_ = false;
    if (!doc()) return;
    const auto point = navigation_point();
    if (current_history_index >= 0 && current_history_index < int(history.size()) &&
        same_location(history[current_history_index], point)) return;
    history.resize(std::max(0, current_history_index + 1));
    history.push_back(point);
    if (int(history.size()) > history_limit) history.erase(history.begin());
    current_history_index = int(history.size()) - 1;
    ++navigation_history_revision_;
    update_navigation_sidebar();
}

bool MainWidget::goto_navigation_history(int index) {
    finish_navigation_jump();
    if (index < 0 || index >= int(history.size())) return false;
    const auto target = history[index];
    const auto file = QString::fromStdWString(target.document_path);
    if (!QFileInfo(file).isFile() || !QFileInfo(file).isReadable()) {
        set_status_message(L"History PDF is no longer available"); return false;
    }
    update_current_history_index();
    if (portal_to_edit) {
        // Returning from a portal still updates its destination, as before.
        auto source = document_manager->get_document(target.document_path);
        const auto state = dv()->get_state().book_state;
        portal_to_edit->dst.book_state = state;
        if (source) source->update_portal(*portal_to_edit);
        db_manager->update_portal(portal_to_edit->uuid, state.offset_x, state.offset_y, state.zoom_level);
        portal_to_edit.reset();
    }
    restoring_navigation_history_ = true;
    if (doc() && doc()->get_path() != target.document_path) persist();
    clear_selected_text();
    set_main_document_view_state(target);
    const bool opened = doc() && doc()->get_path() == target.document_path;
    if (opened) current_history_index = index;
    restoring_navigation_history_ = false;
    update_navigation_sidebar();
    reveal_navigation_ui(true);
    invalidate_render();
    return opened;
}

void MainWidget::prev_state() {
    finish_navigation_jump();
    for (int i = current_history_index - 1; i >= 0; --i)
        if (goto_navigation_history(i)) break;
}
void MainWidget::next_state() {
    finish_navigation_jump();
    for (int i = current_history_index + 1; i < int(history.size()); ++i)
        if (goto_navigation_history(i)) break;
}
void MainWidget::update_navigation_sidebar() {
    if (!central_widget || !opengl_widget) return;
    if (!navigation_sidebar_ && history.size() > 1) navigation_sidebar_ = new NavigationHistorySidebar(this);
    if (navigation_sidebar_) navigation_sidebar_->refresh(this);
}
void MainWidget::toggle_navigation_sidebar() {
    navigation_sidebar_visible_ = !navigation_sidebar_visible_;
    update_navigation_sidebar();
}
void MainWidget::update_navigation_snippet() {
    // Called only once movement settles, after the reading anchor has cached the
    // page text. Rapid page navigation never waits for text extraction here.
    if (restoring_navigation_history_ || navigation_jump_pending_ || !doc()) return;
    update_current_history_index();
    if (current_history_index < 0) return;
    auto& point = history[current_history_index];
    if (!point.snippet.isEmpty()) return;
    const auto& index = doc()->get_page_index(point.page);
    size_t best = 0;
    float distance = std::numeric_limits<float>::max();
    for (size_t i = 0; i < index.rects.size(); ++i) {
        const auto center = index.rects[i].center();
        const auto pos = dv()->document_to_virtual_pos({point.page, center.x, center.y});
        const float d = std::abs(pos.y - dv()->get_virtual_offset().y);
        if (d < distance) { best = i; distance = d; }
    }
    if (best < index.text.size()) {
        const auto begin = best > 45 ? best - 45 : 0;
        point.snippet = QString::fromStdWString(index.text.substr(begin, 180)).simplified();
        ++navigation_history_revision_;
    }
    update_navigation_sidebar();
}

void MainWidget::show_navigation_history() {
    finish_navigation_jump();
    update_current_history_index();
    // Reuse the shared Vim search and PDF preview without moving the reader
    // until Enter is pressed. The list is a snapshot, so row indices stay stable.
    const auto points = history;
    std::vector<DocumentPickerEntry> entries;
    for (int i = 0; i < int(points.size()); ++i) {
        const auto& point = points[i];
        DocumentPickerEntry entry;
        entry.path = QString::fromStdWString(point.document_path);
        entry.value = QString::number(i);
        entry.title = point_label(point, i);
        entry.page = point.page;
        entry.snippet = QString("Stop %1").arg(i + 1);
        if (!point.snippet.isEmpty()) entry.snippet += " | " + point.snippet;
        entry.words = entry.snippet;
        entries.push_back(std::move(entry));
    }
    auto picker = new DocumentPicker("Navigation history", true, this, std::move(entries),
        [this, points](QString value) {
            pop_current_widget();
            const int index = value.toInt();
            if (index < 0 || index >= int(points.size())) return;
            const auto& point = points[index];
            for (int i = 0; i < int(history.size()); ++i)
                if (same_location(history[i], point)) { goto_navigation_history(i); return; }
            // An external open may have changed the trail while this snapshot
            // was visible. Still navigate to the point the preview displayed.
            if (!QFileInfo(QString::fromStdWString(point.document_path)).isFile()) {
                set_status_message(L"History PDF is no longer available"); return;
            }
            push_state(); set_main_document_view_state(point); invalidate_render();
        });
    picker->set_search_placeholder("Search files, pages or passages…");
    picker->set_live_preview(QString::number(current_history_index), {}, {});
    set_current_widget(picker);
    show_current_widget();
}

QJsonObject MainWidget::navigation_history_json() {
    finish_navigation_jump();
    update_current_history_index();
    QJsonArray entries;
    for (const auto& point : history) {
        const auto& view = point.book_state;
        QJsonObject item{{"path", history_path(point.document_path)}, {"zoom", view.zoom_level},
            {"x", view.offset_x}, {"y", view.offset_y}, {"page", point.page}, {"text", point.snippet}};
        if (view.presentation_page) item["presentation_page"] = *view.presentation_page;
        entries.append(item);
    }
    return {{"entries", entries}, {"current", current_history_index}, {"sidebar", navigation_sidebar_visible_}};
}
void MainWidget::restore_navigation_history(const QJsonObject& value) {
    history.clear(); current_history_index = -1; navigation_jump_pending_ = false;
    ++navigation_history_revision_;
    navigation_sidebar_visible_ = value["sidebar"].toBool(true);
    const auto entries = value["entries"].toArray();
    const int selected = value["current"].toInt(-1);
    for (int i = std::max(0, int(entries.size()) - history_limit); i < entries.size(); ++i) {
        const auto item = entries[i].toObject();
        QString path = item["path"].toString();
        if (path == "@canvas") path = blank_canvas_path();
        else if (path.startsWith("~/")) path.replace(0, 1, QDir::homePath());
        const double zoom = item["zoom"].toDouble(-1), x = item["x"].toDouble(1e20), y = item["y"].toDouble(1e20);
        if (!QFileInfo(path).isAbsolute() || !QFileInfo(path).isFile() || !std::isfinite(zoom) ||
            !std::isfinite(x) || !std::isfinite(y) || zoom <= 0 || zoom > 10000 || std::abs(x) > 1e9 || std::abs(y) > 1e9) continue;
        NavigationPoint point;
        point.document_path = get_canonical_path(path.toStdWString());
        point.book_state = {float(zoom), float(x), float(y)};
        if (item["presentation_page"].toInt(-1) >= 0) point.book_state.presentation_page = item["presentation_page"].toInt();
        point.page = std::max(0, item["page"].toInt());
        point.snippet = item["text"].toString().left(180);
        history.push_back(std::move(point));
        if (i <= selected) current_history_index = int(history.size()) - 1;
    }
    if (current_history_index < 0 || !doc() || history[current_history_index].document_path != doc()->get_path()) {
        history.clear(); current_history_index = -1;
    }
    update_current_history_index();
    update_navigation_sidebar();
}
