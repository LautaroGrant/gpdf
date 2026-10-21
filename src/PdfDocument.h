#pragma once
#include "Translations.h"
#include <QImage>
#include <QColor>
#include <QHash>
#include <QCache>
#include <memory>
#include <mutex>
#include <fpdf_annot.h>
#include <QPointF>
#include <QRectF>
#include <QString>
#include <QVector>
#include <fpdfview.h>
#include <fpdf_text.h>

// PDFium calls are serialized with apiMutex(); background reads validate revision().
class PdfDocument {
public:
    struct AnnotationEntry { int page; int index; int type; QString text; };
    QVector<AnnotationEntry> documentAnnotations() const;
    qint64 historyStorageBytes() const;
    qint64 searchCacheStorageBytes() const;
    QString pageText(int page) const;
    QVector<QPair<QString,QString>> properties() const;
    int characterCount() const;
    QPair<int,int> wordRange(int index) const;
    QPair<int,int> lineRange(int index) const;
    bool moveTextAnnotation(int index,QPointF topLeft);
    int moveCommentAnnotation(int sourcePage,int index,int destinationPage,QPointF topLeft);
    bool canUndo() const { return historyPosition_>0; }
    bool canRedo() const { return historyPosition_+1<history_.size(); }
    bool undo();
    bool redo();
    struct TextStyle {
        QString family="Arial";
        int size=14;
        QColor color=QColor("#1f2938");
        double width=300;
        double height=0;
    };
    struct AnnotationCopy { int type=-1; int flags=FPDF_ANNOT_FLAG_PRINT; float strokeWidth=2; QString text; QRectF rect; QColor color; TextStyle style; QVector<FS_QUADPOINTSF> quads; QVector<QVector<FS_POINTF>> strokes; };
    AnnotationCopy copyAnnotation(int index) const;
    bool pasteAnnotation(const AnnotationCopy& copy,QPointF topLeft);
    struct SearchMatch {
        int page;
        int first;
        int count;
        QString context;
        QString contextHtml;
        QVector<QRectF> rectangles;
    };
    static std::recursive_mutex& apiMutex();
    quint64 revision() const;
    bool saveRecovery(const QString& path) const;
    void markRecovered();
    QVector<AnnotationEntry> pageAnnotations(int page) const;
    struct Bookmark { QString title; int page; int depth; };
    QVector<Bookmark> bookmarks() const;
    PdfDocument();
    ~PdfDocument();
    PdfDocument(const PdfDocument&) = delete;
    PdfDocument& operator=(const PdfDocument&) = delete;
    bool open(const QString& path, const QByteArray& password = {});
    void closeDocument();
    void swap(PdfDocument& other);
    bool createSample(int pages=1,const QString& sampleText={});
    bool setPage(int index);
    bool saveCopy(const QString& path);
    bool diskChanged(const QString& path) const;
    bool reload(const QString& path) { const auto password=password_; return open(path,password); }
    QImage render(double scale,QSize pixelSize={}) const;
    QImage renderPage(int index, double scale,QSize pixelSize={}) const;
    QSizeF pageSize(int index) const;
    QPointF pageToNormalized(int index,QPointF point) const;
    QPointF normalizedToPage(int index,QPointF point) const;
    int pageCount() const;
    int pageIndex() const { return index_; }
    QSizeF pageSize() const;
    int characterAt(QPointF point) const;
    bool textAt(int pageIndex, QPointF point, double margin=3.0) const;
    QVector<SearchMatch> searchPage(int index, const QString& query) const;
    QVector<QRectF> selectionRects(int first, int last) const;
    QString selectionText(int first, int last) const;
    bool highlight(int first, int last, const QColor& color=QColor(255,210,50,153));
    bool underline(int first,int last,const QColor& color=QColor(40,85,180),double width=1.5);
    bool note(QPointF point, const QString& text);
    bool freeText(QPointF point, const QString& text);
    bool freeText(QPointF point, const QString& text, const TextStyle& style);
    bool ink(const QVector<QPointF>& points, const QColor& color=QColor(40,85,180), double width=2);
    int drawingAt(QPointF point,double tolerance=4) const;
    bool formatDrawing(int index,const QColor& color,double width);
    int annotationCount() const;
    QString annotationContents(int index) const;
    int annotationType(int index) const;
    QRectF annotationRect(int index) const;
    bool canEditAnnotation(int index) const;
    bool canColorAnnotation(int index) const;
    QColor annotationColor(int index) const;
    TextStyle annotationTextStyle(int index) const;
    bool editAnnotation(int index, const QString& text, const TextStyle* style=nullptr);
    bool recolorAnnotation(int index, const QColor& color);
    bool removeAnnotation(int index);
    QString error() const { return error_; }
    bool dirty() const { return dirty_; }
    bool loaded() const { return doc_ != nullptr; }
    void setHiddenPreview(int index) { std::lock_guard guard(apiMutex()); hiddenTextPreview=index; }
    int hiddenTextPreview=-1;
private:
    struct SearchIndex {
        QString normalized,lastQuery; QVector<int> starts,ends; QVector<SearchMatch> matches;
        qsizetype storageBytes() const {
            qsizetype bytes=sizeof(SearchIndex)+(normalized.capacity()+lastQuery.capacity())*sizeof(QChar)+(starts.capacity()+ends.capacity())*sizeof(int)+matches.capacity()*sizeof(SearchMatch);
            for(const auto& match:matches) bytes+=(match.context.capacity()+match.contextHtml.capacity())*sizeof(QChar)+match.rectangles.capacity()*sizeof(QRectF);
            return bytes;
        }
    };
    mutable std::unique_ptr<QCache<int,SearchIndex>> searchIndex_=std::make_unique<QCache<int,SearchIndex>>(16*1024*1024);
    quint64 revision_=0;
    QByteArray snapshot() const;
    void resetHistory();
    void recordHistory();
    bool restoreHistory(int position);
    struct HistoryEntry { QByteArray base,tail; quint64 id=0; QByteArray materialize() const { return tail.isEmpty() ? base : base+tail; } };
    HistoryEntry captureHistory() const;
    QVector<HistoryEntry> history_;
    int historyPosition_=-1;
    quint64 savedHistoryId_=0;
    mutable bool annotationsCached_=false;
    mutable QVector<AnnotationEntry> annotationEntries_;
    mutable QHash<int,QVector<AnnotationEntry>> annotationPages_;
    void closePage();
    void close();
    bool fail(const QString& message);
    bool canAddAnnotations() const;
    bool textMarkup(int first,int last,const QColor& color,int type,double width);
    bool writeFreeText(FPDF_ANNOTATION annot, QPointF point, const QString& value, const TextStyle& style, bool* mutated=nullptr);
    FPDF_DOCUMENT doc_ = nullptr;
    FPDF_PAGE page_ = nullptr;
    FPDF_TEXTPAGE text_ = nullptr;
    mutable FPDF_PAGE hoverPage_ = nullptr;
    mutable FPDF_TEXTPAGE hoverText_ = nullptr;
    mutable int hoverIndex_ = -1;
    mutable std::unique_ptr<QCache<int,QVector<QRectF>>> hoverRegions_=std::make_unique<QCache<int,QVector<QRectF>>>(2*1024*1024);
    QByteArray bytes_;
    QByteArray diskDigest_;
    QByteArray password_;
    int index_ = 0;
    bool dirty_ = false;
    QString error_;
};
