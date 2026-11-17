#pragma once
#include "PdfDocument.h"
#include <QJsonObject>
#include <QPoint>
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

// Passwords travel only in the in-memory IPC message, never in the bundle on disk.
class DocumentTransfer {
public:
    static QJsonObject access(const PdfDocument& document);
    static bool open(PdfDocument& document,const QString& snapshot,const QJsonObject& access,bool dirty);
#ifdef Q_OS_WIN
    static constexpr ULONG_PTR messageTag=0x47504446;
    static const wchar_t* windowProperty();
    static UINT queryMessage();
    static UINT previewMessage();
    static UINT clearPreviewMessage();
    static bool preview(HWND target,QPoint screenPosition);
    static void clearPreview(HWND target);
    static HWND targetAt(HWND source,QPoint screenPosition,bool ignoreSource);
    static bool accepts(HWND target,QPoint screenPosition);
    static bool send(HWND target,HWND source,const QJsonObject& request);
#endif
};
