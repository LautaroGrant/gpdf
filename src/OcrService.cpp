#include "OcrService.h"
#include "WindowsOcrScript.h"
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>
#include <QSysInfo>
#include <QLocale>

OcrService::OcrService(QObject* parent):QObject(parent) {
    timeout_.setSingleShot(true);
    connect(&timeout_,&QTimer::timeout,this,[this]{ timedOut_=true; process_.kill(); });
    connect(&process_,&QProcess::errorOccurred,this,[this](QProcess::ProcessError error){
        if(error==QProcess::FailedToStart) finish(process_.errorString());
    });
    connect(&process_,&QProcess::finished,this,[this](int code,QProcess::ExitStatus status){
        finish(timedOut_ ? QString("OCR timeout (60 s).") : (code==0 && status==QProcess::NormalExit ? QString() : QString::fromUtf8(process_.readAllStandardError()).trimmed()));
    });
}
OcrService::~OcrService() {
    process_.disconnect(this); completion_={}; process_.kill(); process_.waitForFinished(3000);
}
void OcrService::recognize(const QImage& image,QSizeF pageSize,Completion completion) {
    if(busy()) return;
    if(image.isNull() || pageSize.isEmpty()) { completion({},"Cannot render page for OCR."); return; }
    imageSize_=image.size(); pageSize_=pageSize; completion_=std::move(completion); timedOut_=false;
    // Cache is versioned and keyed by exact rendered pixels and user language.
    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(QByteArrayView(reinterpret_cast<const char*>(image.constBits()),image.sizeInBytes()));
    hash.addData(QByteArray::number(image.width())+":"+QByteArray::number(image.height())+":"+QSysInfo::productVersion().toUtf8()+":"+QLocale::system().uiLanguages().join(',').toUtf8());
    const auto directory=QStandardPaths::writableLocation(QStandardPaths::CacheLocation)+"/ocr-v1";
    QDir().mkpath(directory);
    // Keep the persistent cache bounded to 128 recent pages.
    const auto files=QDir(directory).entryInfoList({"*.json"},QDir::Files,QDir::Time);
    for(int i=127;i<files.size();++i) QFile::remove(files[i].absoluteFilePath());
    cachePath_=directory+"/"+QString::fromLatin1(hash.result().toHex())+".json";
    QFile cached(cachePath_);
    if(cached.open(QIODevice::ReadOnly) && cached.size()<8*1024*1024) {
        const auto payload=cached.readAll();
        if(QJsonDocument::fromJson(payload).object().value("words").isArray()) {
            complete(payload); return;
        }
    }
    QFile script(temporary_.filePath("ocr.ps1"));
    if(!temporary_.isValid() || !image.save(temporary_.filePath("page.png")) || !script.open(QIODevice::WriteOnly)) { finish("Cannot create temporary OCR files."); return; }
    script.write(windowsOcrScript); script.close();
    process_.setProgram(qEnvironmentVariable("SystemRoot","C:/Windows")+"/System32/WindowsPowerShell/v1.0/powershell.exe");
    process_.setArguments({"-NoLogo","-NoProfile","-NonInteractive","-ExecutionPolicy","Bypass","-File",script.fileName(),"-ImagePath",temporary_.filePath("page.png")});
    timeout_.start(60000); process_.start();
}
void OcrService::finish(QString error) {
    if(!completion_) return;
    timeout_.stop();
    const auto payload=process_.readAllStandardOutput();
    complete(payload,error);
}
void OcrService::complete(const QByteArray& payload,QString error) {
    const auto json=QJsonDocument::fromJson(payload).object();
    if(error.isEmpty() && !json.value("words").isArray()) error="Windows OCR returned no valid result.";
    QVector<PdfDocument::OcrWord> words;
    if(error.isEmpty()) {
        const double sx=pageSize_.width()/imageSize_.width(),sy=pageSize_.height()/imageSize_.height();
        for(const auto& value:json["words"].toArray()) {
            const auto w=value.toObject();
            words.append({w["text"].toString(),QRectF(w["x"].toDouble()*sx,pageSize_.height()-(w["y"].toDouble()+w["h"].toDouble())*sy,w["w"].toDouble()*sx,w["h"].toDouble()*sy),w["line"].toInt()});
        }
        QSaveFile cache(cachePath_); if(cache.open(QIODevice::WriteOnly)) { cache.write(payload); cache.commit(); }
    }
    QFile::remove(temporary_.filePath("page.png"));
    auto done=std::move(completion_); completion_={}; done(words,error);
}
