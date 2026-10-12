#include "SessionStore.h"
#include <QFile>
#include <QSaveFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QSet>

QJsonObject SessionStore::read(const QDir& root) {
    QFile file(root.filePath("session.json"));
    if(!file.open(QIODevice::ReadOnly)) return {};
    auto bytes=file.readAll(); file.close();
    return QJsonDocument::fromJson(bytes).object();
}
bool SessionStore::write(const QDir& root,const QJsonObject& manifest) {
    if(!root.exists() && !QDir().mkpath(root.absolutePath())) return false;
    QSaveFile file(root.filePath("session.json"));
    auto bytes=QJsonDocument(manifest).toJson();
    if(!file.open(QIODevice::WriteOnly) || file.write(bytes)!=bytes.size() || !file.commit()) return false;
    // Only discard obsolete backups after the new manifest has committed.
    QSet<QString> retained;
    for(auto item:manifest["documents"].toArray()) retained.insert(item.toObject()["backup"].toString());
    for(const auto& name:root.entryList({"*.pdf"},QDir::Files)) if(!retained.contains(name)) QFile::remove(root.filePath(name));
    return true;
}
