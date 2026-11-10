#pragma once

#include <QCryptographicHash>
#include <QMouseEvent>
#include <QTabletEvent>

// Preserve Qt 5 builds while using the non-deprecated Qt 6 event APIs.
inline QPointF mouse_global_position(const QMouseEvent* event) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return event->globalPosition();
#else
    return event->screenPos();
#endif
}

inline QPointF mouse_local_position(const QMouseEvent* event) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return event->position();
#else
    return event->localPos();
#endif
}

inline QPointF tablet_position(const QTabletEvent* event) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return event->position();
#else
    return event->posF();
#endif
}

inline void hash_add_data(QCryptographicHash& hash, const char* bytes, int size) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 3, 0)
    hash.addData(QByteArrayView(bytes, size));
#else
    hash.addData(bytes, size);
#endif
}
