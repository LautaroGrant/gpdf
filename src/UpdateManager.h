#pragma once
#include <QJsonObject>
#include <QUrl>
#include <QVersionNumber>
#include <optional>
#include <functional>
#include <QObject>
class QWidget;
class QNetworkAccessManager;
struct UpdateRelease {
    QString version, notes, assetName;
    QUrl installer;
    QByteArray sha256;
    qint64 bytes=0;
};
std::optional<UpdateRelease> parseUpdateRelease(const QJsonObject& json,const QString& current);
class UpdateManager : public QObject {
public:
    explicit UpdateManager(QWidget* window);
    void check(bool manual);
    void startAutomaticCheck();
private:
    QWidget* window;
    QNetworkAccessManager* network;
    bool busy=false;
    void offer(const UpdateRelease& release);
    void download(const UpdateRelease& release);
};
