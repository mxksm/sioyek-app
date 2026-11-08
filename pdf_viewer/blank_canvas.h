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
inline constexpr int blank_canvas_pages = 100;

inline QByteArray blank_canvas_pdf(int pages) {
    QByteArray bytes("%PDF-1.4\n% Sioyek persistent blank canvas\n");
    std::vector<int> offsets{0};
    auto object = [&](const QByteArray& body) {
        offsets.push_back(bytes.size());
        bytes += QByteArray::number(int(offsets.size()) - 1) + " 0 obj\n" + body + "\nendobj\n";
    };
    object("<< /Type /Catalog /Pages 2 0 R >>");
    QByteArray kids;
    for (int i = 0; i < pages; ++i) {
        if (i) kids += ' ';
        kids += QByteArray::number(i + 3) + " 0 R";
    }
    object("<< /Type /Pages /Kids [" + kids + "] /Count " + QByteArray::number(pages) + " >>");
    for (int i = 0; i < pages; ++i) object("<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Resources << >> >>");
    const int xref = bytes.size();
    bytes += "xref\n0 " + QByteArray::number(pages + 3) + "\n0000000000 65535 f \n";
    for (size_t i = 1; i < offsets.size(); ++i)
        bytes += QByteArray::number(offsets[i]).rightJustified(10, '0') + " 00000 n \n";
    bytes += "trailer\n<< /Size " + QByteArray::number(pages + 3) + " /Root 1 0 R >>\nstartxref\n" + QByteArray::number(xref) + "\n%%EOF\n";
    return bytes;
}

inline bool ensure_blank_canvas(QString& error) {
    const QString path = blank_canvas_path();
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) {
        error = "Cannot create the canvas folder: " + QFileInfo(path).absolutePath();
        return false;
    }
    QLockFile lock(path + ".lock");
    if (!lock.tryLock(1000)) { error = "Cannot lock the blank canvas: " + path; return false; }
    if (QFileInfo::exists(path)) {
        QFile existing(path);
        if (!existing.open(QIODevice::ReadOnly)) { error = existing.errorString(); return false; }
        // Upgrade only our exact original one-page template. Never rewrite a
        // user-edited PDF. Page 1 keeps its size and all sidecars stay untouched.
        const auto original = existing.readAll();
        if (original != blank_canvas_pdf(1)) return true;
        existing.close();
        const auto backup = path + ".before-expansion";
        if (!QFileInfo::exists(backup) && !QFile::copy(path, backup)) {
            error = "Cannot back up the original canvas: " + backup; return false;
        }
    }
    const auto bytes = blank_canvas_pdf(blank_canvas_pages);
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) {
        error = "Cannot save the blank canvas: " + path + " (" + file.errorString() + ")";
        return false;
    }
    return true;
}
