#include "Window.h"
#include "DocumentDragVisuals.h"
#include <QElapsedTimer>
#include <QSet>
#include <utility>

namespace {
QStringList transferIds(const QJsonArray& items) {
    QStringList ids;
    for(const auto& value:items) ids.append(value.toObject()["id"].toString());
    return ids;
}

void removeTransferFiles(const QDir& directory) {
    // Only files in this one bundle are removed; other windows have their own sessions.
    for(const auto& name:directory.entryList({"*.pdf","session.json","result.json"},QDir::Files))
        QFile::remove(directory.filePath(name));
    QDir().rmdir(directory.absolutePath());
}

bool writeTransferResult(const QJsonObject& request,bool accepted) {
    QSaveFile file(QDir(request["directory"].toString()).filePath("result.json"));
    const auto bytes=QJsonDocument(QJsonObject{{"token",request["token"]},{"accepted",accepted},{"ids",request["ids"]}}).toJson(QJsonDocument::Compact);
    return file.open(QIODevice::WriteOnly) && file.write(bytes)==bytes.size() && file.commit();
}
}

void Window::advertiseDocumentTransfer() {
#ifdef Q_OS_WIN
    if(QApplication::platformName()=="windows")
        SetPropW(reinterpret_cast<HWND>(winId()),DocumentTransfer::windowProperty(),reinterpret_cast<HANDLE>(1));
#endif
}

void Window::updateDocumentDragFeedback(int index,bool wholeWindow) {
    if(transferBusy || receivingTransfer || documents.empty()) return;
    if(!wholeWindow && (index<0 || index>=int(documents.size()))) return;
    dragWholeWindow=wholeWindow;
    dragDocumentId=wholeWindow ? QString() : documents[index]->id;
    if(!dragFeedbackTimer) {
        dragFeedbackTimer=new QTimer(this); dragFeedbackTimer->setInterval(40);
        connect(dragFeedbackTimer,&QTimer::timeout,this,[this]{ refreshDocumentDragFeedback(); });
    }
    dragFeedbackTimer->start(); refreshDocumentDragFeedback();
}

void Window::refreshDocumentDragFeedback() {
    QString title;
    if(dragWholeWindow) title=documents.size()==1 ? ocrLabel("1 documento","1 document") : QString(ocrLabel("%1 documentos","%1 documents")).arg(documents.size());
    else {
        for(const auto& document:documents) if(document->id==dragDocumentId) { title=QFileInfo(document->path).fileName(); break; }
        if(title.isEmpty()) { clearDocumentDragFeedback(); return; }
    }
    auto mode=dragWholeWindow ? DocumentDragPreview::Move : DocumentDragPreview::Separate;
    QString instruction=dragWholeWindow ? ocrLabel("Arrastrando ventana","Moving window") : ocrLabel("Soltar para abrir otra ventana","Release to open a new window");
    quintptr destination=0;
#ifdef Q_OS_WIN
    if(QApplication::platformName()=="windows") {
        POINT cursor{};
        if(GetCursorPos(&cursor)) {
            const QPoint position(cursor.x,cursor.y);
            const auto target=DocumentTransfer::targetAt(reinterpret_cast<HWND>(winId()),position,dragWholeWindow);
            if(target) {
                if(DocumentTransfer::preview(target,position)) {
                    destination=reinterpret_cast<quintptr>(target); mode=DocumentDragPreview::Merge;
                    instruction=dragWholeWindow && documents.size()>1 ? QString(ocrLabel("Soltar para reunir %1 documentos","Release to merge %1 documents")).arg(documents.size())
                                               : ocrLabel("Soltar para añadir aquí","Release to add here");
                } else { mode=DocumentDragPreview::Blocked; instruction=ocrLabel("Soltar en la barra de documentos","Drop onto the document bar"); }
            } else if(!dragWholeWindow && acceptsDocumentTransferAt(position)) {
                if(dragPreviewTarget) DocumentTransfer::clearPreview(reinterpret_cast<HWND>(dragPreviewTarget));
                dragPreviewTarget=0; if(dragPreview) dragPreview->hide(); return;
            }
        }
    }
    if(dragPreviewTarget && dragPreviewTarget!=destination) DocumentTransfer::clearPreview(reinterpret_cast<HWND>(dragPreviewTarget));
#endif
    dragPreviewTarget=destination;
    if(!dragPreview) dragPreview=new DocumentDragPreview(this);
    static_cast<DocumentDragPreview*>(dragPreview.data())->present(title,instruction,mode,view->darkMode,QCursor::pos());
}

void Window::clearDocumentDragFeedback() {
    if(dragFeedbackTimer) dragFeedbackTimer->stop();
#ifdef Q_OS_WIN
    if(dragPreviewTarget) DocumentTransfer::clearPreview(reinterpret_cast<HWND>(dragPreviewTarget));
#endif
    dragPreviewTarget=0; dragDocumentId.clear(); dragWholeWindow=false;
    if(dragPreview) dragPreview->hide();
}

void Window::showDocumentTransferProgress(const QString& message) {
    clearDocumentDragFeedback();
    if(!dragPreview) dragPreview=new DocumentDragPreview(this);
    static_cast<DocumentDragPreview*>(dragPreview.data())->present(QStringLiteral("GPDF"),message,DocumentDragPreview::Working,view->darkMode,QCursor::pos());
}

void Window::showDocumentTransferResult(int insertion,int count) {
    if(layout()) layout()->activate();
    if(!dropIndicator) dropIndicator=new DocumentDropIndicator(this);
    const auto bar=documentToolbar->geometry();
    QRect inserted;
    for(int i=insertion;i<insertion+count && i<documentTabs->count();++i) inserted=inserted.united(documentTabs->tabRect(i));
    inserted.translate(documentTabs->mapTo(this,QPoint())-bar.topLeft());
    static_cast<DocumentDropIndicator*>(dropIndicator.data())->complete(bar,inserted);
    if(!dropIndicatorTimer) {
        dropIndicatorTimer=new QTimer(this); dropIndicatorTimer->setSingleShot(true);
        connect(dropIndicatorTimer,&QTimer::timeout,this,[this]{ if(dropIndicator) dropIndicator->hide(); });
    }
    dropIndicatorTimer->start(1200);
}

void Window::dropDocumentTab(int index,QPoint globalPosition) {
    if(transferBusy || receivingTransfer) return;
#ifdef Q_OS_WIN
    if(QApplication::platformName()=="windows") {
        POINT cursor{};
        if(GetCursorPos(&cursor)) {
            const QPoint position(cursor.x,cursor.y);
            const auto destination=DocumentTransfer::targetAt(reinterpret_cast<HWND>(winId()),position,false);
            if(destination) {
                if(DocumentTransfer::accepts(destination,position)) sendDocumentTransfer(destination,{index},position);
                return;
            }
            // Dropping onto the current window's other toolbar cancels the tear-off.
            if(acceptsDocumentTransferAt(position)) return;
        }
    }
#endif
    Q_UNUSED(globalPosition);
    detachDocument(index);
}

QJsonObject Window::prepareDocumentTransfer(const QVector<int>& indexes) {
    if(indexes.isEmpty() || !view->finishText() || !saveSession()) return {};
    const auto saved=SessionStore::read(QDir(sessionDirectory()));
    const auto items=saved["documents"].toArray();
    const auto token=QUuid::createUuid().toString(QUuid::WithoutBraces);
    QDir directory(QDir(sessionDirectory()).filePath("transfers/"+token));
    if(!QDir().mkpath(directory.absolutePath())) return {};
    QJsonArray selected; QJsonObject access; QSet<int> seen;
    for(const auto index:indexes) {
        if(index<0 || index>=int(documents.size()) || index>=items.size() || seen.contains(index)) {
            removeTransferFiles(directory); return {};
        }
        seen.insert(index);
        auto item=items[index].toObject(); auto& engine=documentPdf(index); const auto& state=*documents[index];
        const auto snapshot=state.id+".pdf";
        if(!engine.saveRecovery(directory.filePath(snapshot))) { removeTransferFiles(directory); return {}; }
        // Snapshot clean documents too: the original file may have changed or disappeared.
        item["backup"]=snapshot; item["dirty"]=engine.dirty();
        QJsonArray history; for(const auto page:state.navigation) history.append(page);
        item["navigation"]=history; item["navigationIndex"]=state.navigationIndex;
        selected.append(item); access[state.id]=DocumentTransfer::access(engine);
    }
    QString active=selected.first().toObject()["id"].toString();
    if(activeDocument>=0 && seen.contains(activeDocument)) active=documents[activeDocument]->id;
    if(!SessionStore::write(directory,{{"version",1},{"token",token},{"activeId",active},{"documents",selected}})) {
        removeTransferFiles(directory); return {};
    }
    return {{"version",1},{"token",token},{"directory",directory.absolutePath()},
            {"ids",QJsonArray::fromStringList(transferIds(selected))},{"access",access}};
}

bool Window::receiveDocumentTransfer(const QJsonObject& request,int insertionIndex) {
    const auto token=request["token"].toString();
    if(request["version"].toInt()!=1 || QUuid(token).isNull() || transferBusy || sessionClosed) return false;
    if(receivedTransferTokens.contains(token)) return true;
    const QDir directory(request["directory"].toString());
    if(directory.dirName()!=token || directory.canonicalPath().isEmpty()) return false;
    const auto manifest=SessionStore::read(directory); const auto items=manifest["documents"].toArray();
    if(manifest["token"]!=request["token"] || items.isEmpty() || items.size()>1000
       || QJsonArray::fromStringList(transferIds(items))!=request["ids"].toArray()) return false;
    if(!view->finishText()) return false;

    // Stage every PDF before touching the destination. A corrupt/missing snapshot
    // rejects the whole transfer, so neither window ends up with half a document set.
    std::vector<std::unique_ptr<DocumentState>> incoming;
    QSet<QString> ids; for(const auto& state:documents) ids.insert(state->id);
    const auto credentials=request["access"].toObject();
    for(const auto& value:items) {
        const auto item=value.toObject(); const auto originalId=item["id"].toString();
        const auto snapshot=item["backup"].toString();
        if(QUuid(originalId).isNull() || snapshot!=originalId+".pdf" || !credentials.contains(originalId)) return false;
        auto state=std::make_unique<DocumentState>();
        if(!DocumentTransfer::open(state->stored,directory.filePath(snapshot),credentials[originalId].toObject(),item["dirty"].toBool())) return false;
        state->id=ids.contains(originalId) ? QUuid::createUuid().toString(QUuid::WithoutBraces) : originalId;
        ids.insert(state->id);
        state->path=item["path"].toString(); state->personalMarks=item["marks"].toArray();
        state->mode=PageView::Mode(std::clamp(item["mode"].toInt(),0,2));
        state->fit=PageView::Fit(std::clamp(item["fit"].toInt(),0,3));
        state->scale=std::clamp(item["scale"].toDouble(1.25),0.5,3.0);
        state->rotation=std::clamp(item["rotation"].toInt(),0,3);
        state->vertical=std::max(0,item["vertical"].toInt()); state->horizontal=std::max(0,item["horizontal"].toInt());
        state->scope=std::clamp(item["scope"].toInt(),0,1); state->type=std::clamp(item["type"].toInt(),0,annotationTypeFilter->count()-1);
        state->query=item["query"].toString(); state->searchVisible=item["searchVisible"].toBool(); state->annotationsVisible=item["annotationsVisible"].toBool();
        for(const auto& page:item["navigation"].toArray()) if(page.toInt(-1)>=0 && page.toInt()<state->stored.pageCount()) state->navigation.append(page.toInt());
        state->navigationIndex=std::clamp(item["navigationIndex"].toInt(-1),-1,int(state->navigation.size())-1);
        if(!state->stored.setPage(std::clamp(item["page"].toInt(),0,state->stored.pageCount()-1))) return false;
        incoming.push_back(std::move(state));
    }

    const int previousActive=activeDocument;
    const int count=int(incoming.size()); insertionIndex=std::clamp(insertionIndex,0,int(documents.size()));
    storeDocument(); activeDocument=-1;
    for(int i=0;i<count;++i) documents.insert(documents.begin()+insertionIndex+i,std::move(incoming[i]));
    const auto sourceIds=transferIds(items);
    const int selected=std::max(0,int(sourceIds.indexOf(manifest["activeId"].toString())));
    const bool committed=switchDocument(insertionIndex+selected) && saveSession();
    if(!committed) {
        storeDocument(); activeDocument=-1;
        documents.erase(documents.begin()+insertionIndex,documents.begin()+insertionIndex+count);
        if(previousActive>=0) switchDocument(previousActive);
        else {
            source.clear(); resetSearch(); view->newDocument(); refresh(); updateDocumentTabs();
        }
        return false;
    }
    receivedTransferTokens.insert(token);
    showDocumentTransferResult(insertionIndex,count);
    return true;
}

void Window::resumeAfterDocumentTransfer() {
    clearDocumentDragFeedback();
    transferBusy=false; setEnabled(true);
    if(!deferredExternalOpens.isEmpty()) {
        const auto paths=std::exchange(deferredExternalOpens,QStringList());
        QTimer::singleShot(0,this,[this,paths]{ openExternalDocuments(paths); });
    }
}

void Window::completeDocumentTransfer(const QJsonObject& request,bool accepted) {
    if(accepted) {
        // Remove by identity, since closing each document changes the tab indexes.
        for(const auto& value:request["ids"].toArray()) {
            const auto id=value.toString();
            for(int i=0;i<int(documents.size());++i) if(documents[i]->id==id) {
                if(switchDocument(i)) closeDocument(true);
                break;
            }
        }
    }
    const bool pendingOpen=!deferredExternalOpens.isEmpty();
    resumeAfterDocumentTransfer();
    if(accepted && documents.empty() && !pendingOpen) close();
}

#ifdef Q_OS_WIN
void Window::showDocumentDropIndicator(QPoint nativePosition) {
    const int insertion=documentTransferInsertionIndex(nativePosition);
    auto toolbar=documentToolbar->isVisible() ? documentToolbar : mainToolbar;
    const auto bar=toolbar->geometry();
    int x=8;
    if(documentToolbar->isVisible() && documentTabs->count()) {
        const int index=qMin(insertion,documentTabs->count()-1);
        const auto tab=documentTabs->tabRect(index);
        x=documentTabs->mapTo(this,QPoint(insertion>=documentTabs->count() ? tab.right()+1 : tab.left(),0)).x()-bar.left();
    }
    if(!dropIndicator) dropIndicator=new DocumentDropIndicator(this);
    static_cast<DocumentDropIndicator*>(dropIndicator.data())->present(bar,x,insertion);

    if(!dropIndicatorTimer) {
        dropIndicatorTimer=new QTimer(this); dropIndicatorTimer->setSingleShot(true);
        connect(dropIndicatorTimer,&QTimer::timeout,this,[this]{ if(dropIndicator) dropIndicator->hide(); });
    }
    dropIndicatorTimer->start(300);
}

void Window::clearDocumentDropIndicator() {
    if(dropIndicator && dropIndicator->property("previewActive").toBool()) {
        dropIndicator->hide(); if(dropIndicatorTimer) dropIndicatorTimer->stop();
    }
}

bool Window::acceptsDocumentTransferAt(QPoint nativePosition) const {
    if(transferBusy || receivingTransfer || sessionClosed || !isEnabled() || !isVisible() || isMinimized()
       || QApplication::activeModalWidget()) return false;
    const auto hwnd=reinterpret_cast<HWND>(winId()); POINT point{nativePosition.x(),nativePosition.y()}; RECT bounds{},client{};
    if(!GetWindowRect(hwnd,&bounds) || !PtInRect(&bounds,point) || !ScreenToClient(hwnd,&point) || !GetClientRect(hwnd,&client)) return false;
    // Native title and menu bars are above the Qt client area.
    if(point.y<0) return point.x>=0 && point.x<client.right;
    const QPoint local(qRound(point.x/devicePixelRatioF()),qRound(point.y/devicePixelRatioF()));
    return (documentToolbar->isVisible() && documentToolbar->geometry().contains(local))
        || (mainToolbar->isVisible() && mainToolbar->geometry().contains(local))
        || (menuBar()->isVisible() && menuBar()->geometry().contains(local));
}

int Window::documentTransferInsertionIndex(QPoint nativePosition) const {
    POINT point{nativePosition.x(),nativePosition.y()};
    if(!documentToolbar->isVisible() || !ScreenToClient(reinterpret_cast<HWND>(winId()),&point)) return int(documents.size());
    const QPoint local(qRound(point.x/devicePixelRatioF()),qRound(point.y/devicePixelRatioF()));
    const auto position=documentTabs->mapFrom(this,local);
    const int index=documentTabs->tabAt(position);
    return index<0 ? int(documents.size()) : index+(position.x()>documentTabs->tabRect(index).center().x() ? 1 : 0);
}

bool Window::handleDocumentTransferEvent(void* message,qintptr* result) {
    const auto msg=static_cast<MSG*>(message);
    if(msg->message==DocumentTransfer::queryMessage() || msg->message==DocumentTransfer::previewMessage()) {
        const QPoint position(int(INT_PTR(msg->wParam)),int(msg->lParam));
        const bool accepted=acceptsDocumentTransferAt(position);
        if(msg->message==DocumentTransfer::previewMessage()) {
            if(accepted) showDocumentDropIndicator(position); else clearDocumentDropIndicator();
        }
        *result=accepted ? 1 : 0;
        return true;
    }
    if(msg->message==DocumentTransfer::clearPreviewMessage()) { clearDocumentDropIndicator(); *result=0; return true; }
    if(msg->message==WM_COPYDATA) {
        const auto data=reinterpret_cast<const COPYDATASTRUCT*>(msg->lParam);
        if(!data || data->dwData!=DocumentTransfer::messageTag) return false;
        *result=0;
        const auto sender=reinterpret_cast<HWND>(msg->wParam);
        if(!sender || sender==reinterpret_cast<HWND>(winId()) || !GetPropW(sender,DocumentTransfer::windowProperty())
           || !data->lpData || !data->cbData || data->cbData>1024*1024) return true;
        const auto request=QJsonDocument::fromJson(QByteArray(static_cast<const char*>(data->lpData),int(data->cbData))).object();
        const QPoint position(request["x"].toInt(),request["y"].toInt());
        if(!acceptsDocumentTransferAt(position)) return true;
        const auto token=request["token"].toString(); const QDir directory(request["directory"].toString());
        if(request["version"].toInt()!=1 || QUuid(token).isNull() || directory.dirName()!=token || !directory.exists()) return true;
        const int insertion=documentTransferInsertionIndex(position);
        receivingTransfer=true;
        QTimer::singleShot(0,this,[this,request,insertion]{
            const bool accepted=receiveDocumentTransfer(request,insertion);
            writeTransferResult(request,accepted);
            receivingTransfer=false;
            if(accepted) presentForExternalOpen();
            resumeAfterDocumentTransfer();
        });
        *result=1; return true;
    }
    if(msg->message==WM_ENTERSIZEMOVE) nativeDocumentMove=false;
    if(msg->message==WM_MOVING) { nativeDocumentMove=true; updateDocumentDragFeedback(-1,true); }
    if(msg->message==WM_CANCELMODE) { nativeDocumentMove=false; clearDocumentDragFeedback(); }
    if(msg->message==WM_EXITSIZEMOVE) clearDocumentDragFeedback();
    if(msg->message==WM_EXITSIZEMOVE && std::exchange(nativeDocumentMove,false) && !transferBusy && !receivingTransfer && !documents.empty()) {
        POINT cursor{};
        if(GetCursorPos(&cursor)) {
            const QPoint position(cursor.x,cursor.y);
            const auto destination=DocumentTransfer::targetAt(reinterpret_cast<HWND>(winId()),position,true);
            if(destination && DocumentTransfer::accepts(destination,position)) {
                QTimer::singleShot(0,this,[this,destination,position]{
                    QVector<int> indexes; for(int i=0;i<int(documents.size());++i) indexes.append(i);
                    sendDocumentTransfer(destination,indexes,position);
                });
            }
        }
    }
    return false;
}

void Window::sendDocumentTransfer(HWND destination,const QVector<int>& indexes,QPoint nativePosition) {
    if(transferBusy || receivingTransfer || !DocumentTransfer::accepts(destination,nativePosition)) return;
    transferBusy=true; stopAutoScroll(); setEnabled(false);
    showDocumentTransferProgress(ocrLabel("Añadiendo documentos…","Adding documents…"));
    auto request=prepareDocumentTransfer(indexes);
    if(request.isEmpty()) {
        resumeAfterDocumentTransfer();
        importantWarning(ocrLabel("No se pudo preparar la transferencia. Los documentos siguen abiertos aquí.","The transfer could not be prepared. The documents are still open here."));
        return;
    }
    request["x"]=nativePosition.x(); request["y"]=nativePosition.y();
    if(!DocumentTransfer::send(destination,reinterpret_cast<HWND>(winId()),request)) {
        removeTransferFiles(QDir(request["directory"].toString())); resumeAfterDocumentTransfer(); return;
    }
    // Drop credentials as soon as Windows has copied the IPC packet.
    request.remove("access");
    auto timer=new QTimer(this); timer->setInterval(100);
    auto elapsed=std::make_shared<QElapsedTimer>(); elapsed->start();
    connect(timer,&QTimer::timeout,this,[this,timer,elapsed,request]{
        QFile result(QDir(request["directory"].toString()).filePath("result.json"));
        if(result.open(QIODevice::ReadOnly)) {
            const auto receipt=QJsonDocument::fromJson(result.readAll()).object(); result.close();
            if(receipt["token"]==request["token"] && receipt["ids"]==request["ids"]) {
                timer->stop(); timer->deleteLater();
                const bool accepted=receipt["accepted"].toBool();
                completeDocumentTransfer(request,accepted);
                removeTransferFiles(QDir(request["directory"].toString()));
                if(!accepted) importantWarning(ocrLabel("No se pudieron reunir los documentos. Siguen abiertos en la ventana de origen.","The documents could not be merged. They are still open in the source window."));
                return;
            }
        }
        if(elapsed->elapsed()>=20000) {
            timer->stop(); timer->deleteLater(); resumeAfterDocumentTransfer();
            importantWarning(ocrLabel("No se pudo confirmar la recepción. Los documentos siguen abiertos aquí.","Receipt could not be confirmed. The documents are still open here."));
        }
    });
    timer->start();
}
#endif
