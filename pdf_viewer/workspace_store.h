#pragma once
#include <functional>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include "blank_canvas.h"

inline QString workspace_store_path() {
    return QDir(QString::fromStdWString(standard_data_path.get_path())).filePath("workspaces.json");
}
inline bool read_workspace_store(QJsonObject& store, QString& error) {
    QFile file(workspace_store_path());
    if (!file.exists()) { store = {{"version", 1}, {"workspaces", QJsonObject()}}; return true; }
    if (!file.open(QIODevice::ReadOnly)) { error = file.errorString(); return false; }
    QJsonParseError parse;
    auto json = QJsonDocument::fromJson(file.readAll(), &parse);
    if (parse.error != QJsonParseError::NoError || !json.isObject() ||
        json.object()["version"].toInt() != 1 || !json.object()["workspaces"].isObject()) {
        error = "Cannot read workspaces.json: invalid or unsupported workspace data."; return false;
    }
    store = json.object(); return true;
}
inline bool update_workspace_store(const std::function<void(QJsonObject&)>& update, QString& error) {
    const auto path = workspace_store_path();
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) { error = "Cannot create the workspace folder."; return false; }
    QLockFile lock(path + ".lock");
    if (!lock.tryLock(1000)) { error = "Workspaces are being saved by another instance. Try again."; return false; }
    QJsonObject store;
    if (!read_workspace_store(store, error)) return false;
    update(store);
    QSaveFile file(path);
    const auto bytes = QJsonDocument(store).toJson();
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) {
        error = "Cannot save workspaces: " + file.errorString(); return false;
    }
    return true;
}
