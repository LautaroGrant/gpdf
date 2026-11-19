#include "DocumentTransfer.h"
#include <QJsonDocument>

QJsonObject DocumentTransfer::access(const PdfDocument& document) {
    std::lock_guard guard(PdfDocument::apiMutex());
    return {{"password",QString::fromLatin1(document.password_.toBase64())},
            {"digest",QString::fromLatin1(document.diskDigest().toBase64())}};
}

bool DocumentTransfer::open(PdfDocument& document,const QString& snapshot,const QJsonObject& access,bool dirty) {
    if(!document.open(snapshot,QByteArray::fromBase64(access["password"].toString().toLatin1()))) return false;
    std::lock_guard guard(PdfDocument::apiMutex());
    // The snapshot may differ from the original file, even for a clean document.
    document.diskDigest_=QByteArray::fromBase64(access["digest"].toString().toLatin1());
    document.originalBytesForDigest_.clear();
    if(dirty) document.markRecovered();
    return true;
}

#ifdef Q_OS_WIN
const wchar_t* DocumentTransfer::windowProperty() { return L"GPDF.DocumentTransfer.v1"; }
UINT DocumentTransfer::queryMessage() {
    static const auto message=RegisterWindowMessageW(L"GPDF.DocumentTransfer.CanAccept.v1");
    return message;
}
UINT DocumentTransfer::previewMessage() {
    static const auto message=RegisterWindowMessageW(L"GPDF.DocumentTransfer.Preview.v1"); return message;
}
UINT DocumentTransfer::clearPreviewMessage() {
    static const auto message=RegisterWindowMessageW(L"GPDF.DocumentTransfer.ClearPreview.v1"); return message;
}
bool DocumentTransfer::preview(HWND target,QPoint screenPosition) {
    DWORD_PTR accepted=0;
    return target && GetPropW(target,windowProperty())
        && SendMessageTimeoutW(target,previewMessage(),WPARAM(INT_PTR(screenPosition.x())),LPARAM(screenPosition.y()),SMTO_ABORTIFHUNG|SMTO_BLOCK,100,&accepted)
        && accepted==1;
}
void DocumentTransfer::clearPreview(HWND target) {
    if(target && GetPropW(target,windowProperty())) PostMessageW(target,clearPreviewMessage(),0,0);
}

HWND DocumentTransfer::targetAt(HWND source,QPoint screenPosition,bool ignoreSource) {
    const POINT point{screenPosition.x(),screenPosition.y()};
    if(!ignoreSource) {
        const auto target=GetAncestor(WindowFromPoint(point),GA_ROOT);
        // Prefer Windows' actual hit testing; only look through our input-transparent preview.
        if(!target || !(GetWindowLongPtrW(target,GWL_EXSTYLE)&WS_EX_TRANSPARENT))
            return target && target!=source && GetPropW(target,windowProperty()) ? target : nullptr;
    }
    // During a native caption drag, the moving window covers its destination.
    // Respect the z-order of the other windows instead of looking through them.
    for(auto target=GetTopWindow(nullptr);target;target=GetWindow(target,GW_HWNDNEXT)) {
        if(!IsWindowVisible(target) || IsIconic(target) || (GetWindowLongPtrW(target,GWL_EXSTYLE)&WS_EX_TRANSPARENT)) continue;
        if(target==source && ignoreSource) continue;
        RECT bounds{};
        if(!GetWindowRect(target,&bounds) || !PtInRect(&bounds,point)) continue;
        return target!=source && GetPropW(target,windowProperty()) ? target : nullptr;
    }
    return nullptr;
}

bool DocumentTransfer::accepts(HWND target,QPoint screenPosition) {
    if(!target || !GetPropW(target,windowProperty())) return false;
    DWORD_PTR accepted=0;
    return SendMessageTimeoutW(target,queryMessage(),WPARAM(INT_PTR(screenPosition.x())),LPARAM(screenPosition.y()),
                               SMTO_ABORTIFHUNG|SMTO_BLOCK,500,&accepted) && accepted==1;
}

bool DocumentTransfer::send(HWND target,HWND source,const QJsonObject& request) {
    auto bytes=QJsonDocument(request).toJson(QJsonDocument::Compact);
    if(bytes.size()>1024*1024) return false;
    COPYDATASTRUCT data{messageTag,DWORD(bytes.size()),bytes.data()};
    DWORD_PTR queued=0;
    return SendMessageTimeoutW(target,WM_COPYDATA,reinterpret_cast<WPARAM>(source),reinterpret_cast<LPARAM>(&data),
                               SMTO_ABORTIFHUNG|SMTO_BLOCK,2000,&queued) && queued==1;
}
#endif
