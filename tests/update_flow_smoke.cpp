#include "UpdateDownload.h"
#include <QtWidgets>
#include <QProcess>
#include <QTemporaryDir>
#include <iostream>
#include <stdexcept>

int main(int argc,char** argv) {
    // Only our own test executable is used to verify Windows launch semantics.
    if(argc>1 && QByteArray(argv[1])=="--update-child") return 0;
    QApplication app(argc,argv);
    QTemporaryDir settingsRoot;
    QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,settingsRoot.path());
    auto require=[](bool ok,const char* message){ if(!ok) throw std::runtime_error(message); };
    try {
        QFile self(QCoreApplication::applicationFilePath()); require(self.open(QIODevice::ReadOnly),"read test payload");
        const auto payload=self.readAll();
        UpdateRelease release; release.assetName="GPDF-99.0.0-Setup.exe"; release.bytes=payload.size();
        release.sha256=QCryptographicHash::hash(payload,QCryptographicHash::Sha256);
        QString temporaryPath;
        {
            UpdateDownload download(release); require(download.open(),"open isolated download"); temporaryPath=download.path();
            require(download.append(payload.left(payload.size()/2)) && download.append(payload.mid(payload.size()/2)),"stream payload");
            require(download.verifyAndClose(),"verify payload and release Windows write handle");
            QProcess child;
            auto environment=QProcessEnvironment::systemEnvironment();
            environment.insert("PATH",QCoreApplication::applicationDirPath()+QDir::listSeparator()+environment.value("PATH"));
            child.setProcessEnvironment(environment); child.start(download.path(),{"--update-child"});
            require(child.waitForStarted(10000),qPrintable(child.errorString()));
            require(child.waitForFinished(10000) && child.exitCode()==0,"verified installer payload can execute before download object destruction");
        }
        require(!QFileInfo::exists(temporaryPath),"unretained download cleaned up");
        {
            UpdateDownload download(release); require(download.open(),"open truncated download"); temporaryPath=download.path();
            require(download.append(payload.left(100)) && !download.verifyAndClose(),"reject truncated download");
        }
        require(!QFileInfo::exists(temporaryPath),"failed download cleaned up");
        {
            auto incorrect=release; incorrect.sha256=QByteArray(32,'x');
            UpdateDownload download(incorrect); require(download.open() && download.append(payload) && !download.verifyAndClose(),"reject incorrect SHA-256");
        }
        {
            auto shortLimit=release; shortLimit.bytes=10;
            UpdateDownload download(shortLimit); require(download.open() && !download.append(payload) && !download.verifyAndClose(),"reject oversized response");
        }
        if(app.arguments().contains("--live")) {
            QSettings settings(QSettings::IniFormat,QSettings::UserScope,"StudyPDF","StudyPDF");
            settings.setValue("updates/automatic",true);
            settings.setValue("updates/lastCheck",QDateTime::currentDateTimeUtc());
            QWidget parent; parent.show(); UpdateManager manager(&parent);
            // Hold a modal past the startup check to verify deferred notification.
            QDialog blocking(&parent); blocking.setModal(true); blocking.show();
            bool offered=false; QElapsedTimer elapsed; elapsed.start(); QEventLoop loop; QTimer poll;
            QObject::connect(&poll,&QTimer::timeout,[&]{
                if(elapsed.elapsed()>8000 && blocking.isVisible()) blocking.accept();
                for(auto widget:QApplication::topLevelWidgets()) if(auto box=qobject_cast<QMessageBox*>(widget)) {
                    if(box->text().contains("0.3.0")) offered=true;
                    // Dismiss the offer; this test never downloads or runs the installer.
                    box->reject(); loop.quit();
                }
                if(elapsed.elapsed()>40000) loop.quit();
            });
            poll.start(50); manager.startAutomaticCheck(); manager.startAutomaticCheck(); loop.exec();
            require(offered,"automatic check ignores a recent shared lastCheck and survives an initial modal");
        }
        std::cout<<"PASS: verified download launches with closed handles; hash, size, cleanup and requested live notification checks\n";
        return 0;
    } catch(const std::exception& error) { std::cerr<<error.what()<<'\n'; return 1; }
}
