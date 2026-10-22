#pragma once
#include "PdfDocument.h"
#include <QObject>
#include <QProcess>
#include <QTemporaryDir>
#include <QTimer>
#include <functional>

class OcrService : public QObject {
public:
    using Completion=std::function<void(QVector<PdfDocument::OcrWord>,QString)>;
    explicit OcrService(QObject* parent=nullptr);
    ~OcrService() override;
    bool busy() const { return bool(completion_); }
    void recognize(const QImage& image,QSizeF pageSize,Completion completion);
private:
    void finish(QString error={});
    void complete(const QByteArray& payload,QString error={});
    QProcess process_;
    QTimer timeout_;
    QTemporaryDir temporary_;
    Completion completion_;
    QSize imageSize_;
    QSizeF pageSize_;
    QString cachePath_;
    bool timedOut_=false;
};
