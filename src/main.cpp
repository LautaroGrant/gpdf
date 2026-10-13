#include "Window.h"
#include "UiFont.h"
#include "WindowsIntegration.h"

int main(int argc,char** argv) {
    configureUiFontBackend();
    QApplication app(argc,argv);
    app.setApplicationName("GPDF");
    app.setApplicationVersion(GPDF_VERSION);
    app.setWindowIcon(QIcon(":/icons/gpdf.png"));
    app.setStyle("Fusion");
    installSpanishTranslations();
    app.setFont(gpdfUiFont());
    const auto arguments=app.arguments();
    const bool snapshot=arguments.size()==4 && arguments[2]=="--snapshot";
    if(!snapshot) registerPdfApplication();
    QStringList paths;
    for(int i=1;i<arguments.size();++i) { if(arguments[i]=="--snapshot") break; paths.append(QFileInfo(arguments[i]).absoluteFilePath()); }
    QString identity=qEnvironmentVariable("USERPROFILE",QDir::homePath());
    SingleInstance instance(QString::fromLatin1(AppIdentity::instancePrefix)+QString::fromLatin1(QCryptographicHash::hash(identity.toUtf8(),QCryptographicHash::Sha256).toHex().left(20)));
#ifdef Q_OS_WIN
    AllowSetForegroundWindow(ASFW_ANY);
#endif
    if(!snapshot) {
        auto result=instance.start(paths);
        if(result==SingleInstance::Forwarded) return 0;
        if(result==SingleInstance::Failed) { QMessageBox::warning(nullptr,"GPDF","No se pudo comunicar con la instancia abierta. Volver a intentar abrir el documento."); return 1; }
    }
    Window window;
    bool startupReady=false; QStringList pendingLaunches;
    instance.received=[&](const QStringList& incoming) {
        if(!startupReady) { pendingLaunches+=incoming; return; }
        window.openExternalDocuments(incoming);
    };
    if(!snapshot) window.restoreSession();
    for(const auto& path:paths) window.openDocument(path);
    if(!snapshot) window.restoreWindow(); else window.show();
    if(!snapshot && !paths.isEmpty()) window.presentForExternalOpen();
    startupReady=true; if(!pendingLaunches.isEmpty()) instance.received(pendingLaunches);
    if(snapshot) {
        QTimer::singleShot(300,[&]{ const bool ok=window.grab().save(arguments[3]); app.exit(ok ? 0 : 1); });
    }
    return app.exec();
}
