#include "PageView.h"
#include <QApplication>
#include <QTemporaryDir>
#include <QFile>
#include <iostream>
#include <stdexcept>

int main(int argc,char** argv) {
    QApplication app(argc,argv);
    try {
        auto require=[](bool ok,const char* message){ if(!ok) throw std::runtime_error(message); };
        QTemporaryDir dir;
        QByteArray data="%PDF-1.7\n";
        QVector<QByteArray> objects={
            "<< /Type /Catalog /Pages 2 0 R >>",
            "<< /Type /Pages /Kids [3 0 R 4 0 R] /Count 2 >>",
            "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 400 600] /Annots [5 0 R 6 0 R 7 0 R 8 0 R] >>",
            "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 400 600] >>",
            "<< /Type /Annot /Subtype /Link /Rect [20 500 180 530] /A << /S /URI /URI (https://example.com/test) >> >>",
            "<< /Type /Annot /Subtype /Link /Rect [20 440 180 470] /Dest [4 0 R /XYZ 30 350 null] >>",
            "<< /Type /Annot /Subtype /Link /Rect [20 380 180 410] /A << /S /URI /URI (file:///C:/test.exe) >> >>",
            "<< /Type /Annot /Subtype /Link /Rect [20 320 180 350] /A << /S /GoTo /D [4 0 R /Fit] >> >>"
        };
        QVector<int> offsets;
        for(int i=0;i<objects.size();++i) { offsets.append(data.size()); data+=QByteArray::number(i+1)+" 0 obj\n"+objects[i]+"\nendobj\n"; }
        int xref=data.size(); data+="xref\n0 9\n0000000000 65535 f \n";
        for(int offset:offsets) data+=QByteArray::number(offset).rightJustified(10,'0')+" 00000 n \n";
        data+="trailer\n<< /Size 9 /Root 1 0 R >>\nstartxref\n"+QByteArray::number(xref)+"\n%%EOF\n";
        QFile file(dir.filePath("links.pdf")); require(file.open(QIODevice::WriteOnly),"fixture open"); file.write(data); file.close();
        PdfDocument pdf; require(pdf.open(file.fileName()),"load links");
        require(pdf.linkAt(0,{50,515}).url==QUrl("https://example.com/test"),"external link");
        auto target=pdf.linkAt(0,{50,455});
        require(target.page==1 && target.hasX && target.hasY && target.position==QPointF(30,350),"XYZ destination");
        require(pdf.linkAt(0,{50,335}).page==1,"GoTo action");
        require(!pdf.linkAt(0,{50,395}).valid(),"file URL rejected");
        require(!pdf.linkAt(0,{300,200}).valid() && !pdf.linkAt(-1,{}).valid(),"miss and invalid page");
        // Queries on a non-active page must share the hover page without changing selection.
        require(pdf.setPage(1),"activate second page");
        for(int run=0;run<100;++run) {
            auto point=pdf.normalizedToPage(0,pdf.pageToNormalized(0,{50,515}));
            require(pdf.linkAt(0,point).valid(),"cached hover link");
            pdf.textAt(0,point);
            require(pdf.pageIndex()==1,"hover preserves active page");
        }
        require(pdf.setPage(0),"restore first page");
        PageView view(pdf); view.resize(800,900); view.newDocument(); view.rebuild();
        int activated=0; view.linkActivated=[&](const PdfDocument::LinkTarget& link){ require(!link.url.isEmpty(),"UI target"); ++activated; };
        auto send=[&](QEvent::Type type,QPointF point,Qt::MouseButton button,Qt::MouseButtons buttons){
            QMouseEvent event(type,point,point,button,buttons,Qt::NoModifier); QApplication::sendEvent(&view,&event);
        };
        for(int rotation=0;rotation<4;++rotation) {
            view.rotation=rotation; view.scale=rotation%2 ? 1.5 : .75; view.rebuild();
            auto point=view.devicePoint({50,515});
            send(QEvent::MouseMove,point,Qt::NoButton,Qt::NoButton);
            require(view.cursor().shape()==Qt::PointingHandCursor,"link hover with rotation/zoom");
            send(QEvent::MouseButtonPress,point,Qt::LeftButton,Qt::LeftButton);
            send(QEvent::MouseButtonRelease,point,Qt::LeftButton,Qt::NoButton);
            require(activated==rotation+1,"single click activation");
            send(QEvent::MouseButtonPress,point,Qt::LeftButton,Qt::LeftButton);
            send(QEvent::MouseMove,point+QPointF(50,0),Qt::NoButton,Qt::LeftButton);
            send(QEvent::MouseButtonRelease,point,Qt::LeftButton,Qt::NoButton);
            require(activated==rotation+1,"drag returning to link does not activate");
        }
        require(!view.activate(-1) && !view.activate(pdf.pageCount()),"invalid activation rejected");
        require(pdf.pageIndex()==0,"failed activation preserves current page");
        require(!pdf.dirty(),"link reads do not modify PDF");
        // A counted but unreadable destination must not activate the previous page.
        auto broken=data;
        broken.replace(objects[3],QByteArray("null").leftJustified(objects[3].size(),' '));
        QFile invalid(dir.filePath("invalid-page.pdf"));
        require(invalid.open(QIODevice::WriteOnly),"invalid fixture open"); invalid.write(broken); invalid.close();
        view.shutdownWorkers();
        require(pdf.open(invalid.fileName()) && pdf.pageCount()==2,"load unreadable second page");
        view.newDocument(); view.rebuild();
        require(!view.activate(1) && pdf.pageIndex()==0,"unreadable destination preserves current page");
        std::cout<<"PASS: link detection, destinations, URL filtering, rotated clicks and drag cancellation\n";
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
