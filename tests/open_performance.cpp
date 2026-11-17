#include "PdfDocument.h"
#include <QApplication>
#include <QElapsedTimer>
#include <QPainter>
#include <QPdfWriter>
#include <QTemporaryDir>
#include <iostream>
#include <stdexcept>
#include <limits>

static void require(bool ok,const char* message) {
    if(!ok) throw std::runtime_error(message);
}

int main(int argc,char** argv) {
    QApplication app(argc,argv);
    PdfDocument library;
    try {
        QTemporaryDir directory;
        const auto path=directory.filePath("dense.pdf");
        {
            QPdfWriter writer(path); writer.setResolution(72);
            QPainter painter(&writer); painter.setFont(QFont("Arial",5));
            for(int page=0;page<3;++page) {
                if(page) require(writer.newPage(),"new page");
                for(int row=0;row<120;++row)
                    painter.drawText(10,10+row*6,QString("Page %1: ").arg(page)+QString("searchable text 0123456789 ").repeated(5));
            }
        }
        double openMs=0,textMs=0;
        for(int run=0;run<6;++run) {
            PdfDocument pdf; QElapsedTimer timer; timer.start();
            require(pdf.open(path),"open dense PDF");
            const double opened=timer.nsecsElapsed()/1e6; timer.restart();
            require(pdf.characterCount()>5000,"text available after opening");
            const double extracted=timer.nsecsElapsed()/1e6;
            if(run) { openMs+=opened; textMs+=extracted; }
            auto prefix=[&] { auto text=pdf.selectionText(0,30); return text.remove('\r').remove('\n').remove(' '); };
            require(prefix().startsWith("Page0:"),"first page text");
            require(pdf.setPage(1),"navigate");
            require(prefix().startsWith("Page1:"),"selection after navigation");
            auto rects=pdf.selectionRects(0,5); require(!rects.isEmpty(),"selection geometry");
            require(pdf.textAt(1,rects.first().center()),"text hit after navigation");
            require(pdf.characterAt(rects.first().center())>=0,"character hit");
            require(pdf.wordRange(1).first>=0 && pdf.lineRange(1).first>=0,"word and line selection");
            const int maximum=std::numeric_limits<int>::max();
            require(pdf.selectionText(0,maximum)==pdf.selectionText(0,pdf.characterCount()-1),"oversized selection is clamped");
            require(pdf.selectionText(maximum,maximum).isEmpty() && pdf.selectionRects(maximum,maximum).isEmpty(),"out of range selection is empty");
            require(pdf.setPage(2),"navigate before lazy geometry");
            require(!pdf.selectionRects(0,5).isEmpty(),"geometry initializes text");
            require(pdf.setPage(0) && pdf.textAt(0,rects.first().center()),"hover initializes text");
            require(pdf.setPage(2) && pdf.characterAt(rects.first().center())>=0,"hit initializes text");
            require(pdf.setPage(1),"navigate before failed open");
            require(!pdf.open(directory.filePath("missing.pdf")),"failed open");
            require(prefix().startsWith("Page1:"),"failed open preserves document");
            PdfDocument other; pdf.swap(other);
            require(other.selectionText(0,30).contains("1"),"swap preserves text");
            require(other.applyOcr(1,{{"recognized",QRectF(10,10,40,10),0}}),"apply OCR");
            require(other.selectionText(0,maximum)=="recognized" && other.selectionText(maximum,maximum).isEmpty(),"OCR selection bounds");
            other.closeDocument(); require(other.characterCount()==0,"close clears text");
        }
        std::cout<<"dense PDF mean (5 warm runs): open_ms="<<openMs/5<<" first_text_ms="<<textMs/5<<'\n';
    } catch(const std::exception& error) { std::cerr<<error.what()<<'\n'; return 1; }
}


