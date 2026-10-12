#include "Window.h"

int main(int argc,char** argv) {
    QApplication app(argc,argv);
    app.setApplicationVersion(STUDYPDF_VERSION);
    app.setStyle("Fusion");
    installSpanishTranslations();
    app.setFont(QFont("Segoe UI",10));
    const auto arguments=app.arguments();
    const bool snapshot=arguments.size()==4 && arguments[2]=="--snapshot";
    QStringList paths;
    for(int i=1;i<arguments.size();++i) { if(arguments[i]=="--snapshot") break; paths.append(QFileInfo(arguments[i]).absoluteFilePath()); }
    QString identity=qEnvironmentVariable("USERPROFILE",QDir::homePath());
    SingleInstance instance("StudyPDF-"+QString::fromLatin1(QCryptographicHash::hash(identity.toUtf8(),QCryptographicHash::Sha256).toHex().left(20)));
#ifdef Q_OS_WIN
    AllowSetForegroundWindow(ASFW_ANY);
#endif
    if(!snapshot) {
        auto result=instance.start(paths);
        if(result==SingleInstance::Forwarded) return 0;
        if(result==SingleInstance::Failed) { QMessageBox::warning(nullptr,"Study PDF","No se pudo comunicar con la instancia abierta. Volvé a intentar abrir el documento."); return 1; }
    }
    Window window; window.show();
    bool startupReady=false; QStringList pendingLaunches;
    instance.received=[&](const QStringList& incoming) {
        if(!startupReady) { pendingLaunches+=incoming; return; }
        for(const auto& path:incoming) window.openDocument(path);
        if(window.isMinimized()) window.showNormal();
        window.raise(); window.activateWindow();
#ifdef Q_OS_WIN
        SetForegroundWindow(reinterpret_cast<HWND>(window.winId()));
#endif
    };
    if(!snapshot) { window.restoreSession(); window.restoreWindow(); }
    for(const auto& path:paths) window.openDocument(path);
    startupReady=true; if(!pendingLaunches.isEmpty()) instance.received(pendingLaunches);
    if(snapshot) {
        QTimer::singleShot(300,[&]{ const bool ok=window.grab().save(arguments[3]); app.exit(ok ? 0 : 1); });
    }
    return app.exec();
}
