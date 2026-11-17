#include "Window.h"
#include "DocumentDragVisuals.h"
#include <QTemporaryDir>
#include <QElapsedTimer>
#include <QThread>
#include <iostream>
#include <stdexcept>

struct WindowTransferTest {
    static PageView* view(Window& window) { return window.view; }
    static int count(const Window& window) { return int(window.documents.size()); }
    static QString path(const Window& window,int index) { return window.documents[index]->path; }
    static QString id(const Window& window,int index) { return window.documents[index]->id; }
    static bool dirty(Window& window,int index) { return window.documentPdf(index).dirty(); }
    static bool select(Window& window,int index) { return window.switchDocument(index); }
    static QJsonObject prepare(Window& window,const QVector<int>& indexes) {
        window.transferBusy=true;
        return window.prepareDocumentTransfer(indexes);
    }
    static bool receive(Window& window,const QJsonObject& request,int index) { return window.receiveDocumentTransfer(request,index); }
    static void complete(Window& window,const QJsonObject& request,bool accepted) { window.completeDocumentTransfer(request,accepted); }
    static QJsonArray marks(Window& window) { return window.documents[window.activeDocument]->personalMarks; }
    static bool highlight(Window& window) { return window.dropIndicator && window.dropIndicator->isVisible(); }
    static void result(Window& window,int index,int count) { window.showDocumentTransferResult(index,count); }
    static void dark(Window& window,bool enabled) { window.applyDarkMode(enabled); }
    static void feedback(Window& window) { window.updateDocumentDragFeedback(0,false); }
    static void clearFeedback(Window& window) { window.clearDocumentDragFeedback(); }
#ifdef Q_OS_WIN
    static QPoint nativePoint(Window& window,QWidget* widget,QPoint local) {
        const auto point=widget->mapTo(&window,local);
        POINT native{qRound(point.x()*window.devicePixelRatioF()),qRound(point.y()*window.devicePixelRatioF())};
        ClientToScreen(reinterpret_cast<HWND>(window.winId()),&native);
        return {native.x,native.y};
    }
    static void send(Window& window,HWND destination,const QVector<int>& indexes,QPoint point) { window.sendDocumentTransfer(destination,indexes,point); }
    static void nativeMove(Window& window,QPoint point) {
        POINT original{}; GetCursorPos(&original);
        const bool positioned=SetCursorPos(point.x(),point.y());
        POINT actual{}; GetCursorPos(&actual);
        const auto candidate=DocumentTransfer::targetAt(reinterpret_cast<HWND>(window.winId()),{actual.x,actual.y},true);
        if(!positioned || !candidate) {
            std::cerr<<"Native move: cursor="<<actual.x<<","<<actual.y<<" requested="<<point.x()<<","<<point.y()<<" moved="<<positioned<<" candidate="<<quintptr(candidate)<<"\n";
            for(auto hwnd=GetTopWindow(nullptr);hwnd;hwnd=GetWindow(hwnd,GW_HWNDNEXT)) {
                RECT rect{}; if(hwnd==reinterpret_cast<HWND>(window.winId()) || !IsWindowVisible(hwnd) || !GetWindowRect(hwnd,&rect) || !PtInRect(&rect,actual)) continue;
                wchar_t name[128]{}; GetClassNameW(hwnd,name,128);
                std::cerr<<"Hit class="<<QString::fromWCharArray(name).toStdString()<<" marker="<<bool(GetPropW(hwnd,DocumentTransfer::windowProperty()))<<"\n"; break;
            }
        }
        MSG message{}; message.hwnd=reinterpret_cast<HWND>(window.winId()); qintptr result=0;
        for(auto type:{WM_ENTERSIZEMOVE,WM_MOVING,WM_EXITSIZEMOVE}) { message.message=type; window.handleDocumentTransferEvent(&message,&result); }
        SetCursorPos(original.x,original.y);
    }
#endif
};

static void require(bool condition,const char* message) { if(!condition) throw std::runtime_error(message); }
static bool spinUntil(const std::function<bool()>& predicate,int timeout=10000) {
    QElapsedTimer elapsed; elapsed.start();
    while(!predicate() && elapsed.elapsed()<timeout) { QApplication::processEvents(); QThread::msleep(10); }
    return predicate();
}
static QJsonObject readJson(const QString& path) {
    QFile file(path); if(!file.open(QIODevice::ReadOnly)) return {};
    return QJsonDocument::fromJson(file.readAll()).object();
}
static void writeJson(const QString& path,const QJsonObject& value) {
    QSaveFile file(path); const auto bytes=QJsonDocument(value).toJson();
    require(file.open(QIODevice::WriteOnly) && file.write(bytes)==bytes.size() && file.commit(),"write test state");
}
static QString fixture(const QDir& root,const QString& name,int pages) {
    PdfDocument document; require(document.createSample(pages,"Transfer example"),"create fixture");
    const auto path=root.filePath(name); require(document.saveCopy(path),"save fixture"); return path;
}

static void visualTests(QApplication& app,const QDir& root,const QString& output={}) {
    Window window(root.filePath("visual-session")); window.resize(1000,650);
    window.openDocument(fixture(root,"First document.pdf",2)); window.openDocument(fixture(root,"Second document.pdf",2));
    window.show(); app.processEvents();
    if(QApplication::platformName()=="offscreen") {
        WindowTransferTest::feedback(window);
        auto badge=window.findChild<QWidget*>("documentDragPreview");
        require(badge && badge->isVisible() && badge->property("instruction").toString()==(currentUiLanguage()=="en" ? "Release to open a new window" : "Soltar para abrir otra ventana"),"tear-off instruction follows UI language");
        WindowTransferTest::clearFeedback(window); require(!badge->isVisible(),"clearing drag hides preview");
    }
    DocumentDragPreview preview(&window);
    for(const bool dark:{false,true}) {
        WindowTransferTest::dark(window,dark); app.processEvents();
        const auto prefix=dark ? "dark-" : "light-";
        preview.present("First document.pdf",QString::fromUtf8("Soltar para abrir otra ventana"),DocumentDragPreview::Separate,dark,{200,200});
        require(preview.isVisible() && preview.testAttribute(Qt::WA_TransparentForMouseEvents)
                && preview.windowFlags().testFlag(Qt::WindowDoesNotAcceptFocus),"tear-off preview is visible without intercepting input");
        if(!output.isEmpty()) require(preview.grab().save(output+"/"+prefix+"separate.png"),"render separate preview");
        preview.present("First document.pdf",QString::fromUtf8("Soltar para añadir aquí"),DocumentDragPreview::Merge,dark,{200,200});
        if(!output.isEmpty()) require(preview.grab().save(output+"/"+prefix+"merge.png"),"render merge preview");
        preview.hide();
        auto toolbar=window.findChild<QToolBar*>("documentToolbar"); auto tabs=window.findChild<QTabBar*>("documentTabs");
        DocumentDropIndicator marker(&window);
        const int x=tabs->mapTo(&window,tabs->tabRect(1).topLeft()).x()-toolbar->x();
        marker.present(toolbar->geometry(),x,1);
        require(marker.property("insertionIndex").toInt()==1 && marker.testAttribute(Qt::WA_TransparentForMouseEvents),"drop marker shows insertion position without intercepting input");
        if(!output.isEmpty()) require(window.grab(QRect(0,0,window.width(),150)).save(output+"/"+prefix+"insertion.png"),"render insertion marker");
        marker.hide(); WindowTransferTest::result(window,1,1);
        require(!window.findChild<QLabel*>("documentTransferNotice"),"completed transfer has no popup notice");
        require(WindowTransferTest::highlight(window),"completed transfer keeps tab highlight");
        if(!output.isEmpty()) require(window.grab(QRect(0,0,window.width(),150)).save(output+"/"+prefix+"added.png"),"render added confirmation");
    }
    DocumentTabBar tabs; tabs.addTab("First"); tabs.addTab("Second"); tabs.resize(400,30); tabs.show();
    bool outside=false; int finished=0,detached=0;
    tabs.dragMoved=[&](int,QPoint,bool value){ outside=value; };
    tabs.dragFinished=[&]{ ++finished; }; tabs.detachRequested=[&](int,QPoint){ ++detached; };
    const QPoint start=tabs.tabRect(0).center(),end(start.x(),100);
    auto press=[&]{ QMouseEvent event(QEvent::MouseButtonPress,start,tabs.mapToGlobal(start),Qt::LeftButton,Qt::LeftButton,Qt::NoModifier); QApplication::sendEvent(&tabs,&event); };
    auto move=[&]{ QMouseEvent event(QEvent::MouseMove,end,tabs.mapToGlobal(end),Qt::NoButton,Qt::LeftButton,Qt::NoModifier); QApplication::sendEvent(&tabs,&event); };
    auto release=[&]{ QMouseEvent event(QEvent::MouseButtonRelease,end,tabs.mapToGlobal(end),Qt::LeftButton,Qt::NoButton,Qt::NoModifier); QApplication::sendEvent(&tabs,&event); };
    press(); move(); require(outside && finished==0,"dragging outside reports preview before release");
    release(); require(finished==1 && detached==1,"release clears preview before tear-off");
    press(); move(); tabs.cancelDocumentDrag(); release(); require(detached==1,"cancelling drag prevents tear-off");
    require(spinUntil([&]{return !WindowTransferTest::highlight(window);},2000),"completion indication expires");
    std::cout<<"PASS: light/dark previews, click-through insertion marker, completion indication, drag lifecycle and cancellation.\n";
}

static void coreTests(QApplication& app,const QDir& root) {
    const auto first=fixture(root,QString::fromUtf8("Primer documento á.pdf"),4);
    const auto second=fixture(root,"second.pdf",2);
    const auto third=fixture(root,"third.pdf",3);
    Window source(root.filePath("source-session")),destination(root.filePath("destination-session"));
    source.show(); destination.show(); source.openDocument(first); destination.openDocument(second); app.processEvents();
    auto canvas=WindowTransferTest::view(source);
    require(canvas->pdf.note({100,500},"Unsaved source annotation"),"source annotation");
    require(canvas->pdf.setPage(2),"source page");
    canvas->mode=PageView::Single; canvas->fit=PageView::Manual; canvas->scale=1.75; canvas->rotation=1;
    canvas->refresh();
    require(!source.addPersonalBookmark("Transferred mark",2).isEmpty(),"source bookmark");
    source.openDocument(third); require(WindowTransferTest::select(source,0),"activate source");
    auto targetCanvas=WindowTransferTest::view(destination);
    require(targetCanvas->pdf.note({100,500},"Keep destination edits"),"destination annotation");
    const auto existingId=WindowTransferTest::id(destination,0);

    const auto request=WindowTransferTest::prepare(source,{0});
    require(!request.isEmpty() && WindowTransferTest::count(source)==2,"source retains documents while awaiting receipt");
    const auto bundle=SessionStore::read(QDir(request["directory"].toString()));
    require(!QJsonDocument(bundle).toJson().contains("password"),"credentials never written into transfer manifest");
    require(WindowTransferTest::receive(destination,request,0),"receive dirty document");
    require(WindowTransferTest::count(destination)==2 && WindowTransferTest::id(destination,1)==existingId,"insert without replacing destination");
    require(WindowTransferTest::dirty(destination,0) && WindowTransferTest::dirty(destination,1),"both unsaved versions retained");
    require(targetCanvas->pdf.pageIndex()==2 && targetCanvas->mode==PageView::Single && targetCanvas->rotation==1 && targetCanvas->scale==1.75,"page zoom view and rotation retained");
    require(targetCanvas->pdf.setPage(0) && targetCanvas->pdf.annotationContents(0)=="Unsaved source annotation","source annotation content retained");
    require(WindowTransferTest::marks(destination).size()==1,"personal bookmarks retained");
    require(WindowTransferTest::count(source)==2,"receiver does not prematurely close source");
    require(WindowTransferTest::receive(destination,request,0) && WindowTransferTest::count(destination)==2,"retry is idempotent");
    WindowTransferTest::complete(source,request,true);
    require(WindowTransferTest::count(source)==1 && source.isVisible(),"only the transferred tab closes");

    const auto clean=WindowTransferTest::prepare(source,{0});
    require(WindowTransferTest::receive(destination,clean,2),"receive clean document");
    require(!WindowTransferTest::dirty(destination,2) && !targetCanvas->pdf.diskChanged(third),"clean transfer preserves original disk fingerprint");
    WindowTransferTest::complete(source,clean,true);
    require(WindowTransferTest::count(source)==0 && !source.isVisible(),"empty source window closes");

    {
        Window left(root.filePath("same-file-left")),right(root.filePath("same-file-right"));
        left.openDocument(first); right.openDocument(first);
        require(WindowTransferTest::view(left)->pdf.note({100,500},"Left version")
                && WindowTransferTest::view(right)->pdf.note({100,500},"Right version"),"two independently edited copies");
        const auto sameFile=WindowTransferTest::prepare(left,{0});
        require(WindowTransferTest::receive(right,sameFile,1) && WindowTransferTest::count(right)==2,"same-path documents remain separate tabs");
        require(WindowTransferTest::view(right)->pdf.annotationContents(0)=="Left version","incoming same-path edits retained");
        require(WindowTransferTest::select(right,0) && WindowTransferTest::view(right)->pdf.annotationContents(0)=="Right version","existing same-path edits retained");
        WindowTransferTest::complete(left,sameFile,true);
    }

    Window failing(root.filePath("failing-source-session")); failing.show(); failing.openDocument(first); failing.openDocument(third);
    auto broken=WindowTransferTest::prepare(failing,{0,1}); require(!broken.isEmpty(),"prepare all documents");
    const QDir brokenRoot(broken["directory"].toString()); const auto brokenItems=SessionStore::read(brokenRoot)["documents"].toArray();
    require(QFile::remove(brokenRoot.filePath(brokenItems[1].toObject()["backup"].toString())),"remove second snapshot");
    require(!WindowTransferTest::receive(destination,broken,0) && WindowTransferTest::count(destination)==3,"failed batch leaves destination unchanged");
    WindowTransferTest::complete(failing,broken,false);
    require(WindowTransferTest::count(failing)==2 && failing.isEnabled(),"rejected batch retains source documents");

    // A successful PDF load is insufficient: the destination must persist it too.
    const auto blocked=root.filePath("blocked-session"); QFile block(blocked); require(block.open(QIODevice::WriteOnly),"create blocked session"); block.close();
    Window cannotPersist(blocked); cannotPersist.openDocument(second);
    const auto valid=WindowTransferTest::prepare(failing,{0,1});
    require(!WindowTransferTest::receive(cannotPersist,valid,0) && WindowTransferTest::count(cannotPersist)==1
            && WindowTransferTest::path(cannotPersist,0)==second,"persistence failure rolls back inserted documents");
    WindowTransferTest::complete(failing,valid,false);

    const auto encryptedPath=QFileInfo(QString::fromUtf8(__FILE__)).absolutePath()+"/data/recovery-encrypted.pdf";
    PdfDocument encrypted,restored,withoutPassword;
    require(encrypted.open(encryptedPath,"audit-test"),"open encrypted fixture");
    const auto encryptedSnapshot=root.filePath("encrypted-snapshot.pdf");
    require(encrypted.saveRecovery(encryptedSnapshot),"write encrypted snapshot");
    require(!withoutPassword.open(encryptedSnapshot),"snapshot retains password protection");
    require(DocumentTransfer::open(restored,encryptedSnapshot,DocumentTransfer::access(encrypted),false)
            && !restored.dirty() && !restored.diskChanged(encryptedPath),"encrypted transfer uses in-memory credentials and preserves disk state");
    std::cout<<"PASS: transfer state, dirty/clean documents, bookmarks, insertion, receipt, empty-window closing, retries and rollback.\n";
}

#ifdef Q_OS_WIN
static int receiver(QApplication& app,const QDir& root) {
    Window window(root.filePath("receiver-session")); window.setGeometry(80,80,800,650);
    window.openDocument(root.filePath("destination.pdf"));
    require(WindowTransferTest::view(window)->pdf.note({100,500},"Receiver changes"),"receiver annotation");
    window.show(); app.processEvents();
    const auto toolbar=window.findChild<QToolBar*>("documentToolbar");
    const auto header=WindowTransferTest::nativePoint(window,toolbar,toolbar->rect().center());
    const auto body=WindowTransferTest::nativePoint(window,WindowTransferTest::view(window),{200,400});
    writeJson(root.filePath("endpoint.json"),{{"hwnd",QString::number(quintptr(window.winId()))},{"x",header.x()},{"y",header.y()},{"bodyX",body.x()},{"bodyY",body.y()}});
    QTimer checkpoint;
    QObject::connect(&checkpoint,&QTimer::timeout,[&]{
        window.saveSession();
        const auto indicator=window.findChild<QWidget*>("documentDropIndicator");
        const auto notice=window.findChild<QWidget*>("documentTransferNotice");
        writeJson(root.filePath("feedback.json"),{{"visible",indicator && indicator->isVisible()},
                  {"preview",indicator && indicator->property("previewActive").toBool()},
                  {"completed",indicator && indicator->isVisible() && !indicator->property("previewActive").toBool()},{"notice",notice && notice->isVisible()},{"count",WindowTransferTest::count(window)}});
        if(QFileInfo::exists(root.filePath("stop"))) app.exit(0);
    }); checkpoint.start(100);
    return app.exec();
}

static void nativeTests(QApplication& app,const QDir& root) {
    const auto first=fixture(root,"source.pdf",4); const auto second=fixture(root,"remaining.pdf",2);
    const auto third=fixture(root,"third.pdf",3); fixture(root,"destination.pdf",1);
    Window source(root.filePath("native-source-session")); source.setGeometry(950,100,650,600);
    source.openDocument(first);
    auto canvas=WindowTransferTest::view(source);
    require(canvas->pdf.note({100,500},"Cross-process annotation") && canvas->pdf.setPage(2),"native source state");
    canvas->scale=1.5; canvas->fit=PageView::Manual;
    source.openDocument(second); source.show(); app.processEvents();
    QProcess child;
    child.start(QCoreApplication::applicationFilePath(),{"--receiver",root.absolutePath()});
    require(child.waitForStarted(5000),"start receiver process");
    // Ensure the child is reaped even if an assertion fails.
    struct Cleanup { QProcess& process; ~Cleanup(){ if(process.state()!=QProcess::NotRunning) { process.kill(); process.waitForFinished(5000); } } } cleanup{child};
    require(spinUntil([&]{return QFileInfo::exists(root.filePath("endpoint.json"));}),"receiver endpoint ready");
    const auto endpoint=readJson(root.filePath("endpoint.json"));
    const auto hwnd=reinterpret_cast<HWND>(quintptr(endpoint["hwnd"].toString().toULongLong()));
    const QPoint header(endpoint["x"].toInt(),endpoint["y"].toInt());
    // Keep the test's drop zone visible even when the test runner owns foreground.
    require(SetWindowPos(hwnd,HWND_TOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE),"expose receiver drop zone");
    require(DocumentTransfer::targetAt(reinterpret_cast<HWND>(source.winId()),header,false)==hwnd,"discover another GPDF window under drop point");
    require(DocumentTransfer::accepts(hwnd,header),"accept drop onto document toolbar");
    require(!DocumentTransfer::accepts(hwnd,{endpoint["bodyX"].toInt(),endpoint["bodyY"].toInt()}),"reject drop onto page body");
    auto previewVisible=[&]{const auto state=readJson(root.filePath("feedback.json")); return state["visible"].toBool() && state["preview"].toBool();};
    // Match the real drag's heartbeat: a busy receiver can miss an individual
    // short preview request while it is repainting or checkpointing its session.
    auto hover=[&]{return spinUntil([&]{return DocumentTransfer::preview(hwnd,header) && previewVisible();},3000);};
    require(hover(),"another process displays insertion preview");
    require(readJson(root.filePath("feedback.json"))["count"].toInt()==1,"preview does not move documents");
    DocumentTransfer::clearPreview(hwnd);
    require(spinUntil([&]{return !readJson(root.filePath("feedback.json"))["visible"].toBool();}),"leaving clears remote preview");
    require(hover(),"preview can reappear");
    require(spinUntil([&]{return !readJson(root.filePath("feedback.json"))["visible"].toBool();},2000),"abandoned preview expires without a release message");
    WindowTransferTest::send(source,hwnd,{0},header);
    require(source.findChild<QWidget*>("documentDragPreview")->isVisible(),"transfer shows opening progress until receipt");
    require(spinUntil([&]{return WindowTransferTest::count(source)==1 && source.isEnabled();}),"cross-process single-tab receipt");
    auto saved=SessionStore::read(QDir(root.filePath("receiver-session"))); auto items=saved["documents"].toArray();
    require(items.size()==2,"receiver retains original and incoming tab");
    bool readingState=false;
    for(const auto& value:items) { const auto item=value.toObject(); if(item["path"].toString()==first) readingState=item["page"].toInt()==2 && item["scale"].toDouble()==1.5 && !item["backup"].toString().isEmpty(); }
    require(readingState,"cross-process dirty state and reading position persisted before receipt");
    require(!source.findChild<QWidget*>("documentDragPreview")->isVisible(),"receipt clears sender progress");
    require(spinUntil([&]{const auto state=readJson(root.filePath("feedback.json")); return state["completed"].toBool() && !state["notice"].toBool();}),"receiver highlights insertion without a popup");

    source.openDocument(third); require(canvas->pdf.note({100,500},"Whole-window annotation"),"second native source annotation");
    WindowTransferTest::nativeMove(source,header);
    require(spinUntil([&]{return WindowTransferTest::count(source)==0 && !source.isVisible();}),"caption drag merges all tabs and closes source");
    saved=SessionStore::read(QDir(root.filePath("receiver-session"))); items=saved["documents"].toArray();
    require(items.size()==4,"whole-window merge retains all four documents");
    int dirty=0; for(const auto& item:items) if(!item.toObject()["backup"].toString().isEmpty()) ++dirty;
    require(dirty==3,"whole-window merge preserves three dirty documents and one clean document");
    QFile stop(root.filePath("stop")); require(stop.open(QIODevice::WriteOnly),"stop receiver"); stop.close();
    require(spinUntil([&]{return child.state()==QProcess::NotRunning;}),"receiver exits");
    require(child.exitCode()==0,"receiver process succeeded");
    std::cout<<"PASS: native discovery, toolbar hit testing, cross-process tab transfer, caption drag, durable receipt and source-window closing.\n";
}
#endif

int main(int argc,char** argv) {
    QApplication app(argc,argv); app.setStyle("Fusion"); app.setQuitOnLastWindowClosed(false);
    try {
        if(app.arguments().value(1)=="--receiver") {
#ifdef Q_OS_WIN
            const QDir root(app.arguments().value(2));
            QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,root.filePath("child-settings"));
            return receiver(app,root);
#endif
        }
        QTemporaryDir temporary; require(temporary.isValid(),"temporary test root"); const QDir root(temporary.path());
        QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,root.filePath("settings"));
        initializeUiLanguage(app.arguments().contains("--english") ? "en" : "es");
        if(app.arguments().contains("--visuals")) {
            const auto output=QFileInfo(app.arguments().value(app.arguments().indexOf("--visuals")+1)).absoluteFilePath();
            require(QDir().mkpath(output),"visual output directory"); visualTests(app,root,output);
        } else if(app.arguments().contains("--feedback")) visualTests(app,root);
        else if(app.arguments().contains("--native")) {
#ifdef Q_OS_WIN
            nativeTests(app,root);
#endif
        } else { coreTests(app,root); visualTests(app,root); }
        return 0;
    } catch(const std::exception& error) { std::cerr<<"FAIL: "<<error.what()<<"\n"; return 1; }
}
