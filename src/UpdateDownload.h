#pragma once
#include "UpdateManager.h"
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>

// QFile::close releases the Windows write handle. QTemporaryFile::close does
// not, so a QTemporaryFile cannot be used directly as a runnable installer.
class UpdateDownload {
    QTemporaryDir directory_{QDir::tempPath()+"/GPDF-update-XXXXXX"};
    QFile file_;
    QCryptographicHash hash_{QCryptographicHash::Sha256};
    qint64 expectedBytes_;
    QByteArray expectedHash_;
    bool failed_=false,verified_=false;
public:
    explicit UpdateDownload(const UpdateRelease& release):expectedBytes_(release.bytes),expectedHash_(release.sha256) {
        if(directory_.isValid() && QFileInfo(release.assetName).fileName()==release.assetName)
            file_.setFileName(directory_.filePath(release.assetName));
    }
    bool open() {
        return directory_.isValid() && !file_.fileName().isEmpty() && expectedBytes_>0
            && file_.open(QIODevice::WriteOnly|QIODevice::NewOnly);
    }
    bool append(const QByteArray& data) {
        if(failed_ || !file_.isOpen() || data.size()>expectedBytes_-file_.size() || file_.write(data)!=data.size()) {
            failed_=true; return false;
        }
        hash_.addData(data); return true;
    }
    bool verifyAndClose() {
        verified_=!failed_ && file_.isOpen() && file_.size()==expectedBytes_
            && hash_.result()==expectedHash_ && file_.flush();
        file_.close(); // Must happen before attempting CreateProcess/startDetached.
        return verified_;
    }
    QString path() const { return file_.fileName(); }
    void retainForInstaller() { if(verified_) directory_.setAutoRemove(false); }
};
