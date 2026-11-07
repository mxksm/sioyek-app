#pragma once
#include <vector>
#include "path.h"
#include <QDir>
#include <QFileInfo>
#include <QLockFile>
#include <QSaveFile>

extern Path standard_data_path;

// A single application-owned PDF, independent of windows, tabs and build caches.
inline QString blank_canvas_path() {
    return QDir(QString::fromStdWString(standard_data_path.get_path())).filePath("canvas/blank.pdf");
}
inline bool is_blank_canvas(const std::wstring& path) {
    const QFileInfo candidate(QString::fromStdWString(path));
    const QFileInfo canvas(blank_canvas_path());
    return candidate.absoluteFilePath() == canvas.absoluteFilePath() ||
        (candidate.exists() && canvas.exists() && candidate.canonicalFilePath() == canvas.canonicalFilePath());
}
inline bool ensure_blank_canvas(QString& error) {
    const QString path = blank_canvas_path();
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) {
        error = "Cannot create the canvas folder: " + QFileInfo(path).absolutePath();
        return false;
    }
    QLockFile lock(path + ".lock");
    if (!lock.tryLock(1000)) { error = "Cannot lock the blank canvas: " + path; return false; }
    // Never truncate or regenerate an existing canvas, including an unreadable one.
    if (QFileInfo::exists(path)) return true;
    QByteArray bytes("%PDF-1.4\n% Sioyek persistent blank canvas\n");
    std::vector<int> offsets{0};
    auto object = [&](const QByteArray& body) {
        offsets.push_back(bytes.size());
        bytes += QByteArray::number(int(offsets.size()) - 1) + " 0 obj\n" + body + "\nendobj\n";
    };
    object("<< /Type /Catalog /Pages 2 0 R >>");
    object("<< /Type /Pages /Kids [3 0 R] /Count 1 >>");
    object("<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Resources << >> >>");
    const int xref = bytes.size();
    bytes += "xref\n0 4\n0000000000 65535 f \n";
    for (size_t i = 1; i < offsets.size(); ++i)
        bytes += QByteArray::number(offsets[i]).rightJustified(10, '0') + " 00000 n \n";
    bytes += "trailer\n<< /Size 4 /Root 1 0 R >>\nstartxref\n" + QByteArray::number(xref) + "\n%%EOF\n";
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) {
        error = "Cannot save the blank canvas: " + path + " (" + file.errorString() + ")";
        return false;
    }
    return true;
}
