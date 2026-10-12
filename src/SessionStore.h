#pragma once
#include <QDir>
#include <QJsonObject>
namespace SessionStore {
QJsonObject read(const QDir& root);
bool write(const QDir& root,const QJsonObject& manifest);
}
