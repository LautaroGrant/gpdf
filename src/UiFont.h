#pragma once
#include <QByteArray>
#include <QFont>
#include <QtGlobal>

// Restore the original UI font and let Qt use its default platform backend.
inline void configureUiFontBackend() {}

inline QFont gpdfUiFont() {
    return QFont("Segoe UI",10);
}
