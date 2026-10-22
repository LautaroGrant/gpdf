#include "UpdateManager.h"
#include "UpdateDownload.h"
#include "AppIdentity.h"
#include "Translations.h"
#include <QtWidgets>
#include <QtNetwork>
#include <memory>

namespace {
constexpr auto releasesUrl="https://github.com/LautaroGrant/gpdf/releases/latest";
constexpr qint64 maxInstaller=256LL*1024*1024;
QSettings settings() { return QSettings(QSettings::IniFormat,QSettings::UserScope,AppIdentity::settingsOrganization,AppIdentity::settingsApplication); }
QVersionNumber versionNumber(QString text) {
    if(text.startsWith('v')) text.remove(0,1);
    if(!QRegularExpression("^[0-9]+\\.[0-9]+\\.[0-9]+$").match(text).hasMatch()) return {};
    return QVersionNumber::fromString(text);
}
QNetworkRequest request(const QUrl& url) {
    QNetworkRequest req(url); req.setRawHeader("User-Agent","GPDF/" GPDF_VERSION);
    req.setRawHeader("Accept","application/vnd.github+json");
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::NoLessSafeRedirectPolicy);
    req.setTransferTimeout(30000); return req;
}
bool installedCopy() {
#ifdef Q_OS_WIN
    return QFileInfo::exists(QCoreApplication::applicationDirPath()+"/unins000.exe");
#else
    return false;
#endif
}
}
std::optional<UpdateRelease> parseUpdateRelease(const QJsonObject& json,const QString& current) {
    if(json.value("draft").toBool() || json.value("prerelease").toBool()) return {};
    auto tag=json.value("tag_name").toString(); auto version=versionNumber(tag);
    if(version.isNull() || version<=versionNumber(current)) return {};
    UpdateRelease release; release.version=version.toString(); release.notes=json.value("body").toString().left(12000);
    release.assetName="GPDF-"+release.version+"-Setup.exe";
    for(const auto& value:json.value("assets").toArray()) {
        auto asset=value.toObject(); if(asset.value("name").toString()!=release.assetName) continue;
        auto url=QUrl(asset.value("browser_download_url").toString());
        const auto prefix="/LautaroGrant/gpdf/releases/download/"+tag+"/";
        auto digest=asset.value("digest").toString();
        auto bytes=asset.value("size").toInteger();
        if(url.scheme()!="https" || url.host()!="github.com" || !url.userInfo().isEmpty() || url.port()!=-1 || url.hasQuery() || url.hasFragment() || url.path()!=prefix+release.assetName) continue;
        if(!QRegularExpression("^sha256:[a-fA-F0-9]{64}$").match(digest).hasMatch() || bytes<=0 || bytes>maxInstaller) continue;
        release.installer=url; release.sha256=QByteArray::fromHex(digest.mid(7).toLatin1()); release.bytes=bytes;
    }
    return release;
}
UpdateManager::UpdateManager(QWidget* parent):QObject(parent),window(parent),network(new QNetworkAccessManager(this)) {}
void UpdateManager::startAutomaticCheck() {
    if(automaticStarted) return;
    automaticStarted=true;
    QTimer::singleShot(5000,this,[this]{ if(settings().value("updates/automatic",true).toBool()) check(false); });
}
void UpdateManager::check(bool manual) {
    if(busy) { if(manual) QMessageBox::information(window,gpdfTr("Actualizaciones"),gpdfTr("Ya hay una comprobación o descarga en curso.")); return; }
    busy=true;
    auto reply=network->get(request(QUrl("https://api.github.com/repos/LautaroGrant/gpdf/releases/latest")));
    reply->setReadBufferSize(1024*1024+1);
    connect(reply,&QNetworkReply::readyRead,this,[reply]{ if(reply->bytesAvailable()>1024*1024) reply->abort(); });
    connect(reply,&QNetworkReply::finished,this,[this,reply,manual]{
        busy=false; reply->deleteLater();
        if(reply->error()!=QNetworkReply::NoError) {
            if(manual) QMessageBox::warning(window,gpdfTr("Actualizaciones"),gpdfTr("No se pudo consultar GitHub. Comprobar la conexión e intentar más tarde.")); return;
        }
        QJsonParseError error; auto document=QJsonDocument::fromJson(reply->readAll(),&error);
        if(error.error!=QJsonParseError::NoError || !document.isObject() || !document.object().contains("tag_name")) {
            if(manual) QMessageBox::warning(window,gpdfTr("Actualizaciones"),gpdfTr("La respuesta de GitHub no es válida.")); return;
        }
        settings().setValue("updates/lastCheck",QDateTime::currentDateTimeUtc());
        auto release=parseUpdateRelease(document.object(),GPDF_VERSION);
        if(!release) { if(manual) QMessageBox::information(window,gpdfTr("Actualizaciones"),gpdfTr("GPDF está actualizado.")); return; }
        if(manual) offer(*release); else offerAutomatically(*release);
    });
}
void UpdateManager::offerAutomatically(const UpdateRelease& release) {
    if(!settings().value("updates/automatic",true).toBool() || settings().value("updates/skipped").toString()==release.version) return;
    if(!window->isVisible() || window->isMinimized() || QApplication::activeModalWidget() || busy) {
        // Keep the known result; do not lose the notification or re-query GitHub.
        QTimer::singleShot(1000,this,[this,release]{ offerAutomatically(release); }); return;
    }
    offer(release);
}
void UpdateManager::offer(const UpdateRelease& release) {
    QMessageBox box(QMessageBox::Information,gpdfTr("Actualizaciones"),QString(gpdfTr("GPDF %1 está disponible.")).arg(release.version),QMessageBox::NoButton,window);
    box.setTextFormat(Qt::PlainText); box.setInformativeText(release.notes);
    const bool canInstall=installedCopy() && !release.installer.isEmpty();
    auto install=box.addButton(canInstall ? gpdfTr("Descargar e instalar") : gpdfTr("Abrir página de descarga"),QMessageBox::AcceptRole);
    auto later=box.addButton(gpdfTr("Más tarde"),QMessageBox::RejectRole);
    auto skip=box.addButton(gpdfTr("Omitir esta versión"),QMessageBox::ActionRole); box.setDefaultButton(later); box.exec();
    if(box.clickedButton()==skip) settings().setValue("updates/skipped",release.version);
    if(box.clickedButton()==install) {
        if(canInstall) download(release); else QDesktopServices::openUrl(QUrl(releasesUrl));
    }
}
void UpdateManager::download(const UpdateRelease& release) {
    busy=true;
    auto file=std::make_shared<UpdateDownload>(release);
    if(!file->open()) { busy=false; QMessageBox::warning(window,gpdfTr("Actualizaciones"),gpdfTr("No se pudo crear el archivo de actualización.")); return; }
    auto failed=std::make_shared<bool>(false);
    auto reply=network->get(request(release.installer));
    auto progress=new QProgressDialog(gpdfTr("Descargando actualización…"),gpdfTr("Cancelar"),0,100,window);
    progress->setWindowModality(Qt::WindowModal); progress->setMinimumDuration(0); progress->setAutoClose(false);
    connect(progress,&QProgressDialog::canceled,reply,&QNetworkReply::abort);
    connect(reply,&QNetworkReply::downloadProgress,progress,[progress](qint64 done,qint64 total){ if(total>0) progress->setValue(int(done*100/total)); });
    auto consume=[reply,file,failed]{
        auto data=reply->readAll();
        if(*failed) return;
        if(!file->append(data)) { *failed=true; reply->abort(); }
    };
    connect(reply,&QNetworkReply::readyRead,this,consume);
    connect(reply,&QNetworkReply::finished,this,[this,reply,file,failed,progress,consume]{
        consume(); busy=false;
        disconnect(progress,&QProgressDialog::canceled,reply,&QNetworkReply::abort);
        progress->hide(); progress->deleteLater(); reply->deleteLater();
        const bool verified=file->verifyAndClose();
        if(reply->error()==QNetworkReply::OperationCanceledError && !*failed) return;
        if(reply->error()!=QNetworkReply::NoError || *failed || !verified) {
            QMessageBox::warning(window,gpdfTr("Actualizaciones"),gpdfTr("La descarga falló o no superó la verificación SHA-256. No se ejecutó ningún archivo.")); return;
        }
        if(QMessageBox::question(window,gpdfTr("Actualizaciones"),gpdfTr("La actualización está lista. Cerrar GPDF y abrir el instalador? Se solicitará guardar los documentos modificados."),QMessageBox::Yes|QMessageBox::No,QMessageBox::No)!=QMessageBox::Yes) return;
        const bool quit=QApplication::quitOnLastWindowClosed(); QApplication::setQuitOnLastWindowClosed(false);
        if(!window->close()) { QApplication::setQuitOnLastWindowClosed(quit); return; }
        const auto path=file->path();
        if(!QProcess::startDetached(path,{"/DIR="+QDir::toNativeSeparators(QCoreApplication::applicationDirPath()),"/LANG="+QString(currentUiLanguage()=="en" ? "english" : "spanish")})) {
            QMessageBox::warning(nullptr,gpdfTr("Actualizaciones"),gpdfTr("No se pudo iniciar el instalador. Volver a abrir GPDF e intentar de nuevo."));
        } else file->retainForInstaller();
        QApplication::quit();
    });
}
