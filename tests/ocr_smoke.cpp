#include "OcrService.h"
#include "Window.h"
#include <QApplication>
#include <QEventLoop>
#include <QPainter>
#include <QTemporaryDir>
#include <QPdfWriter>
#include <QThread>
#include <iostream>
#include <stdexcept>

int main(int argc,char** argv) {
    QApplication app(argc,argv);
    QCoreApplication::setApplicationName("GPDF-OCR-Test");
    QTemporaryDir testDirectory;
    QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,testDirectory.path());
    { QSettings settings(QSettings::IniFormat,QSettings::UserScope,AppIdentity::settingsOrganization,AppIdentity::settingsApplication); settings.setValue("updates/automatic",false); settings.setValue("ocr/automatic",false); }
    auto require=[](bool value,const char* message){ if(!value) throw std::runtime_error(message); };
    try {
        PdfDocument pdf; require(pdf.createSample(2),"create PDF");
        const auto revision=pdf.revision(); const bool dirty=pdf.dirty();
        QVector<PdfDocument::OcrWord> words={{QString::fromUtf8("Árbol"),{30,500,60,20},0},{"OCR",{100,500,40,20},0},{"Second",{30,460,70,20},1}};
        require(!pdf.applyOcr(-1,words),"reject invalid page");
        require(pdf.applyOcr(0,words),"apply OCR");
        require(pdf.revision()==revision && pdf.dirty()==dirty,"OCR must not edit PDF");
        require(pdf.pageText(0)==QString::fromUtf8("Árbol OCR\nSecond"),"OCR reading order");
        require(pdf.characterCount()==16,"OCR character count");
        require(pdf.characterAt({31,510})==0,"OCR hit test");
        require(pdf.characterAt({89,510})==4,"OCR word end hit test");
        require(pdf.textAt(0,{110,510}),"hover OCR");
        require(!pdf.textAt(0,{250,250}),"no text outside words");
        require(pdf.wordRange(2)==qMakePair(0,4),"OCR word selection");
        require(pdf.lineRange(7)==qMakePair(0,8),"OCR line selection");
        require(pdf.selectionText(8,6)=="OCR","reverse selection");
        require(pdf.selectionRects(0,8).size()==2,"one box per OCR word");
        auto matches=pdf.searchPage(0,"arbol ocr");
        require(matches.size()==1 && matches[0].rectangles.size()==2,"accent insensitive OCR search and boxes");
        require(pdf.searchPage(0,"\"OC\"").isEmpty(),"whole word search");
        require(pdf.searchPage(0,"\"OCR\"").size()==1,"whole word OCR search");
        require(pdf.highlight(0,8),"OCR highlight");
        require(pdf.annotationCount()==1,"OCR annotation created");
        const auto content=pdf.contentRevision();
        require(pdf.undo() && pdf.hasOcr(0) && pdf.contentRevision()==content,"undo retains recognized text and content identity");
        require(pdf.redo() && pdf.hasOcr(0) && pdf.contentRevision()==content,"redo retains recognized text and content identity");
        PdfDocument large; require(large.createSample(70),"create large PDF");
        for(int page=0;page<70;++page) require(large.applyOcr(page,words),"retain OCR for every page");
        require(large.hasOcr(0) && large.searchPage(0,"OCR").size()==1 && large.searchPage(69,"OCR").size()==1,"full document search beyond 64 OCR pages");
        PdfDocument other; require(other.createSample(),"other PDF");
        pdf.swap(other); require(!pdf.hasOcr(0) && other.hasOcr(0),"OCR follows document tabs");
        other.closeDocument(); require(!other.hasOcr(0),"OCR cleared on close");
        if(app.arguments().contains("--native")) {
            QImage image(1200,400,QImage::Format_ARGB32); image.fill(Qt::white);
            QPainter painter(&image); painter.setPen(Qt::black); painter.setFont(QFont("Arial",40)); painter.drawText(50,160,"GPDF OCR test 123"); painter.end();
            OcrService service;
            for(int attempt=0;attempt<2;++attempt) {
                QEventLoop loop; bool done=false; QString error; QVector<PdfDocument::OcrWord> result;
                service.recognize(image,{600,200},[&](auto words,auto message){ result=words; error=message; done=true; loop.quit(); });
                if(!done) loop.exec();
                require(error.isEmpty(),qPrintable(error));
                QString text; for(const auto& word:result) { text+=word.text+" "; require(word.rect.left()>=0 && word.rect.bottom()<=200,"OCR coordinate mapping"); }
                require(text.contains("GPDF") && text.contains("123"),"real Windows recognition and cached result");
            }
            const auto scanned=testDirectory.filePath("scan.pdf");
            QImage lastImage=image.copy();
            { QPainter lastPainter(&lastImage); lastPainter.fillRect(lastImage.rect(),Qt::white); lastPainter.setPen(Qt::black); lastPainter.setFont(QFont("Arial",40)); lastPainter.drawText(50,160,"GPDF last page 789"); }
            { QPdfWriter writer(scanned); writer.setResolution(144); QPainter pdfPainter(&writer);
              pdfPainter.drawImage(QRect(50,50,1100,367),image);
              writer.newPage(); pdfPainter.setFont(QFont("Arial",18)); pdfPainter.drawText(50,150,"Native digital text 456");
              writer.newPage(); pdfPainter.drawImage(QRect(50,50,1100,367),lastImage); }
            Window window; window.openDocument(scanned);
            PageView* view=nullptr;
            for(auto widget:window.findChildren<QWidget*>()) if(auto page=dynamic_cast<PageView*>(widget)) { view=page; break; }
            require(view && view->pdf.loaded(),"open scanned PDF in reader");
            require(view->pdf.pageText(0).trimmed().isEmpty(),"scan has no native text");
            require(!window.findChild<QMenu*>("ocrMenu") && !window.findChild<QAction*>("ocrAutomatic") && !window.findChild<QAction*>("ocrCurrentPage"),"no OCR options in View menu");
            auto input=window.findChild<QLineEdit*>("searchInput"); auto button=window.findChild<QPushButton*>("searchButton");
            auto results=window.findChild<QListWidget*>("searchResults"); auto status=window.findChild<QLabel*>("searchStatus");
            require(input && button && results && status,"search controls"); input->setText("GPDF"); button->click();
            require(results->count()==0,"search waits for whole document OCR");
            QElapsedTimer elapsed; elapsed.start();
            while((!view->pdf.hasOcr(2) || results->count()!=2) && elapsed.elapsed()<65000) { app.processEvents(); QThread::msleep(20); }
            require(view->pdf.pageIndex()==0,"OCR must not navigate to other pages");
            require(view->pdf.hasOcr(0) && view->pdf.hasOcr(2),"automatic OCR covers unvisited scanned pages");
            require(!view->pdf.hasOcr(1) && view->pdf.pageText(1).contains("Native digital text"),"preserve native text in mixed PDF");
            require(view->pdf.pageText(2).contains("789") && results->count()==2,"search includes last unvisited scanned page");
            require(!view->pdf.dirty(),"whole document OCR does not modify the PDF");
            input->setText("789"); button->click(); elapsed.restart();
            while(results->count()!=1 && elapsed.elapsed()<5000) { app.processEvents(); QThread::msleep(20); }
            require(results->count()==1,"repeat search uses prepared document text");

        }
        std::cout<<"PASS OCR selection, search, annotations, document lifecycle and requested native recognition\n";
        return 0;
    } catch(const std::exception& error) { std::cerr<<error.what()<<'\n'; return 1; }
}
