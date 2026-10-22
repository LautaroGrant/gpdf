#include "PdfDocument.h"
#include <QApplication>
#include <QCryptographicHash>
#include <QFile>
#include <QDir>
#include <QTemporaryDir>
#include <QPdfWriter>
#include <QPainter>
#include <QElapsedTimer>
#include <iostream>
#include <stdexcept>

static QByteArray hashFile(const QString& path) {
    QFile file(path);
    if(!file.open(QIODevice::ReadOnly)) throw std::runtime_error("Cannot read input for hashing");
    QCryptographicHash hash(QCryptographicHash::Sha256);
    if(!hash.addData(&file)) throw std::runtime_error("Cannot hash input");
    return hash.result();
}
static void require(bool ok,const char* message) { if(!ok) throw std::runtime_error(message); }
static void generate(const QString& path,int count,bool scanned) {
    QPdfWriter writer(path); writer.setResolution(72);
    QPainter painter(&writer);
    require(painter.isActive(),"Cannot generate fixture");
    for(int page=0;page<count;++page) {
        if(page) require(writer.newPage(),"Cannot append fixture page");
        if(scanned) {
            QImage image(540,720,QImage::Format_RGB32); image.fill(Qt::white);
            QPainter bitmap(&image); bitmap.setPen(Qt::black); bitmap.setFont(QFont("Arial",18));
            bitmap.drawText(QRect(30,30,480,650),Qt::TextWordWrap,QString("Scanned study material\nPage %1\nText exists only in pixels.").arg(page+1)); bitmap.end();
            painter.drawImage(QRect(0,0,writer.width(),writer.height()),image);
        } else {
            painter.setFont(QFont("Arial",16));
            painter.drawText(QRect(20,20,writer.width()-40,writer.height()-40),Qt::TextWordWrap,QString("GPDF corpus page %1\nUnicode: Cortázar, investigación.\nA searchable document with many pages.").arg(page+1));
            painter.setPen(QPen(Qt::blue,2)); painter.drawEllipse(QRect(60,140,180,100));
        }
    }
}
static void validate(const QString& path,const QString& output,bool scanned=false,int expected=0) {
    const auto before=hashFile(path);
    QElapsedTimer elapsed; elapsed.start();
    PdfDocument document; require(document.open(path),"Open failed");
    const int count=document.pageCount(); require(count>0,"Empty document");
    if(expected) require(count==expected,"Unexpected fixture page count");
    qint64 openMs=elapsed.elapsed(); elapsed.restart();
    for(int page=0;page<count;++page) {
        auto size=document.pageSize(page); require(size.width()>0 && size.height()>0,"Invalid page dimensions");
        document.pageText(page);
    }
    for(int page:QVector<int>{0,count/2,count-1}) require(!document.renderPage(page,0.75).isNull(),"Representative page render failed");
    if(scanned) require(document.pageText(0).trimmed().isEmpty() && document.searchPage(0,"Scanned").isEmpty(),"Image-only PDF should have no searchable text");
    if(expected && !scanned) require(!document.searchPage(count-1,"Cortazar").isEmpty(),"Last-page accent-insensitive search failed");
    qint64 readMs=elapsed.elapsed();
    const int page=count/2; require(document.setPage(page),"Select annotation page failed");
    const int baseline=document.annotationCount(); const QString note="Corpus verification — investigación, Cortázar";
    require(document.note(QPointF(20,20),note),"Create annotation failed");
    require(document.undo() && document.annotationCount()==baseline,"Undo failed");
    require(document.redo() && document.annotationCount()==baseline+1,"Redo failed");
    elapsed.restart(); require(document.saveRecovery(output+".recovery.pdf"),"Recovery failed");
    const qint64 recoveryMs=elapsed.elapsed();
    PdfDocument recovery; require(recovery.open(output+".recovery.pdf") && recovery.setPage(page),"Recovery reopen failed");
    require(recovery.annotationCount()==baseline+1 && recovery.annotationContents(baseline)==note,"Recovery content mismatch");
    elapsed.restart(); require(document.saveCopy(output),"Save copy failed"); const qint64 saveMs=elapsed.elapsed();
    PdfDocument reopened; require(reopened.open(output) && reopened.setPage(page),"Saved copy reopen failed");
    require(reopened.annotationCount()==baseline+1 && reopened.annotationContents(baseline)==note,"Saved annotation mismatch");
    require(!reopened.renderPage(page,0.75).isNull(),"Saved page render failed");
    require(hashFile(path)==before,"Original file changed");
    std::cout<<"PASS pages="<<count<<" open_ms="<<openMs<<" read_render_ms="<<readMs<<" recovery_ms="<<recoveryMs<<" save_ms="<<saveMs<<" original_unchanged=yes\n";
}
int main(int argc,char** argv) {
    QApplication app(argc,argv);
    try {
        auto inputs=app.arguments().mid(1);
        QString outputRoot;
        if(inputs.size()>=2 && inputs[0]=="--output-directory") {
            outputRoot=inputs[1]; inputs.removeFirst(); inputs.removeFirst();
            require(QDir().mkpath(outputRoot),"Cannot create output directory");
        }
        const bool stress=inputs.removeAll("--stress")>0;
        QTemporaryDir temporary(outputRoot.isEmpty() ? QDir::tempPath()+"/GPDF-corpus-XXXXXX" : QDir(outputRoot).filePath("corpus-XXXXXX"));
        require(temporary.isValid(),"No temporary directory");
        if(!outputRoot.isEmpty()) { temporary.setAutoRemove(false); std::cout<<"OUTPUT "<<temporary.path().toStdString()<<'\n'; }
        if(inputs.isEmpty()) {
            PdfDocument fontInitializer; // Load Windows fonts before painting offscreen fixtures.
            const QString scan=temporary.filePath("scan.pdf"),large=temporary.filePath("many-pages.pdf");
            generate(scan,3,true); generate(large,130,false);
            validate(scan,temporary.filePath("scan-copy.pdf"),true,3);
            validate(large,temporary.filePath("large-copy.pdf"),false,130);
            if(stress) {
                QFile padded(large); require(padded.open(QIODevice::Append),"Cannot pad stress fixture");
                const QByteArray padding(1024*1024,' ');
                for(int block=0;block<64;++block) require(padded.write(padding)==padding.size(),"Cannot write stress padding");
                padded.close(); validate(large,temporary.filePath("stress-copy.pdf"),false,130);
            }
        } else for(int i=0;i<inputs.size();++i) validate(inputs[i],temporary.filePath(QString("copy-%1.pdf").arg(i)));
        return 0;
    } catch(const std::exception& error) { std::cerr<<"FAIL: "<<error.what()<<'\n'; return 1; }
}
