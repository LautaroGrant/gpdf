#pragma once
#include <QCursor>
#include <QImage>
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
namespace NativeScrollCursor {
enum Direction { Up=-1, Neutral=0, Down=1 };
QCursor cursor(Direction direction);
#ifdef Q_OS_WIN
HCURSOR handle(Direction direction);
#endif
}
