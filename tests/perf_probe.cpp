#define STUDYPDF_NO_MAIN
#include "../src/main.cpp"
#include <QTemporaryDir>
#include <QElapsedTimer>
#include <iostream>
int main(int argc,char** argv) {
    QApplication app(argc,argv); app.setStyle("Fusion"); qInstallMessageHandler([](QtMsgType,const QMessageLogContext&,const QString& message){ std::cerr<<message.toStdString()<<std::endl; });
    if(argc<2) return 2;
    QTemporaryDir settings; QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,settings.path());
    QString path=QString::fromLocal8Bit(argv[1]);
    QFile original(path); if(!original.open(QIODevice::ReadOnly)) return 3; auto hash=QCryptographicHash::hash(original.readAll(),QCryptographicHash::Sha256); original.close();
    if(app.arguments().contains("--search-ui")) {
        Window window; window.show(); window.openDocument(path);
        auto input=window.findChild<QLineEdit*>("searchInput"); auto results=window.findChild<QListWidget*>("searchResults"); auto status=window.findChild<QLabel*>("searchStatus");
        auto panel=window.findChild<QAction*>("focusSearch"); panel->trigger();
        for(int run=0;run<3;++run) {
            input->setText("Marechal"); QElapsedTimer elapsed; elapsed.start(); window.findChild<QPushButton*>("searchButton")->click();
            QEventLoop loop; QTimer completion,deadline; completion.setInterval(5); deadline.setSingleShot(true);
            QObject::connect(&completion,&QTimer::timeout,&loop,[&]{ if(!status->text().contains("Buscando") && !status->text().contains("buscando")) loop.quit(); });
            QObject::connect(&deadline,&QTimer::timeout,&loop,&QEventLoop::quit);
            completion.start(); deadline.start(30000); loop.exec();
            if(status->text().contains("Buscando") || status->text().contains("buscando") || results->count()!=427) return 10;
            std::cout<<"UI_SEARCH run="<<run<<" ms="<<elapsed.nsecsElapsed()/1000000.0<<" matches="<<results->count()<<std::endl;
        }
        if(!original.open(QIODevice::ReadOnly) || hash!=QCryptographicHash::hash(original.readAll(),QCryptographicHash::Sha256)) return 8;
        std::cout<<"PASS original unchanged"<<std::endl; return 0;
    }
    if(app.arguments().contains("--search")) {
        PdfDocument document; if(!document.open(path)) return 9;
        for(const auto& query:{QString("Marechal"),QString("zzzxxy-no-match-zzzxxy")}) for(int run=0;run<3;++run) {
            QElapsedTimer elapsed; elapsed.start(); int matches=0;
            for(int page=0;page<document.pageCount();++page) matches+=document.searchPage(page,query).size();
            std::cout<<"SEARCH query="<<query.toStdString()<<" run="<<run<<" ms="<<elapsed.nsecsElapsed()/1000000.0<<" matches="<<matches<<" cache_bytes="<<document.searchCacheStorageBytes()<<std::endl;
        }
        if(!original.open(QIODevice::ReadOnly) || hash!=QCryptographicHash::hash(original.readAll(),QCryptographicHash::Sha256)) return 8;
        std::cout<<"PASS original unchanged"<<std::endl; return 0;
    }
    auto pump=[&](int duration){ QElapsedTimer span; span.start(); qint64 maxGap=0; while(span.elapsed()<duration) { QElapsedTimer tick; tick.start(); app.processEvents(); maxGap=std::max(maxGap,tick.nsecsElapsed()); QThread::msleep(1); } return maxGap/1000000.0; };
    for(bool panel:{false,true}) {
        Window window; window.show(); window.openDocument(path);
        PageView* view=nullptr; for(auto widget:window.findChildren<QWidget*>()) if(auto canvas=dynamic_cast<PageView*>(widget)) view=canvas; if(!view || !view->pdf.loaded()) return 4;
        int page=std::min(10,view->pdf.pageCount()-1); while(page<view->pdf.pageCount()-1 && view->pdf.pageText(page).trimmed().size()<100) ++page;
        view->activate(page); auto scroll=window.findChild<QScrollArea*>(); scroll->verticalScrollBar()->setValue(int(view->activeRect().top()));
        window.findChild<QAction*>("toggleAnnotations")->setChecked(panel); pump(1500);
        std::cout<<"DOCUMENT pages="<<view->pdf.pageCount()<<" page="<<page+1<<" panel="<<panel<<std::endl;
        for(int step=0;step<8;++step) {
            int base=0; while(base<view->pdf.characterCount()-80 && view->pdf.selectionRects(base,base+60).isEmpty()) base+=60;
            view->first=base; view->last=std::min(base+60,view->pdf.characterCount()-1);
            QElapsedTimer action; action.start(); window.findChild<QAction*>("highlightAction")->trigger(); double ms=action.nsecsElapsed()/1000000.0;
            double gap=pump(450); std::cout<<"ACTION step="<<step<<" ms="<<ms<<" event_max_ms="<<gap<<std::endl;
        }
        int count=view->pdf.annotationCount(); if(!view->pdf.undo() || view->pdf.annotationCount()!=count-1 || !view->pdf.redo() || view->pdf.annotationCount()!=count) return 5;
        auto out=settings.path()+"/saved.pdf"; if(!view->pdf.saveCopy(out)) return 6; PdfDocument reopen; if(!reopen.open(out) || !reopen.setPage(page) || reopen.annotationCount()!=count) return 7;
    }
    if(!original.open(QIODevice::ReadOnly) || hash!=QCryptographicHash::hash(original.readAll(),QCryptographicHash::Sha256)) return 8;
    std::cout<<"PASS original unchanged; undo redo and save roundtrip"<<std::endl;
}
