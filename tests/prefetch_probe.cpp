#include "Window.h"
#include <QTemporaryDir>
#include <windows.h>
#include <psapi.h>
#include <iostream>
struct PageViewRenderTest {
 static bool full(PageView& v,int p) { return v.cache.contains(p) && !v.stalePages.contains(p); }
 static bool preview(PageView& v,int p) { return v.previewCache.contains(p); }
};
int main(int argc,char** argv) {
 QApplication app(argc,argv); if(argc<2) return 2;
 QTemporaryDir settings; QSettings::setDefaultFormat(QSettings::IniFormat); QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,settings.path()); app.setApplicationName("GPDF-Prefetch-Probe"); Window window; window.resize(1100,950); window.show(); window.openDocument(QString::fromLocal8Bit(argv[1])); PageView* canvas=nullptr; for(auto widget:window.findChildren<QWidget*>()) if(auto v=dynamic_cast<PageView*>(widget)) canvas=v; if(!canvas) return 4; auto& view=*canvas; view.setMode(PageView::Single); view.setFit(PageView::Page);
 auto pump=[&](int ms){ QElapsedTimer t;t.start();while(t.elapsed()<ms){app.processEvents();QThread::msleep(1);} };
 FILETIME c,e,k0,u0,k1,u1; GetProcessTimes(GetCurrentProcess(),&c,&e,&k0,&u0);
 auto ticks=[](FILETIME t){return (quint64(t.dwHighDateTime)<<32)|t.dwLowDateTime;};
 double total=0,maximum=0; int hits=0,n=std::min(30,view.pdf.pageCount());
 for(int p=0;p<n;++p){view.pdf.setPage(p);view.refreshNavigation(); bool hit=PageViewRenderTest::preview(view,p); hits+=hit; QElapsedTimer t;t.start(); view.repaint(); while(!PageViewRenderTest::full(view,p) && t.elapsed()<10000) pump(1); if(!PageViewRenderTest::full(view,p)) return 3; double ms=t.nsecsElapsed()/1e6;total+=ms;maximum=std::max(maximum,ms);pump(200);}
 GetProcessTimes(GetCurrentProcess(),&c,&e,&k1,&u1); PROCESS_MEMORY_COUNTERS_EX m{};m.cb=sizeof(m);GetProcessMemoryInfo(GetCurrentProcess(),reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&m),sizeof(m));
 std::cout<<"pages="<<n<<" preview_hits="<<hits<<" full_mean_ms="<<total/n<<" full_max_ms="<<maximum<<" cpu_ms="<<(ticks(k1)-ticks(k0)+ticks(u1)-ticks(u0))/10000.0<<" working_MiB="<<m.WorkingSetSize/1048576.0<<" private_MiB="<<m.PrivateUsage/1048576.0<<" peak_MiB="<<m.PeakWorkingSetSize/1048576.0<<std::endl;
 view.shutdownWorkers();
}

