#include "PageView.h"
#include <QPdfWriter>
#include <QTemporaryDir>
#include <iostream>
#include <stdexcept>
struct PageViewRenderTest {
 static void verify(PageView& v,QApplication& app) {
  auto require=[](bool ok,const char* why){if(!ok) throw std::runtime_error(why);};
  auto settle=[&]{QElapsedTimer t;t.start();while(t.elapsed()<700){app.processEvents();QThread::msleep(1);}};
  settle();
  require(v.cache.contains(0),"visible page rendered");
  require(v.previewCache.contains(1) && !v.cache.contains(1),"next page has only a lightweight preview");
  require(v.pdf.setPage(3),"jump to middle");v.refreshNavigation();settle();
  require(v.previewCache.contains(2) && v.previewCache.contains(4),"both neighbors preloaded after a jump");
  require(!v.cache.contains(2) && !v.cache.contains(4),"neighbors do not occupy full raster cache");
  for(int turn=1;turn<4;++turn){
   v.setRotation(turn);require(v.previewCache.isEmpty(),"rotation clears obsolete previews");settle();
   auto expected=v.pdf.renderPage(4,1.0,v.sizes[4].scaled(QSizeF(512,512),Qt::KeepAspectRatio).toSize());
   expected=expected.transformed(QTransform().rotate(turn*90));
   require(v.previewCache.contains(4) && *v.previewCache.object(4)==expected,"neighbor preview matches rotated PDF raster");
  }
  for(int p=0;p<6;++p){v.pdf.setPage(p);v.refreshNavigation();app.processEvents();}
  settle();require(v.cache.contains(5),"rapid navigation finishes visible page");
  require(v.cache.totalCost()<=64*1024 && v.previewCache.totalCost()<=8*1024,"cache budgets respected");
  v.newDocument();require(v.previewCache.isEmpty() && v.cache.isEmpty(),"document reset clears previews");
 }
};
int main(int argc,char** argv){QApplication app(argc,argv);try{
 QTemporaryDir tmp;auto path=tmp.filePath("neighbors.pdf");
 {QPdfWriter writer(path);writer.setResolution(72);QPainter p(&writer);for(int i=0;i<6;++i){if(i)writer.newPage();p.drawText(30,50,QString("Neighbor %1").arg(i));}}
 PdfDocument pdf;if(!pdf.open(path))return 2;QWidget host;PageView view(pdf);view.setParent(&host);view.newDocument();view.setMode(PageView::Single);view.setViewportSize({850,900});view.setFit(PageView::Page);host.resize(870,920);host.show();view.show();
 PageViewRenderTest::verify(view,app);view.shutdownWorkers();std::cout<<"PASS neighbor previews, rotations, rapid navigation, cache budgets and reset\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
