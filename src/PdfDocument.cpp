#ifdef GPDF_PERF_PROBE
#include <QElapsedTimer>
#include <QDebug>
#endif
#include "PdfDocument.h"
#include <QFile>
#include <QSaveFile>
#include <QPainterPath>
#include <QTextLayout>
#include <QFont>
#include <QFontDatabase>
#include <QGuiApplication>
#include <algorithm>
#include <cmath>
#include <fpdf_annot.h>
#include <fpdf_edit.h>
#include <fpdf_save.h>
#include <fpdf_doc.h>
#include <QSet>
#include <QLineF>
#include <functional>
#include <memory>
#include <type_traits>

namespace {
int libraryUsers=0;
quint64 nextRevision=0;
std::recursive_mutex pdfiumMutex;
bool setContents(FPDF_ANNOTATION annot, const QString& value) {
    return FPDFAnnot_SetStringValue(annot, "Contents", reinterpret_cast<FPDF_WIDESTRING>(value.utf16()));
}
QString styleValue(const PdfDocument::TextStyle& style) { std::lock_guard apiGuard(pdfiumMutex);
    return style.family+"\n"+QString::number(style.size)+"\n"+style.color.name()+"\n"+QString::number(style.width)+"\n"+QString::number(style.height);
}
struct Writer : FPDF_FILEWRITE {
    QSaveFile* file;
    explicit Writer(QSaveFile& output) : file(&output) {
        version = 1;
        WriteBlock = [](FPDF_FILEWRITE* self, const void* data, unsigned long size) -> int {
            return static_cast<Writer*>(self)->file->write(static_cast<const char*>(data), size) == qint64(size);
        };
    }
};
}
PdfDocument::PdfDocument() { std::lock_guard apiGuard(pdfiumMutex);
    // The Windows offscreen plugin does not enumerate system fonts on its own.
    if (QGuiApplication::platformName()=="offscreen") {
        static const bool fontsLoaded=[] {
            const QString fonts=qEnvironmentVariable("WINDIR","C:/Windows")+"/Fonts/";
            QFontDatabase::addApplicationFont(fonts+"arial.ttf");
            QFontDatabase::addApplicationFont(fonts+"segoeui.ttf");
            return true;
        }();
        Q_UNUSED(fontsLoaded);
    }
    if(libraryUsers++==0) FPDF_InitLibrary();
}
PdfDocument::~PdfDocument() { std::lock_guard apiGuard(pdfiumMutex); close(); if(--libraryUsers==0) FPDF_DestroyLibrary(); }
void PdfDocument::swap(PdfDocument& other) { std::lock_guard apiGuard(pdfiumMutex);
    std::swap(searchIndex_,other.searchIndex_);
    std::swap(revision_,other.revision_);
    std::swap(history_,other.history_);
    std::swap(historyPosition_,other.historyPosition_);
    std::swap(savedHistoryId_,other.savedHistoryId_);
    std::swap(annotationsCached_,other.annotationsCached_);
    std::swap(annotationEntries_,other.annotationEntries_);
    std::swap(annotationPages_,other.annotationPages_);
    std::swap(doc_,other.doc_);
    std::swap(page_,other.page_);
    std::swap(text_,other.text_);
    std::swap(hoverPage_,other.hoverPage_);
    std::swap(hoverText_,other.hoverText_);
    std::swap(hoverIndex_,other.hoverIndex_);
    std::swap(hoverRegions_,other.hoverRegions_);
    std::swap(bytes_,other.bytes_);
    std::swap(password_,other.password_);
    std::swap(index_,other.index_);
    std::swap(dirty_,other.dirty_);
    std::swap(error_,other.error_);
    std::swap(hiddenTextPreview,other.hiddenTextPreview);
}
std::recursive_mutex& PdfDocument::apiMutex() { return pdfiumMutex; }
quint64 PdfDocument::revision() const { std::lock_guard apiGuard(pdfiumMutex); return revision_; }
void PdfDocument::closeDocument() { std::lock_guard apiGuard(pdfiumMutex); revision_=++nextRevision; close(); history_.clear(); historyPosition_=-1; savedHistoryId_=0; password_.clear(); index_=0; hiddenTextPreview=-1; }
QByteArray PdfDocument::snapshot() const { std::lock_guard apiGuard(pdfiumMutex);
    struct MemoryWriter : FPDF_FILEWRITE {
        QByteArray bytes;
        MemoryWriter() { version=1; WriteBlock=[](FPDF_FILEWRITE* writer,const void* data,unsigned long size)->int { static_cast<MemoryWriter*>(writer)->bytes.append(static_cast<const char*>(data),qsizetype(size)); return 1; }; }
    } writer;
    if(!doc_ || !FPDF_SaveAsCopy(doc_,&writer,FPDF_NO_INCREMENTAL)) return {};
    return writer.bytes;
}
PdfDocument::HistoryEntry PdfDocument::captureHistory() const {
    struct IncrementalWriter : FPDF_FILEWRITE {
        const QByteArray& base; QByteArray tail; qsizetype consumed=0; bool matching=true;
        explicit IncrementalWriter(const QByteArray& original):base(original) {
            version=1; WriteBlock=[](FPDF_FILEWRITE* raw,const void* block,unsigned long amount)->int {
                auto& self=*static_cast<IncrementalWriter*>(raw); auto data=static_cast<const char*>(block); qsizetype size=amount;
                if(self.matching) {
                    qsizetype prefix=std::min(size,self.base.size()-self.consumed);
                    if(prefix && memcmp(data,self.base.constData()+self.consumed,size_t(prefix))!=0) {
                        self.tail=self.base.left(self.consumed); self.matching=false;
                    } else { self.consumed+=prefix; data+=prefix; size-=prefix; }
                }
                self.tail.append(data,size); return 1;
            };
        }
    } writer(bytes_);
    if(!doc_ || !FPDF_SaveAsCopy(doc_,&writer,bytes_.isEmpty() ? FPDF_NO_INCREMENTAL : FPDF_INCREMENTAL)) return {};
    return {writer.matching ? bytes_.left(writer.consumed) : QByteArray(),writer.tail,revision_};
}
qint64 PdfDocument::searchCacheStorageBytes() const { std::lock_guard guard(pdfiumMutex); return searchIndex_->totalCost(); }
qint64 PdfDocument::historyStorageBytes() const { std::lock_guard guard(pdfiumMutex);
    QSet<const char*> allocations; qint64 total=0;
    for(const auto& entry:history_) for(const auto& part:{entry.base,entry.tail}) if(!part.isEmpty() && !allocations.contains(part.constData())) { allocations.insert(part.constData()); total+=part.size(); }
    return total;
}
void PdfDocument::resetHistory() { std::lock_guard apiGuard(pdfiumMutex);
    history_.clear(); historyPosition_=-1; revision_=++nextRevision;
    auto bytes=!dirty_ && !bytes_.isEmpty() ? bytes_ : snapshot();
    if(!bytes.isEmpty()) { history_.append({bytes,{},revision_}); historyPosition_=0; }
    savedHistoryId_=dirty_ ? 0 : revision_;
}
void PdfDocument::recordHistory() { std::lock_guard apiGuard(pdfiumMutex);
#ifdef GPDF_PERF_PROBE
    QElapsedTimer profile; profile.start();
#endif
    revision_=++nextRevision; annotationsCached_=false; annotationPages_.remove(index_); searchIndex_->remove(index_); hoverRegions_.remove(index_);
    auto entry=captureHistory(); if(entry.base.isEmpty() && entry.tail.isEmpty()) return;
    
#ifdef GPDF_PERF_PROBE
    qInfo()<<"PERF history_ms"<<profile.nsecsElapsed()/1000000.0;
#endif
    history_.resize(historyPosition_+1); history_.append(entry); historyPosition_=history_.size()-1;
    while(history_.size()>2 && (history_.size()>30 || historyStorageBytes()>128LL*1024*1024)) { history_.removeFirst(); --historyPosition_; }
}
bool PdfDocument::restoreHistory(int position) { std::lock_guard apiGuard(pdfiumMutex);
    if(position<0 || position>=history_.size()) return false;
    QByteArray next=history_[position].materialize();
    auto doc=FPDF_LoadMemDocument64(next.constData(),size_t(next.size()),password_.constData());
    if(!doc) return fail("No se pudo restaurar el cambio.");
    int page=index_; close(); bytes_=std::move(next); doc_=doc; hiddenTextPreview=-1;
    revision_=++nextRevision;
    historyPosition_=position; dirty_=history_[position].id!=savedHistoryId_;
    return setPage(std::min(page,pageCount()-1));
}
bool PdfDocument::undo() { std::lock_guard apiGuard(pdfiumMutex); return canUndo() && restoreHistory(historyPosition_-1); }
bool PdfDocument::redo() { std::lock_guard apiGuard(pdfiumMutex); return canRedo() && restoreHistory(historyPosition_+1); }
QVector<PdfDocument::AnnotationEntry> PdfDocument::documentAnnotations() const { std::lock_guard apiGuard(pdfiumMutex);
    if(annotationsCached_) return annotationEntries_;
    QVector<AnnotationEntry> entries;
    for(int index=0;index<pageCount();++index) entries+=pageAnnotations(index);
    annotationEntries_=entries; annotationsCached_=true; return entries;
}
QString PdfDocument::pageText(int index) const { std::lock_guard apiGuard(pdfiumMutex);
    auto page=doc_ ? FPDF_LoadPage(doc_,index) : nullptr; if(!page) return {};
    auto text=FPDFText_LoadPage(page); QString result;
    if(text) { int count=FPDFText_CountChars(text); QVector<unsigned short> buffer(count+1); FPDFText_GetText(text,0,count,buffer.data()); result=QString::fromUtf16(reinterpret_cast<const char16_t*>(buffer.constData())); FPDFText_ClosePage(text); }
    FPDF_ClosePage(page); return result;
}
int PdfDocument::characterCount() const { std::lock_guard apiGuard(pdfiumMutex); return text_ ? FPDFText_CountChars(text_) : 0; }
int PdfDocument::moveCommentAnnotation(int sourcePage,int index,int destinationPage,QPointF topLeft) {
    std::lock_guard apiGuard(pdfiumMutex);
    const int originalPage=index_;
    if(sourcePage<0 || sourcePage>=pageCount() || destinationPage<0 || destinationPage>=pageCount()) { fail("La página de destino no existe."); return -1; }
    if(!setPage(sourcePage)) return -1;
    if(annotationType(index)!=FPDF_ANNOT_TEXT || !canEditAnnotation(index)) { setPage(originalPage); fail("Seleccioná un comentario editable."); return -1; }
    auto source=FPDFPage_GetAnnot(page_,index);
    FS_RECTF previous{}; FPDFAnnot_GetRect(source,&previous);
    const float width=previous.right-previous.left,height=previous.top-previous.bottom;
    const auto size=pageSize(destinationPage);
    if(width>size.width() || height>size.height()) { FPDFPage_CloseAnnot(source); setPage(originalPage); fail("El comentario no cabe en la página de destino."); return -1; }
    topLeft.setX(std::clamp(topLeft.x(),0.0,size.width()-width));
    topLeft.setY(std::clamp(topLeft.y(),double(height),size.height()));
    FS_RECTF target{float(topLeft.x()),float(topLeft.y()),float(topLeft.x()+width),float(topLeft.y()-height)};
    if(sourcePage==destinationPage) {
        bool ok=FPDFAnnot_SetRect(source,&target); FPDFPage_CloseAnnot(source);
        if(!ok) { setPage(originalPage); fail("No se pudo mover el comentario."); return -1; }
        dirty_=true; recordHistory(); return index;
    }
    auto destination=FPDF_LoadPage(doc_,destinationPage);
    auto moved=destination ? FPDFPage_CreateAnnot(destination,FPDF_ANNOT_TEXT) : nullptr;
    bool ok=moved && FPDFAnnot_SetRect(moved,&target);
    if(moved) {
        ok=FPDFAnnot_SetFlags(moved,FPDFAnnot_GetFlags(source)) && ok;
        unsigned int red,green,blue,alpha;
        if(FPDFAnnot_GetColor(source,FPDFANNOT_COLORTYPE_Color,&red,&green,&blue,&alpha)) ok=FPDFAnnot_SetColor(moved,FPDFANNOT_COLORTYPE_Color,red,green,blue,alpha) && ok;
        for(const char* key:{"Contents","T","Subj","Name","NM","M","CreationDate"}) {
            auto length=FPDFAnnot_GetStringValue(source,key,nullptr,0);
            if(length>2) { QVector<unsigned short> value(length/2); FPDFAnnot_GetStringValue(source,key,value.data(),length); ok=FPDFAnnot_SetStringValue(moved,key,value.data()) && ok; }
        }
        FPDFPage_CloseAnnot(moved);
    }
    FPDFPage_CloseAnnot(source);
    const int result=destination ? FPDFPage_GetAnnotCount(destination)-1 : -1;
    if(ok) ok=FPDFPage_RemoveAnnot(page_,index);
    if(!ok && moved) FPDFPage_RemoveAnnot(destination,result);
    if(destination) FPDF_ClosePage(destination);
    if(!ok) { setPage(originalPage); fail("No se pudo mover el comentario."); return -1; }
    annotationPages_.clear(); searchIndex_->clear(); hoverRegions_.clear();
    setPage(destinationPage); dirty_=true; recordHistory(); return result;
}
bool PdfDocument::moveTextAnnotation(int index,QPointF topLeft) { std::lock_guard apiGuard(pdfiumMutex);
    if(annotationType(index)!=FPDF_ANNOT_FREETEXT || !canEditAnnotation(index)) return fail("Seleccioná un cuadro de texto editable.");
    auto annot=FPDFPage_GetAnnot(page_,index); auto rect=annotationRect(index); auto value=annotationContents(index); auto style=annotationTextStyle(index);
    style.width=rect.width(); style.height=rect.height();
    topLeft.setX(std::clamp(topLeft.x(),0.0,std::max(0.0,pageSize().width()-rect.width())));
    topLeft.setY(std::clamp(topLeft.y(),rect.height(),pageSize().height()));
    FS_RECTF oldRect{}; FPDFAnnot_GetRect(annot,&oldRect);
    auto size=FPDFAnnot_GetAP(annot,FPDF_ANNOT_APPEARANCEMODE_NORMAL,nullptr,0); QVector<unsigned short> appearance(size/2);
    if(size) FPDFAnnot_GetAP(annot,FPDF_ANNOT_APPEARANCEMODE_NORMAL,appearance.data(),size);
    bool mutated=false,ok=writeFreeText(annot,topLeft,value,style,&mutated);
    if(!ok && mutated) { FPDFAnnot_SetRect(annot,&oldRect); FPDFAnnot_SetAP(annot,FPDF_ANNOT_APPEARANCEMODE_NORMAL,size ? appearance.data() : nullptr); }
    FPDFPage_CloseAnnot(annot);
    if(ok) { dirty_=true; recordHistory(); } return ok;
}
PdfDocument::AnnotationCopy PdfDocument::copyAnnotation(int index) const { std::lock_guard apiGuard(pdfiumMutex);
    AnnotationCopy copy;
    if(!canEditAnnotation(index)) return copy;
    copy.type=annotationType(index); copy.text=annotationContents(index); copy.rect=annotationRect(index); copy.color=annotationColor(index); copy.style=annotationTextStyle(index);
    auto annot=FPDFPage_GetAnnot(page_,index);
    copy.flags=FPDFAnnot_GetFlags(annot); float horizontal=0,vertical=0; FPDFAnnot_GetBorder(annot,&horizontal,&vertical,&copy.strokeWidth);
    for(size_t i=0;i<FPDFAnnot_CountAttachmentPoints(annot);++i) { FS_QUADPOINTSF quad{}; if(FPDFAnnot_GetAttachmentPoints(annot,i,&quad)) copy.quads.append(quad); }
    for(unsigned long i=0;i<FPDFAnnot_GetInkListCount(annot);++i) { auto count=FPDFAnnot_GetInkListPath(annot,i,nullptr,0); QVector<FS_POINTF> points(count); if(count && FPDFAnnot_GetInkListPath(annot,i,points.data(),count)) copy.strokes.append(points); }
    FPDFPage_CloseAnnot(annot); return copy;
}
bool PdfDocument::pasteAnnotation(const AnnotationCopy& copy,QPointF topLeft) { std::lock_guard apiGuard(pdfiumMutex);
    if(!canAddAnnotations()) return fail("El documento no permite agregar anotaciones.");
    if(!page_ || copy.type<0 || copy.rect.width()>pageSize().width() || copy.rect.height()>pageSize().height()) return fail("No se puede pegar esta anotación en la página.");
    topLeft.setX(std::clamp(topLeft.x(),0.0,pageSize().width()-copy.rect.width()));
    topLeft.setY(std::clamp(topLeft.y(),copy.rect.height(),pageSize().height()));
    if(copy.type==FPDF_ANNOT_FREETEXT) { auto style=copy.style; style.width=copy.rect.width(); style.height=copy.rect.height(); return freeText(topLeft,copy.text,style); }
    auto annot=FPDFPage_CreateAnnot(page_,FPDF_ANNOTATION_SUBTYPE(copy.type)); if(!annot) return fail("No se pudo pegar la anotación.");
    FS_RECTF rect{float(topLeft.x()),float(topLeft.y()),float(topLeft.x()+copy.rect.width()),float(topLeft.y()-copy.rect.height())};
    FPDFAnnot_SetFlags(annot,copy.flags);
    if(copy.type==FPDF_ANNOT_INK) FPDFAnnot_SetBorder(annot,0,0,copy.strokeWidth);
    bool ok=FPDFAnnot_SetRect(annot,&rect) && setContents(annot,copy.text);
    if(copy.color.isValid()) ok=FPDFAnnot_SetColor(annot,FPDFANNOT_COLORTYPE_Color,copy.color.red(),copy.color.green(),copy.color.blue(),copy.color.alpha()) && ok;
    float dx=float(topLeft.x()-copy.rect.left()),dy=float(topLeft.y()-copy.rect.bottom());
    for(auto quad:copy.quads) { quad.x1+=dx;quad.x2+=dx;quad.x3+=dx;quad.x4+=dx;quad.y1+=dy;quad.y2+=dy;quad.y3+=dy;quad.y4+=dy; ok=FPDFAnnot_AppendAttachmentPoints(annot,&quad) && ok; }
    for(auto points:copy.strokes) { for(auto& point:points) { point.x+=dx;point.y+=dy; } ok=(FPDFAnnot_AddInkStroke(annot,points.data(),points.size())>=0) && ok; }
    FPDFPage_CloseAnnot(annot);
    if(!ok) { FPDFPage_RemoveAnnot(page_,annotationCount()-1); return fail("No se pudo pegar la anotación."); }
    dirty_=true; recordHistory(); return true;
}
bool PdfDocument::fail(const QString& message) { std::lock_guard apiGuard(pdfiumMutex); error_ = message; return false; }
void PdfDocument::closePage() { std::lock_guard apiGuard(pdfiumMutex);
    if (text_) FPDFText_ClosePage(text_);
    if (page_) FPDF_ClosePage(page_);
    text_ = nullptr; page_ = nullptr;
}
void PdfDocument::close() { std::lock_guard apiGuard(pdfiumMutex);
    annotationsCached_=false; annotationEntries_.clear(); annotationPages_.clear(); searchIndex_->clear();
    if(hoverText_) FPDFText_ClosePage(hoverText_);
    if(hoverPage_) FPDF_ClosePage(hoverPage_);
    hoverText_=nullptr; hoverPage_=nullptr; hoverIndex_=-1;
    hoverRegions_.clear();
    closePage();
    if (doc_) FPDF_CloseDocument(doc_);
    doc_ = nullptr; bytes_.clear(); dirty_ = false;
}
bool PdfDocument::open(const QString& path, const QByteArray& password) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) { std::lock_guard apiGuard(pdfiumMutex); return fail(file.errorString()); }
    QByteArray bytes = file.readAll();
    const auto readError=file.error(); const auto readMessage=file.errorString(); file.close();
    std::lock_guard apiGuard(pdfiumMutex);
    if(readError!=QFileDevice::NoError) return fail(readMessage);
    auto next = FPDF_LoadMemDocument64(bytes.constData(), size_t(bytes.size()), password.constData());
    if (!next) return fail(QString("No se pudo abrir el PDF (error %1). Puede requerir una contraseña.").arg(FPDF_GetLastError()));
    // Validate the first page before replacing the current document and its undo history.
    auto firstPage=FPDF_LoadPage(next,0);
    if(!firstPage) { FPDF_CloseDocument(next); return fail("El PDF no contiene una primera página válida."); }
    auto firstText=FPDFText_LoadPage(firstPage);
    close(); bytes_=std::move(bytes); doc_=next; password_=password;
    page_=firstPage; text_=firstText; index_=0; resetHistory(); return true;
}
bool PdfDocument::createSample(int pages,const QString& sampleText) { std::lock_guard apiGuard(pdfiumMutex);
    if(pages<1 || pages>100) return fail("Cantidad de páginas de prueba inválida.");
    close(); password_.clear(); doc_ = FPDF_CreateNewDocument();
    if (!doc_) return fail("No se pudo crear el documento de prueba.");
    for(int i=0;i<pages;++i) {
    auto page = FPDFPage_New(doc_, i, 595, 842);
    auto font = FPDFText_LoadStandardFont(doc_, "Helvetica");
    auto object = FPDFPageObj_CreateTextObj(doc_, font, 18);
    const QString content = !sampleText.isEmpty() ? sampleText : (pages==1 ? "Study PDF: select this text and highlight it." : QString("Page %1: select this text and highlight it.").arg(i+1));
    bool ok = page && font && object;
    if (ok) {
        ok = FPDFText_SetText(object, reinterpret_cast<FPDF_WIDESTRING>(content.utf16()));
        FPDFPageObj_Transform(object, 1, 0, 0, 1, 45, 750);
        FPDFPage_InsertObject(page, object);
        ok = FPDFPage_GenerateContent(page) && ok;
    } else if (object) FPDFPageObj_Destroy(object);
    if (font) FPDFFont_Close(font);
    if (page) FPDF_ClosePage(page);
    if (!ok) return fail("Falló la generación de la página de prueba.");
    }
    dirty_ = true;
    bool ok=setPage(0); if(ok) resetHistory(); return ok;
}
bool PdfDocument::setPage(int index) { std::lock_guard apiGuard(pdfiumMutex);
    if (!doc_ || index < 0 || index >= pageCount()) return fail("Página fuera de rango.");
    if(page_ && index==index_) return true;
    auto nextPage=FPDF_LoadPage(doc_,index);
    if(!nextPage) return fail("No se pudo cargar la página.");
    auto nextText=FPDFText_LoadPage(nextPage);
    closePage(); page_=nextPage; text_=nextText; index_=index;
    return true;
}
int PdfDocument::pageCount() const { std::lock_guard apiGuard(pdfiumMutex); return doc_ ? FPDF_GetPageCount(doc_) : 0; }
QSizeF PdfDocument::pageSize() const { std::lock_guard apiGuard(pdfiumMutex); return page_ ? QSizeF(FPDF_GetPageWidthF(page_), FPDF_GetPageHeightF(page_)) : QSizeF(); }
QImage PdfDocument::render(double scale) const { std::lock_guard apiGuard(pdfiumMutex);
    if (!page_ || !std::isfinite(scale) || scale<=0) return {};
    QSize size = (pageSize() * scale).toSize();
    if(size.width()<=0 || size.height()<=0) return {};
    if(qint64(size.width())*size.height()>16LL*1024*1024) size=(QSizeF(size)*std::sqrt(16.0*1024*1024/(double(size.width())*size.height()))).toSize();
    QImage image(size, QImage::Format_ARGB32);
    image.fill(Qt::white);
    auto bitmap = FPDFBitmap_CreateEx(size.width(), size.height(), FPDFBitmap_BGRA, image.bits(), image.bytesPerLine());
    if (!bitmap) return {};
    auto hidden=hiddenTextPreview>=0 ? FPDFPage_GetAnnot(page_,hiddenTextPreview) : nullptr;
    int flags=hidden ? FPDFAnnot_GetFlags(hidden) : 0;
    if(hidden) FPDFAnnot_SetFlags(hidden,flags|FPDF_ANNOT_FLAG_HIDDEN);
    FPDF_RenderPageBitmap(bitmap, page_, 0, 0, size.width(), size.height(), 0, FPDF_ANNOT);
    if(hidden) { FPDFAnnot_SetFlags(hidden,flags); FPDFPage_CloseAnnot(hidden); }
    FPDFBitmap_Destroy(bitmap);
    return image;
}
QSizeF PdfDocument::pageSize(int index) const { std::lock_guard apiGuard(pdfiumMutex);
    FS_SIZEF size{};
    return doc_ && FPDF_GetPageSizeByIndexF(doc_,index,&size) ? QSizeF(size.width,size.height) : QSizeF();
}
QImage PdfDocument::renderPage(int index,double scale) const { std::lock_guard apiGuard(pdfiumMutex);
    if(!doc_ || index<0 || index>=pageCount() || !std::isfinite(scale) || scale<=0) return {};
    if(index==index_) return render(scale);
    auto page=FPDF_LoadPage(doc_,index);
    if(!page) return {};
    std::unique_ptr<std::remove_pointer_t<FPDF_PAGE>,decltype(&FPDF_ClosePage)> pageGuard(page,&FPDF_ClosePage);
    QSize size=(pageSize(index)*scale).toSize();
    if(size.width()<=0 || size.height()<=0) return {};
    if(qint64(size.width())*size.height()>16LL*1024*1024) size=(QSizeF(size)*std::sqrt(16.0*1024*1024/(double(size.width())*size.height()))).toSize();
    QImage image(size,QImage::Format_ARGB32);
    if(!image.isNull()) {
        image.fill(Qt::white);
        auto bitmap=FPDFBitmap_CreateEx(size.width(),size.height(),FPDFBitmap_BGRA,image.bits(),image.bytesPerLine());
        if(bitmap) { FPDF_RenderPageBitmap(bitmap,page,0,0,size.width(),size.height(),0,FPDF_ANNOT); FPDFBitmap_Destroy(bitmap); }
        else image={};
    }
    return image;
}
int PdfDocument::characterAt(QPointF p) const { std::lock_guard apiGuard(pdfiumMutex); return text_ ? FPDFText_GetCharIndexAtPos(text_, p.x(), p.y(), 12, 12) : -1; }
QVector<PdfDocument::SearchMatch> PdfDocument::searchPage(int index,const QString& query) const { std::lock_guard apiGuard(pdfiumMutex);
    QVector<SearchMatch> matches;
    if(!doc_ || index<0 || index>=pageCount() || query.trimmed().isEmpty()) return matches;
    auto fold=[](const QString& value) {
        QString result;
        for(QChar c:value.normalized(QString::NormalizationForm_D).toCaseFolded()) {
            auto category=c.category();
            if(category!=QChar::Mark_NonSpacing && category!=QChar::Mark_SpacingCombining && category!=QChar::Mark_Enclosing) result+=c;
        }
        return result;
    };
    QString needle=fold(query).simplified();
    if(needle.isEmpty()) return matches;
    if(auto cached=searchIndex_->object(index); cached && cached->lastQuery==needle) return cached->matches;
    if(auto cached=searchIndex_->object(index); cached && !cached->normalized.contains(needle)) return matches;
    auto page=FPDF_LoadPage(doc_,index);
    if(!page) return matches;
    std::unique_ptr<std::remove_pointer_t<FPDF_PAGE>,decltype(&FPDF_ClosePage)> pageGuard(page,&FPDF_ClosePage);
    auto text=FPDFText_LoadPage(page);
    std::unique_ptr<std::remove_pointer_t<FPDF_TEXTPAGE>,decltype(&FPDFText_ClosePage)> textGuard(text,&FPDFText_ClosePage);
    if(text) {
        int total=FPDFText_CountChars(text);
        auto cached=searchIndex_->object(index);
        SearchIndex fresh;
        if(!cached) {
            auto& data=fresh; data.normalized.reserve(total); data.starts.reserve(total); data.ends.reserve(total);
            // Map normalized UTF-16 positions back to PDFium's character indices.
            // Removing accents must not shift the geometry of the highlighted match.
            for(int i=0;i<total;++i) {
                char32_t codepoint=FPDFText_GetUnicode(text,i);
                if(!codepoint) continue;
                QString folded=codepoint<128 ? QString(QChar(ushort(codepoint))).toLower() : fold(QString::fromUcs4(&codepoint,1));
                if(folded.isEmpty()) { if(!data.ends.isEmpty()) data.ends.last()=i; continue; }
                for(QChar c:folded) {
                    if(c.isSpace()) {
                        if(!data.normalized.isEmpty() && data.normalized.back()==' ') { data.ends.last()=i; continue; }
                        c=' ';
                    }
                    data.normalized+=c; data.starts.append(i); data.ends.append(i);
                }
            }
            qsizetype cost=fresh.storageBytes();
            if(cost<=searchIndex_->maxCost()) { searchIndex_->insert(index,new SearchIndex(std::move(fresh)),cost); cached=searchIndex_->object(index); }
        }
        const auto& data=cached ? *cached : fresh;
        const auto& normalized=data.normalized; const auto& starts=data.starts; const auto& ends=data.ends;
        int position=0;
        while(!needle.isEmpty() && (position=normalized.indexOf(needle,position))>=0) {
            int first=starts[position],count=ends[position+needle.size()-1]-first+1;
            SearchMatch match{index,first,count,{}, {}, {}};
            int start=std::max(0,first-40),length=std::min(total-start,count+80);
            auto excerpt=[&](int from,int amount) {
                QVector<unsigned short> buffer(amount+1);
                int actual=FPDFText_GetText(text,from,amount,buffer.data());
                return actual>0 ? QString::fromUtf16(reinterpret_cast<const char16_t*>(buffer.constData()),actual-1) : QString();
            };
            match.context=excerpt(start,length).simplified();
            match.contextHtml=(excerpt(start,first-start).toHtmlEscaped()+"<b>"+excerpt(first,count).toHtmlEscaped()+"</b>"+excerpt(first+count,std::max(0,start+length-first-count)).toHtmlEscaped()).replace('\n',' ').replace('\r',' ');
            int rectangles=FPDFText_CountRects(text,first,count);
            for(int i=0;i<rectangles;++i) {
                double left,top,right,bottom;
                if(FPDFText_GetRect(text,i,&left,&top,&right,&bottom)) match.rectangles.append(QRectF(left,bottom,right-left,top-bottom));
            }
            matches.append(std::move(match));
            position+=needle.size();
        }
    }
    if(auto cached=searchIndex_->object(index)) {
        SearchIndex updated=*cached; updated.lastQuery=needle; updated.matches=matches;
        auto cost=updated.storageBytes();
        if(cost<=searchIndex_->maxCost()) searchIndex_->insert(index,new SearchIndex(std::move(updated)),cost);
    }
    return matches;
}
bool PdfDocument::textAt(int index,QPointF point,double margin) const { std::lock_guard apiGuard(pdfiumMutex);
    if(!doc_ || index<0 || index>=pageCount()) return false;
    if(!hoverRegions_.contains(index)) {
    if(index!=index_ && index!=hoverIndex_) {
        if(hoverText_) FPDFText_ClosePage(hoverText_);
        if(hoverPage_) FPDF_ClosePage(hoverPage_);
        hoverPage_=FPDF_LoadPage(doc_,index);
        hoverText_=hoverPage_ ? FPDFText_LoadPage(hoverPage_) : nullptr;
        hoverIndex_=index;
    }
    auto text=index==index_ ? text_ : hoverText_;
    if(!text) return false;
    QVector<QRectF> regions;
    // PDFium's text rectangles span runs, including the spaces between words.
    int count=FPDFText_CountRects(text,0,FPDFText_CountChars(text));
    for(int i=0;i<count;++i) {
        double left,top,right,bottom;
        if(FPDFText_GetRect(text,i,&left,&top,&right,&bottom)) regions.append(QRectF(left,bottom,right-left,top-bottom));
    }
    hoverRegions_.insert(index,regions);
    }
    margin=std::max(0.0,margin);
    for(const auto& r:hoverRegions_[index]) if(r.adjusted(-margin,-margin,margin,margin).contains(point)) return true;
    return false;
}
QVector<QRectF> PdfDocument::selectionRects(int first, int last) const { std::lock_guard apiGuard(pdfiumMutex);
    QVector<QRectF> result;
    if (!text_ || first < 0 || last < 0) return result;
    if (first > last) std::swap(first, last);
    int count = FPDFText_CountRects(text_, first, last - first + 1);
    for (int i = 0; i < count; ++i) {
        double left, top, right, bottom;
        if (FPDFText_GetRect(text_, i, &left, &top, &right, &bottom)) result.append(QRectF(left, bottom, right-left, top-bottom));
    }
    return result;
}
QString PdfDocument::selectionText(int first, int last) const { std::lock_guard apiGuard(pdfiumMutex);
    if (!text_ || first < 0 || last < 0) return {};
    if (first > last) std::swap(first, last);
    QVector<unsigned short> buffer(last-first+2);
    int length = FPDFText_GetText(text_, first, last-first+1, buffer.data());
    return length > 0 ? QString::fromUtf16(reinterpret_cast<const char16_t*>(buffer.data()), length-1) : QString();
}
bool PdfDocument::highlight(int first, int last,const QColor& color) {
#ifdef GPDF_PERF_PROBE
    QElapsedTimer profile; profile.start();
#endif
    std::lock_guard apiGuard(pdfiumMutex);
#ifdef GPDF_PERF_PROBE
    qInfo()<<"PERF highlight_lock_ms"<<profile.nsecsElapsed()/1000000.0;
#endif
    if(!canAddAnnotations()) return fail("El documento no permite agregar anotaciones.");
    // PDFium can split a visual line at font runs and omit the spaces between them.
    // Join nearby runs on the same baseline, without spanning column-sized gaps.
    QVector<QRectF> rects;
    for(const auto& raw:selectionRects(first,last)) {
        auto r=raw.normalized(); if(r.isEmpty()) continue;
        bool joined=false;
        if(!rects.isEmpty()) {
            auto& line=rects.last();
            double height=std::max(line.height(),r.height());
            double overlap=std::min(line.bottom(),r.bottom())-std::max(line.top(),r.top());
            double gap=std::max({0.0,r.left()-line.right(),line.left()-r.right()});
            if(overlap>=0.6*std::min(line.height(),r.height()) && std::abs(line.center().y()-r.center().y())<=height*0.35 && gap<=height*0.85) {
                line=line.united(r); joined=true;
            }
        }
        if(!joined) rects.append(r);
    }
    if (rects.isEmpty()) return fail("Seleccioná texto antes de resaltar.");
    auto annot = FPDFPage_CreateAnnot(page_, FPDF_ANNOT_HIGHLIGHT);
    if (!annot) return fail("Esta versión de PDFium no permite crear resaltados.");
    FPDFAnnot_SetFlags(annot,FPDF_ANNOT_FLAG_PRINT);
    QRectF bounds;
    bool ok = FPDFAnnot_SetColor(annot, FPDFANNOT_COLORTYPE_Color, color.red(), color.green(), color.blue(), color.alpha());
    for (const auto& r : rects) {
        bounds = bounds.united(r);
        FS_QUADPOINTSF quad{float(r.left()),float(r.bottom()),float(r.right()),float(r.bottom()),float(r.left()),float(r.top()),float(r.right()),float(r.top())};
        ok = FPDFAnnot_AppendAttachmentPoints(annot, &quad) && ok;
    }
    FS_RECTF box{float(bounds.left()),float(bounds.bottom()),float(bounds.right()),float(bounds.top())};
    ok = FPDFAnnot_SetRect(annot, &box) && setContents(annot, selectionText(first,last)) && ok;
    FPDFPage_CloseAnnot(annot);
    if (!ok) { FPDFPage_RemoveAnnot(page_, annotationCount()-1); return fail("Falló la creación del resaltado."); }
    dirty_ = true; recordHistory();
#ifdef GPDF_PERF_PROBE
    qInfo()<<"PERF highlight_total_ms"<<profile.nsecsElapsed()/1000000.0;
#endif
    return true;
}
bool PdfDocument::note(QPointF p, const QString& value) { std::lock_guard apiGuard(pdfiumMutex);
    if(!canAddAnnotations()) return fail("El documento no permite agregar anotaciones.");
    auto annot = page_ ? FPDFPage_CreateAnnot(page_, FPDF_ANNOT_TEXT) : nullptr;
    if (!annot) return fail("No se pudo crear la nota.");
    FPDFAnnot_SetFlags(annot,FPDF_ANNOT_FLAG_PRINT);
    FS_RECTF box{float(p.x()),float(p.y()+20),float(p.x()+20),float(p.y())};
    bool ok = FPDFAnnot_SetRect(annot,&box) && setContents(annot,value) && FPDFAnnot_SetColor(annot,FPDFANNOT_COLORTYPE_Color,255,210,50,255);
    FPDFPage_CloseAnnot(annot);
    if (!ok) { FPDFPage_RemoveAnnot(page_, annotationCount()-1); return fail("Falló la creación de la nota."); }
    dirty_ = true; recordHistory(); return true;
}
bool PdfDocument::freeText(QPointF p, const QString& value) { std::lock_guard apiGuard(pdfiumMutex);
    return freeText(p,value,TextStyle{});
}
bool PdfDocument::freeText(QPointF p, const QString& value, const TextStyle& style) { std::lock_guard apiGuard(pdfiumMutex);
    if(!canAddAnnotations()) return fail("El documento no permite agregar anotaciones.");
    if(value.trimmed().isEmpty() || style.size<6 || style.size>96 || !style.color.isValid()) return fail("Texto o formato inválido.");
    // Provide an explicit appearance stream for consistent display in other readers.
    auto annot = page_ ? FPDFPage_CreateAnnot(page_, FPDF_ANNOT_FREETEXT) : nullptr;
    if (!annot) return fail("Esta versión de PDFium no permite texto libre.");
    FPDFAnnot_SetFlags(annot,FPDF_ANNOT_FLAG_PRINT);
    bool ok = writeFreeText(annot,p,value,style);
    if(ok) {
        QString format=styleValue(style);
        ok=FPDFAnnot_SetStringValue(annot,"StudyPDFTextStyle",reinterpret_cast<FPDF_WIDESTRING>(format.utf16()));
    }
    FPDFPage_CloseAnnot(annot);
    if (!ok) { FPDFPage_RemoveAnnot(page_, annotationCount()-1); return false; }
    dirty_ = true; recordHistory(); return true;
}
bool PdfDocument::writeFreeText(FPDF_ANNOTATION annot, QPointF p, const QString& value, const TextStyle& style, bool* mutated) { std::lock_guard apiGuard(pdfiumMutex);
    if(mutated) *mutated=false;
    const float left = float(p.x()), top = float(p.y());
    const float width = float(std::min(std::max(30.0,style.width), pageSize().width()-p.x()));
    if (width < 30 || top < 30) { return fail("Elegí un punto más alejado del borde derecho o inferior."); }
    QFont font(style.family); font.setPixelSize(style.size);
    QPainterPath glyphs;
    double y = 5;
    for (const auto& paragraph : value.split('\n')) {
        QTextLayout layout(paragraph,font);
        layout.beginLayout();
        for (;;) {
            auto line = layout.createLine();
            if (!line.isValid()) break;
            line.setLineWidth(width-10);
            glyphs.addText(QPointF(5,y+line.ascent()),font,paragraph.mid(line.textStart(),line.textLength()));
            y += line.height();
        }
        layout.endLayout();
        if (paragraph.isEmpty()) y += style.size*1.2;
    }
    const float height = float(std::max(y+5,style.height));
    if (height > top) { return fail("El texto no cabe en la página. Elegí un punto más arriba o un comentario más corto."); }
    FS_RECTF box{left,top,left+width,top-height};
    // Vector glyph appearances preserve accents without depending on viewer font resources.
    // Editable Unicode text remains in the standard annotation Contents entry.
    QString appearance = QString("q %1 %2 %3 rg ").arg(style.color.redF(),0,'f',4).arg(style.color.greenF(),0,'f',4).arg(style.color.blueF(),0,'f',4);
    // SetAP uses the annotation's absolute rectangle as its appearance BBox.
    auto point=[left,top](double x,double y){ return QString::number(left+x,'f',4)+" "+QString::number(top-y,'f',4); };
    for(int i=0;i<glyphs.elementCount();++i) {
        auto e=glyphs.elementAt(i);
        if(e.type==QPainterPath::MoveToElement) appearance+=point(e.x,e.y)+" m ";
        else if(e.type==QPainterPath::LineToElement) appearance+=point(e.x,e.y)+" l ";
        else if(e.type==QPainterPath::CurveToElement) {
            auto b=glyphs.elementAt(++i), c=glyphs.elementAt(++i);
            appearance+=point(e.x,e.y)+" "+point(b.x,b.y)+" "+point(c.x,c.y)+" c ";
        }
    }
    appearance += "f Q";
    if(mutated) *mutated=true;
    bool ok = FPDFAnnot_SetRect(annot,&box) && setContents(annot,value) && FPDFAnnot_SetAP(annot,FPDF_ANNOT_APPEARANCEMODE_NORMAL,reinterpret_cast<FPDF_WIDESTRING>(appearance.utf16()));
    if(ok) {
        QString defaultAppearance=QString("/Helvetica %1 Tf %2 %3 %4 rg").arg(style.size).arg(style.color.redF(),0,'f',4).arg(style.color.greenF(),0,'f',4).arg(style.color.blueF(),0,'f',4);
        ok=FPDFAnnot_SetStringValue(annot,"DA",reinterpret_cast<FPDF_WIDESTRING>(defaultAppearance.utf16()));
    }
    return ok || fail("Falló el texto libre o su apariencia.");
}
bool PdfDocument::ink(const QVector<QPointF>& points,const QColor& color,double width) { std::lock_guard apiGuard(pdfiumMutex);
    if(!canAddAnnotations()) return fail("El documento no permite agregar anotaciones.");
    if (!page_ || points.size() < 2) return fail("Dibujá un trazo de al menos dos puntos.");
    auto annot = FPDFPage_CreateAnnot(page_, FPDF_ANNOT_INK);
    if (!annot) return fail("No se pudo crear el trazo.");
    FPDFAnnot_SetFlags(annot,FPDF_ANNOT_FLAG_PRINT);
    QVector<FS_POINTF> stroke;
    double left=points.first().x(), right=left, bottom=points.first().y(), top=bottom;
    for (auto p : points) { stroke.append({float(p.x()),float(p.y())}); left=std::min(left,p.x()); right=std::max(right,p.x()); bottom=std::min(bottom,p.y()); top=std::max(top,p.y()); }
    FS_RECTF box{float(left-width),float(top+width),float(right+width),float(bottom-width)};
    bool ok = FPDFAnnot_SetRect(annot,&box) && FPDFAnnot_SetColor(annot,FPDFANNOT_COLORTYPE_Color,color.red(),color.green(),color.blue(),color.alpha()) && FPDFAnnot_SetBorder(annot,0,0,float(width)) && FPDFAnnot_AddInkStroke(annot,stroke.constData(),stroke.size()) >= 0;
    FPDFPage_CloseAnnot(annot);
    if (!ok) { FPDFPage_RemoveAnnot(page_, annotationCount()-1); return fail("Falló la creación del trazo."); }
    dirty_ = true; recordHistory(); return true;
}
bool PdfDocument::saveCopy(const QString& path) { std::unique_lock apiGuard(pdfiumMutex);
    if (!doc_) return fail("No hay un documento abierto.");
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) return fail(file.errorString());
    Writer writer(file);
    if (!FPDF_SaveAsCopy(doc_, &writer, FPDF_NO_INCREMENTAL)) { file.cancelWriting(); return fail("PDFium no pudo guardar el documento."); }
    const auto savedRevision=revision_;
    const auto savedId=historyPosition_>=0 ? history_[historyPosition_].id : 0;
    // PDFium serialization is finished; flushing and replacing the destination
    // must not prevent another document from rendering or searching.
    apiGuard.unlock();
    const bool committed=file.commit(); const auto message=file.errorString();
    apiGuard.lock();
    if(!committed) return fail(message);
    if(revision_==savedRevision) { dirty_=false; savedHistoryId_=savedId; }
    return true;
}
int PdfDocument::annotationCount() const { std::lock_guard apiGuard(pdfiumMutex); return page_ ? FPDFPage_GetAnnotCount(page_) : 0; }
QString PdfDocument::annotationContents(int index) const { std::lock_guard apiGuard(pdfiumMutex);
    if (!page_ || index < 0 || index >= annotationCount()) return {};
    auto annot = FPDFPage_GetAnnot(page_,index);
    if (!annot) return {};
    auto size = FPDFAnnot_GetStringValue(annot,"Contents",nullptr,0);
    QVector<unsigned short> buffer(size/2);
    if (size) FPDFAnnot_GetStringValue(annot,"Contents",buffer.data(),size);
    FPDFPage_CloseAnnot(annot);
    return size >= 2 ? QString::fromUtf16(reinterpret_cast<const char16_t*>(buffer.data()),int(size/2)-1) : QString();
}

int PdfDocument::annotationType(int index) const { std::lock_guard apiGuard(pdfiumMutex);
    if (!page_ || index < 0 || index >= annotationCount()) return FPDF_ANNOT_UNKNOWN;
    auto annot=FPDFPage_GetAnnot(page_,index);
    if (!annot) return FPDF_ANNOT_UNKNOWN;
    int type=FPDFAnnot_GetSubtype(annot);
    FPDFPage_CloseAnnot(annot);
    return type;
}
QRectF PdfDocument::annotationRect(int index) const { std::lock_guard apiGuard(pdfiumMutex);
    if (!page_ || index < 0 || index >= annotationCount()) return {};
    auto annot=FPDFPage_GetAnnot(page_,index);
    if (!annot) return {};
    FS_RECTF box{};
    bool ok=FPDFAnnot_GetRect(annot,&box);
    FPDFPage_CloseAnnot(annot);
    return ok ? QRectF(box.left,box.bottom,box.right-box.left,box.top-box.bottom) : QRectF();
}
bool PdfDocument::canAddAnnotations() const { std::lock_guard apiGuard(pdfiumMutex);
    return doc_ && (FPDF_GetSecurityHandlerRevision(doc_)<0 || (FPDF_GetDocPermissions(doc_) & (1u << 5)));
}
bool PdfDocument::canEditAnnotation(int index) const { std::lock_guard apiGuard(pdfiumMutex);
    if(!canAddAnnotations()) return false;
    int type=annotationType(index);
    if (type!=FPDF_ANNOT_TEXT && type!=FPDF_ANNOT_FREETEXT && type!=FPDF_ANNOT_HIGHLIGHT && type!=FPDF_ANNOT_INK) return false;
    auto annot=FPDFPage_GetAnnot(page_,index);
    if (!annot) return false;
    int flags=FPDFAnnot_GetFlags(annot);
    FPDFPage_CloseAnnot(annot);
    constexpr int lockedContents=1 << 9; // ISO 32000 annotation flag, absent from PDFium's header.
    return !(flags & (FPDF_ANNOT_FLAG_READONLY | FPDF_ANNOT_FLAG_LOCKED | lockedContents));
}
bool PdfDocument::canColorAnnotation(int index) const { std::lock_guard apiGuard(pdfiumMutex);
    const int type=annotationType(index);
    return canEditAnnotation(index) && (type==FPDF_ANNOT_HIGHLIGHT || type==FPDF_ANNOT_INK);
}
QColor PdfDocument::annotationColor(int index) const { std::lock_guard apiGuard(pdfiumMutex);
    if (!page_ || index<0 || index>=annotationCount()) return {};
    auto annot=FPDFPage_GetAnnot(page_,index);
    if (!annot) return {};
    unsigned int r=0,g=0,b=0,a=255;
    bool ok=FPDFAnnot_GetColor(annot,FPDFANNOT_COLORTYPE_Color,&r,&g,&b,&a);
    if(!ok) {
        for(int i=0;i<FPDFAnnot_GetObjectCount(annot);++i) {
            auto object=FPDFAnnot_GetObject(annot,i);
            if(!object) continue;
            ok=FPDFAnnot_GetSubtype(annot)==FPDF_ANNOT_INK ?
                FPDFPageObj_GetStrokeColor(object,&r,&g,&b,&a) : FPDFPageObj_GetFillColor(object,&r,&g,&b,&a);
            if(ok) break;
        }
    }
    FPDFPage_CloseAnnot(annot);
    return ok ? QColor(r,g,b,a) : QColor();
}
PdfDocument::TextStyle PdfDocument::annotationTextStyle(int index) const { std::lock_guard apiGuard(pdfiumMutex);
    TextStyle style;
    if(annotationType(index)!=FPDF_ANNOT_FREETEXT) return style;
    auto annot=FPDFPage_GetAnnot(page_,index);
    auto size=FPDFAnnot_GetStringValue(annot,"StudyPDFTextStyle",nullptr,0);
    QVector<unsigned short> buffer(size/2);
    if(size) {
        FPDFAnnot_GetStringValue(annot,"StudyPDFTextStyle",buffer.data(),size);
        auto parts=QString::fromUtf16(reinterpret_cast<const char16_t*>(buffer.constData())).split('\n');
        if(parts.size()>=3 && parts[1].toInt()>=6 && parts[1].toInt()<=96 && QColor(parts[2]).isValid()) {
            style.family=parts[0]; style.size=parts[1].toInt(); style.color=QColor(parts[2]);
            if(parts.size()>=5) { style.width=std::max(30.0,parts[3].toDouble()); style.height=std::max(0.0,parts[4].toDouble()); }
        }
    }
    FPDFPage_CloseAnnot(annot); return style;
}
bool PdfDocument::editAnnotation(int index,const QString& value,const TextStyle* requestedStyle) { std::lock_guard apiGuard(pdfiumMutex);
    if (!canEditAnnotation(index)) return fail("La anotación no se puede editar: selección inválida, tipo no admitido o protección del PDF.");
    if (value.trimmed().isEmpty()) return fail("El texto no puede estar vacío. Usá Borrar para eliminar la anotación.");
    const QString oldContents=annotationContents(index);
    if (value==oldContents && !requestedStyle) return true;
    if(requestedStyle && (requestedStyle->size<6 || requestedStyle->size>96 || !requestedStyle->color.isValid())) return fail("Formato de texto inválido.");
    const auto oldStyle=annotationTextStyle(index);
    auto annot=FPDFPage_GetAnnot(page_,index);
    if (!annot) return fail("No se pudo cargar la anotación.");
    bool ok=false;
    if (FPDFAnnot_GetSubtype(annot)==FPDF_ANNOT_FREETEXT) {
        FS_RECTF oldRect{};
        FPDFAnnot_GetRect(annot,&oldRect);
        unsigned long size=FPDFAnnot_GetAP(annot,FPDF_ANNOT_APPEARANCEMODE_NORMAL,nullptr,0);
        QVector<unsigned short> oldAP(size/2);
        if (size) FPDFAnnot_GetAP(annot,FPDF_ANNOT_APPEARANCEMODE_NORMAL,oldAP.data(),size);
        bool mutated=false;
        const auto style=requestedStyle ? *requestedStyle : oldStyle;
        ok=writeFreeText(annot,{oldRect.left,oldRect.top},value,style,&mutated);
        if(ok) {
            auto format=styleValue(style);
            ok=FPDFAnnot_SetStringValue(annot,"StudyPDFTextStyle",reinterpret_cast<FPDF_WIDESTRING>(format.utf16()));
        }
        if (!ok && mutated) {
            FPDFAnnot_SetRect(annot,&oldRect);
            setContents(annot,oldContents);
            FPDFAnnot_SetAP(annot,FPDF_ANNOT_APPEARANCEMODE_NORMAL,size ? oldAP.data() : nullptr);
            auto format=styleValue(oldStyle);
            FPDFAnnot_SetStringValue(annot,"StudyPDFTextStyle",reinterpret_cast<FPDF_WIDESTRING>(format.utf16()));
        }
    } else {
        ok=setContents(annot,value);
        if (!ok) fail("No se pudo actualizar el comentario.");
    }
    FPDFPage_CloseAnnot(annot);
    if (ok) { dirty_=true; recordHistory(); }
    return ok;
}
bool PdfDocument::recolorAnnotation(int index,const QColor& color) { std::lock_guard apiGuard(pdfiumMutex);
    if (!canColorAnnotation(index) || !color.isValid()) return fail("Seleccioná un resaltado o un trazo editable y un color válido.");
    auto annot=FPDFPage_GetAnnot(page_,index);
    if (!annot) return fail("No se pudo cargar la anotación.");
    // SetColor cannot modify an annotation with an explicit appearance stream.
    // Remove it so PDFium regenerates the appearance from its standard geometry.
    unsigned long size=FPDFAnnot_GetAP(annot,FPDF_ANNOT_APPEARANCEMODE_NORMAL,nullptr,0);
    QVector<unsigned short> oldAP(size/2);
    if(size) FPDFAnnot_GetAP(annot,FPDF_ANNOT_APPEARANCEMODE_NORMAL,oldAP.data(),size);
    unsigned int r=0,g=0,b=0,a=255;
    bool ok=FPDFAnnot_SetAP(annot,FPDF_ANNOT_APPEARANCEMODE_NORMAL,nullptr);
    if(ok) {
        FPDFAnnot_GetColor(annot,FPDFANNOT_COLORTYPE_Color,&r,&g,&b,&a);
        ok=FPDFAnnot_SetColor(annot,FPDFANNOT_COLORTYPE_Color,color.red(),color.green(),color.blue(),a);
    }
    if(!ok) {
        FPDFAnnot_SetColor(annot,FPDFANNOT_COLORTYPE_Color,r,g,b,a);
        if(size) FPDFAnnot_SetAP(annot,FPDF_ANNOT_APPEARANCEMODE_NORMAL,oldAP.data());
    }
    FPDFPage_CloseAnnot(annot);
    if(!ok) return fail("No se pudo cambiar el color.");
    dirty_=true; recordHistory(); return true;
}
bool PdfDocument::removeAnnotation(int index) { std::lock_guard apiGuard(pdfiumMutex);
    if (!canEditAnnotation(index)) return fail("La anotación no se puede borrar: selección inválida, tipo no admitido o protección del PDF.");
    if(!FPDFPage_RemoveAnnot(page_,index)) return fail("No se pudo borrar la anotación.");
    dirty_=true; recordHistory(); return true;
}

QVector<QPair<QString,QString>> PdfDocument::properties() const { std::lock_guard apiGuard(pdfiumMutex);
    QVector<QPair<QString,QString>> values;
    if(!doc_) return values;
    int version=0; FPDF_GetFileVersion(doc_,&version);
    values.append(QPair<QString,QString>{"Versión PDF",version ? QString("%1.%2").arg(version/10).arg(version%10) : "—"});
    const char* tags[]={"Title","Author","Subject","Keywords","Creator","Producer","CreationDate","ModDate"};
    const QStringList labels={"Título","Autor","Tema","Palabras clave","Aplicación creadora","Productor PDF","Fecha de creación","Última modificación"};
    for(int i=0;i<8;++i) {
        unsigned long size=FPDF_GetMetaText(doc_,tags[i],nullptr,0);
        QVector<unsigned short> buffer(size/2+1);
        if(size) FPDF_GetMetaText(doc_,tags[i],buffer.data(),size);
        QString text=size>=2 ? QString::fromUtf16(reinterpret_cast<const char16_t*>(buffer.constData()),int(size/2)-1) : QString();
        values.append(QPair<QString,QString>{labels[i],text.isEmpty() ? "—" : text});
    }
    values.append(QPair<QString,QString>{"Seguridad",FPDF_GetSecurityHandlerRevision(doc_)<0 ? "Sin cifrado" : "Documento cifrado"});
    auto permissions=FPDF_GetDocPermissions(doc_);
    values.append(QPair<QString,QString>{"Impresión permitida",permissions & (1UL<<2) ? "Sí" : "No"});
    values.append(QPair<QString,QString>{"Modificación permitida",permissions & (1UL<<3) ? "Sí" : "No"});
    values.append(QPair<QString,QString>{"Copia de texto permitida",permissions & (1UL<<4) ? "Sí" : "No"});
    return values;
}

QVector<PdfDocument::AnnotationEntry> PdfDocument::pageAnnotations(int index) const { std::lock_guard apiGuard(pdfiumMutex);
    if(auto cached=annotationPages_.constFind(index); cached!=annotationPages_.cend()) return cached.value();
    QVector<AnnotationEntry> entries;
        auto page=FPDF_LoadPage(doc_,index); if(!page) return entries;
        for(int i=0;i<FPDFPage_GetAnnotCount(page);++i) {
            auto annotation=FPDFPage_GetAnnot(page,i); if(!annotation) continue;
            auto size=FPDFAnnot_GetStringValue(annotation,"Contents",nullptr,0);
            QVector<unsigned short> text(size/2);
            if(size) FPDFAnnot_GetStringValue(annotation,"Contents",text.data(),size);
            entries.append({index,i,FPDFAnnot_GetSubtype(annotation),size ? QString::fromUtf16(reinterpret_cast<const char16_t*>(text.constData())) : QString()});
            FPDFPage_CloseAnnot(annotation);
        }
        FPDF_ClosePage(page);
    annotationPages_.insert(index,entries); return entries;
}

bool PdfDocument::saveRecovery(const QString& path) const {
    // QByteArray shares immutable storage: retain a consistent revision while disk I/O
    // runs without the global PDFium mutex. Edits detach or replace their buffers.
    QByteArray base,tail;
    {
        std::lock_guard apiGuard(pdfiumMutex);
        if(!loaded()) return false;
        if(historyPosition_>=0) { const auto& entry=history_[historyPosition_]; base=entry.base; tail=entry.tail; }
        else { base=snapshot(); if(base.isEmpty()) return false; }
    }
    QSaveFile file(path); if(!file.open(QIODevice::WriteOnly)) return false;
    if(file.write(base)!=base.size() || file.write(tail)!=tail.size()) return false;
    return file.commit();
}
void PdfDocument::markRecovered() { std::lock_guard apiGuard(pdfiumMutex); savedHistoryId_=0; dirty_=true; }
QVector<PdfDocument::Bookmark> PdfDocument::bookmarks() const { std::lock_guard apiGuard(pdfiumMutex);
    QVector<Bookmark> result; QSet<FPDF_BOOKMARK> seen;
    std::function<void(FPDF_BOOKMARK,int)> visit=[&](FPDF_BOOKMARK parent,int depth) {
        if(depth>64 || result.size()>10000) return;
        for(auto bookmark=FPDFBookmark_GetFirstChild(doc_,parent);bookmark;bookmark=FPDFBookmark_GetNextSibling(doc_,bookmark)) {
            if(seen.contains(bookmark)) break;
            seen.insert(bookmark);
            unsigned long size=FPDFBookmark_GetTitle(bookmark,nullptr,0); QVector<unsigned short> text(size/2+1);
            if(size) FPDFBookmark_GetTitle(bookmark,text.data(),size);
            auto dest=FPDFBookmark_GetDest(doc_,bookmark);
            if(!dest) { auto action=FPDFBookmark_GetAction(bookmark); if(action) dest=FPDFAction_GetDest(doc_,action); }
            result.append(Bookmark{size>=2 ? QString::fromUtf16(reinterpret_cast<const char16_t*>(text.constData()),int(size/2)-1) : QString(),dest ? FPDFDest_GetDestPageIndex(doc_,dest) : -1,depth}); visit(bookmark,depth+1);
        }
    };
    if(doc_) visit(nullptr,0);
    return result;
}

int PdfDocument::drawingAt(QPointF point,double tolerance) const {
    std::lock_guard guard(apiMutex());
    for(int index=annotationCount()-1;index>=0;--index) {
        auto type=annotationType(index); if(type!=FPDF_ANNOT_INK && type!=FPDF_ANNOT_HIGHLIGHT) continue;
        auto copy=copyAnnotation(index);
        if(type==FPDF_ANNOT_HIGHLIGHT) { for(const auto& quad:copy.quads) { QRectF box(QPointF(std::min(quad.x1,quad.x3),std::min(quad.y1,quad.y3)),QPointF(std::max(quad.x2,quad.x4),std::max(quad.y2,quad.y4))); if(box.normalized().adjusted(-tolerance,-tolerance,tolerance,tolerance).contains(point)) return index; } }
        else {
            auto annot=FPDFPage_GetAnnot(page_,index); float horizontal=0,vertical=0,width=2; FPDFAnnot_GetBorder(annot,&horizontal,&vertical,&width); FPDFPage_CloseAnnot(annot);
            for(const auto& stroke:copy.strokes) for(int i=1;i<stroke.size();++i) {
                QPointF a(stroke[i-1].x,stroke[i-1].y),b(stroke[i].x,stroke[i].y),delta=b-a; double squared=QPointF::dotProduct(delta,delta);
                double fraction=squared>0 ? std::clamp(QPointF::dotProduct(point-a,delta)/squared,0.0,1.0) : 0;
                if(QLineF(point,a+fraction*delta).length()<=tolerance+width/2) return index;
            }
        }
    }
    return -1;
}

bool PdfDocument::formatDrawing(int index,const QColor& color,double width) {
    std::lock_guard guard(apiMutex()); int type=annotationType(index);
    if((type!=FPDF_ANNOT_INK && type!=FPDF_ANNOT_HIGHLIGHT) || !canEditAnnotation(index) || !color.isValid() || width<0.5 || width>12) return fail("Seleccioná un trazo o resaltado editable.");
    auto annot=FPDFPage_GetAnnot(page_,index); auto oldColor=annotationColor(index); float horizontal=0,vertical=0,oldWidth=2; FPDFAnnot_GetBorder(annot,&horizontal,&vertical,&oldWidth);
    unsigned long size=FPDFAnnot_GetAP(annot,FPDF_ANNOT_APPEARANCEMODE_NORMAL,nullptr,0); QVector<unsigned short> ap(size/2); if(size) FPDFAnnot_GetAP(annot,FPDF_ANNOT_APPEARANCEMODE_NORMAL,ap.data(),size);
    bool ok=FPDFAnnot_SetAP(annot,FPDF_ANNOT_APPEARANCEMODE_NORMAL,nullptr) && FPDFAnnot_SetColor(annot,FPDFANNOT_COLORTYPE_Color,color.red(),color.green(),color.blue(),color.alpha());
    if(ok && type==FPDF_ANNOT_INK) ok=FPDFAnnot_SetBorder(annot,0,0,float(width));
    if(!ok) { FPDFAnnot_SetColor(annot,FPDFANNOT_COLORTYPE_Color,oldColor.red(),oldColor.green(),oldColor.blue(),oldColor.alpha()); FPDFAnnot_SetBorder(annot,horizontal,vertical,oldWidth); if(size) FPDFAnnot_SetAP(annot,FPDF_ANNOT_APPEARANCEMODE_NORMAL,ap.data()); }
    FPDFPage_CloseAnnot(annot); if(ok) { dirty_=true; recordHistory(); } return ok;
}
