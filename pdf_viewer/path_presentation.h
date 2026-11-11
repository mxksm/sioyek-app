#pragma once

#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QString>
#include <string>
#include <vector>

extern std::wstring PATH_ALIASES;

// Presentation only: never use these aliases as document/database identities.
// Resolve roots once per UI refresh, rather than stat every result in a library.
class PathPresentation {
    struct Alias { QString target; QString friendly; };
    std::vector<Alias> aliases_;
public:
    PathPresentation() {
        for (QString root : QString::fromStdWString(PATH_ALIASES).split(';', Qt::SkipEmptyParts)) {
            root = root.trimmed();
            if (root == "~") root = QDir::homePath();
            else if (root.startsWith("~/")) root.replace(0, 1, QDir::homePath());
            if (!QDir::isAbsolutePath(root)) continue;
            const QFileInfo info(QDir::cleanPath(root));
            if (!info.isDir()) continue;
            const QString target = info.canonicalFilePath();
            if (!target.isEmpty()) aliases_.push_back({target, info.absoluteFilePath()});
        }
    }
    QString display(const QString& path) const {
        if (path.isEmpty() || !QDir::isAbsolutePath(path)) return path;
        const QString clean = QDir::cleanPath(path);
        QString result = clean;
        int matched = -1;
        for (const auto& alias : aliases_) {
            const QString prefix = alias.target.endsWith('/') ? alias.target : alias.target + '/';
            if (alias.target.size() > matched && (clean == alias.target || clean.startsWith(prefix))) {
                const QString suffix = clean == alias.target ? QString() : clean.mid(prefix.size());
                result = suffix.isEmpty() ? alias.friendly : QDir(alias.friendly).filePath(suffix);
                matched = alias.target.size();
            }
        }
        return result;
    }
    QString for_shell(const QString& path) const {
        QString result = display(path);
#ifndef Q_OS_WIN
        static const QRegularExpression safe("\\A[A-Za-z0-9_./-]+\\z");
        if (!result.isEmpty() && !safe.match(result).hasMatch()) {
            result.replace("'", "'\\''");
            result = "'" + result + "'";
        }
#endif
        return result;
    }
};
