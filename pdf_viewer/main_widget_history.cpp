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

extern int NAVIGATION_SIDEBAR_VISIBILITY;
extern int NAVIGATION_UI_DURATION;

namespace {
constexpr int history_limit = 15;
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
        list_->setAccessibleName("Saved locations");
        list_->setFrameShape(QFrame::NoFrame);
        list_->setFocusPolicy(Qt::NoFocus);
        list_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        list_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        list_->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
        list_->setUniformItemSizes(true);
        list_->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Ignored);
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
        const int mode = pane->navigation_sidebar_visibility_mode();
        const bool visible = !pane->history.empty() && pane->doc() && (mode == 1 ||
            (mode == 2 && pane->navigation_sidebar_reveal_timer_ && pane->navigation_sidebar_reveal_timer_->isActive()));
        if (!visible) { set_navigation_overlay_visible(this, false); return; }
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
        if (selected_ < 0) list_->clearSelection();
        const int width = std::max(34, list_->fontMetrics().horizontalAdvance(QString::number(pane->history.size())) + 14);
        // Measure the styled rows instead of assuming a font-dependent height.
        // The old 26px estimate accumulated a large empty area below the list.
        list_->ensurePolished();
        expand_->ensurePolished();
        const int row_height = list_->sizeHintForRow(0);
        const auto margins = layout()->contentsMargins();
        const int header_height = expand_->sizeHint().height();
        expand_->setFixedHeight(header_height);
        const int preferred_height = row_height * list_->count() + header_height +
            margins.top() + margins.bottom() + layout()->spacing();
        const int height = std::min(std::max(1, parentWidget()->height() - 112), preferred_height);
        setGeometry(8, std::max(52, (parentWidget()->height() - height) / 2), width, height);
        expand_->setFixedWidth(width - 4);
        layout()->activate();
        set_navigation_overlay_visible(this, true); raise();
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

bool MainWidget::goto_navigation_history(int index) {
    if (index < 0 || index >= int(history.size())) return false;
    const auto target = history[index];
    const auto file = QString::fromStdWString(target.document_path);
    if (!QFileInfo(file).isFile() || !QFileInfo(file).isReadable()) {
        set_status_message(L"Saved PDF is no longer available"); return false;
    }
    if (portal_to_edit) {
        // Returning from a portal still updates its destination, as before.
        auto source = document_manager->get_document(target.document_path);
        const auto state = dv()->get_state().book_state;
        portal_to_edit->dst.book_state = state;
        if (source) source->update_portal(*portal_to_edit);
        db_manager->update_portal(portal_to_edit->uuid, state.offset_x, state.offset_y, state.zoom_level);
        portal_to_edit.reset();
    }
    const auto departure_id = current_history_index >= 0 ? history[current_history_index].id : last_navigation_point_id_;
    if (doc() && doc()->get_path() != target.document_path) persist();
    clear_selected_text();
    set_main_document_view_state(target);
    const bool opened = doc() && doc()->get_path() == target.document_path;
    if (opened) {
        if (departure_id && departure_id != target.id) previous_navigation_point_id_ = departure_id;
        current_history_index = index;
        last_navigation_point_id_ = target.id;
    }
    reveal_navigation_sidebar();
    reveal_navigation_ui(true);
    invalidate_render();
    return opened;
}

void MainWidget::save_navigation_state() {
    if (!doc()) return;
    auto point = navigation_point();
    capture_navigation_snippet(point);
    previous_navigation_point_id_ = last_navigation_point_id_;
    point.id = next_navigation_point_id_++;
    // Only an explicit save appends a stop. Visiting older stops never truncates
    // the list, and reading elsewhere never changes a saved location.
    history.push_back(std::move(point));
    if (int(history.size()) > history_limit) history.erase(history.begin(), history.end() - history_limit);
    current_history_index = int(history.size()) - 1;
    last_navigation_point_id_ = history.back().id;
    ++navigation_history_revision_;
    reveal_navigation_sidebar();
    set_status_message(L"Saved location " + std::to_wstring(history.size()));
    invalidate_render();
}

void MainWidget::toggle_recent_history() {
    for (int i = 0; i < int(history.size()); ++i) {
        if (i != current_history_index && history[i].id == previous_navigation_point_id_ && goto_navigation_history(i)) return;
    }
    // Initial use (or a deleted/evicted alternate): choose the nearest preceding
    // valid stop, then the next one if currently at the beginning.
    const int previous = current_history_index < 0 ? int(history.size()) - 1 : current_history_index - 1;
    for (int i = previous; i >= 0; --i) if (goto_navigation_history(i)) return;
    for (int i = current_history_index + 1; i < int(history.size()); ++i) if (goto_navigation_history(i)) return;
}

void MainWidget::prev_state() {
    // If the reader has moved away, restore the selected saved stop first.
    // Keep the traversal cursor independent of scrolling and tab changes.
    const int start = current_history_index < 0 ? int(history.size()) - 1 :
        current_history_index - (doc() && same_location(navigation_point(), history[current_history_index]) ? 1 : 0);
    for (int i = start; i >= 0; --i)
        if (goto_navigation_history(i)) break;
}
void MainWidget::next_state() {
    if (current_history_index < 0) return;
    for (int i = current_history_index + 1; i < int(history.size()); ++i)
        if (goto_navigation_history(i)) break;
}
void MainWidget::update_navigation_sidebar() {
    if (!central_widget || !opengl_widget) return;
    if (!navigation_sidebar_ && !history.empty()) navigation_sidebar_ = new NavigationHistorySidebar(this);
    if (navigation_sidebar_) navigation_sidebar_->refresh(this);
}
int MainWidget::navigation_sidebar_visibility_mode() const {
    return navigation_sidebar_visibility_override_ < 0 ? NAVIGATION_SIDEBAR_VISIBILITY : navigation_sidebar_visibility_override_;
}
void MainWidget::reveal_navigation_sidebar() {
    if (!navigation_sidebar_reveal_timer_) {
        navigation_sidebar_reveal_timer_ = new QTimer(this);
        navigation_sidebar_reveal_timer_->setSingleShot(true);
        connect(navigation_sidebar_reveal_timer_, &QTimer::timeout, this, [this] {
            if (navigation_sidebar_ && navigation_sidebar_->isVisible() && navigation_sidebar_->underMouse())
                navigation_sidebar_reveal_timer_->start(300);
            else update_navigation_sidebar();
        });
    }
    navigation_sidebar_reveal_timer_->start(NAVIGATION_UI_DURATION);
    update_navigation_sidebar();
}
void MainWidget::toggle_navigation_sidebar() {
    // Match st/sb: visible -> hidden -> automatic -> visible.
    navigation_sidebar_visibility_override_ = (navigation_sidebar_visibility_mode() + 2) % 3;
    reveal_navigation_sidebar();
}
bool MainWidget::remove_navigation_point(quint64 id) {
    for (int i = 0; i < int(history.size()); ++i) {
        if (history[i].id != id) continue;
        history.erase(history.begin() + i);
        if (current_history_index == i) current_history_index = -1;
        else if (current_history_index > i) --current_history_index;
        ++navigation_history_revision_;
        reveal_navigation_sidebar();
        return true;
    }
    // Already removed by an external action while the picker was open.
    return true;
}
void MainWidget::capture_navigation_snippet(NavigationPoint& point) {
    // Capture the excerpt with the view, never later from a different location.
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
    }
}

void MainWidget::show_navigation_history() {
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
    auto picker = new DocumentPicker("Saved locations", true, this, std::move(entries),
        [this, points](QString value) {
            pop_current_widget();
            const int index = value.toInt();
            if (index < 0 || index >= int(points.size())) return;
            const auto& point = points[index];
            for (int i = 0; i < int(history.size()); ++i)
                if (history[i].id == point.id) { goto_navigation_history(i); return; }
            // An external open may have changed the trail while this snapshot
            // was visible. Still navigate to the point the preview displayed.
            if (!QFileInfo(QString::fromStdWString(point.document_path)).isFile()) {
                set_status_message(L"Saved PDF is no longer available"); return;
            }
            set_main_document_view_state(point); invalidate_render();
        }, [this, points](QString value) {
            bool ok = false;
            const int index = value.toInt(&ok);
            return ok && index >= 0 && index < int(points.size()) && remove_navigation_point(points[index].id);
        });
    picker->set_search_placeholder("Search files, pages or passages…");
    picker->set_live_preview(QString::number(current_history_index), {}, {});
    set_current_widget(picker);
    show_current_widget();
}

QJsonObject MainWidget::navigation_history_json() {
    QJsonArray entries;
    int alternate = -1, last_visited = -1;
    for (const auto& point : history) {
        if (point.id == previous_navigation_point_id_) alternate = entries.size();
        if (point.id == last_navigation_point_id_) last_visited = entries.size();
        const auto& view = point.book_state;
        QJsonObject item{{"path", history_path(point.document_path)}, {"zoom", view.zoom_level},
            {"x", view.offset_x}, {"y", view.offset_y}, {"page", point.page}, {"text", point.snippet}};
        if (view.presentation_page) item["presentation_page"] = *view.presentation_page;
        entries.append(item);
    }
    return {{"entries", entries}, {"current", current_history_index}, {"alternate", alternate}, {"last_visited", last_visited}, {"sidebar_mode", navigation_sidebar_visibility_override_}};
}
void MainWidget::restore_navigation_history(const QJsonObject& value) {
    history.clear(); current_history_index = -1;
    previous_navigation_point_id_ = 0;
    last_navigation_point_id_ = 0;
    ++navigation_history_revision_;
    navigation_sidebar_visibility_override_ = value.contains("sidebar_mode") ?
        std::clamp(value["sidebar_mode"].toInt(-1), -1, 2) :
        value.contains("sidebar") ? (value["sidebar"].toBool() ? 1 : 0) : -1;
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
        point.id = next_navigation_point_id_++;
        if (i == value["alternate"].toInt(-1)) previous_navigation_point_id_ = point.id;
        if (i == value["last_visited"].toInt(selected)) last_navigation_point_id_ = point.id;
        history.push_back(std::move(point));
        if (i == selected) current_history_index = int(history.size()) - 1;
    }
    reveal_navigation_sidebar();
}
