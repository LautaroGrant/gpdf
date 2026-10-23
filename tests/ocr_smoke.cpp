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
        // Policy boundaries and mixed documents do not require native recognition.
        const auto makePolicyPdf=[&](const QString& name,int pages,int scanPage,bool numbered=false) {
            const auto path=testDirectory.filePath(name);
            QPdfWriter writer(path); writer.setResolution(72); QPainter painter(&writer);
            QImage scan(600,800,QImage::Format_RGB32); scan.fill(Qt::white);
            { QPainter raster(&scan); raster.setPen(Qt::black); raster.setFont(QFont("Arial",24)); raster.drawText(30,120,"Scanned document text 123"); }
            for(int page=0;page<pages;++page) {
                if(page) writer.newPage();
                if(page==scanPage) {
                    painter.drawImage(QRect(20,20,500,650),scan);
                    if(numbered) painter.drawText(250,710,"6");
                } else if(scanPage!=-2) painter.drawText(30,100,"Native digital text for the OCR policy test");
            }
            painter.end(); return path;
        };
        const auto shortScan=makePolicyPdf("policy-15.pdf",15,0);
        const auto longScan=makePolicyPdf("policy-16.pdf",16,0);
        const auto mixed=makePolicyPdf("policy-mixed.pdf",16,5,true);
        const auto blank=makePolicyPdf("policy-blank.pdf",16,-2);
        {
            PdfDocument detect; require(detect.open(mixed),"open mixed policy fixture");
            require(!detect.pageNeedsOcr(0),"native page needs no OCR");
            require(detect.pageNeedsOcr(5),"image and page number still need OCR");
            const auto content=detect.contentRevision();
            require(!detect.renderOcrPage(5,1).isNull(),"isolated OCR raster");
            require(detect.pageIndex()==0 && !detect.dirty() && detect.contentRevision()==content,"detection and raster preserve document");
            require(detect.open(blank) && !detect.pageNeedsOcr(0),"blank page needs no OCR");
            require(detect.open(shortScan) && !detect.renderOcrPage(0,1).isNull(),"isolated OCR raster includes first page");
        }
        const auto pageView=[](Window& window) {
            for(auto widget:window.findChildren<QWidget*>()) if(auto page=dynamic_cast<PageView*>(widget)) return page;
            return static_cast<PageView*>(nullptr);
        };
        const auto tick=[](Window& window,int times=1) {
            auto timer=window.findChild<QTimer*>("ocrTimer"); timer->stop();
            for(int i=0;i<times;++i) QMetaObject::invokeMethod(timer,"timeout",Qt::DirectConnection);
        };
        const auto capture=[&](Window& window,const QString& name) {
            for(const auto& arg:app.arguments()) if(arg.startsWith("--snapshot-dir=")) {
                QDir directory(arg.mid(15)); directory.mkpath(".");
                window.show(); app.processEvents();
                require(window.grab().save(directory.filePath(name+".png")),"save OCR toolbar preview");
            }
        };
        {
            Window window; window.openDocument(shortScan); tick(window);
            auto bar=window.findChild<QToolBar*>("ocrToolbar"); auto apply=window.findChild<QPushButton*>("ocrApply");
            require(!bar->isHidden() && apply->isHidden(),"15 pages starts automatically after detection");
            require(!window.findChild<QAction*>("ocrDocumentAction")->isEnabled(),"OCR menu disabled during automatic OCR");
            capture(window,"ocr-progress");
            window.findChild<QPushButton*>("ocrCancel")->click(); tick(window,8);
            require(bar->isHidden() && !pageView(window)->pdf.hasOcr(0),"cancel automatic OCR without later restart");
        }
        {
            Window window; window.openDocument(longScan); tick(window);
            auto bar=window.findChild<QToolBar*>("ocrToolbar"); auto apply=window.findChild<QPushButton*>("ocrApply");
            require(!bar->isHidden() && !apply->isHidden(),"16 pages requires confirmation");
            auto menuAction=window.findChild<QAction*>("ocrDocumentAction");
            require(menuAction && menuAction->isEnabled(),"OCR menu enabled for scanned document");
            auto entries=window.findChild<QMenu*>("viewMenu")->actions(); int position=entries.indexOf(menuAction);
            require(position>0 && position+1<entries.size() && entries[position-1]->isSeparator() && entries[position+1]->isSeparator(),"OCR menu has its own section");
            capture(window,"ocr-prompt");
            tick(window,8); require(!pageView(window)->pdf.hasOcr(0),"prompt does not start recognition");
            menuAction->trigger(); require(apply->isHidden() && !bar->isHidden() && !menuAction->isEnabled(),"menu starts OCR and disables duplicate activation");
            window.findChild<QPushButton*>("ocrCancel")->click(); tick(window,8);
            require(bar->isHidden(),"cancel confirmed OCR remains dismissed");
            require(menuAction->isEnabled(),"cancelled OCR can resume from menu");
        }
        {
            Window window; window.openDocument(mixed); tick(window,6);
            auto bar=window.findChild<QToolBar*>("ocrToolbar");
            require(bar->isHidden(),"first five native pages do not trigger OCR");
            require(!window.findChild<QAction*>("ocrDocumentAction")->isEnabled(),"native PDF disables OCR menu");
            tick(window,20); require(bar->isHidden(),"idle does not scan sixth page");
            pageView(window)->activate(5); tick(window);
            require(!bar->isHidden() && !window.findChild<QPushButton*>("ocrApply")->isHidden(),"visiting mixed scan beyond sample offers OCR");
            window.findChild<QPushButton*>("ocrCancel")->click();
            window.openDocument(longScan); tick(window);
            require(!bar->isHidden(),"dismissal belongs to its own document");
        }
        {
            Window window; window.openDocument(blank); tick(window,10);
            require(window.findChild<QToolBar*>("ocrToolbar")->isHidden(),"blank document does not offer OCR");
            require(!window.findChild<QAction*>("ocrDocumentAction")->isEnabled(),"blank PDF disables OCR menu");
        }
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
            QImage cancelled=image.copy(); cancelled.setPixel(0,0,qRgb(12,34,56));
            bool cancelledCallback=false;
            service.recognize(cancelled,{600,200},[&](auto,auto){ cancelledCallback=true; });
            service.cancel();
            require(!service.busy() && !cancelledCallback,"cancel running OCR without delivering stale results");
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
            while((!view->pdf.hasOcr(2) || results->count()!=2) && elapsed.elapsed()<65000) {
                app.processEvents(); QThread::msleep(20);
                if(status->text().contains("incompleto") || status->text().contains("incomplete")) break;
            }
            require(view->pdf.pageIndex()==0,"OCR must not navigate to other pages");
            require(view->pdf.hasOcr(0) && view->pdf.hasOcr(2),qPrintable("automatic OCR covers unvisited scanned pages: "+status->text()));
            require(!view->pdf.hasOcr(1) && view->pdf.pageText(1).contains("Native digital text"),"preserve native text in mixed PDF");
            require(view->pdf.pageText(2).contains("789") && results->count()==2,"search includes last unvisited scanned page");
            require(!window.findChild<QAction*>("ocrDocumentAction")->isEnabled(),"completed OCR disables menu action");
            require(!view->pdf.dirty(),"whole document OCR does not modify the PDF");
            input->setText("789"); button->click(); elapsed.restart();
            while(results->count()!=1 && elapsed.elapsed()<5000) { app.processEvents(); QThread::msleep(20); }
            require(results->count()==1,"repeat search uses prepared document text");

        }
        std::cout<<"PASS OCR selection, search, annotations, document lifecycle and requested native recognition\n";
        return 0;
    } catch(const std::exception& error) { std::cerr<<error.what()<<'\n'; return 1; }
}
