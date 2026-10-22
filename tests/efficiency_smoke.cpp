#include "PageView.h"
#include <QPdfWriter>
#include <QTemporaryDir>
#include <iostream>
#include <stdexcept>

static void require(bool ok,const char* message) {
    if(!ok) throw std::runtime_error(message);
}

int main(int argc,char** argv) {
    QApplication app(argc,argv);
    try {
        QTemporaryDir temporary;
        const auto path=temporary.filePath("mixed-pages.pdf");
        {
            QPdfWriter writer(path); writer.setResolution(72);
            QPainter painter(&writer);
            for(int page=0;page<257;++page) {
                if(page) {
                    writer.setPageSize(QPageSize(page%2 ? QPageSize::A5 : QPageSize::A4));
                    require(writer.newPage(),"append page");
                }
                painter.drawText(30,30,QString("Page %1").arg(page));
            }
        }
        PdfDocument pdf; require(pdf.open(path),"open mixed pages");
        PageView view(pdf); view.newDocument(); view.setViewportSize({850,900});
        auto linearHit=[&](QPointF point) {
            for(int i=0;i<view.pageRects.size();++i)
                if(!view.pageRects[i].isEmpty() && view.pageRects[i].contains(point)) return i;
            return -1;
        };
        auto verify=[&] {
            for(const auto& rect:std::as_const(view.pageRects)) {
                if(rect.isEmpty()) continue;
                for(auto point:{rect.topLeft(),rect.bottomRight(),rect.center(),
                                rect.topLeft()-QPointF(1,1),rect.bottomRight()+QPointF(1,1),
                                QPointF(-10,rect.center().y())})
                    require(view.pageAt(point)==linearHit(point),"page hit agrees with linear reference");
                for(double y:{rect.top()-15,rect.top(),rect.center().y(),rect.bottom(),rect.bottom()+10,rect.bottom()+15}) {
                    const QRectF area(0,y,view.width(),85);
                    const auto range=view.pageRange(area.top(),area.bottom());
                    for(int page=0;page<view.pageRects.size();++page)
                        if(view.pageRects[page].intersects(area))
                            require(page>=range.first && page<range.second,"visible range contains every intersecting page");
                    if(view.mode==PageView::Continuous) {
                        int closest=-1; double distance=std::numeric_limits<double>::max();
                        for(int i=0;i<view.pageRects.size();++i) {
                            const auto& r=view.pageRects[i];
                            const double d=std::max({r.top()-y,y-r.bottom(),0.0});
                            if(d<distance) { closest=i; distance=d; }
                        }
                        require(view.closestPage(y)==closest,"nearest page agrees at edges and gaps");
                    }
                }
            }
        };
        for(auto mode:{PageView::Continuous,PageView::Single,PageView::Double}) {
            view.setMode(mode);
            for(int rotation=0;rotation<4;++rotation) {
                view.setRotation(rotation);
                for(auto fit:{PageView::Manual,PageView::Width,PageView::Page,PageView::Automatic}) {
                    view.setFit(fit);
                    for(int page:{0,127,256}) {
                        require(pdf.setPage(page),"activate mixed page"); view.refreshNavigation();
                        const auto before=view.pageRects; const double scale=view.effectiveScale;
                        verify(); view.rebuild();
                        require(before==view.pageRects && qFuzzyCompare(scale,view.effectiveScale),"navigation matches full layout rebuild");
                    }
                }
            }
        }
        view.setMode(PageView::Continuous); view.setFit(PageView::Manual);
        QElapsedTimer timer; timer.start(); qint64 checksum=0;
        for(int run=0;run<100000;++run) checksum+=view.pageAt(view.pageRects[run%257].center());
        const auto indexedNs=timer.nsecsElapsed(); timer.restart(); qint64 reference=0;
        for(int run=0;run<100000;++run) reference+=linearHit(view.pageRects[run%257].center());
        require(checksum==reference,"benchmark results agree");
        std::cout<<"257 pages, 100000 hits: indexed_ms="<<indexedNs/1e6<<" linear_ms="<<timer.nsecsElapsed()/1e6<<'\n';
        require(!pdf.diskChanged(path),"original digest matches");
        require(pdf.note({40,40},"save digest check"),"create note");
        const auto saved=temporary.filePath("saved.pdf");
        require(pdf.saveCopy(saved) && !pdf.diskChanged(saved),"streamed save digest matches file");
        QFile changed(saved); require(changed.open(QIODevice::Append),"open saved file");
        changed.write("\n% external modification\n"); changed.close();
        require(pdf.diskChanged(saved),"external change detected");
        pdf.closeDocument(); view.newDocument(); view.rebuild();
        require(view.pageAt({0,0})==-1 && view.closestPage(0)==-1,"empty layout");
        std::cout<<"PASS navigation geometry and save digest\n";
    } catch(const std::exception& error) { std::cerr<<error.what()<<'\n'; return 1; }
}
