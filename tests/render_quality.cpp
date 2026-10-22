#include "PageView.h"
#include "UiFont.h"
#include <QApplication>
#include <QDir>
#include <iostream>
#include <stdexcept>

int main(int argc,char** argv) {
    configureUiFontBackend(); QApplication app(argc,argv);
    try {
        auto require=[](bool ok,const char* message) { if(!ok) throw std::runtime_error(message); };
        require(argc>=2,"PDF fixture required");
        PdfDocument pdf; require(pdf.open(QString::fromLocal8Bit(argv[1])),"open quality fixture");
        QWidget host;
        PdfDocument reference; require(reference.open(QString::fromLocal8Bit(argv[1])),"open independent reference document");
        PageView view(pdf); view.setParent(&host); view.move(7,9);
        view.newDocument(); view.setMode(PageView::Single);
        view.setViewportSize({920,1100}); view.setZoom(1.17); view.show(); host.show();
        const double dpr=view.devicePixelRatioF();
        QString output=argc>2 ? QString::fromLocal8Bit(argv[2]) : QString();
        if(!output.isEmpty()) QDir().mkpath(output);
        for(int page=0;page<std::min(2,pdf.pageCount());++page) {
            require(pdf.setPage(page),"activate quality page");
            for(int turn=0;turn<4;++turn) {
                view.setRotation(turn);
                host.resize(view.size()+QSize(14,18));
                app.processEvents(); host.grab(); view.workers.waitForDone(); app.processEvents();
                auto capture=host.grab().toImage();
                auto logical=view.activeRect().translated(view.pos());
                QRect physical(QPoint(qRound(logical.left()*dpr),qRound(logical.top()*dpr)),
                               QSize(qRound(logical.width()*dpr),qRound(logical.height()*dpr)));
                QSize raster=physical.size(); if(turn%2) raster.transpose();
                auto expected=reference.renderPage(page,view.effectiveScale*dpr,raster);
                if(turn) expected=expected.transformed(QTransform().rotate(turn*90));
                auto actual=capture.copy(physical).convertToFormat(QImage::Format_RGB32);
                expected=expected.convertToFormat(QImage::Format_RGB32);
                require(actual==expected,"displayed page differs from exact physical-pixel PDF raster");
                if(!output.isEmpty() && page==0 && turn==0) {
                    actual.save(output+"/gpdf-physical.png");
                    auto previous=pdf.renderPage(page,view.effectiveScale);
                    previous=previous.scaled(actual.size(),Qt::IgnoreAspectRatio,Qt::SmoothTransformation);
                    previous.save(output+"/previous-scaling-simulation.png");
                }
                std::cout<<"PASS dpr="<<dpr<<" page="<<page<<" rotation="<<turn*90
                         <<" pixels="<<actual.width()<<"x"<<actual.height()<<" exact raster\n";
            }
        }
        return 0;
    } catch(const std::exception& error) { std::cerr<<"FAIL: "<<error.what()<<"\n"; return 1; }
}
