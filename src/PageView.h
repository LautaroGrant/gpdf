#pragma once
#include "UiSupport.h"
#include "CommentPopup.h"
#include <QElapsedTimer>

class PageView : public QWidget {
public:
    enum Tool { Select, Note, Text, Pen, Eraser };
    enum Mode { Continuous, Single, Double };
    enum Fit { Manual, Width, Page, Automatic };
    Fit fit=Manual;
    int rotation=0;
    PdfDocument& pdf;
    Tool tool=Select;
    QColor penColor=QColor(40,85,180),highlightColor=QColor(255,210,50,153),secondaryHighlightColor=QColor(232,136,16,153);
    double penWidth=2;
    QColor underlineColor=QColor(40,85,180);
    double underlineWidth=1.5;
    bool darkMode=false;
    void eraseAt(QPointF position) {
        int page=pageAt(position); if(page<0) return; activate(page);
        int index=pdf.drawingAt(pdfPoint(position),4/effectiveScale);
        if(index>=0 && pdf.removeAnnotation(index)) { resetSelection(); if(changed) changed(); }
    }
    Mode mode=Continuous;
    double scale=1.25;
    double effectiveScale=1.25;
    int first=-1,last=-1,selectedAnnotation=-1;
    bool allTextSelected=false;
    QVector<QPointF> stroke;
    PdfDocument::TextStyle textStyle={"Arial",14,Qt::black};
    int searchPage=-1;
    QVector<QRectF> searchRects;
    QVector<QRectF> pageRects;
    std::function<void()> changed;
    std::function<void(QString)> report;
    std::function<void(int)> activated;
    std::function<void(int)> annotationSelected;
    std::function<void(int)> navigate;
    std::function<void(int)> annotationMoved;
    explicit PageView(PdfDocument& document) : pdf(document),cache(64*1024),previewCache(8*1024) {
        setMouseTracking(true); setFocusPolicy(Qt::StrongFocus); workers.setMaxThreadCount(2);
        dragScroll=new QTimer(this); dragScroll->setInterval(40);
        connect(dragScroll,&QTimer::timeout,this,[this]{
            if(!draggingText || !dragIsComment) { dragScroll->stop(); return; }
            QScrollArea* area=nullptr; for(auto parent=parentWidget();parent;parent=parent->parentWidget()) if((area=qobject_cast<QScrollArea*>(parent))) break;
            if(!area) return;
            auto point=area->viewport()->mapFromGlobal(QCursor::pos());
            int direction=point.y()<35 ? -1 : point.y()>area->viewport()->height()-35 ? 1 : 0;
            if(!direction || point.x()<0 || point.x()>area->viewport()->width()) { dragEdgeTicks=0; return; }
            auto bar=area->verticalScrollBar(); int before=bar->value(); bar->setValue(before+direction*18);
            if(mode!=Continuous && bar->value()==before && ++dragEdgeTicks>=15) { dragEdgeTicks=0; if(navigate) navigate(direction); }
            dragOffset=mapFromGlobal(QCursor::pos())-dragStart; update();
        });
    }
    QThreadPool workers;
    void shutdownWorkers() { workers.clear(); workers.waitForDone(); }
    ~PageView() override { shutdownWorkers(); qApp->removeEventFilter(this); }
    bool finishText() {
        if(!textEditor) return true;
        auto value=textInput->toPlainText();
        if(!value.trimmed().isEmpty()) {
            bool ok=editingText>=0 ? pdf.editAnnotation(editingText,value,&textStyle) : pdf.freeText(textPoint,value,textStyle);
            if(!ok) { if(report) report(pdf.error()); return false; }
            cancelText(); if(changed) changed();
        } else cancelText();
        return true;
    }
    void cancelText() {
        qApp->removeEventFilter(this);
        pdf.setHiddenPreview(-1); editingText=-1; invalidateCache(pdf.pageIndex()); update();
        if(textEditor) { textEditor->hide(); textEditor->deleteLater(); textEditor=nullptr; textInput=nullptr; }
    }
    void beginText(QPointF point,int annotation=-1) {
        hideComment();
        if(!finishText()) return;
        editingText=annotation;
        QString contents;
        if(annotation>=0) {
            if(!pdf.canEditAnnotation(annotation)) { editingText=-1; return; }
            auto rect=pdf.annotationRect(annotation); point={rect.left(),rect.bottom()};
            textStyle=pdf.annotationTextStyle(annotation); textStyle.width=rect.width(); textStyle.height=rect.height();
            contents=pdf.annotationContents(annotation);
            pdf.setHiddenPreview(annotation); invalidateCache(pdf.pageIndex()); update();
        }
        textPoint=point;
        textEditor=new InlineTextBox(this); textEditor->setObjectName("inlineTextEditor");
        textEditor->setStyleSheet("QWidget#inlineTextEditor { background: rgba(128, 128, 128, 30); border: 1px dashed #0078d4; }");
        auto layout=new QVBoxLayout(textEditor); layout->setContentsMargins(4,4,4,4); layout->setSpacing(3); layout->setSizeConstraint(QLayout::SetNoConstraint);
        textInput=new QTextEdit; textInput->setObjectName("inlineTextInput"); textInput->setAcceptRichText(false);
        textInput->setAutoFillBackground(false); textInput->viewport()->setAutoFillBackground(false);
        textInput->setPlaceholderText(gpdfTr("Escribir aquí…")); layout->addWidget(textInput);
        auto controls=new QWidget; auto row=new QHBoxLayout(controls); row->setContentsMargins(0,0,0,0); row->setSpacing(3);
        auto formatScroll=new QScrollArea; formatScroll->setWidget(controls); formatScroll->setWidgetResizable(true);
        formatScroll->setFrameShape(QFrame::NoFrame); formatScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        formatScroll->setFixedHeight(44); layout->addWidget(formatScroll);
        auto colorButton=new QToolButton; colorButton->setText("A ▾"); colorButton->setToolTip(gpdfTr("Color del texto"));
        colorButton->setPopupMode(QToolButton::InstantPopup); auto colors=new QMenu(colorButton);
        const QList<QPair<QString,QColor>> palette={{gpdfTr("Negro"),Qt::black},{gpdfTr("Azul"),QColor("#0078d4")},{gpdfTr("Verde"),QColor("#128012")},{gpdfTr("Rojo"),QColor("#cc0000")},{gpdfTr("Blanco"),Qt::white}};
        for(const auto& entry:palette) {
            QPixmap swatch(16,16); swatch.fill(entry.second);
            auto action=colors->addAction(QIcon(swatch),entry.first);
            connect(action,&QAction::triggered,textEditor,[this,entry]{ textStyle.color=entry.second; previewText(); });
        }
        colorButton->setMenu(colors); row->addWidget(colorButton);
        auto family=new QFontComboBox; family->setObjectName("inlineTextFont"); family->setCurrentFont(QFont(textStyle.family)); family->setMaximumWidth(135);
        row->addWidget(family);
        connect(family,&QFontComboBox::currentFontChanged,textEditor,[this](const QFont& font){ textStyle.family=font.family(); previewText(); });
        auto size=new QSpinBox; size->setObjectName("inlineTextSize"); size->setRange(6,96); size->setValue(textStyle.size); size->setSuffix(" pt");
        row->addWidget(size);
        connect(size,&QSpinBox::valueChanged,textEditor,[this](int value){ textStyle.size=value; previewText(); });
        auto apply=new QToolButton; apply->setText("✓"); apply->setToolTip(gpdfTr("Aplicar texto")); row->addWidget(apply);
        connect(apply,&QToolButton::clicked,textEditor,[this]{ finishText(); });
        auto discard=new QToolButton; discard->setText("×"); discard->setToolTip(gpdfTr("Descartar texto (Esc)")); row->addWidget(discard);
        connect(discard,&QToolButton::clicked,textEditor,[this]{ cancelText(); });
        row->addWidget(new TextResizeGrip(textEditor));
        auto anchor=devicePoint(point); auto page=activeRect();
        int availableWidth=int(page.right()-anchor.x()),availableHeight=int(page.bottom()-anchor.y());
        if(availableWidth<120 || availableHeight<100) { cancelText(); if(report) report(gpdfTr("Elegir un punto más alejado de los bordes derecho e inferior.")); return; }
        textEditor->setMinimumSize(std::min(180,availableWidth),100); textEditor->setMaximumSize(availableWidth,availableHeight);
        textEditor->setGeometry(int(anchor.x()),int(anchor.y()),std::min(availableWidth,std::max(330,int(textStyle.width*effectiveScale))),std::min(availableHeight,std::max(140,int(textStyle.height*effectiveScale)+44)));
        textInput->setPlainText(contents);
        static_cast<InlineTextBox*>(textEditor)->resized=[this]{
            textStyle.width=textEditor->width()/effectiveScale;
            textStyle.height=std::max(0.0,(textEditor->height()-44.0)/effectiveScale);
        };
        textStyle.width=textEditor->width()/effectiveScale;
        qApp->installEventFilter(this);
        previewText(); textEditor->show(); textEditor->raise(); textInput->setFocus();
    }
    void previewText() {
        if(!textInput) return;
        QFont font(textStyle.family); font.setPixelSize(int(std::lround(textStyle.size*effectiveScale))); textInput->setFont(font);
        textInput->setStyleSheet(QString("QTextEdit { color: %1; background: transparent; border: none; }").arg(textStyle.color.name()));
    }
    QJsonObject editorDraft() const {
        if(!textInput) return {};
        return {{"text",textInput->toPlainText()},{"x",textPoint.x()},{"y",textPoint.y()},{"index",editingText},{"family",textStyle.family},{"size",textStyle.size},{"color",textStyle.color.name()},{"width",textEditor->width()/effectiveScale},{"height",std::max(0,textEditor->height()-44)/effectiveScale}};
    }
    void restoreDraft(const QJsonObject& draft) {
        if(draft.isEmpty()) return;
        textStyle={draft["family"].toString("Arial"),draft["size"].toInt(14),QColor(draft["color"].toString("#000000")),draft["width"].toDouble(300),draft["height"].toDouble()};
        beginText({draft["x"].toDouble(),draft["y"].toDouble()},draft["index"].toInt(-1));
        if(textInput) textInput->setPlainText(draft["text"].toString());
    }
    void newDocument() { draggingText=dragCandidate=false; invalidateCache(); resetSelection(); searchPage=-1; searchRects.clear(); sizes.clear(); for(int i=0;i<pdf.pageCount();++i) sizes.append(pdf.pageSize(i)); }
    void refresh() {
        hideComment();
        invalidateCache(pdf.pageIndex(),true);
        rebuild(); update();
    }
    void refreshNavigation() {
        const double previousScale=effectiveScale;
        rebuild();
        if(!qFuzzyCompare(previousScale,effectiveScale)) invalidateCache(-1,true);
        update();
    }
    void setDarkMode(bool value) { darkMode=value; invalidateCache(); previewText(); update(); }
    void setMode(Mode value) { mode=value; resetSelection(); invalidateCache(); rebuild(); update(); }
    void setZoom(double value) { hideComment(); if(qFuzzyCompare(scale,value)) return; scale=value; invalidateCache(-1,true); rebuild(); update(); }
    void setFit(Fit value) { hideComment(); if(fit==value) return; fit=value; invalidateCache(-1,true); rebuild(); update(); }
    void setRotation(int value) { hideComment(); rotation=(value%4+4)%4; invalidateCache(); rebuild(); update(); }
    QSizeF displaySize(int index) const { auto size=sizes[index]; return rotation%2 ? QSizeF(size.height(),size.width()) : size; }
    void setViewportSize(QSize size) {
        bool heightChanged=viewportHeight!=size.height(); viewportHeight=size.height();
        setViewportWidth(size.width());
        if(heightChanged && fit!=Manual) { invalidateCache(-1,true); rebuild(); update(); }
    }
    void setViewportWidth(int width) {
        if(viewportWidth==width) return;
        viewportWidth=width;
        if(mode==Double || fit!=Manual) invalidateCache(-1,true);
        rebuild(); update();
    }
    void rebuild() {
        if(sizes.size()!=pdf.pageCount()) { sizes.clear(); for(int i=0;i<pdf.pageCount();++i) sizes.append(pdf.pageSize(i)); }
        pageRects.fill(QRectF(),pdf.pageCount());
        if(!pdf.loaded()) { setFixedSize(700,850); return; }
        int start=mode==Continuous ? 0 : (mode==Double ? pdf.pageIndex()/2*2 : pdf.pageIndex());
        int end=mode==Continuous ? pdf.pageCount() : std::min(pdf.pageCount(),start+(mode==Double ? 2 : 1));
        effectiveScale=scale;
        double spreadWidth=0,spreadHeight=0;
        for(int i=start;i<end;++i) { auto size=displaySize(i); spreadWidth=mode==Double ? spreadWidth+size.width() : std::max(spreadWidth,size.width()); spreadHeight=std::max(spreadHeight,size.height()); }
        if(mode==Continuous) spreadHeight=displaySize(pdf.pageIndex()).height();
        const double widthScale=std::max(0.01,(viewportWidth-(mode==Double ? 50.0 : 34.0))/spreadWidth);
        const double pageScale=std::min(widthScale,std::max(0.01,(viewportHeight-34.0)/spreadHeight));
        if(fit==Width) effectiveScale=widthScale;
        else if(fit==Page || fit==Automatic) effectiveScale=(fit==Automatic && mode==Continuous) ? widthScale : pageScale;
        if(mode==Double) {
            double width=displaySize(start).width()+(start+1<end ? displaySize(start+1).width() : 0);
            // Leave two pixels for rounding each page's rendered dimensions.
            if(fit==Manual) effectiveScale=std::min(scale,std::max(0.1,(viewportWidth-50.0)/width));
        }
        double maxWidth=0;
        for(int i=start;i<end;++i) maxWidth=std::max(maxWidth,displaySize(i).width()*effectiveScale);
        double x=16,y=16,totalWidth=mode==Double ? 16 : maxWidth+32,maxHeight=0;
        for(int i=start;i<end;++i) {
            QSize size=(displaySize(i)*effectiveScale).toSize();
            if(mode==Double) {
                pageRects[i]=QRectF(QPointF(x,16),size); x+=size.width()+16;
                maxHeight=std::max(maxHeight,double(size.height()));
            } else {
                pageRects[i]=QRectF(QPointF(16+(maxWidth-size.width())/2,y),size); y+=size.height()+20;
            }
        }
        if(mode==Double) { totalWidth=x; y=maxHeight+32; }
        setFixedSize(int(std::ceil(totalWidth)),int(std::ceil(y)));
    }
    bool commentVisible() const { return commentPopup && commentPopup->isVisible(); }
    bool commentContains(QWidget* widget) const { return commentVisible() && (widget==commentPopup || commentPopup->isAncestorOf(widget)); }
    void hideComment() { if(commentPopup) commentPopup->hide(); }
    void showComment(int index) {
        if(index<0 || pdf.annotationType(index)!=FPDF_ANNOT_TEXT) { hideComment(); return; }
        if(!commentPopup) commentPopup=new CommentPopup(this);
        auto available=rect();
        if(auto viewport=parentWidget()) available=QRect(mapFrom(viewport,QPoint()),viewport->size()).intersected(rect());
        available=available.intersected(activeRect().toAlignedRect());
        commentPopup->showContents(pdf.annotationContents(index),deviceRect(pdf.annotationRect(index)).toAlignedRect(),available);
    }
    bool annotationDragging() const { return draggingText; }
    void cancelAnnotationDrag() { draggingText=dragCandidate=false; dragScroll->stop(); dragOffset={}; unsetCursor(); update(); }
    void resetSelection(bool preserveDrag=false) { if(!preserveDrag) cancelAnnotationDrag(); hideComment(); first=last=selectedAnnotation=-1; allTextSelected=false; stroke.clear(); }
    int pageAt(QPointF point) const { for(int i=0;i<pageRects.size();++i) if(!pageRects[i].isEmpty() && pageRects[i].contains(point)) return i; return -1; }
    QRectF activeRect() const { return pdf.pageIndex()<pageRects.size() ? pageRects[pdf.pageIndex()] : QRectF(); }
    QPointF pdfPoint(QPointF point) const {
        return pdfPointOnPage(pdf.pageIndex(),point);
    }
    QPointF pdfPointOnPage(int index,QPointF point) const {
        auto r=pageRects[index];
        double x=(point.x()-r.left())/r.width(),y=(point.y()-r.top())/r.height();
        QPointF original=rotation==1 ? QPointF(y,1-x) : rotation==2 ? QPointF(1-x,1-y) : rotation==3 ? QPointF(1-y,x) : QPointF(x,y);
        return pdf.normalizedToPage(index,{std::clamp(original.x(),0.0,1.0),std::clamp(original.y(),0.0,1.0)});
    }
    QPointF devicePoint(QPointF point) const {
        auto r=activeRect(); auto normalized=pdf.pageToNormalized(pdf.pageIndex(),point); double x=normalized.x(),y=normalized.y();
        QPointF display=rotation==1 ? QPointF(1-y,x) : rotation==2 ? QPointF(1-x,1-y) : rotation==3 ? QPointF(y,1-x) : QPointF(x,y);
        return r.topLeft()+QPointF(display.x()*r.width(),display.y()*r.height());
    }
    QRectF deviceRect(QRectF r) const { return QRectF(devicePoint(r.topLeft()),devicePoint(r.bottomRight())).normalized(); }
    void activate(int index) {
        if(index<0 || index==pdf.pageIndex()) return;
        if(!finishText()) return;
        if(pdf.setPage(index)) { resetSelection(true); refreshNavigation(); if(activated) activated(index); }
    }
protected:
    bool eventFilter(QObject* target,QEvent* event) override {
        if(textInput && target==textInput && event->type()==QEvent::KeyPress) {
            auto key=static_cast<QKeyEvent*>(event);
            if((key->key()==Qt::Key_Return || key->key()==Qt::Key_Enter) && !key->modifiers().testFlag(Qt::ShiftModifier)) { finishText(); return true; }
        }
        if(textEditor && !finishingOutside && event->type()==QEvent::MouseButtonPress) {
            auto widget=qobject_cast<QWidget*>(target);
            bool inside=false;
            for(auto parent=target;parent;parent=parent->parent()) if(parent==textEditor) { inside=true; break; }
            if(widget && !inside && widget->window()==window()) {
                finishingOutside=true;
                bool ok=finishText();
                finishingOutside=false;
                if(!ok || (widget==this && static_cast<QMouseEvent*>(event)->button()==Qt::LeftButton)) return true;
            }
        }
        return QWidget::eventFilter(target,event);
    }
    void paintEvent(QPaintEvent* event) override {
        QPainter painter(this); painter.setRenderHint(QPainter::SmoothPixmapTransform);
        painter.fillRect(event->rect(),darkMode ? QColor("#20252d") : QColor("#e9edf2"));
        if(!pdf.loaded()) { painter.setPen(QColor("#6b7280")); painter.drawText(rect(),Qt::AlignCenter,gpdfTr("Hacer clic aquí para abrir un PDF")); return; }
        // Painting never waits for a background PDFium operation. Keep cached pages visible.
        std::unique_lock apiLock(PdfDocument::apiMutex(),std::try_to_lock);
        if(!apiLock.owns_lock() && !renderRetryScheduled) {
            renderRetryScheduled=true; QTimer::singleShot(16,this,[this]{ renderRetryScheduled=false; update(); });
        }
        QRect visible=visibleRegion().boundingRect(); int firstVisible=-1,lastVisible=-1;
        for(int page=0;page<pageRects.size();++page) if(pageRects[page].intersects(visible)) { if(firstVisible<0) firstVisible=page; lastVisible=page; }
        visibleFirst=firstVisible; visibleLast=lastVisible;
        for(int i=0;i<pageRects.size();++i) {
            auto r=pageRects[i];
            if(r.isEmpty() || !r.intersects(event->rect())) continue;
            painter.fillRect(r.translated(2,3),QColor("#d0d5dd")); painter.fillRect(r,Qt::white);
            auto image=cache.object(i);
            if(apiLock.owns_lock() && (!image || stalePages.contains(i)) && !pendingRenders.contains(i)) {
                pendingRenders.insert(i); int generation=cacheGeneration,turn=rotation;
                int visibleCount=std::max(1,lastVisible-firstVisible+1);
                double pixels=std::min(12.0*1024*1024,cache.maxCost()*1024.0*0.75/(4*visibleCount));
                auto dimensions=sizes.value(i); double area=dimensions.width()*dimensions.height();
                double zoom=area>0 ? std::min(effectiveScale,std::sqrt(pixels/area)) : effectiveScale;
                quint64 revision=pdf.revision(); QPointer<PageView> target(this);
                workers.start([target,i,generation,turn,zoom,revision]{
                    QImage rendered;
                    { std::lock_guard guard(PdfDocument::apiMutex()); if(target && target->cacheGeneration==generation && i>=target->visibleFirst && i<=target->visibleLast && target->pdf.revision()==revision) rendered=target->pdf.renderPage(i,zoom); }
                    if(turn && !rendered.isNull()) rendered=rendered.transformed(QTransform().rotate(turn*90));
                    if(target) QMetaObject::invokeMethod(target.data(),[target,i,generation,rendered]{
                        if(!target || target->cacheGeneration!=generation) return;
                        target->pendingRenders.remove(i);
                        int cost=std::max(1,int(rendered.sizeInBytes()/1024));
                        if(!rendered.isNull()) {
                            QImage preview=rendered.scaled(QSize(512,512),Qt::KeepAspectRatio,Qt::SmoothTransformation);
                            target->previewCache.insert(i,new QImage(preview),std::max(1,int(preview.sizeInBytes()/1024)));
                            if(cost<=target->cache.maxCost()) { target->cache.insert(i,new QImage(rendered),cost); target->stalePages.remove(i); }
                        }
                        target->update();
                    },Qt::QueuedConnection);
                });
            }
            if(!image) image=previewCache.object(i);
            if(image) painter.drawImage(r,*image);
        }
        if(!apiLock.owns_lock()) return;
        painter.setPen(Qt::NoPen); painter.setBrush(QColor(65,125,220,65));
        if(allTextSelected) for(const auto& rect:pageRects) if(!rect.isEmpty()) painter.drawRect(rect);
        painter.setBrush(QColor(65,125,220,65));
        if(first>=0 && last>=0) for(auto r:pdf.selectionRects(first,last)) painter.drawRect(deviceRect(r));
        if(searchPage==pdf.pageIndex() && !allTextSelected && !(first>=0 && last>=0)) {
            painter.setBrush(QColor(255,180,30,110)); painter.setPen(QPen(QColor("#d97706"),1));
            for(auto r:searchRects) painter.drawRect(deviceRect(r).adjusted(-1,-1,1,1));
        }
        if(draggingText) {
            painter.setBrush(Qt::NoBrush); painter.setPen(QPen(QColor("#2563eb"),1.5,Qt::DashLine));
            painter.drawRect(dragOutline.translated(dragOffset).adjusted(-3,-3,3,3));
        } else if(selectedAnnotation>=0) {
            painter.setBrush(Qt::NoBrush); painter.setPen(QPen(QColor("#2563eb"),1.5,Qt::DashLine));
            auto outline=deviceRect(pdf.annotationRect(selectedAnnotation));
            if(draggingText) outline.translate(dragOffset);
            painter.drawRect(outline.adjusted(-3,-3,3,3));
        }
        painter.setPen(QPen(penColor,penWidth*effectiveScale,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));
        for(int i=1;i<stroke.size();++i) painter.drawLine(devicePoint(stroke[i-1]),devicePoint(stroke[i]));
    }
    void mousePressEvent(QMouseEvent* event) override {
        if(event->button()!=Qt::LeftButton || !pdf.loaded()) return;
        setFocus();
        int index=pageAt(event->position()); if(index<0) return;
        activate(index);
        const auto p=pdfPoint(event->position());
        if(tool==Select && wordClickTimer.isValid() && wordClickTimer.elapsed()<=QApplication::doubleClickInterval() && wordClickRevision==pdf.revision() && wordClickPage==pdf.pageIndex() && (event->position()-wordClickPosition).manhattanLength()<=QApplication::startDragDistance()) {
            wordClickTimer.invalidate();
            auto range=pdf.lineRange(pdf.characterAt(p));
            resetSelection(); allTextSelected=false; first=range.first; last=range.second; searchPage=-1; searchRects.clear();
            if(annotationSelected) annotationSelected(-1);
            update(); event->accept(); return;
        }
        wordClickTimer.invalidate();
        allTextSelected=false;
        int hit=(tool==Select || tool==Note) ? annotationAt(p) : textAnnotationAt(p);
        if(tool==Note && hit>=0 && pdf.annotationType(hit)==FPDF_ANNOT_TEXT) {
            first=last=-1; selectedAnnotation=hit; if(annotationSelected) annotationSelected(hit);
            showComment(hit); update(); return;
        }
        if(tool==Eraser) { eraseAt(event->position()); return; }
        if(tool==Select) { searchPage=-1; searchRects.clear(); selectedAnnotation=hit; if(annotationSelected) annotationSelected(hit); first=last=hit>=0 ? -1 : pdf.characterAt(p); dragStart=event->position(); dragOffset={}; draggingText=false; dragCandidate=hit>=0 && (pdf.annotationType(hit)==FPDF_ANNOT_FREETEXT || pdf.annotationType(hit)==FPDF_ANNOT_TEXT) && pdf.canEditAnnotation(hit); if(dragCandidate) { dragPage=pdf.pageIndex(); dragIndex=hit; dragRevision=pdf.revision(); dragIsComment=pdf.annotationType(hit)==FPDF_ANNOT_TEXT; auto rect=pdf.annotationRect(hit); dragAnchor=p-QPointF(rect.left(),rect.bottom()); dragOutline=deviceRect(rect); } showComment(hit); update(); }
        else if(tool==Pen) { stroke={p}; update(); }
        else if(tool==Text) {
            beginText(p,hit);
        } else {
            bool accepted;
            const QString value=studyMultilineText(this,tool==Note ? gpdfTr("Comentario") : gpdfTr("Escribir en la página"),{},&accepted);
            if(accepted && !value.trimmed().isEmpty()) {
                if(!(tool==Note ? pdf.note(p,value) : pdf.freeText(p,value))) report(pdf.error());
                else { changed(); if(tool==Note) showComment(pdf.annotationCount()-1); }
            }
        }
    }
    void mouseMoveEvent(QMouseEvent* event) override {
        if(!(event->buttons() & Qt::LeftButton)) {
            int hoveredPage=pageAt(event->position());
            if(hoveredPage<0) { setCursor(Qt::ArrowCursor); return; }
            if(tool==Eraser || tool==Pen || tool==Note) { setCursor(Qt::CrossCursor); return; }
            if(tool==Text) { setCursor(Qt::IBeamCursor); return; }
            std::unique_lock apiLock(PdfDocument::apiMutex(),std::try_to_lock); if(!apiLock.owns_lock()) return;
            int index=pageAt(event->position());
            bool text=false;
            if(tool==Select && index>=0) {
                QPointF point=pdfPointOnPage(index,event->position());
                text=pdf.textAt(index,point,4.0/effectiveScale);
            }
            setCursor(text ? Qt::IBeamCursor : Qt::ArrowCursor);
            return;
        }
        if(!(event->buttons() & Qt::LeftButton) || !pdf.loaded()) return;
        if(tool==Eraser) { eraseAt(event->position()); return; }
        if(tool==Select && dragCandidate) {
            dragOffset=event->position()-dragStart;
            if(dragOffset.manhattanLength()>=QApplication::startDragDistance()) draggingText=true;
            if(draggingText) { hideComment(); if(dragIsComment) dragScroll->start(); setCursor(Qt::SizeAllCursor); update(); } return;
        }
        auto p=pdfPoint(event->position());
        if(tool==Select && first>=0) { int index=pdf.characterAt(p); if(index>=0) last=index; }
        if(tool==Pen && !stroke.isEmpty()) stroke.append(p);
        update();
    }
    void leaveEvent(QEvent*) override { unsetCursor(); }
    void mouseReleaseEvent(QMouseEvent* event) override {
        if(event->button()!=Qt::LeftButton) return;
        if(draggingText && dragIsComment && pdf.revision()==dragRevision) {
            int destination=pageAt(event->position());
            if(destination>=0) {
                int moved=pdf.moveCommentAnnotation(dragPage,dragIndex,destination,pdfPointOnPage(destination,event->position())-dragAnchor);
                if(moved<0) { if(report) report(pdf.error()); }
                else { selectedAnnotation=moved; draggingText=dragCandidate=false; invalidateCache(); if(annotationMoved) annotationMoved(moved); }
            }
        } else if(draggingText && selectedAnnotation>=0 && pdf.revision()==dragRevision) {
            int index=selectedAnnotation; auto rect=pdf.annotationRect(index);
            QPointF delta=pdfPoint(dragStart+dragOffset)-pdfPoint(dragStart);
            QPointF target(rect.left()+delta.x(),rect.bottom()+delta.y());
            if(!pdf.moveTextAnnotation(index,target)) report(pdf.error());
            else if(annotationMoved) annotationMoved(index);
        }
        draggingText=dragCandidate=false; dragOffset={};
        dragScroll->stop(); dragEdgeTicks=0;
        setCursor(tool==Pen || tool==Note || tool==Eraser ? Qt::CrossCursor : tool==Text ? Qt::IBeamCursor : Qt::ArrowCursor);
        if(tool==Pen && stroke.size()>1) { if(!pdf.ink(stroke,penColor,penWidth)) report(pdf.error()); else changed(); }
        stroke.clear(); update();
    }
    void mouseDoubleClickEvent(QMouseEvent* event) override {
        if(event->button()!=Qt::LeftButton || tool!=Select || !pdf.loaded()) return;
        int page=pageAt(event->position()); if(page<0) return; activate(page);
        auto p=pdfPoint(event->position()); int hit=annotationAt(p);
        if(hit>=0 && pdf.annotationType(hit)==FPDF_ANNOT_TEXT) showComment(hit);
        else if(hit>=0 && pdf.annotationType(hit)==FPDF_ANNOT_FREETEXT) beginText(p,hit);
        else {
            auto range=pdf.wordRange(pdf.characterAt(p));
            resetSelection(); allTextSelected=false; first=range.first; last=range.second; searchPage=-1; searchRects.clear();
            if(annotationSelected) annotationSelected(-1);
            if(first>=0) { wordClickTimer.start(); wordClickPosition=event->position(); wordClickPage=pdf.pageIndex(); wordClickRevision=pdf.revision(); }
            setFocus(); update(); event->accept();
        }
    }
    void keyPressEvent(QKeyEvent* event) override {
        QWidget::keyPressEvent(event);
    }
private:
    QElapsedTimer wordClickTimer;
    QPointF wordClickPosition;
    int wordClickPage=-1;
    quint64 wordClickRevision=0;
    CommentPopup* commentPopup=nullptr;
    bool draggingText=false,dragCandidate=false;
    bool dragIsComment=false;
    QTimer* dragScroll=nullptr;
    int dragEdgeTicks=0;
    int dragPage=-1,dragIndex=-1;
    quint64 dragRevision=0;
    QPointF dragAnchor;
    QRectF dragOutline;
    QPointF dragStart,dragOffset;
    int annotationAt(QPointF point) const {
        int drawing=pdf.drawingAt(point,4/effectiveScale);
        for(int i=pdf.annotationCount()-1;i>=0;--i) {
            auto type=pdf.annotationType(i);
            if((type==FPDF_ANNOT_FREETEXT || type==FPDF_ANNOT_TEXT) && pdf.annotationRect(i).adjusted(-2/effectiveScale,-2/effectiveScale,2/effectiveScale,2/effectiveScale).contains(point)) return std::max(i,drawing);
        }
        return drawing;
    }
    int textAnnotationAt(QPointF point) const {
        for(int i=pdf.annotationCount()-1;i>=0;--i) if(pdf.annotationType(i)==FPDF_ANNOT_FREETEXT && pdf.annotationRect(i).contains(point)) return i;
        return -1;
    }
    int editingText=-1;
    bool finishingOutside=false;
    QWidget* textEditor=nullptr;
    QTextEdit* textInput=nullptr;
    QPointF textPoint;
    QVector<QSizeF> sizes;
    QCache<int,QImage> cache;
    QCache<int,QImage> previewCache;
    std::atomic<int> cacheGeneration{0};
    std::atomic<int> visibleFirst{0},visibleLast{0};
    QSet<int> pendingRenders;
    QSet<int> stalePages;
    bool renderRetryScheduled=false;
    void invalidateCache(int page=-1,bool preservePreview=false) {
        ++cacheGeneration; pendingRenders.clear();
        if(preservePreview) { if(page>=0) { if(cache.object(page) || previewCache.object(page)) stalePages.insert(page); } else { for(int key:cache.keys()) stalePages.insert(key); for(int key:previewCache.keys()) stalePages.insert(key); } }
        else if(page<0) { cache.clear(); previewCache.clear(); stalePages.clear(); }
        else { cache.remove(page); previewCache.remove(page); stalePages.remove(page); }
    }
    int viewportWidth=820;
    int viewportHeight=780;
};
