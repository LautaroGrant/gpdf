#pragma once
#include "PdfDocument.h"
#include <QRawFont>
#include <functional>
#include <atomic>

inline QString workbenchTr(const char* es,const char* en) {
    return QString::fromUtf8(currentUiLanguage()=="en" ? en : es);
}

// A separate output document: page operations never edit an input document.
class WorkbenchPdf {
public:
    WorkbenchPdf()=default;
    ~WorkbenchPdf();
    bool appendPage(const PdfDocument& source,int page,int quarterTurns=0);
    bool addOcrText(int page,const QVector<PdfDocument::OcrWord>& words);
    bool save(const QString& path);
    const PdfDocument& document() const { return output_; }
    QString error() const { return error_; }
    static bool parsePages(const QString& text,int count,QVector<int>& pages,QString& error);
private:
    bool fail(const QString& error) { error_=error; return false; }
    PdfDocument output_;
    FPDF_FONT font_=nullptr;
    QRawFont rawFont_;
    QString error_;
};

struct WorkbenchPage {
    std::shared_ptr<PdfDocument> document;
    int page=0,quarterTurns=0;
    QString sourcePath;
};
struct WorkbenchOutput {
    QVector<WorkbenchPage> pages;
    QString path;
    bool ocr=false,overwrite=false;
};
class WorkbenchExport {
public:
    using Recognizer=std::function<bool(const QImage&,QSizeF,QVector<PdfDocument::OcrWord>&,QString&)>;
    static QString run(const QVector<WorkbenchOutput>& outputs,const std::atomic_bool& cancelled,
                       std::function<void(int,int)> progress={},Recognizer recognize={});
};
