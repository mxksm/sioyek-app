#include "main_widget.h"
#include "workspace_store.h"
#include "document.h"
#include "database.h"
#include "checksum.h"
#include "document_view.h"
#include "pdf_view_opengl_widget.h"
#include "ui.h"
#include "utils.h"
#include <cmath>
#include <QTimer>

extern bool FUZZY_SEARCHING;

namespace {
QString stored_path(const std::wstring& path) {
    if (is_blank_canvas(path)) return "@canvas";
    auto text = QString::fromStdWString(path);
    if (text.startsWith(QDir::homePath() + '/')) text.replace(0, QDir::homePath().size(), "~");
    return text;
}
std::wstring restored_path(QString path) {
    if (path == "@canvas") return blank_canvas_path().toStdWString();
    if (path.startsWith("~/")) path.replace(0, 1, QDir::homePath());
    return path.toStdWString();
}
QJsonObject view_json(const OpenedBookState& view) {
    QJsonObject result{{"zoom", view.zoom_level}, {"x", view.offset_x}, {"y", view.offset_y}};
    if (view.presentation_page) result["presentation_page"] = *view.presentation_page;
    return result;
}
std::optional<OpenedBookState> read_view(const QJsonValue& value) {
    if (!value.isObject()) return {};
    auto object = value.toObject();
    for (const auto key : {"zoom", "x", "y"})
        if (!object[key].isDouble() || !std::isfinite(object[key].toDouble())) return {};
    const double zoom = object["zoom"].toDouble();
    if (zoom <= 0 || zoom > 10000 || std::abs(object["x"].toDouble()) > 1e9 || std::abs(object["y"].toDouble()) > 1e9) return {};
    OpenedBookState state{float(zoom), float(object["x"].toDouble()), float(object["y"].toDouble())};
    if (object["presentation_page"].isDouble() && object["presentation_page"].toInt(-1) >= 0)
        state.presentation_page = object["presentation_page"].toInt();
    return state;
}
bool validate_workspace(const QJsonObject& workspace, QString& error) {
    if (!workspace["tabs"].isArray() || !workspace["panes"].isArray()) {
        error = "Invalid workspace layout."; return false;
    }
    const auto tabs = workspace["tabs"].toArray(), panes = workspace["panes"].toArray();
    if (panes.size() < 1 || panes.size() > 2) { error = "A workspace must contain one or two panes."; return false; }
    QStringList paths;
    for (const auto& tab : tabs) {
        if (!tab.isString() || tab.toString() == "@canvas" || paths.contains(tab.toString())) {
            error = "Invalid workspace tab list."; return false;
        }
        paths.append(tab.toString());
    }
    for (const auto& item : panes) {
        const auto pane = item.toObject();
        const auto path = pane["path"].toString();
        if ((!paths.contains(path) && path != "@canvas") || !read_view(pane["view"])) {
            error = "Invalid workspace pane."; return false;
        }
        for (const auto& view : pane["tab_views"].toArray()) {
            if (!paths.contains(view.toObject()["path"].toString()) || !read_view(view.toObject()["view"])) {
                error = "Invalid saved tab position."; return false;
            }
        }
        if (path == "@canvas" && !ensure_blank_canvas(error)) return false;
    }
    // Check before changing the current window; a disconnected folder must not
    // replace a working layout with a half-restored one.
    for (const auto& path : paths) {
        const QFileInfo file(QString::fromStdWString(restored_path(path)));
        if (!file.isAbsolute() || !file.isFile() || !file.isReadable()) {
            error = "Workspace PDF unavailable: " + file.filePath(); return false;
        }
    }
    return true;
}
}

QJsonObject MainWidget::capture_workspace() {
    auto owner = tab_owner();
    owner->finish_recent_tab_cycle(false);
    std::vector<MainWidget*> panes{owner};
    if (owner->split_pane_) panes.push_back(owner->split_pane_);
    for (auto pane : panes) { pane->remember_current_tab(); pane->main_document_view->persist(true); }
    QJsonArray tabs, views;
    const auto paths = owner->window_tabs.paths();
    for (const auto& path : paths) tabs.append(stored_path(path));
    for (auto pane : panes) {
        if (!pane->doc()) continue;
        QJsonArray states;
        for (const auto& path : paths) {
            auto state = pane->window_tabs.state(path);
            if (!state) state = owner->window_tabs.state(path);
            if (!state && owner->split_pane_) state = owner->split_pane_->window_tabs.state(path);
            if (!state) {
                const auto hash = pane->checksummer->get_checksum_fast(path);
                std::vector<OpenedBookState> saved;
                if (hash && pane->db_manager->select_opened_book(*hash, saved) && !saved.empty()) state = saved.front();
            }
            if (state) states.append(QJsonObject{{"path", stored_path(path)}, {"view", view_json(*state)}});
        }
        views.append(QJsonObject{{"path", stored_path(pane->doc()->get_path())},
            {"view", view_json(pane->main_document_view->get_state().book_state)}, {"tab_views", states}});
    }
    double fraction = 0.5;
    if (owner->pdf_splitter_) {
        const auto sizes = owner->pdf_splitter_->sizes();
        if (sizes.size() == 2 && sizes[0] + sizes[1] > 0) fraction = double(sizes[0]) / (sizes[0] + sizes[1]);
    }
    return {{"tabs", tabs}, {"panes", views}, {"focus", owner->active_pane() == owner ? 0 : 1},
        {"split_fraction", fraction}};
}

bool MainWidget::save_workspace(QString name, QString& error) {
    auto owner = tab_owner();
    name = name.trimmed();
    if (name.isEmpty() || name.size() > 120 || name.contains('\n') || name.contains('\r')) {
        error = "Use a workspace name of 1–120 characters on one line."; return false;
    }
    const auto snapshot = owner->capture_workspace();
    if (snapshot["panes"].toArray().isEmpty()) { error = "Open a PDF or the blank canvas first."; return false; }
    if (!update_workspace_store([&](QJsonObject& store) {
        auto entries = store["workspaces"].toObject(); entries[name] = snapshot; store["workspaces"] = entries;
    }, error)) return false;
    owner->workspace_name_ = name;
    owner->set_status_message(("Saved workspace: " + name).toStdWString());
    return true;
}

bool MainWidget::apply_workspace(const QJsonObject& snapshot, QString& error) {
    auto owner = tab_owner();
    if (!validate_workspace(snapshot, error)) return false;
    owner->finish_recent_tab_cycle(false);
    owner->pop_all_widgets();
    if (owner->split_pane_) {
        owner->split_pane_->pop_all_widgets();
        owner->focus_split_pane(0); owner->toggle_split();
    }
    const auto panes = snapshot["panes"].toArray();
    std::vector<std::wstring> paths;
    for (const auto& path : snapshot["tabs"].toArray()) paths.push_back(restored_path(path.toString()));
    owner->window_tabs = {};
    owner->open_tabs(paths);
    auto open_pane = [&](MainWidget* pane, const QJsonObject& saved) {
        const auto path = restored_path(saved["path"].toString());
        pane->open_document(path);
        if (!pane->doc() || pane->doc()->get_path() != get_canonical_path(path)) {
            error = "Could not open workspace PDF: " + QString::fromStdWString(path); return false;
        }
        // Seed every pane's tab positions only after opening: opening remembers
        // the outgoing view and would otherwise overwrite the saved position.
        pane->window_tabs = {};
        pane->open_tabs(paths);
        for (const auto& item : saved["tab_views"].toArray()) {
            const auto object = item.toObject();
            pane->window_tabs.save(restored_path(object["path"].toString()), *read_view(object["view"]));
        }
        pane->main_document_view->set_book_state(*read_view(saved["view"]));
        pane->record_current_tab(false);
        pane->invalidate_render();
        return true;
    };
    if (!open_pane(owner, panes[0].toObject())) return false;
    if (panes.size() == 2) {
        owner->toggle_split();
        if (!owner->split_pane_) { error = "Could not create the workspace split."; return false; }
        if (!open_pane(owner->split_pane_, panes[1].toObject())) return false;
        const double fraction = std::clamp(snapshot["split_fraction"].toDouble(0.5), 0.05, 0.95);
        // A newly installed central widget still has Qt's 100px placeholder
        // geometry until the next event loop. Size it before applying the ratio.
        owner->pdf_splitter_->setGeometry(owner->contentsRect());
        const int width = owner->pdf_splitter_->width();
        owner->pdf_splitter_->setSizes({int(width * fraction), int(width * (1-fraction))});
        // Resizing panes may refit views. Reapply the exact saved coordinates.
        owner->main_document_view->set_book_state(*read_view(panes[0].toObject()["view"]));
        owner->split_pane_->main_document_view->set_book_state(*read_view(panes[1].toObject()["view"]));
        owner->focus_split_pane(snapshot["focus"].toInt() == 1 ? 1 : 0);
    } else owner->setFocus();
    owner->refresh_titlebar(); owner->persist(true);
    return true;
}

bool MainWidget::load_workspace(const QString& name, QString& error) {
    auto owner = tab_owner();
    QJsonObject store;
    if (!read_workspace_store(store, error)) return false;
    // Empty name denotes the automatic recovery entry, never a user workspace.
    const auto target = name.isEmpty() ? store["previous"] : store["workspaces"].toObject()[name];
    if (!target.isObject()) { error = "Workspace not found."; return false; }
    const auto snapshot = target.toObject();
    if (!validate_workspace(snapshot, error)) return false;
    const auto before = owner->capture_workspace();
    const auto before_name = owner->workspace_name_;
    if (!update_workspace_store([&](QJsonObject& data) {
        data["previous"] = before; data["previous_name"] = before_name;
    }, error)) return false;
    if (!owner->apply_workspace(snapshot, error)) {
        QString rollback_error;
        owner->apply_workspace(before, rollback_error);
        if (!rollback_error.isEmpty()) error += "\nCould not restore previous layout: " + rollback_error;
        return false;
    }
    owner->workspace_name_ = name.isEmpty() ? store["previous_name"].toString() : name;
    return true;
}

void MainWidget::handle_open_workspace() {
    QJsonObject store; QString error;
    if (!read_workspace_store(store, error)) { show_error_message(error.toStdWString()); return; }
    std::vector<DocumentPickerEntry> entries;
    auto add = [&](const QString& label, const QString& value, const QJsonObject& snapshot) {
        const int tabs = snapshot["tabs"].toArray().size();
        QString description = QString("%1 PDF%2 · %3").arg(tabs).arg(tabs == 1 ? "" : "s")
            .arg(snapshot["panes"].toArray().size() == 2 ? "split view" : "single pane");
        entries.push_back({label, description, {}, value, false});
    };
    if (store["previous"].isObject()) add("Previous session (automatic)", "", store["previous"].toObject());
    const auto named = store["workspaces"].toObject();
    for (auto it = named.begin(); it != named.end(); ++it) add(it.key(), it.key(), it.value().toObject());
    set_current_widget(new DocumentPicker("Workspaces", FUZZY_SEARCHING, this, std::move(entries),
        [this](QString name) {
            if (pending_command_instance) {
                pending_command_instance->set_generic_requirement(name);
                advance_command(std::move(pending_command_instance));
            }
        }, nullptr, false));
    show_current_widget();
}
