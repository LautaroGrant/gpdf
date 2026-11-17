#include "Window.h"
#include "UiFont.h"
#include "WindowsIntegration.h"
#include "WelcomeDialog.h"

int main(int argc,char** argv) {
    QApplication app(argc,argv);
    app.setApplicationName("GPDF");
    app.setApplicationVersion(GPDF_VERSION);
    app.setWindowIcon(QIcon(":/icons/gpdf.png"));
    app.setStyle("Fusion");
    const auto initialArguments=app.arguments();
    for(const auto& argument:initialArguments) if(argument.startsWith("--initial-language=")) {
        if(!initializeUiLanguage(argument.mid(19))) return 1;
    }
    if(initialArguments.contains("--initialize-language-only")) return 0;
    installUiTranslations();
    app.setFont(gpdfUiFont());
    const auto arguments=app.arguments();
    const bool snapshot=arguments.size()==4 && arguments[2]=="--snapshot";
    QString detachedSession;
    for(const auto& argument:arguments) if(argument.startsWith("--detached-session=")) detachedSession=argument.mid(19);
    QStringList paths;
    for(int i=1;i<arguments.size();++i) { if(arguments[i]=="--snapshot") break; if(arguments[i].startsWith("--initial-language=") || arguments[i].startsWith("--detached-session=")) continue; paths.append(QFileInfo(arguments[i]).absoluteFilePath()); }
    QString identity=qEnvironmentVariable("USERPROFILE",QDir::homePath());
    SingleInstance instance(QString::fromLatin1(AppIdentity::instancePrefix)+QString::fromLatin1(QCryptographicHash::hash(identity.toUtf8(),QCryptographicHash::Sha256).toHex().left(20)));
#ifdef Q_OS_WIN
    AllowSetForegroundWindow(ASFW_ANY);
#endif
    if(!snapshot && detachedSession.isEmpty()) {
        auto result=instance.start(paths);
        if(result==SingleInstance::Forwarded) return 0;
        if(result==SingleInstance::Failed) { QMessageBox::warning(nullptr,"GPDF",gpdfTr("No se pudo comunicar con la instancia abierta. Volver a intentar abrir el documento.")); return 1; }
    }
    Window window(detachedSession);
    bool startupReady=false; QStringList pendingLaunches;
    instance.received=[&](const QStringList& incoming) {
        if(!startupReady) { pendingLaunches+=incoming; return; }
        window.openExternalDocuments(incoming);
    };
    // Display the shell before disk I/O and session recovery begin.
    if(!snapshot) window.restoreWindow(); else window.show();
    QTimer::singleShot(0,&window,[&] {
        if(!snapshot || !detachedSession.isEmpty()) window.restoreSession();
        for(const auto& path:paths) window.openDocument(path);
        if(!snapshot && !paths.isEmpty()) window.presentForExternalOpen();
        if(!detachedSession.isEmpty() && window.findChild<QTabBar*>("documentTabs")->count()>0) {
            QSaveFile ready(QDir(detachedSession).filePath("ready"));
            if(ready.open(QIODevice::WriteOnly)) { ready.write("ready"); ready.commit(); }
        }
        startupReady=true; if(!pendingLaunches.isEmpty()) instance.received(pendingLaunches);
        if(!snapshot) window.startUpdateChecks();
        if(!snapshot && detachedSession.isEmpty()) showWelcomeOnce(&window);
        if(snapshot) {
            QTimer::singleShot(300,[&]{ const bool ok=window.grab().save(arguments[3]); app.exit(ok ? 0 : 1); });
        }
    });
    // Forwarding processes need no registry work; defer shell notification until
    // the primary window has been displayed.
    if(!snapshot) QTimer::singleShot(1000,&window,[]{ registerPdfApplication(); });
    return app.exec();
}
