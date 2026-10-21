#pragma once
#include "PageView.h"
#include "AnnotationExport.h"
#include "SessionStore.h"
#include "NativeScrollCursor.h"
#include "UpdateManager.h"

class Window : public QMainWindow {
public:
    Window() {
        installUiTranslations();
        setWindowTitle("GPDF"); resize(1280,850);
        view = new PageView(pdf);
        scroll = new QScrollArea;
        scroll->setWidget(view); scroll->setAlignment(Qt::AlignHCenter);
        scroll->setStyleSheet("QScrollArea { background: #e9edf2; border: 0; }");
        annotations = new QListWidget; annotations->setObjectName("annotationList");
        annotations->setMinimumWidth(220); annotations->setWordWrap(true);
        annotations->setItemDelegate(new SearchResultDelegate(annotations,Qt::UserRole+2));
        annotations->setToolTip(gpdfTr("Seleccionar una anotación para editarla, cambiar su color o borrarla. Doble clic para editar el comentario."));
        auto panel=new QWidget;
        panel->setObjectName("annotationsPanel");
        auto panelLayout=new QVBoxLayout(panel);
        panelLayout->setContentsMargins(8,8,8,8);
        panelLayout->addWidget(new QLabel(gpdfTr("Anotaciones")));
        auto filterRow=new QHBoxLayout; filterRow->addWidget(new QLabel(gpdfTr("Filtro:")));
        annotationTypeFilter=new ViewModeCombo; annotationTypeFilter->setObjectName("annotationTypeFilter");
        annotationTypeFilter->addItems({gpdfTr("Predeterminado"),gpdfTr("Todas las anotaciones"),gpdfTr("Comentarios"),gpdfTr("Textos"),gpdfTr("Lápiz"),gpdfTr("Resaltado"),gpdfTr("Subrayado")});
        filterRow->addWidget(annotationTypeFilter,1); panelLayout->addLayout(filterRow);
        connect(annotationTypeFilter,&QComboBox::currentIndexChanged,[this]{ refreshPanel(); });
        annotationScope=new ViewModeCombo; annotationScope->setObjectName("annotationScope");
        annotationScope->addItems({gpdfTr("Todo el documento"),gpdfTr("Página actual")}); panelLayout->addWidget(annotationScope);
        connect(annotationScope,&QComboBox::currentIndexChanged,[this]{ refreshPanel(); });
        annotationSearch=new QLineEdit; annotationSearch->setObjectName("annotationSearch"); annotationSearch->setPlaceholderText(gpdfTr("Buscar comentarios y textos…")); annotationSearch->setClearButtonEnabled(true); panelLayout->addWidget(annotationSearch);
        auto exportButton=new QPushButton(gpdfTr("Exportar anotaciones…")); exportButton->setObjectName("exportFromAnnotations");
        connect(exportButton,&QPushButton::clicked,this,[this]{ if(auto action=findChild<QAction*>("exportAnnotationsAction")) action->trigger(); });
        connect(annotationSearch,&QLineEdit::textChanged,this,[this]{ refreshPanel(); });
        panelLayout->addWidget(annotations);
        auto buttons=new QHBoxLayout;
        editButton=new QPushButton(gpdfTr("Editar"));
        colorButton=new QPushButton(gpdfTr("Color"));
        deleteButton=new QPushButton(gpdfTr("Borrar"));
        buttons->addWidget(editButton); buttons->addWidget(colorButton); buttons->addWidget(deleteButton);
        panelLayout->addLayout(buttons);
        panelLayout->addWidget(exportButton);
        connect(editButton,&QPushButton::clicked,[this]{ editSelected(); });
        connect(colorButton,&QPushButton::clicked,[this]{ colorSelected(); });
        connect(deleteButton,&QPushButton::clicked,[this]{ deleteSelected(); });
        auto splitter = new QSplitter;
        navigationPanel=new QTabWidget; navigationPanel->setObjectName("navigationPanel"); navigationPanel->setMinimumWidth(240); navigationPanel->setMaximumWidth(280);
        thumbnails=new QListWidget; thumbnails->setObjectName("thumbnails"); thumbnails->setViewMode(QListView::IconMode); thumbnails->setFlow(QListView::TopToBottom); thumbnails->setWrapping(false); thumbnails->setIconSize(QSize(140,190)); thumbnails->setGridSize(QSize(160,220)); thumbnails->setResizeMode(QListView::Adjust); thumbnails->setMovement(QListView::Static); thumbnails->setDragDropMode(QAbstractItemView::NoDragDrop); thumbnails->setDragEnabled(false); thumbnails->setAcceptDrops(false);
        outline=new QTreeWidget; outline->setObjectName("documentBookmarks"); outline->setHeaderHidden(true);
        navigationPanel->addTab(thumbnails,gpdfTr("Miniaturas")); navigationPanel->addTab(outline,gpdfTr("Marcadores"));
        auto personalPanel=new QWidget; personalPanel->setObjectName("personalBookmarksPanel"); auto personalLayout=new QVBoxLayout(personalPanel); personalBookmarks=new QListWidget; personalBookmarks->setObjectName("personalBookmarks"); personalBookmarks->setWordWrap(true); personalLayout->addWidget(personalBookmarks);
        auto markButtons=new QHBoxLayout; auto addMark=new QPushButton(gpdfTr("Añadir")); addMark->setObjectName("addPersonalBookmarkButton"); auto renameMark=new QPushButton(gpdfTr("Renombrar")); renameMark->setObjectName("renamePersonalBookmarkButton"); auto deleteMark=new QPushButton(gpdfTr("Borrar")); deleteMark->setObjectName("deletePersonalBookmarkButton"); markButtons->addWidget(addMark); markButtons->addWidget(renameMark); markButtons->addWidget(deleteMark); personalLayout->addLayout(markButtons);
        navigationPanel->addTab(personalPanel,gpdfTr("Personales")); splitter->addWidget(navigationPanel); navigationPanel->hide();
        connect(personalBookmarks,&QListWidget::itemClicked,this,[this](QListWidgetItem* item){ personalBookmarks->setCurrentItem(item); jumpTo(item->data(Qt::UserRole).toInt()); });
        connect(addMark,&QPushButton::clicked,this,[this]{ promptPersonalBookmark(); });
        connect(renameMark,&QPushButton::clicked,this,[this]{ auto item=personalBookmarks->currentItem(); if(!item) return; QString id=item->data(Qt::UserRole+1).toString(),title=item->data(Qt::UserRole+2).toString(); bool ok=false; auto text=QInputDialog::getText(this,gpdfTr("Renombrar marcador"),gpdfTr("Nombre:"),QLineEdit::Normal,title,&ok); if(ok) renamePersonalBookmark(id,text); });
        connect(deleteMark,&QPushButton::clicked,this,[this]{ if(auto item=personalBookmarks->currentItem()) removePersonalBookmark(item->data(Qt::UserRole+1).toString()); });
        connect(thumbnails,&QListWidget::itemClicked,[this](QListWidgetItem* item){ jumpTo(thumbnails->row(item)); });
        connect(thumbnails,&QListWidget::currentRowChanged,this,[this](int row){ if(row>=0 && row!=pdf.pageIndex()) jumpTo(row); });
        connect(outline,&QTreeWidget::itemClicked,[this](QTreeWidgetItem* item,int){ if(item->data(0,Qt::UserRole).isValid()) jumpTo(item->data(0,Qt::UserRole).toInt()); });
        connect(thumbnails->verticalScrollBar(),&QScrollBar::valueChanged,[this]{ requestThumbnails(); });
        connect(navigationPanel,&QTabWidget::currentChanged,[this]{ requestThumbnails(); syncViewActions(); });
        searchPanel=new QWidget; searchPanel->setObjectName("searchPanel");
        searchPanel->setMinimumWidth(240);
        auto searchLayout=new QVBoxLayout(searchPanel); searchLayout->setContentsMargins(8,8,8,8);
        auto searchHeader=new QHBoxLayout;
        searchHeader->addWidget(new QLabel(gpdfTr("Buscar en el documento"))); searchHeader->addStretch();
        searchLayout->addLayout(searchHeader);
        searchInput=new QLineEdit; searchInput->setObjectName("searchInput"); searchInput->setPlaceholderText(gpdfTr("Texto a buscar…")); searchInput->setClearButtonEnabled(true);
        searchLayout->addWidget(searchInput);
        auto searchButton=new QPushButton(gpdfTr("Buscar")); searchButton->setObjectName("searchButton"); searchLayout->addWidget(searchButton);
        searchStatus=new QLabel(gpdfTr("Ingresar texto para buscar.")); searchStatus->setObjectName("searchStatus"); searchStatus->setWordWrap(true); searchLayout->addWidget(searchStatus);
        searchResults=new QListWidget; searchResults->setObjectName("searchResults"); searchResults->setWordWrap(true); searchResults->setLayoutMode(QListView::Batched); searchResults->setBatchSize(64); searchResults->setItemDelegate(new SearchResultDelegate(searchResults)); searchLayout->addWidget(searchResults);
        searchTask=new QTimer(this); searchTask->setObjectName("searchTask"); searchTask->setSingleShot(true); searchTask->setInterval(0);
        searchDebounce=new QTimer(this); searchDebounce->setObjectName("searchDebounce"); searchDebounce->setSingleShot(true); searchDebounce->setInterval(600);
        connect(searchTask,&QTimer::timeout,[this]{ searchNextPage(); });
        connect(searchDebounce,&QTimer::timeout,[this]{ startSearch(); });
        connect(searchButton,&QPushButton::clicked,[this]{ startSearch(); });
        connect(searchInput,&QLineEdit::returnPressed,[this]{ auto action=findChild<QAction*>("searchEnterAction"); if(action && action->shortcut()==QKeySequence(Qt::Key_Return)) searchEnter(); });
        connect(searchInput,&QLineEdit::textChanged,[this]{ ++searchGeneration; searchBusy=false; searchTask->stop(); pendingSearchNext=false; lastSearchRow=-1; searchDebounce->start(); });
        connect(searchResults,&QListWidget::currentRowChanged,[this](int row){ showSearchResult(row); });
        connect(searchResults,&QListWidget::itemClicked,[this](QListWidgetItem* item){ showSearchResult(searchResults->row(item)); });
        splitter->addWidget(searchPanel); splitter->addWidget(scroll); splitter->addWidget(panel); splitter->setSizes({200,260,820,300});
        searchPanel->hide();
        setCentralWidget(splitter);
        documentToolbar=addToolBar(gpdfTr("Documentos")); documentToolbar->setObjectName("documentToolbar"); documentToolbar->setMovable(false); documentToolbar->setFloatable(false); documentToolbar->setContextMenuPolicy(Qt::PreventContextMenu);
        documentTabs=new DocumentTabBar; documentTabs->setObjectName("documentTabs"); documentTabs->setTabsClosable(true); documentTabs->setElideMode(Qt::ElideRight); documentTabs->setUsesScrollButtons(true); documentTabs->setExpanding(false); documentTabs->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Preferred);
        documentToolbar->addWidget(documentTabs); documentToolbar->hide(); addToolBarBreak();
        connect(documentTabs,&QTabBar::currentChanged,[this](int index){ if(!changingTabs && index>=0) switchDocument(index); });
        connect(documentTabs,&QTabBar::tabCloseRequested,[this](int index){ if(switchDocument(index)) closeDocument(); });
        mainToolbar=addToolBar(gpdfTr("Herramientas")); auto toolbar=mainToolbar; toolbar->setObjectName("mainToolbar"); toolbar->setMovable(false);
        toolbar->setContextMenuPolicy(Qt::PreventContextMenu);
        auto host=new QWidget; host->setObjectName("toolbarHost");
        host->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Preferred);
        auto toolbarLayout=new QGridLayout(host); toolbarLayout->setContentsMargins(0,0,0,0); toolbarLayout->setSpacing(4);
        auto left=new QWidget; left->setObjectName("toolbarLeft"); auto leftLayout=new QHBoxLayout(left); leftLayout->setContentsMargins(0,0,0,0);
        auto bar=new QToolBar; bar->setObjectName("centerTools"); bar->setMovable(false);
        toolbarLayout->addWidget(left,0,0); toolbarLayout->addWidget(bar,0,1,Qt::AlignCenter); toolbarLayout->setColumnStretch(1,1);
        auto right=new QWidget; right->setObjectName("toolbarRight"); auto rightLayout=new QHBoxLayout(right); rightLayout->setContentsMargins(0,0,0,0);
        toolbarLayout->addWidget(right,0,2); toolbar->addWidget(host);
        auto action = [bar](QString title, auto handler){ auto a=bar->addAction(title); QObject::connect(a,&QAction::triggered,handler); return a; };
        auto separator=[](const QString& name){
            auto line=new QFrame; line->setObjectName(name);
            line->setFrameShape(QFrame::VLine); line->setFrameShadow(QFrame::Plain);
            line->setFixedHeight(22); line->setStyleSheet("color: #b8bec6;"); return line;
        };
        viewMode=new ViewModeCombo; viewMode->setObjectName("viewMode");
        viewMode->addItems({gpdfTr("Vista continua"),gpdfTr("Página individual"),gpdfTr("Dos páginas")});
        viewMode->setToolTip(gpdfTr("Vista del documento")); bar->addWidget(viewMode);
        connect(viewMode,&QComboBox::currentIndexChanged,[this](int index){ changeMode(PageView::Mode(index)); });
        bar->addSeparator();
        action("‹",[this]{ page(-1); });
        auto pageIndicator=new QWidget; auto pageLayout=new QHBoxLayout(pageIndicator); pageLayout->setContentsMargins(0,0,0,0); pageLayout->setSpacing(3);
        pageNumber=new QSpinBox; pageNumber->setObjectName("pageNumber"); pageNumber->setRange(1,1); pageNumber->setButtonSymbols(QAbstractSpinBox::NoButtons); pageNumber->setAlignment(Qt::AlignCenter); pageNumber->setFixedWidth(54); pageNumber->setKeyboardTracking(false); pageNumber->setToolTip(gpdfTr("Página actual: escribir un número y presionar Enter para ir a esa página.")); pageNumber->setAccessibleName(gpdfTr("Página actual"));
        pageLabel=new QLabel("/ 0"); pageLayout->addWidget(pageNumber); pageLayout->addWidget(pageLabel); bar->addWidget(pageIndicator);
        connect(pageNumber,&QSpinBox::valueChanged,[this](int value){ jumpTo(value-1); QSignalBlocker blocker(pageNumber); pageNumber->setValue(pdf.loaded() ? pdf.pageIndex()+1 : 1); });
        action("›",[this]{ page(1); });
        bar->addSeparator();
        action("−",[this]{ zoom(-0.10); });
        zoomPercent=new QSpinBox; zoomPercent->setObjectName("zoomPercent");
        zoomPercent->setRange(1,10000); zoomPercent->setSuffix("%"); zoomPercent->setSingleStep(10);
        zoomPercent->setButtonSymbols(QAbstractSpinBox::NoButtons);
        zoomPercent->setAlignment(Qt::AlignCenter); zoomPercent->setFixedWidth(72);
        zoomPercent->setKeyboardTracking(false); zoomPercent->setValue(int(std::lround(view->scale*100)));
        zoomPercent->setToolTip(gpdfTr("Zoom del documento (50%–300%). En dos páginas se ajusta al ancho disponible."));
        bar->addWidget(zoomPercent);
        connect(zoomPercent,&QSpinBox::valueChanged,[this](int value){ setZoom(value/100.0); });
        action("+",[this]{ zoom(0.10); });
        bar->addSeparator();
        auto rotate=action(gpdfTr("Girar 90° a la derecha"),[this]{ rotateView((view->rotation+1)%4); });
        rotate->setObjectName("rotateClockwiseAction");
        rotate->setIcon(QIcon(new GpdfToolIcon(6)));
        rotate->setShortcut(QKeySequence("Ctrl+R")); addAction(rotate);
        auto rotateButton=qobject_cast<QToolButton*>(bar->widgetForAction(rotate));
        rotateButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
        rotateButton->setAccessibleName(rotate->text());
        bar->addSeparator();
        searchAction=new QAction(gpdfTr("Búsqueda"),this); addAction(searchAction);
        searchAction->setObjectName("searchAction"); searchAction->setCheckable(true);
        auto searchToggle=new QToolButton; searchToggle->setDefaultAction(searchAction);
        leftLayout->addWidget(searchToggle,0,Qt::AlignLeft);
        leftLayout->addWidget(separator("searchToolsSeparator"));
        searchAction->setToolTip(gpdfTr("Buscar texto"));
        auto focusSearch=new QAction(this); focusSearch->setObjectName("focusSearch");
        focusSearch->setShortcut(QKeySequence("Ctrl+B")); addAction(focusSearch);
        focusSearch->setCheckable(true);
        connect(searchAction,&QAction::toggled,focusSearch,&QAction::setChecked);
        connect(searchAction,&QAction::toggled,this,[this]{ syncViewActions(); });
        connect(focusSearch,&QAction::triggered,[this]{ searchAction->setChecked(!searchAction->isChecked()); if(searchAction->isChecked()) { searchInput->setFocus(); searchInput->selectAll(); } else view->setFocus(); });
        connect(searchAction,&QAction::toggled,[this](bool visible){
            searchPanel->setVisible(visible);
            if(visible) { searchInput->setFocus(); searchInput->selectAll(); }
            else { view->searchPage=-1; view->searchRects.clear(); view->update(); }
        });
        auto editingTools=new QToolBar; editingTools->setObjectName("editingTools"); editingTools->setMovable(false); editingTools->setToolButtonStyle(Qt::ToolButtonIconOnly); editingTools->setIconSize(QSize(24,24));
        leftLayout->addWidget(editingTools);
        auto editingAction=[editingTools](QString title,auto handler){ auto a=editingTools->addAction(title); QObject::connect(a,&QAction::triggered,handler); return a; };
        auto group = new QActionGroup(this);
        const QStringList labels{gpdfTr("Seleccionar"),gpdfTr("Comentario"),gpdfTr("Texto"),gpdfTr("Lápiz")};
        for(int i=0;i<labels.size();++i) {
            auto a=editingAction(labels[i],[this,i]{ if(!view->finishText()) return; view->tool=PageView::Tool(i); view->resetSelection(); view->update(); statusBar()->showMessage(i==0 ? gpdfTr("Arrastrar sobre el texto y luego presionar Resaltar.") : gpdfTr("Hacer clic en la página o arrastrar para dibujar.")); });
            a->setObjectName(QString("editTool%1").arg(i)); a->setIcon(QIcon(new GpdfToolIcon(i))); a->setShortcut(QKeySequence(Qt::Key_F1+i)); addAction(a);
            a->setCheckable(true); group->addAction(a); if(i==0) a->setChecked(true);
        }
        auto eraser=editingTools->addAction(QIcon(new GpdfToolIcon(7)),gpdfTr("Borrador de resaltados, subrayados y trazos")); eraser->setObjectName("editTool4"); eraser->setShortcut(QKeySequence(Qt::Key_F5)); eraser->setCheckable(true); group->addAction(eraser); addAction(eraser);
        connect(eraser,&QAction::triggered,this,[this]{ if(!view->finishText()) { if(auto previous=findChild<QAction*>(QString("editTool%1").arg(int(view->tool)))) previous->setChecked(true); return; } view->tool=PageView::Eraser; view->resetSelection(); view->updateEraserCursor(view->mapFromGlobal(QCursor::pos())); view->update(); });
        auto highlightAction=editingAction(gpdfTr("Resaltar"),[this]{ std::lock_guard guard(PdfDocument::apiMutex()); if(!pdf.loaded() || view->first<0 || view->last<0) return; if(!pdf.highlight(view->first,view->last,view->highlightColor)) error(pdf.error()); else { view->resetSelection(); refresh(); } });
        highlightAction->setObjectName("highlightAction"); highlightAction->setIcon(QIcon(new GpdfToolIcon(4))); addAction(highlightAction);
        auto underlineAction=editingAction(gpdfTr("Subrayar"),[this]{ std::lock_guard guard(PdfDocument::apiMutex()); if(!pdf.loaded() || view->first<0 || view->last<0) return; if(!pdf.underline(view->first,view->last,view->underlineColor,view->underlineWidth)) error(pdf.error()); else { view->resetSelection(); refresh(); } });
        underlineAction->setObjectName("underlineAction"); underlineAction->setIcon(QIcon(new GpdfToolIcon(5,view->underlineColor))); addAction(underlineAction);
        togglePanel=new QAction(gpdfTr("Anotaciones"),this);
        addAction(togglePanel);
        auto annotationsToggle=new QToolButton; annotationsToggle->setDefaultAction(togglePanel);
        rightLayout->addWidget(annotationsToggle,0,Qt::AlignRight);

        togglePanel->setObjectName("toggleAnnotations");
        togglePanel->setCheckable(true); togglePanel->setChecked(false);
        panel->hide();
        togglePanel->setToolTip(gpdfTr("Mostrar u ocultar anotaciones"));
        togglePanel->setShortcut(QKeySequence("Ctrl+N"));
        connect(togglePanel,&QAction::toggled,this,[this,panel](bool visible){ panel->setVisible(visible); if(visible) refreshPanel(view->selectedAnnotation); });
        connect(annotations,&QListWidget::itemActivated,[this](QListWidgetItem*){ editSelected(); });
        connect(annotations,&QListWidget::currentRowChanged,[this](int row){ selectAnnotation(row); });
        auto removeAction=new QAction(annotations);
        removeAction->setShortcut(QKeySequence()); removeAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);
        annotations->addAction(removeAction);
        connect(removeAction,&QAction::triggered,[this]{ deleteSelected(); });
        auto editAction=new QAction(annotations);
        editAction->setObjectName("editSelectedAnnotationAction");
        editAction->setShortcut(QKeySequence()); editAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);
        annotations->addAction(editAction);
        connect(editAction,&QAction::triggered,[this]{ editSelected(); });
        view->activated=[this](int){ refreshPanel(); };
        view->annotationSelected=[this](int index){ int row=annotationRow(pdf.pageIndex(),index); annotations->setCurrentRow(row); view->selectedAnnotation=index; view->update(); };
        view->navigate=[this](int direction){ page(direction); };
        view->annotationMoved=[this](int index){ refresh(index); };
        scroll->viewport()->installEventFilter(this);
        connect(scroll->verticalScrollBar(),&QScrollBar::valueChanged,[this](int){ view->hideComment(); if(!updatingView) lastSearchRow=-1; scrolling(); });
        view->changed=[this]{ refresh(view->tool==PageView::Eraser ? -1 : pdf.annotationCount()-1); }; view->report=[this](QString message){ error(message); };
        selectAnnotation(-1);
        createFileActions(); createEditActions(); createViewActions(); createReadingActions(); createGoActions(); createShortcutActions(); createHelpActions(); updateToolTips(); installFileMenu();
        autoScrollTimer=new QTimer(this); autoScrollTimer->setObjectName("autoScrollTimer"); autoScrollTimer->setInterval(16);
        connect(autoScrollTimer,&QTimer::timeout,this,[this]{
            int distance=QCursor::pos().y()-autoScrollOrigin.y();
            updateAutoScrollCursor(distance);
            double beyond=std::max(0,std::abs(distance)-12);
            int speed=int(std::min(1200.0,beyond*beyond/600.0))*(distance<0 ? -1 : 1);
            auto bar=scroll->verticalScrollBar(); bar->setValue(bar->value()+speed);
            int horizontal=QCursor::pos().x()-autoScrollOrigin.x();
            double sideways=std::max(0,std::abs(horizontal)-12);
            int horizontalSpeed=int(std::min(1200.0,sideways*sideways/600.0))*(horizontal<0 ? -1 : 1);
            auto horizontalBar=scroll->horizontalScrollBar(); horizontalBar->setValue(horizontalBar->value()+horizontalSpeed);
        });
        qApp->installEventFilter(this);
        checkpointTimer=new QTimer(this); checkpointTimer->setObjectName("checkpointTimer"); checkpointTimer->setSingleShot(true); checkpointTimer->setInterval(1500); connect(checkpointTimer,&QTimer::timeout,this,[this]{ saveSession(); });
        auto periodic=new QTimer(this); periodic->setInterval(15000); connect(periodic,&QTimer::timeout,this,[this]{ if(!documents.empty()) saveSession(); }); periodic->start();
        statusBar()->hide(); view->refresh();
    }
    ~Window() override {
        stopAutoScroll();
        view->shutdownWorkers();
        qApp->removeEventFilter(this);
#ifdef Q_OS_WIN
        if(nativeMenu) {
            SetMenu(reinterpret_cast<HWND>(winId()),nullptr);
            DestroyMenu(nativeMenu);
        }
#endif
    }
    void openDocument(const QString& path) { openPath(path); }
    void presentForExternalOpen() {
        bool minimized=isMinimized();
        auto restore=windowState() & ~Qt::WindowMinimized;
#ifdef Q_OS_WIN
        auto hwnd=reinterpret_cast<HWND>(winId());
        WINDOWPLACEMENT placement{}; placement.length=sizeof(placement);
        if(IsIconic(hwnd)) {
            minimized=true;
            if(GetWindowPlacement(hwnd,&placement) && (placement.flags & WPF_RESTORETOMAXIMIZED)) restore|=Qt::WindowMaximized;
        }
#endif
        if(minimized && !(restore & (Qt::WindowMaximized|Qt::WindowFullScreen))) restore=lastVisibleState;
        if(restore.testFlag(Qt::WindowFullScreen)) showFullScreen();
        else if(restore.testFlag(Qt::WindowMaximized)) showMaximized();
        else if(minimized) showNormal();
        else show();
        raise(); activateWindow();
#ifdef Q_OS_WIN
        if(QApplication::platformName()=="windows") SetForegroundWindow(hwnd);
#endif
    }
    void openExternalDocuments(const QStringList& paths) {
        presentForExternalOpen();
        for(const auto& path:paths) openDocument(path);
        presentForExternalOpen();
    }
    void loadPath(const QString& path) {
        if(!view->finishText()) return;
        if(pdf.open(path)) { documents.clear(); documents.push_back(std::make_unique<DocumentState>()); activeDocument=0; documentOpened(path); }
        else error(pdf.error());
    }
    QString addPersonalBookmark(QString title,int page=-1) {
        if(activeDocument<0 || !pdf.loaded() || title.trimmed().isEmpty()) return {};
        if(page<0) page=pdf.pageIndex();
        if(page>=pdf.pageCount()) return {};
        QString id=QUuid::createUuid().toString(QUuid::WithoutBraces);
        documents[activeDocument]->personalMarks.append(QJsonObject{{"id",id},{"title",title.trimmed().left(200)},{"page",page}}); persistReading(); refreshPersonalBookmarks(); return id;
    }
    bool renamePersonalBookmark(const QString& id,QString title) {
        if(activeDocument<0 || title.trimmed().isEmpty()) return false;
        auto& marks=documents[activeDocument]->personalMarks; for(int i=0;i<marks.size();++i) { auto mark=marks[i].toObject(); if(mark["id"].toString()==id) { mark["title"]=title.trimmed().left(200); marks[i]=mark; persistReading(); refreshPersonalBookmarks(); return true; } } return false;
    }
    bool removePersonalBookmark(const QString& id) {
        if(activeDocument<0) return false;
        auto& marks=documents[activeDocument]->personalMarks;
        for(int i=0;i<marks.size();++i) if(marks[i].toObject()["id"].toString()==id) { marks.removeAt(i); persistReading(); refreshPersonalBookmarks(); return true; }
        return false;
    }
    bool exportAnnotationsTo(const QString& path) {
        if(!pdf.loaded() || !view->finishText()) return false;
        QFileInfo destination(path); QString suffix=destination.suffix().toLower();
        auto normalized=[](const QString& file) { QFileInfo info(file); QString canonical=info.canonicalFilePath(); return canonical.isEmpty() ? info.absoluteFilePath() : canonical; };
        for(int i=0;i<int(documents.size());++i) if(normalized(path).compare(normalized(i==activeDocument ? source : documents[i]->path),Qt::CaseInsensitive)==0) {
            statusBar()->showMessage(gpdfTr("La exportación no puede reemplazar un documento abierto.")); return false;
        }
        if(suffix!="md" && suffix!="txt" && suffix!="html") { statusBar()->showMessage(gpdfTr("Elegir una extensión .md, .txt o .html para exportar.")); return false; }
        bool ok=AnnotationExport::write(path,QFileInfo(source).fileName(),pdf.documentAnnotations());
        if(ok) statusBar()->showMessage(gpdfTr("Anotaciones exportadas: ")+path);
        return ok;
    }

    bool saveSession(bool clean=false) {
        if(restoringSession || sessionClosed) return true;
        if(activeDocument>=0) captureDocument();
        QDir root(sessionDirectory()); if(!root.exists() && !QDir().mkpath(root.absolutePath())) return false;
        QJsonArray list;
        for(int i=0;!clean && i<int(documents.size());++i) {
            auto& doc=*documents[i]; auto& engine=i==activeDocument ? pdf : doc.stored;
            QString backup=doc.id+".pdf"; bool dirty=engine.dirty() && !doc.discardOnExit;
            if(dirty && (doc.backupRevision!=engine.revision() || !root.exists(backup))) {
                if(!engine.saveRecovery(root.filePath(backup))) { statusBar()->showMessage(gpdfTr("No se pudo crear la copia de recuperación.")); return false; } doc.backupRevision=engine.revision();
            }
            QJsonObject item={{"path",doc.path},{"id",doc.id},{"backup",dirty ? backup : QString()},{"page",engine.pageIndex()},{"mode",int(doc.mode)},{"fit",int(doc.fit)},{"scale",doc.scale},{"rotation",doc.rotation},{"vertical",doc.vertical},{"horizontal",doc.horizontal},{"scope",doc.scope},{"type",doc.type},{"query",doc.query},{"searchVisible",doc.searchVisible},{"annotationsVisible",doc.annotationsVisible}};
            item["marks"]=doc.personalMarks;
            if(i==activeDocument && !clean) item["draft"]=view->editorDraft();
            list.append(item);
        }
        for(const auto& pending:pendingRecoveryItems) list.append(pending);
        QString activeId=!clean && activeDocument>=0 ? documents[activeDocument]->id : QString();
        return SessionStore::write(root,QJsonObject{{"version",1},{"clean",clean},{"active",clean ? -1 : activeDocument},{"activeId",activeId},{"documents",list}});
    }

    void restoreWindow() {
        QSettings settings(QSettings::IniFormat,QSettings::UserScope,AppIdentity::settingsOrganization,AppIdentity::settingsApplication);
        auto geometry=settings.value("window/geometry").toByteArray();
        if(!geometry.isEmpty()) restoreGeometry(geometry);
        QSize savedSize=settings.value("window/size").toSize(); if(savedSize.isValid()) resize(savedSize);
        auto state=Qt::WindowStates(settings.value("window/state",0).toInt()) & Qt::WindowMaximized;
        setWindowState(state);
        show();
    }
    void restoreSession() {
        QDir root(sessionDirectory()); auto manifest=SessionStore::read(root); if(manifest.isEmpty()) return;
        auto list=manifest["documents"].toArray(); if(list.isEmpty()) return;
        if(manifest["clean"].toBool()) {
            QJsonArray pending;
            for(auto value:list) {
                auto item=value.toObject();
                if(!item["backup"].toString().isEmpty() || !item["draft"].toObject().isEmpty()) pending.append(item);
            }
            list=pending; if(list.isEmpty()) return;
        }
        bool recovery=false; for(auto value:list) { auto item=value.toObject(); if(!item["backup"].toString().isEmpty() || !item["draft"].toObject().isEmpty()) recovery=true; }
        bool recover=!recovery || QMessageBox::question(this,gpdfTr("Recuperar cambios"),gpdfTr("Hay cambios de la sesión anterior que no se guardaron. ¿Desea recuperarlos?"),QMessageBox::Yes|QMessageBox::No,QMessageBox::Yes)==QMessageBox::Yes;
        restoringSession=true; pendingRecoveryItems=QJsonArray(); QVector<QJsonObject> restored;
        QVector<int> originalIndexes; int manifestIndex=-1;
        for(auto value:list) {
            ++manifestIndex;
            auto item=value.toObject(); QString path=item["path"].toString(),backup=item["backup"].toString();
            auto preservePending=[&]{ if(recover && (!backup.isEmpty() || !item["draft"].toObject().isEmpty())) pendingRecoveryItems.append(item); };
            bool useBackup=recover && !backup.isEmpty() && QFileInfo(backup).fileName()==backup && root.exists(backup);
            QString readPath=useBackup ? root.filePath(backup) : path;
            if(!QFileInfo::exists(readPath)) { preservePending(); continue; }
            int oldSize=int(documents.size()); openPath(readPath);
            if(int(documents.size())==oldSize) { preservePending(); continue; }
            auto& doc=*documents.back(); doc.id=item["id"].toString(doc.id); if(QUuid(doc.id).isNull()) doc.id=QUuid::createUuid().toString(QUuid::WithoutBraces);
            source=path; doc.path=path; doc.personalMarks=item["marks"].toArray(); if(doc.personalMarks.isEmpty()) { QSettings reading(QSettings::IniFormat,QSettings::UserScope,AppIdentity::settingsOrganization,AppIdentity::settingsApplication); doc.personalMarks=QJsonDocument::fromJson(reading.value(readingKey(path)).toByteArray()).object()["marks"].toArray(); } if(useBackup) pdf.markRecovered();
            doc.mode=PageView::Mode(std::clamp(item["mode"].toInt(),0,2)); doc.fit=PageView::Fit(std::clamp(item["fit"].toInt(),0,3)); doc.scale=std::clamp(item["scale"].toDouble(1.25),0.5,3.0); doc.rotation=std::clamp(item["rotation"].toInt(),0,3); doc.vertical=std::max(0,item["vertical"].toInt()); doc.horizontal=std::max(0,item["horizontal"].toInt()); doc.scope=std::clamp(item["scope"].toInt(),0,1); doc.type=std::clamp(item["type"].toInt(),0,annotationTypeFilter->count()-1); doc.query=item["query"].toString(); doc.searchVisible=item["searchVisible"].toBool(); doc.annotationsVisible=item["annotationsVisible"].toBool();
            pdf.setPage(std::clamp(item["page"].toInt(),0,std::max(0,pdf.pageCount()-1)));
            view->mode=doc.mode; view->fit=doc.fit; view->scale=doc.scale; view->rotation=doc.rotation;
            { QSignalBlocker a(viewMode),b(annotationScope),c(annotationTypeFilter); viewMode->setCurrentIndex(int(doc.mode)); annotationScope->setCurrentIndex(doc.scope); annotationTypeFilter->setCurrentIndex(doc.type); } refresh();
            // Store this restored state before opening the next file.
            searchInput->setText(doc.query); searchAction->setChecked(doc.searchVisible); togglePanel->setChecked(doc.annotationsVisible); scroll->verticalScrollBar()->setValue(doc.vertical); scroll->horizontalScrollBar()->setValue(doc.horizontal);
            restored.append(item); originalIndexes.append(manifestIndex);
        }
        if(!documents.empty()) {
            int active=-1; QString activeId=manifest["activeId"].toString();
            if(!activeId.isEmpty()) for(int i=0;i<int(documents.size());++i) if(documents[i]->id==activeId) { active=i; break; }
            if(active<0) active=originalIndexes.indexOf(manifest["active"].toInt());
            if(active<0) active=0;
            switchDocument(active);
            auto& doc=*documents[active]; viewMode->setCurrentIndex(int(doc.mode)); annotationScope->setCurrentIndex(doc.scope); annotationTypeFilter->setCurrentIndex(doc.type); refresh();
            updatingView=true; scroll->verticalScrollBar()->setValue(doc.vertical); scroll->horizontalScrollBar()->setValue(doc.horizontal); updatingView=false;
            if(recover && active<restored.size()) view->restoreDraft(restored[active]["draft"].toObject());
        }
        restoringSession=false; saveSession();
    }
protected:
    void resizeEvent(QResizeEvent* event) override { QMainWindow::resizeEvent(event); updateToolbarLayout(); }
    void changeEvent(QEvent* event) override {
        QMainWindow::changeEvent(event);
        if(event->type()==QEvent::WindowStateChange) {
            if(!isMinimized()) lastVisibleState=windowState() & (Qt::WindowMaximized|Qt::WindowFullScreen);
            else {
                auto old=static_cast<QWindowStateChangeEvent*>(event)->oldState();
                if(!old.testFlag(Qt::WindowMinimized)) lastVisibleState=old & (Qt::WindowMaximized|Qt::WindowFullScreen);
            }
        }
        if(event->type()==QEvent::WindowStateChange && isFullScreen()) {
            installFileMenu();
            QTimer::singleShot(0,this,[this]{ if(isFullScreen()) installFileMenu(); });
        }
    }
    QMenu* createPopupMenu() override { return nullptr; }
#ifdef Q_OS_WIN
    bool nativeEvent(const QByteArray& type,void* message,qintptr* result) override {
        auto msg=static_cast<MSG*>(message);
        if(msg->message==WM_SETCURSOR && LOWORD(msg->lParam)==HTCLIENT && autoScrollTimer && autoScrollTimer->isActive()) {
            if(auto cursor=NativeScrollCursor::handle(autoScrollDirection)) { SetCursor(cursor); *result=TRUE; return true; }
        }
        if(restoringMaximizedFrame && msg->message==WM_WINDOWPOSCHANGING) {
            auto position=reinterpret_cast<WINDOWPOS*>(msg->lParam);
            // Qt first applies the old normal rectangle. Keep the frame at its final
            // maximized bounds throughout that intermediate native operation.
            if(!(position->flags & SWP_NOMOVE)) { position->x=fullscreenMaximizedFrame.left; position->y=fullscreenMaximizedFrame.top; }
            if(!(position->flags & SWP_NOSIZE)) { position->cx=fullscreenMaximizedFrame.right-fullscreenMaximizedFrame.left; position->cy=fullscreenMaximizedFrame.bottom-fullscreenMaximizedFrame.top; }
        }
        // Qt can reattach its cached native menu during the first fullscreen transition.
        if(isFullScreen() && (msg->message==WM_WINDOWPOSCHANGED || msg->message==WM_SHOWWINDOW)) {
            auto hwnd=reinterpret_cast<HWND>(winId());
            if(GetMenu(hwnd)) { SetMenu(hwnd,nullptr); DrawMenuBar(hwnd); }
        }
        if(nativeMenu && msg->message==WM_COMMAND && HIWORD(msg->wParam)==0 && msg->lParam==0) {
            const UINT id=LOWORD(msg->wParam);
            if(id>=fileCommandBase && id<fileCommandBase+UINT(fileActions.size())) {
                *result=0;
                fileActions[int(id-fileCommandBase)]->trigger();
                return true;
            }
            if(id>=editCommandBase && id<editCommandBase+UINT(editActions.size())) {
                *result=0; auto action=editActions[int(id-editCommandBase)]; if(action->isEnabled()) action->trigger(); return true;
            }
            if(id>=viewCommandBase && id<viewCommandBase+UINT(viewActions.size())) {
                *result=0; auto action=viewActions[int(id-viewCommandBase)]; if(action->isEnabled()) action->trigger(); return true;
            }
        }
        if(nativeMenu && msg->message==WM_COMMAND && HIWORD(msg->wParam)==0 && msg->lParam==0) {
            UINT id=LOWORD(msg->wParam);
            for(auto list:{goActions,shortcutMenuActions,helpActions}) for(auto action:list) if(action->property("nativeCommand").toUInt()==id && action->isEnabled()) { action->trigger(); *result=0; return true; }
        }
        if(nativeMenu && msg->message==WM_INITMENUPOPUP) { updateEditActions(); updateGoActions(); syncViewActions(); }
        return QMainWindow::nativeEvent(type,message,result);
    }
#endif
    bool eventFilter(QObject* object,QEvent* event) override {
#ifdef Q_OS_WIN
        if(event->type()==QEvent::Show && QApplication::platformName()=="windows") {
            auto widget=qobject_cast<QWidget*>(object);
            // Native dialogs own their chrome; do not materialize Qt's hidden print container.
            if(widget && widget->isWindow() && !widget->testAttribute(Qt::WA_DontShowOnScreen) && !qobject_cast<QPrintDialog*>(widget)) {
                static HMODULE dwm=LoadLibraryW(L"dwmapi.dll");
                using SetAttribute=HRESULT (WINAPI*)(HWND,DWORD,LPCVOID,DWORD);
                static auto setAttribute=dwm ? reinterpret_cast<SetAttribute>(GetProcAddress(dwm,"DwmSetWindowAttribute")) : nullptr;
                BOOL dark=QApplication::palette().color(QPalette::Window).lightness()<100;
                if(setAttribute) setAttribute(reinterpret_cast<HWND>(widget->winId()),20,&dark,sizeof(dark));
            }
        }
#endif
        if(view && object==this && (event->type()==QEvent::WindowDeactivate || event->type()==QEvent::Hide)) view->cancelAnnotationDrag();
        if(view && view->commentVisible() && (event->type()==QEvent::MouseButtonPress || event->type()==QEvent::MouseButtonDblClick)) {
            auto widget=qobject_cast<QWidget*>(object);
            if(widget && widget->window()==this && !view->commentContains(widget)) view->hideComment();
        }
        if(event->type()==QEvent::Wheel && static_cast<QWheelEvent*>(event)->modifiers().testFlag(Qt::ControlModifier)) {
            auto widget=qobject_cast<QWidget*>(object);
            if(widget && (widget==view || widget==scroll->viewport())) {
                auto wheel=static_cast<QWheelEvent*>(event);
                if(pdf.loaded() && wheel->angleDelta().y()) {
                    int delta=wheel->angleDelta().y();
                    if((delta>0 && zoomWheelRemainder<0) || (delta<0 && zoomWheelRemainder>0)) zoomWheelRemainder=0;
                    zoomWheelRemainder+=delta; int steps=zoomWheelRemainder/120; zoomWheelRemainder%=120;
                    if(steps) { int percent=int(std::lround((view->fit==PageView::Manual ? view->scale : view->effectiveScale)*100)); setZoom(std::clamp((percent+steps*25)/100.0,0.5,3.0),scroll->viewport()->mapFromGlobal(wheel->globalPosition().toPoint())); }
                }
                wheel->accept(); return true;
            }
        }
        if(autoScrollTimer && autoScrollTimer->isActive()) {
            if((event->type()==QEvent::MouseButtonPress || event->type()==QEvent::MouseButtonDblClick)) { stopAutoScroll(); return true; }
            if(object==this && (event->type()==QEvent::WindowDeactivate || event->type()==QEvent::Hide)) stopAutoScroll();
        }
        if(pdf.loaded() && (object==view || object==scroll->viewport()) && (event->type()==QEvent::MouseButtonPress || event->type()==QEvent::MouseButtonDblClick) && static_cast<QMouseEvent*>(event)->button()==Qt::MiddleButton) {
            if(!view->finishText()) return true;
            autoScrollOrigin=static_cast<QMouseEvent*>(event)->globalPosition().toPoint();
            autoScrollDirection=NativeScrollCursor::Neutral;
            QApplication::setOverrideCursor(NativeScrollCursor::cursor(autoScrollDirection));
            autoScrollTimer->start(); updateAutoScrollCursor(0); return true;
        }
        if(!pdf.loaded() && (object==view || object==scroll->viewport()) && event->type()==QEvent::MouseButtonPress) {
            auto mouse=static_cast<QMouseEvent*>(event);
            if(mouse->button()==Qt::LeftButton) { open(); return true; }
        }
        if(event->type()==QEvent::KeyPress || event->type()==QEvent::ShortcutOverride) {
            auto key=static_cast<QKeyEvent*>(event);
            auto widget=qobject_cast<QWidget*>(object);
            if(widget && widget->window()==this) {
                if(view->commentContains(widget) && (key->matches(QKeySequence::Copy) || key->matches(QKeySequence::SelectAll))) {
                    if(event->type()==QEvent::ShortcutOverride) { key->accept(); return true; }
                    return false;
                }
                auto isTextControl=[this](QWidget* target) {
                    for(auto parent=target;parent && parent!=this;parent=parent->parentWidget())
                        if(qobject_cast<QLineEdit*>(parent) || qobject_cast<QTextEdit*>(parent) || qobject_cast<QPlainTextEdit*>(parent) || qobject_cast<QComboBox*>(parent) || qobject_cast<QAbstractSpinBox*>(parent)) return true;
                    return false;
                };
                // Ignored keys can bubble from an editor to its parent widget.
                auto focused=QApplication::focusWidget();
                const bool editing=isTextControl(widget) || (focused && focused->window()==this && isTextControl(focused));
                QKeySequence pressed=key->key()==Qt::Key_Backtab ? QKeySequence(Qt::CTRL|Qt::SHIFT|Qt::Key_Tab) : QKeySequence(key->keyCombination());
                const bool searchFocused=focused==searchInput || focused==searchResults || (focused && searchResults->isAncestorOf(focused));
                const bool inlineActive=view->findChild<QWidget*>("inlineTextEditor")!=nullptr;
                // Reserve typing and editor navigation before Qt's QAction shortcut dispatch.
                // Enter/Escape retain their contextual search and annotation behavior.
                if(editing && !(key->modifiers() & (Qt::ControlModifier|Qt::AltModifier|Qt::MetaModifier))
                   && key->key()!=Qt::Key_Return && key->key()!=Qt::Key_Enter && key->key()!=Qt::Key_Escape) {
                    if(event->type()==QEvent::ShortcutOverride) { key->accept(); return true; }
                    if(focused && focused->window()==this && isTextControl(focused)
                       && widget!=focused && !focused->isAncestorOf(widget) && !key->text().isEmpty()) return true;
                    return false;
                }
                for(auto action:shortcutActions) {
                    if(editing && (action->objectName()=="deleteSelectionAction" || ((action->objectName()=="goPrevious" || action->objectName()=="goNext") && key->modifiers()==Qt::NoModifier))) continue;
                    if(!action->isEnabled()) continue;
                    QString name=action->objectName();
                    if(name=="confirmTextAction" && (!inlineActive || searchFocused)) continue;
                    if(name=="cancelContextAction" && !inlineActive && !autoScrollTimer->isActive() && !view->annotationDragging() && !view->commentVisible() && !isFullScreen()) continue;
                    if(name=="searchEnterAction" && !searchFocused) continue;
                    for(const auto& shortcut:action->shortcuts()) if(!shortcut.isEmpty() && shortcut==pressed) {
                        if(event->type()==QEvent::ShortcutOverride) key->accept(); else action->trigger();
                        return true;
                    }
                }
            }
        }
        if(object==scroll->viewport() && event->type()==QEvent::Resize) {
            bool previous=updatingView; updatingView=true;
            view->setViewportSize(scroll->viewport()->size());
            updatingView=previous; syncViewActions();
        }
        return QMainWindow::eventFilter(object,event);
    }
    void closeEvent(QCloseEvent* event) override {
        int original=activeDocument;
        for(int i=0;i<int(documents.size());++i) { if(!switchDocument(i) || !confirmDiscard()) { for(auto& doc:documents) doc->discardOnExit=false; switchDocument(original); event->ignore(); return; } }
        switchDocument(original); saveSession(true); sessionClosed=true; checkpointTimer->stop();
        QSettings settings(QSettings::IniFormat,QSettings::UserScope,AppIdentity::settingsOrganization,AppIdentity::settingsApplication);
        settings.setValue("window/geometry",isFullScreen() ? fullscreenRestoreSavedGeometry : saveGeometry());
        settings.setValue("window/size",isFullScreen() ? fullscreenRestoreGeometry.size() : (isMaximized() || isMinimized() ? normalGeometry().size() : size()));
        settings.setValue("window/state",int((isFullScreen() ? fullscreenRestoreState : isMinimized() ? lastVisibleState : windowState()) & Qt::WindowMaximized));
        event->accept();
    }
private:
    bool sessionClosed=false;
    Qt::WindowStates lastVisibleState=Qt::WindowNoState;
    bool printDialogActive=false;
    int zoomWheelRemainder=0;
    Qt::WindowStates fullscreenRestoreState=Qt::WindowNoState;
    QRect fullscreenRestoreGeometry;
    QByteArray fullscreenRestoreSavedGeometry;
#ifdef Q_OS_WIN
    bool restoringMaximizedFrame=false;
    RECT fullscreenMaximizedFrame{};
    WINDOWPLACEMENT fullscreenNativePlacement{};
#endif
    void setFullscreen(bool enabled) {
        if(enabled==isFullScreen()) return;
        stopAutoScroll();
#ifdef Q_OS_WIN
        HWND hwnd=QApplication::platformName()=="windows" ? reinterpret_cast<HWND>(winId()) : nullptr;
        WindowTransitionsGuard transitions(!enabled ? hwnd : nullptr);
        if(enabled && hwnd && isMaximized()) {
            GetWindowRect(hwnd,&fullscreenMaximizedFrame);
            fullscreenNativePlacement={}; fullscreenNativePlacement.length=sizeof(WINDOWPLACEMENT);
            if(!GetWindowPlacement(hwnd,&fullscreenNativePlacement)) fullscreenNativePlacement.length=0;
        }
        QScopedValueRollback<bool> restoringFrame(restoringMaximizedFrame,!enabled && hwnd && fullscreenRestoreState.testFlag(Qt::WindowMaximized) && fullscreenNativePlacement.length!=0);
#endif
        if(enabled) {
            fullscreenRestoreState=windowState() & ~(Qt::WindowFullScreen|Qt::WindowMinimized);
            fullscreenRestoreGeometry=isMaximized() ? normalGeometry() : geometry();
            fullscreenRestoreSavedGeometry=saveGeometry();
            showFullScreen();
        } else if(fullscreenRestoreState.testFlag(Qt::WindowMaximized)) showMaximized();
        else showNormal();
        if(auto action=findChild<QAction*>("fullscreenAction")) { QSignalBlocker blocker(action); action->setChecked(enabled); }
        installFileMenu();
        if(!enabled && !fullscreenRestoreState.testFlag(Qt::WindowMaximized) && fullscreenRestoreGeometry.isValid()) setGeometry(fullscreenRestoreGeometry);
#ifdef Q_OS_WIN
        if(restoringMaximizedFrame) { auto placement=fullscreenNativePlacement; placement.showCmd=SW_SHOWMAXIMIZED; SetWindowPlacement(hwnd,&placement); }
#endif
    }
    void updateToolbarLayout() {
        auto host=findChild<QWidget*>("toolbarHost"); if(!host) return;
        auto layout=qobject_cast<QGridLayout*>(host->layout());
        auto left=findChild<QWidget*>("toolbarLeft"),right=findChild<QWidget*>("toolbarRight"); auto center=findChild<QToolBar*>("centerTools");
        if(!layout || !left || !right || !center) return;
        int edge=left->layout()->sizeHint().width()+8;
        bool compact=width()<2*edge+center->sizeHint().width()+40;
        if(host->property("compact").isValid() && host->property("compact").toBool()==compact && left->width()==edge && center->minimumWidth()==center->sizeHint().width()) return;
        host->setProperty("compact",compact);
        layout->removeWidget(left); layout->removeWidget(right); layout->removeWidget(center);
        left->setFixedWidth(edge); right->setFixedWidth(compact ? right->layout()->sizeHint().width()+8 : edge);
        layout->addWidget(left,0,0); layout->addWidget(right,0,2);
        if(compact) layout->addWidget(center,1,0,1,3,Qt::AlignCenter);
        else layout->addWidget(center,0,1,Qt::AlignCenter);
        center->setMinimumWidth(center->sizeHint().width());
        layout->invalidate();
    }
    QList<QAction*> fileActions;
    QList<QAction*> editActions;
    QList<QAction*> viewActions;
    QList<QAction*> goActions,shortcutMenuActions,shortcutActions;
    QMenu* goMenu=nullptr;
    QMenu* shortcutsMenu=nullptr;
    bool navigatingHistory=false;
    bool restoringSession=false;
    QJsonArray pendingRecoveryItems;
    QTimer* checkpointTimer=nullptr;
    QString sessionDirectory() const { QSettings settings(QSettings::IniFormat,QSettings::UserScope,AppIdentity::settingsOrganization,AppIdentity::settingsApplication); return QFileInfo(settings.fileName()).absolutePath()+"/recovery"; }
    void captureDocument() {
        if(activeDocument<0) return;
        auto& doc=*documents[activeDocument]; doc.path=source; doc.query=searchInput->text(); doc.mode=view->mode; doc.fit=view->fit; doc.scale=view->scale; doc.rotation=view->rotation;
        doc.vertical=scroll->verticalScrollBar()->value(); doc.horizontal=scroll->horizontalScrollBar()->value(); doc.scope=annotationScope->currentIndex(); doc.type=annotationTypeFilter->currentIndex(); doc.searchVisible=searchAction->isChecked(); doc.annotationsVisible=togglePanel->isChecked(); persistReading();
    }
    QLineEdit* annotationSearch=nullptr;
    QListWidget* personalBookmarks=nullptr;
    static QString readingKey(QString path) {
        QFileInfo info(path); QString canonical=info.canonicalFilePath(); if(canonical.isEmpty()) canonical=info.absoluteFilePath();
        return "files/"+QString::fromLatin1(QCryptographicHash::hash(canonical.toLower().toUtf8(),QCryptographicHash::Sha256).toHex());
    }
    void persistReading() {
        if(restoringSession || activeDocument<0 || source.isEmpty() || !pdf.loaded()) return;
        QSettings settings(QSettings::IniFormat,QSettings::UserScope,AppIdentity::settingsOrganization,AppIdentity::settingsApplication); auto& doc=*documents[activeDocument];
        QJsonObject state={{"page",pdf.pageIndex()},{"mode",int(view->mode)},{"fit",int(view->fit)},{"scale",view->scale},{"rotation",view->rotation},{"vertical",scroll->verticalScrollBar()->value()},{"horizontal",scroll->horizontalScrollBar()->value()},{"marks",doc.personalMarks}};
        settings.setValue(readingKey(source),QJsonDocument(state).toJson(QJsonDocument::Compact));
    }
    void restoreReading() {
        if(restoringSession || activeDocument<0) return;
        QSettings settings(QSettings::IniFormat,QSettings::UserScope,AppIdentity::settingsOrganization,AppIdentity::settingsApplication); auto state=QJsonDocument::fromJson(settings.value(readingKey(source)).toByteArray()).object();
        if(state.isEmpty()) return;
        bool previous=updatingView; updatingView=true;
        documents[activeDocument]->personalMarks=state["marks"].toArray();
        pdf.setPage(std::clamp(state["page"].toInt(),0,std::max(0,pdf.pageCount()-1)));
        view->mode=PageView::Mode(std::clamp(state["mode"].toInt(),0,2)); view->fit=PageView::Fit(std::clamp(state["fit"].toInt(),0,3)); view->scale=std::clamp(state["scale"].toDouble(1.25),0.5,3.0); view->rotation=std::clamp(state["rotation"].toInt(),0,3);
        { QSignalBlocker blocker(viewMode); viewMode->setCurrentIndex(int(view->mode)); } refresh();
        scroll->verticalScrollBar()->setValue(state["vertical"].toInt()); scroll->horizontalScrollBar()->setValue(state["horizontal"].toInt()); updatingView=previous;
    }
    void refreshPersonalBookmarks() {
        if(!personalBookmarks) return;
        const QString path=activeDocument>=0 ? documents[activeDocument]->id : QString();
        const QJsonArray marks=activeDocument>=0 ? documents[activeDocument]->personalMarks : QJsonArray();
        if(personalBookmarksPath==path && personalBookmarksSnapshot==marks) return;
        QString selectedId=personalBookmarksPath==path && personalBookmarks->currentItem() ? personalBookmarks->currentItem()->data(Qt::UserRole+1).toString() : QString();
        personalBookmarksPath=path; personalBookmarksSnapshot=marks;
        QSignalBlocker blocker(personalBookmarks); personalBookmarks->clear(); if(activeDocument<0) return;
        for(auto value:documents[activeDocument]->personalMarks) { auto mark=value.toObject(); int page=mark["page"].toInt(); if(page<0 || page>=pdf.pageCount()) continue; auto item=new QListWidgetItem(QString(gpdfTr("Página %1 · %2")).arg(page+1).arg(mark["title"].toString()),personalBookmarks); item->setData(Qt::UserRole,page); item->setData(Qt::UserRole+1,mark["id"].toString()); item->setData(Qt::UserRole+2,mark["title"].toString()); }
        for(int row=0;row<personalBookmarks->count();++row) if(personalBookmarks->item(row)->data(Qt::UserRole+1).toString()==selectedId) { personalBookmarks->setCurrentRow(row); break; }
    }
    QString personalBookmarksPath;
    QJsonArray personalBookmarksSnapshot;
    void promptPersonalBookmark() { if(!pdf.loaded()) return; bool ok=false; auto title=QInputDialog::getText(this,gpdfTr("Marcador personal"),gpdfTr("Nombre:"),QLineEdit::Normal,QString(gpdfTr("Página %1")).arg(pdf.pageIndex()+1),&ok); if(ok) addPersonalBookmark(title); }
    QTabWidget* navigationPanel=nullptr;
    QListWidget* thumbnails=nullptr;
    QTreeWidget* outline=nullptr;
    QMenu* viewMenu=nullptr;
    QMenu* fileMenu=nullptr;
    QMenu* recentMenu=nullptr;
    QList<QAction*> recentActions;
    QAction* noRecentFiles=nullptr;
    QStringList recentFiles;
    QToolBar* mainToolbar=nullptr;
    bool pendingSearchNext=false;
    int lastSearchRow=-1;
    PdfDocument::AnnotationCopy copiedAnnotation;
#ifdef Q_OS_WIN
    HMENU nativeMenu=nullptr;
    static constexpr UINT fileCommandBase=0x7100;
    static constexpr UINT editCommandBase=0x7200;
    static constexpr UINT viewCommandBase=0x7300;
    static constexpr UINT goCommandBase=0x7400;
    static constexpr UINT shortcutsCommandBase=0x7500;
    static constexpr UINT helpCommandBase=0x7600;
#endif
    QMenu* helpMenu=nullptr;
    QList<QAction*> helpActions;
    UpdateManager* updates=nullptr;
public:
    void startUpdateChecks() { updates->startAutomaticCheck(); }
private:
    void createHelpActions() {
        helpMenu=new QMenu(gpdfTr("&Ayuda"),this); helpMenu->setObjectName("helpMenu");
        updates=new UpdateManager(this);
        auto checkUpdates=helpMenu->addAction(gpdfTr("Buscar actualizaciones…"));
        checkUpdates->setObjectName("checkUpdatesAction"); helpActions.append(checkUpdates);
        connect(checkUpdates,&QAction::triggered,this,[this]{ updates->check(true); });
        auto automatic=helpMenu->addAction(gpdfTr("Buscar actualizaciones automáticamente"));
        automatic->setObjectName("automaticUpdatesAction"); automatic->setCheckable(true);
        QSettings updateSettings(QSettings::IniFormat,QSettings::UserScope,AppIdentity::settingsOrganization,AppIdentity::settingsApplication);
        automatic->setChecked(updateSettings.value("updates/automatic",true).toBool()); helpActions.append(automatic);
        connect(automatic,&QAction::triggered,this,[this,automatic]{
            QSettings settings(QSettings::IniFormat,QSettings::UserScope,AppIdentity::settingsOrganization,AppIdentity::settingsApplication);
            settings.setValue("updates/automatic",automatic->isChecked()); installFileMenu();
        });
        helpMenu->addSeparator();
        auto languageMenu=helpMenu->addMenu(gpdfTr("Idioma")); languageMenu->setObjectName("languageMenu");
        auto languages=new QActionGroup(this); languages->setExclusive(true);
        for(const auto& language:{QString("es"),QString("en")}) {
            auto item=languageMenu->addAction(language=="es" ? "Español" : "English");
            item->setObjectName(language=="es" ? "languageSpanishAction" : "languageEnglishAction");
            item->setCheckable(true); item->setChecked(savedUiLanguage()==language);
            languages->addAction(item); helpActions.append(item);
            connect(item,&QAction::triggered,this,[this,language,languages]{
                const bool saved=saveUiLanguage(language);
                for(auto choice:languages->actions()) choice->setChecked((choice->objectName()=="languageEnglishAction" ? "en" : "es")==savedUiLanguage());
                installFileMenu();
                if(saved) QMessageBox::information(this,gpdfTr("Idioma"),gpdfTr("El idioma se aplicará la próxima vez que inicie GPDF."));
                else QMessageBox::warning(this,gpdfTr("Idioma"),gpdfTr("No se pudo guardar el idioma."));
            });
        }
        helpMenu->addSeparator();
        auto action=helpMenu->addAction(gpdfTr("Acerca de GPDF")); action->setObjectName("aboutGpdfAction");
        helpActions.append(action); addAction(action);
        connect(action,&QAction::triggered,this,[this]{
            QDialog dialog(this); dialog.setObjectName("aboutGpdfDialog"); dialog.setWindowTitle(gpdfTr("Acerca de GPDF")); dialog.resize(360,160);
            auto layout=new QVBoxLayout(&dialog);
            auto text=new QLabel(QString(gpdfTr("Grant's PDF Reader versión %1\nCreado por Lautaro Grant")).arg(GPDF_VERSION));
            text->setObjectName("aboutGpdfText"); text->setAlignment(Qt::AlignCenter); layout->addWidget(text,1);
            auto close=new QDialogButtonBox; close->addButton(gpdfTr("Cerrar"),QDialogButtonBox::AcceptRole); layout->addWidget(close);
            connect(close,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);
            dialog.exec();
        });
    }
    void createFileActions() {
        fileMenu=new QMenu(gpdfTr("&Archivo"),this);
        auto action=[this](QMenu* menu,const QString& title,const QString& name,auto handler,const QKeySequence& shortcut=QKeySequence()) {
            auto item=menu->addAction(title); item->setObjectName(name); item->setShortcut(shortcut); addAction(item); fileActions.append(item);
            connect(item,&QAction::triggered,this,handler); return item;
        };
        action(fileMenu,gpdfTr("Abrir"),"openFileAction",[this]{ open(); },QKeySequence::Open);
        recentMenu=fileMenu->addMenu(gpdfTr("Abrir reciente"));
        noRecentFiles=recentMenu->addAction(gpdfTr("No hay archivos recientes")); noRecentFiles->setEnabled(false);
        for(int i=0;i<10;++i) recentActions.append(action(recentMenu,QString(),QString("recentFile%1").arg(i),[this,i]{ if(i<recentFiles.size()) openPath(recentFiles[i]); }));
        fileMenu->addSeparator();
        action(fileMenu,gpdfTr("Guardar"),"saveFileAction",[this]{ save(); },QKeySequence("Ctrl+G"));
        action(fileMenu,gpdfTr("Guardar como"),"saveAsFileAction",[this]{ saveAs(); },QKeySequence("Ctrl+Shift+G"));
        fileMenu->addSeparator();
        action(fileMenu,gpdfTr("Imprimir"),"printAction",[this]{ printDocument(false); },QKeySequence::Print);
        action(fileMenu,gpdfTr("Vista previa"),"printPreviewAction",[this]{ printDocument(true); });
        action(fileMenu,gpdfTr("Abrir la carpeta contenedora"),"openFolderAction",[this]{ if(!source.isEmpty()) QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(source).absolutePath())); });
        action(fileMenu,gpdfTr("Propiedades…"),"propertiesAction",[this]{ showProperties(); });
        fileMenu->addSeparator();
        action(fileMenu,gpdfTr("Cerrar documento"),"closeDocumentAction",[this]{ closeDocument(); },QKeySequence("Ctrl+W"));
        action(fileMenu,gpdfTr("Salir"),"exitAction",[this]{ close(); },QKeySequence("Ctrl+Q"));
        QSettings settings(QSettings::IniFormat,QSettings::UserScope,AppIdentity::settingsOrganization,AppIdentity::settingsApplication);
        recentFiles=settings.value("recentFiles").toStringList(); recentFiles=recentFiles.mid(0,10); updateRecentActions(); updateFileActions();
    }
    void updateRecentActions() {
        noRecentFiles->setVisible(recentFiles.isEmpty());
        for(int i=0;i<recentActions.size();++i) {
            auto item=recentActions[i]; item->setVisible(i<recentFiles.size());
            if(i<recentFiles.size()) {
                auto path=recentFiles[i]; item->setText(QString("%1  %2").arg(i+1).arg(QFileInfo(path).fileName().replace("&","&&")));
                item->setToolTip(path); item->setEnabled(QFileInfo::exists(path));
            }
        }
    }
    void rememberFile(const QString& path) {
        QString absolute=QFileInfo(path).absoluteFilePath();
        for(int i=recentFiles.size()-1;i>=0;--i) if(recentFiles[i].compare(absolute,Qt::CaseInsensitive)==0) recentFiles.removeAt(i);
        recentFiles.prepend(absolute); recentFiles=recentFiles.mid(0,10);
        QSettings settings(QSettings::IniFormat,QSettings::UserScope,AppIdentity::settingsOrganization,AppIdentity::settingsApplication); settings.setValue("recentFiles",recentFiles);
        updateRecentActions(); installFileMenu();
    }
    void updateFileActions() {
        if(!fileMenu) return;
        for(const auto& name:{"saveFileAction","saveAsFileAction","printAction","printPreviewAction","closeDocumentAction","propertiesAction","exportAnnotationsAction"}) if(auto action=findChild<QAction*>(name)) action->setEnabled(pdf.loaded());
        findChild<QAction*>("openFolderAction")->setEnabled(pdf.loaded() && !source.isEmpty());
#ifdef Q_OS_WIN
        if(nativeMenu) for(int i=0;i<fileActions.size();++i) EnableMenuItem(nativeMenu,fileCommandBase+UINT(i),MF_BYCOMMAND|(fileActions[i]->isEnabled() ? MF_ENABLED : MF_GRAYED));
#endif
    }
    bool paintPrintedDocument(QPrinter* printer) {
        QPainter painter;
        if(!painter.begin(printer)) { error(gpdfTr("No se pudo iniciar la impresión.")); return false; }
        int first=0,last=pdf.pageCount()-1;
        if(printer->printRange()==QPrinter::CurrentPage) first=last=pdf.pageIndex();
        else if(printer->printRange()==QPrinter::PageRange) { first=std::max(0,printer->fromPage()-1); last=std::min(last,printer->toPage()-1); }
        for(int page=first;page<=last;++page) {
            if(page>first && !printer->newPage()) { painter.end(); error(gpdfTr("No se pudo imprimir la siguiente página.")); return false; }
            auto image=pdf.renderPage(page,std::clamp(printer->resolution(),72,300)/72.0);
            if(image.isNull()) { painter.end(); error(gpdfTr("No se pudo preparar una página para imprimir.")); return false; }
            auto area=printer->pageLayout().paintRectPixels(printer->resolution());
            QSizeF size=image.size(); size.scale(area.size(),Qt::KeepAspectRatio);
            QRectF target(QPointF(area.center())-QPointF(size.width()/2,size.height()/2),size);
            // QPrinter's default origin is already the printable area's top-left.
            target.translate(-area.topLeft()); painter.drawImage(target,image);
        }
        painter.end(); return true;
    }
    void printDocument(bool preview) {
        if(printDialogActive || !pdf.loaded() || !view->finishText()) return;
        QScopedValueRollback<bool> printGuard(printDialogActive,true);
        QPrinter printer(QPrinter::HighResolution); printer.setDocName(QFileInfo(source).fileName());
        auto size=pdf.pageSize(); printer.setPageOrientation(size.width()>size.height() ? QPageLayout::Landscape : QPageLayout::Portrait);
        if(preview) {
            QPrintPreviewDialog dialog(&printer,this); dialog.setWindowTitle(gpdfTr("Vista previa de impresión")); dialog.resize(1000,800);
            connect(&dialog,&QPrintPreviewDialog::paintRequested,this,[this](QPrinter* target){ paintPrintedDocument(target); }); dialog.exec();
        } else {
            QPrintDialog dialog(&printer,this); dialog.setMinMax(1,pdf.pageCount());
            dialog.setOption(QAbstractPrintDialog::PrintPageRange); dialog.setOption(QAbstractPrintDialog::PrintCurrentPage);
            if(dialog.exec()==QDialog::Accepted) paintPrintedDocument(&printer);
        }
    }
    bool closeDocument() {
        if(!pdf.loaded() || !confirmDiscard()) return false;
        stopAutoScroll();
        captureDocument();
        int removed=activeDocument;
        pdf.closeDocument();
        if(removed>=0) documents.erase(documents.begin()+removed);
        activeDocument=-1;
        if(!documents.empty()) { bool switched=switchDocument(std::min(removed,int(documents.size())-1)); if(switched) saveSession(); return switched; }
        source.clear(); resetSearch(); searchAction->setChecked(false); togglePanel->setChecked(false);
        view->newDocument(); view->setRotation(0); view->setFit(PageView::Manual); view->setZoom(1.25);
        viewMode->setCurrentIndex(0); view->setMode(PageView::Continuous); refresh();
        saveSession(); statusBar()->showMessage(gpdfTr("Hacer clic en el área vacía para abrir un PDF.")); return true;
    }
    QTextEdit* focusedTextEditor() const {
        return qobject_cast<QTextEdit*>(QApplication::focusWidget());
    }
    void copySelection(bool noBreaks=false) {
        QString text;
        if(auto editor=focusedTextEditor()) text=editor->textCursor().selectedText().replace(QChar::ParagraphSeparator,'\n');
        else if(auto input=qobject_cast<QLineEdit*>(QApplication::focusWidget())) text=input->selectedText();
        else if(view->allTextSelected) { for(int page=0;page<pdf.pageCount();++page) { if(page) text+='\n'; text+=pdf.pageText(page); } }
        else text=pdf.selectionText(view->first,view->last);
        if(noBreaks) text=text.simplified();
        if(!text.isEmpty()) { QApplication::clipboard()->setText(text); return; }
        int index=selectedAnnotationIndex();
        if(index>=0) {
            copiedAnnotation=pdf.copyAnnotation(index);
            if(copiedAnnotation.type>=0) {
                auto data=new QMimeData; data->setText(copiedAnnotation.text); data->setData("application/x-study-pdf-annotation","1");
                QApplication::clipboard()->setMimeData(data);
                updateEditActions();
            }
        }
    }
    void selectAllText(bool wholeDocument) {
        if(auto editor=focusedTextEditor()) { editor->selectAll(); return; }
        if(auto input=qobject_cast<QLineEdit*>(QApplication::focusWidget())) { input->selectAll(); return; }
        if(!view->finishText() || !pdf.loaded()) return;
        view->resetSelection(); view->allTextSelected=wholeDocument;
        view->first=0; view->last=pdf.characterCount()-1; view->update();
    }
    void historyAction(bool redo) {
        if(auto editor=focusedTextEditor()) { if(redo) editor->redo(); else editor->undo(); return; }
        if(auto input=qobject_cast<QLineEdit*>(QApplication::focusWidget())) { if(redo) input->redo(); else input->undo(); return; }
        if(!view->finishText()) return;
        if(redo ? pdf.redo() : pdf.undo()) { view->newDocument(); refresh(); }
    }
    void createEditActions() {
        auto action=[this](const QString& title,const QString& name,auto handler,const QKeySequence& shortcut=QKeySequence()) {
            auto item=new QAction(title,this); item->setObjectName(name); item->setShortcut(shortcut); addAction(item); editActions.append(item);
            connect(item,&QAction::triggered,this,handler); return item;
        };
        auto separator=[this]{ auto item=new QAction(this); item->setSeparator(true); editActions.append(item); };
        action(gpdfTr("Deshacer"),"undoAction",[this]{ historyAction(false); },QKeySequence("Ctrl+Z"));
        action(gpdfTr("Rehacer"),"redoAction",[this]{ historyAction(true); },QKeySequence("Ctrl+Shift+Z")); separator();
        action(gpdfTr("Copiar"),"copyAction",[this]{ copySelection(); },QKeySequence::Copy);
        action(gpdfTr("Copiar texto (sin saltos de línea)"),"copyPlainAction",[this]{ copySelection(true); },QKeySequence("Ctrl+Shift+C"));
        action(gpdfTr("Pegar anotación"),"pasteAnnotationAction",[this]{
            if(auto editor=focusedTextEditor()) { editor->paste(); return; }
            if(auto input=qobject_cast<QLineEdit*>(QApplication::focusWidget())) { input->paste(); return; }
            if(!view->finishText() || copiedAnnotation.type<0 || !QApplication::clipboard()->mimeData()->hasFormat("application/x-study-pdf-annotation")) return;
            if(!pdf.pasteAnnotation(copiedAnnotation,{60,pdf.pageSize().height()-60})) error(pdf.error());
            else refresh(pdf.annotationCount()-1);
        },QKeySequence::Paste); separator();
        action(gpdfTr("Seleccionar todo"),"selectAllAction",[this]{ selectAllText(true); },QKeySequence("Ctrl+Shift+A"));
        action(gpdfTr("Seleccionar todo el texto de la página actual"),"selectPageTextAction",[this]{ selectAllText(false); });
        separator();
        action(gpdfTr("Agregar marcador personal…"),"addPersonalBookmarkAction",[this]{ promptPersonalBookmark(); },QKeySequence("Ctrl+D"));
        updateEditActions();
        connect(QApplication::clipboard(),&QClipboard::dataChanged,this,[this]{ updateEditActions(); });
        connect(qApp,&QApplication::focusChanged,this,[this]{ updateEditActions(); });
    }
    void updateEditActions() {
        if(editActions.isEmpty()) return;
        bool input=focusedTextEditor() || qobject_cast<QLineEdit*>(QApplication::focusWidget());
        bool canUndo=pdf.canUndo(),canRedo=pdf.canRedo();
        if(auto editor=focusedTextEditor()) { canUndo=editor->document()->isUndoAvailable(); canRedo=editor->document()->isRedoAvailable(); }
        if(auto line=qobject_cast<QLineEdit*>(QApplication::focusWidget())) { canUndo=line->isUndoAvailable(); canRedo=line->isRedoAvailable(); }
        // Keep shortcuts available inside text fields; those fields update their own undo state.
        editActions[0]->setEnabled(input || canUndo); editActions[1]->setEnabled(input || canRedo);
        findChild<QAction*>("pasteAnnotationAction")->setEnabled(input || (pdf.loaded() && copiedAnnotation.type>=0 && QApplication::clipboard()->mimeData()->hasFormat("application/x-study-pdf-annotation")));
#ifdef Q_OS_WIN
        if(nativeMenu) for(int i=0;i<editActions.size();++i) {
            if(editActions[i]->isSeparator()) continue;
            EnableMenuItem(nativeMenu,editCommandBase+UINT(i),MF_BYCOMMAND|(editActions[i]->isEnabled() ? MF_ENABLED : MF_GRAYED));
        }
#endif
    }
    void syncViewActions() {
        if(viewActions.isEmpty()) return;
        const QStringList panels={"showThumbnailsAction","showBookmarksAction","showPersonalBookmarksAction"};
        for(int i=0;i<viewActions.size();++i) {
            auto action=viewActions[i]; QSignalBlocker blocker(action);
            if(action->property("fitMode").isValid()) action->setChecked(view->fit==PageView::Fit(action->property("fitMode").toInt()));
            if(action->property("pageMode").isValid()) action->setChecked(view->mode==PageView::Mode(action->property("pageMode").toInt()));
            if(action->property("rotation").isValid()) action->setChecked(view->rotation==action->property("rotation").toInt());
            if(action->objectName()=="toggleToolbarAction") action->setChecked(!mainToolbar->isHidden());
            if(action->objectName()=="focusSearch") action->setChecked(searchAction->isChecked());
            int panelIndex=panels.indexOf(action->objectName());
            if(panelIndex>=0) action->setChecked(!navigationPanel->isHidden() && navigationPanel->currentIndex()==panelIndex);
#ifdef Q_OS_WIN
            if(nativeMenu) CheckMenuItem(nativeMenu,viewCommandBase+UINT(i),MF_BYCOMMAND|(action->isChecked() ? MF_CHECKED : MF_UNCHECKED));
#endif
        }
        { QSignalBlocker blocker(zoomPercent); zoomPercent->setValue(int(std::lround((view->fit==PageView::Manual ? view->scale : view->effectiveScale)*100))); }
    }
    void fitView(PageView::Fit fit) {
        if(!view->finishText()) { syncViewActions(); return; }
        bool previous=updatingView; updatingView=true;
        view->setViewportSize(scroll->viewport()->size()); view->setFit(fit);
        scroll->verticalScrollBar()->setValue(int(view->activeRect().top()));
        updatingView=previous; syncViewActions();
    }
    void rotateView(int rotation) {
        if(!view->finishText()) { syncViewActions(); return; }
        bool previous=updatingView; updatingView=true;
        view->setRotation(rotation); scroll->verticalScrollBar()->setValue(int(view->activeRect().top()));
        updatingView=previous; syncViewActions();
    }
    void createViewActions() {
        viewMenu=new QMenu(gpdfTr("&Ver"),this); viewMenu->setObjectName("viewMenu");
        auto action=[this](QMenu* menu,const QString& title,const QString& name,auto handler,const QKeySequence& shortcut=QKeySequence()) {
            auto item=menu->addAction(title); item->setObjectName(name); item->setShortcut(shortcut); addAction(item); viewActions.append(item);
            connect(item,&QAction::triggered,this,handler); return item;
        };
        action(viewMenu,gpdfTr("Reducir"),"viewZoomOut",[this]{ zoom(-0.25); },QKeySequence("Ctrl+-"));
        auto plus=action(viewMenu,gpdfTr("Ampliar"),"viewZoomIn",[this]{ zoom(0.25); },QKeySequence("Ctrl++"));
        plus->setShortcuts({QKeySequence("Ctrl++"),QKeySequence("Ctrl+=" )});
        action(viewMenu,gpdfTr("Ampliar al 100%"),"viewActualSize",[this]{ setZoom(1.0); },QKeySequence("Ctrl+0"));
        const QStringList fitNames={gpdfTr("Ajustar al ancho"),gpdfTr("Ajustar a la página")};
        auto fits=new QActionGroup(this); fits->setExclusionPolicy(QActionGroup::ExclusionPolicy::ExclusiveOptional);
        for(int i=0;i<fitNames.size();++i) {
            auto item=action(viewMenu,fitNames[i],QString("fitMode%1").arg(i+1),[this,i](bool checked){ fitView(checked ? PageView::Fit(i+1) : PageView::Manual); });
            item->setCheckable(true); item->setProperty("fitMode",i+1); fits->addAction(item);
        }
        viewMenu->addSeparator(); auto modes=viewMenu->addMenu(gpdfTr("Modo de vista")); auto modesGroup=new QActionGroup(this);
        const QStringList names={gpdfTr("Vista continua"),gpdfTr("Página individual"),gpdfTr("Dos páginas")};
        for(int i=0;i<3;++i) {
            auto item=action(modes,names[i],QString("menuPageMode%1").arg(i),[this,i]{ viewMode->setCurrentIndex(i); });
            item->setCheckable(true); item->setProperty("pageMode",i); modesGroup->addAction(item);
        }
        auto orientations=viewMenu->addMenu(gpdfTr("Orientación")); auto rotationGroup=new QActionGroup(this);
        const QStringList rotations={gpdfTr("Original"),gpdfTr("90° a la derecha"),gpdfTr("180°"),gpdfTr("90° a la izquierda")};
        for(int i=0;i<4;++i) {
            auto item=action(orientations,rotations[i],QString("viewRotation%1").arg(i),[this,i]{ rotateView(i); });
            item->setCheckable(true); item->setProperty("rotation",i); rotationGroup->addAction(item);
        }
        viewMenu->addSeparator();
        auto toolbar=action(viewMenu,gpdfTr("Mostrar barra de herramientas"),"toggleToolbarAction",[this](bool visible){ mainToolbar->setVisible(visible); syncViewActions(); });
        toolbar->setCheckable(true); toolbar->setChecked(!mainToolbar->isHidden());
        connect(mainToolbar,&QToolBar::visibilityChanged,this,[this]{ syncViewActions(); });
        syncViewActions();
    }


    quint64 navigationRevision=0;
    QString navigationDocumentId;
    int thumbnailRotation=-1,thumbnailFirst=0,thumbnailLast=-1;
    std::atomic<int> thumbnailGeneration{0};
    QSet<int> thumbnailPending,thumbnailLoaded;
    void updateNavigation() {
        if(!navigationPanel) return;
        if(!pdf.loaded()) { QSignalBlocker blocker(thumbnails); thumbnails->clear(); outline->clear(); navigationRevision=0; navigationDocumentId.clear(); ++thumbnailGeneration; thumbnailPending.clear(); thumbnailLoaded.clear(); return; }
        if(navigationPanel->isHidden()) return;
        QSignalBlocker thumbnailBlocker(thumbnails);
        quint64 revision=pdf.revision();
        const QString documentId=activeDocument>=0 ? documents[activeDocument]->id : QString();
        const bool newDocument=navigationDocumentId!=documentId || thumbnails->count()!=pdf.pageCount();
        if(newDocument || navigationRevision!=revision || thumbnailRotation!=view->rotation) {
            navigationRevision=revision; thumbnailRotation=view->rotation; ++thumbnailGeneration;
            thumbnailPending.clear();
            for(int page:thumbnailLoaded) if(auto item=thumbnails->item(page)) item->setIcon(QIcon());
            thumbnailLoaded.clear();
        }
        if(newDocument) {
            navigationDocumentId=documentId; thumbnails->clear(); outline->clear();
            for(int page=0;page<pdf.pageCount();++page) { auto item=new QListWidgetItem(QString(gpdfTr("Página %1")).arg(page+1),thumbnails); item->setSizeHint(QSize(160,220)); }
            QVector<QTreeWidgetItem*> parents;
            for(const auto& bookmark:pdf.bookmarks()) {
                while(parents.size()>bookmark.depth) parents.removeLast();
                auto item=parents.isEmpty() ? new QTreeWidgetItem(outline) : new QTreeWidgetItem(parents.last());
                item->setText(0,bookmark.title); if(bookmark.page>=0) { item->setData(0,Qt::UserRole,bookmark.page); item->setToolTip(0,QString(gpdfTr("Página %1")).arg(bookmark.page+1)); } parents.append(item);
            }
            if(outline->topLevelItemCount()==0) { auto item=new QTreeWidgetItem(outline); item->setText(0,gpdfTr("Sin marcadores en este PDF")); item->setDisabled(true); }
            outline->expandToDepth(0);
        }
        { QSignalBlocker block(thumbnails); thumbnails->setCurrentRow(pdf.pageIndex()); }
        requestThumbnails();
    }
    void requestThumbnails() {
        if(!pdf.loaded() || navigationPanel->isHidden() || navigationPanel->currentIndex()!=0 || navigationRevision!=pdf.revision()) return;
        int top=thumbnails->indexAt(QPoint(10,10)).row(); if(top<0) top=pdf.pageIndex();
        int bottom=std::min(pdf.pageCount()-1,top+std::max(2,thumbnails->height()/220)+2); top=std::max(0,top-2);
        thumbnailFirst=top; thumbnailLast=bottom;
        for(int loaded:thumbnailLoaded.values()) if(loaded<top || loaded>bottom) { if(auto item=thumbnails->item(loaded)) item->setIcon(QIcon()); thumbnailLoaded.remove(loaded); }
        for(int page=top;page<=bottom;++page) if(!thumbnailLoaded.contains(page) && !thumbnailPending.contains(page)) {
            thumbnailPending.insert(page); quint64 revision=navigationRevision; int rotation=view->rotation,generation=thumbnailGeneration; QSizeF size=pdf.pageSize(page);
            const double width=rotation%2 ? size.height() : size.width(),height=rotation%2 ? size.width() : size.height();
            if(width<=0 || height<=0) { thumbnailPending.remove(page); continue; }
            double scale=std::min(140.0/width,190.0/height); QPointer<Window> target(this);
            view->workers.start([target,revision,page,scale,rotation,generation]{
                QImage image; { std::lock_guard guard(PdfDocument::apiMutex()); if(target && target->thumbnailGeneration==generation && target->pdf.revision()==revision) image=target->pdf.renderPage(page,scale); }
                if(rotation) image=image.transformed(QTransform().rotate(rotation*90));
                if(target) QMetaObject::invokeMethod(target.data(),[target,revision,page,image,generation]{ if(!target || target->navigationRevision!=revision || target->thumbnailGeneration!=generation) return; target->thumbnailPending.remove(page); if(page<target->thumbnailFirst || page>target->thumbnailLast) return; if(auto item=target->thumbnails->item(page); item && !image.isNull()) { item->setIcon(QPixmap::fromImage(image)); target->thumbnailLoaded.insert(page); } },Qt::QueuedConnection);
            });
        }
    }
    QMenu* drawingMenu(bool pencil,QWidget* parent) {
        auto menu=new DownwardToolMenu(parent); auto panel=new QWidget; auto form=new QFormLayout(panel);
        auto colors=new QComboBox; colors->setObjectName(pencil ? "penColor" : "highlightColor");
        const QStringList names={gpdfTr("Negro"),gpdfTr("Azul"),gpdfTr("Rojo"),gpdfTr("Verde"),gpdfTr("Naranja"),gpdfTr("Amarillo"),gpdfTr("Violeta"),gpdfTr("Blanco")};
        const QStringList values={"#000000","#2855b4","#cc2244","#128012","#e88810","#ffd232","#8844cc","#ffffff"};
        for(int i=0;i<names.size();++i) { QPixmap swatch(16,16); swatch.fill(QColor(values[i])); colors->addItem(QIcon(swatch),names[i],QColor(values[i])); }
        colors->setCurrentIndex(pencil ? 1 : 5); form->addRow(pencil ? gpdfTr("Color:") : gpdfTr("Color primario:"),colors);
        QComboBox* secondary=nullptr;
        if(!pencil) { secondary=new QComboBox; secondary->setObjectName("secondaryHighlightColor"); for(int i=0;i<colors->count();++i) secondary->addItem(colors->itemIcon(i),colors->itemText(i),colors->itemData(i)); secondary->setCurrentIndex(4); form->addRow(gpdfTr("Color secundario:"),secondary); }
        auto opacity=new QSpinBox; opacity->setObjectName(pencil ? "penOpacity" : "highlightOpacity"); opacity->setRange(5,100); opacity->setSuffix("%"); opacity->setValue(pencil ? 100 : 60); form->addRow(gpdfTr("Opacidad:"),opacity);
        auto width=new QDoubleSpinBox; width->setObjectName("penWidth"); width->setRange(0.5,12); width->setSingleStep(0.5); width->setSuffix(" pt"); width->setValue(2); if(pencil) form->addRow(gpdfTr("Grosor:"),width); else width->deleteLater();
        auto widgetAction=new QWidgetAction(menu); widgetAction->setDefaultWidget(panel); menu->addAction(widgetAction);
        auto update=[this,pencil,colors,secondary,opacity,width]{
            QColor color=colors->currentData().value<QColor>(); color.setAlpha(int(std::lround(opacity->value()*255.0/100)));
            QSettings settings(QSettings::IniFormat,QSettings::UserScope,AppIdentity::settingsOrganization,AppIdentity::settingsApplication);
            if(pencil) { view->penColor=color; view->penWidth=width->value(); settings.setValue("drawing/penColor",color); settings.setValue("drawing/penWidth",view->penWidth); }
            else { view->highlightColor=color; view->secondaryHighlightColor=secondary->currentData().value<QColor>(); view->secondaryHighlightColor.setAlpha(color.alpha()); settings.setValue("drawing/highlightColor",color); settings.setValue("drawing/secondaryHighlightColor",view->secondaryHighlightColor); settings.setValue("drawing/highlightOpacity",opacity->value()); }
            updateDrawingIcons(); view->update();
        };
        QSettings settings(QSettings::IniFormat,QSettings::UserScope,AppIdentity::settingsOrganization,AppIdentity::settingsApplication);
        if(!pencil && !settings.value("drawing/highlightOpacity60Migrated",false).toBool()) {
            if(settings.value("drawing/highlightOpacity",40).toInt()==40) settings.setValue("drawing/highlightOpacity",60);
            settings.setValue("drawing/highlightOpacity60Migrated",true);
        }
        QColor saved=settings.value(pencil ? "drawing/penColor" : "drawing/highlightColor",pencil ? view->penColor : view->highlightColor).value<QColor>();
        for(int i=0;i<colors->count();++i) if(colors->itemData(i).value<QColor>().rgb()==saved.rgb()) colors->setCurrentIndex(i);
        opacity->setValue(pencil ? int(std::lround(saved.alphaF()*100)) : settings.value("drawing/highlightOpacity",60).toInt());
        if(pencil) width->setValue(settings.value("drawing/penWidth",2).toDouble());
        else { QColor savedSecondary=settings.value("drawing/secondaryHighlightColor",view->secondaryHighlightColor).value<QColor>(); for(int i=0;i<secondary->count();++i) if(secondary->itemData(i).value<QColor>().rgb()==savedSecondary.rgb()) secondary->setCurrentIndex(i); }
        connect(colors,&QComboBox::currentIndexChanged,this,update); connect(opacity,&QSpinBox::valueChanged,this,update); if(pencil) connect(width,&QDoubleSpinBox::valueChanged,this,update); else connect(secondary,&QComboBox::currentIndexChanged,this,update);
        update();
        return menu;
    }
    void applyDarkMode(bool enabled) {
        QPalette palette=QApplication::style()->standardPalette();
        if(enabled) {
            palette.setColor(QPalette::Window,QColor("#252b35")); palette.setColor(QPalette::WindowText,QColor("#e5e7eb")); palette.setColor(QPalette::Base,QColor("#191f28")); palette.setColor(QPalette::AlternateBase,QColor("#252b35")); palette.setColor(QPalette::Text,QColor("#e5e7eb")); palette.setColor(QPalette::Button,QColor("#303846")); palette.setColor(QPalette::ButtonText,QColor("#e5e7eb")); palette.setColor(QPalette::Highlight,QColor("#356fbb")); palette.setColor(QPalette::HighlightedText,Qt::white);
        }
        if(enabled) {
            palette.setColor(QPalette::PlaceholderText,QColor("#aeb8c7"));
            for(auto role:{QPalette::WindowText,QPalette::Text,QPalette::ButtonText}) palette.setColor(QPalette::Disabled,role,QColor("#aeb8c7"));
            palette.setColor(QPalette::Link,QColor("#8fc4ff"));
        }
        // Windows controls may use system text colors instead of the inherited palette.
        // Scope explicit colors to interface controls; PDF text retains its own color.
        setStyleSheet(enabled ?
            "QLabel, QCheckBox, QRadioButton, QGroupBox, QTabBar, QStatusBar { color: #e5e7eb; }"
            "QToolBar, QTabWidget::pane, QWidget#personalBookmarksPanel, QDialog { background-color: #252b35; color: #e5e7eb; }"
            "QTabWidget::pane { border: 1px solid #303846; }"
            "QKeySequenceEdit, QTextBrowser, QPlainTextEdit, QTextEdit { color: #e5e7eb; background-color: #191f28; }"
            "QHeaderView::section { color: #e5e7eb; background-color: #303846; }"
            "QTabBar::tab { color: #e5e7eb; background-color: #303846; padding: 5px 8px; }"
            "QTabBar::tab:selected { background-color: #356fbb; }"
            "QLineEdit, QComboBox, QAbstractSpinBox, QAbstractItemView { color: #e5e7eb; background-color: #191f28; selection-color: white; selection-background-color: #356fbb; }"
            "QPushButton, QToolButton, QMenu { color: #e5e7eb; background-color: #303846; }"
            "QMenu::item:selected { background-color: #356fbb; }"
            "QLabel:disabled, QCheckBox:disabled, QRadioButton:disabled, QPushButton:disabled, QToolButton:disabled, QComboBox:disabled, QAbstractSpinBox:disabled, QMenu::item:disabled { color: #aeb8c7; }"
            : QString());
        QApplication::setPalette(palette); setPalette(palette); scroll->setStyleSheet(enabled ? "QScrollArea { background: #20252d; border: 0; }" : "QScrollArea { background: #e9edf2; border: 0; }"); view->setDarkMode(enabled); updateToolbarLayout();
        QSettings settings(QSettings::IniFormat,QSettings::UserScope,AppIdentity::settingsOrganization,AppIdentity::settingsApplication); settings.setValue("reading/darkMode",enabled);
    }
    void createReadingActions() {
        auto action=[this](QMenu* menu,QList<QAction*>& list,const QString& title,const QString& name,auto handler,const QString& shortcut=QString()) { auto item=menu->addAction(title); item->setObjectName(name); item->setShortcut(QKeySequence(shortcut)); addAction(item); list.append(item); connect(item,&QAction::triggered,this,handler); return item; };
        viewMenu->addSeparator();
        auto search=findChild<QAction*>("focusSearch"); search->setText(gpdfTr("Búsqueda"));
        viewMenu->addAction(search); viewActions.append(search);
        viewMenu->addAction(togglePanel); viewActions.append(togglePanel);
        action(viewMenu,viewActions,gpdfTr("Miniaturas"),"showThumbnailsAction",[this]{ navigationPanel->setVisible(navigationPanel->isHidden() || navigationPanel->currentIndex()!=0); navigationPanel->setCurrentIndex(0); updateNavigation(); });
        action(viewMenu,viewActions,gpdfTr("Marcadores del PDF"),"showBookmarksAction",[this]{ navigationPanel->setVisible(navigationPanel->isHidden() || navigationPanel->currentIndex()!=1); navigationPanel->setCurrentIndex(1); updateNavigation(); });
        action(viewMenu,viewActions,gpdfTr("Marcadores personales"),"showPersonalBookmarksAction",[this]{ navigationPanel->setVisible(navigationPanel->isHidden() || navigationPanel->currentIndex()!=2); navigationPanel->setCurrentIndex(2); refreshPersonalBookmarks(); });
        for(const auto& name:{"showThumbnailsAction","showBookmarksAction","showPersonalBookmarksAction"}) {
            auto item=findChild<QAction*>(name); item->setCheckable(true);
            connect(item,&QAction::triggered,this,[this]{ syncViewActions(); });
        }
        viewMenu->addSeparator();
        auto dark=action(viewMenu,viewActions,gpdfTr("Modo oscuro"),"darkModeAction",[this](bool enabled){ applyDarkMode(enabled); }); dark->setCheckable(true);
        auto fullscreen=action(viewMenu,viewActions,gpdfTr("Pantalla completa"),"fullscreenAction",[this](bool enabled){ setFullscreen(enabled); },"F11"); fullscreen->setCheckable(true);
        auto tools=findChild<QToolBar*>("editingTools");
        for(bool pencil:{true,false}) { auto button=qobject_cast<QToolButton*>(tools->widgetForAction(findChild<QAction*>(pencil ? "editTool3" : "highlightAction"))); button->setMenu(drawingMenu(pencil,button)); button->setPopupMode(QToolButton::MenuButtonPopup); }
        auto underlineButton=qobject_cast<QToolButton*>(tools->widgetForAction(findChild<QAction*>("underlineAction"))); underlineButton->setMenu(underlineMenu(underlineButton)); underlineButton->setPopupMode(QToolButton::MenuButtonPopup);
        auto primary=findChild<QAction*>("highlightAction"); primary->setShortcut(QKeySequence("Ctrl+S"));
        auto secondaryAction=new QAction(gpdfTr("Resaltar con color secundario"),this); secondaryAction->setObjectName("secondaryHighlightAction"); secondaryAction->setShortcut(QKeySequence("Ctrl+Shift+S")); addAction(secondaryAction);
        connect(secondaryAction,&QAction::triggered,this,[this]{ std::lock_guard guard(PdfDocument::apiMutex()); if(!pdf.loaded() || view->first<0 || view->last<0) return; if(!pdf.highlight(view->first,view->last,view->secondaryHighlightColor)) error(pdf.error()); else { view->resetSelection(); refresh(); } });
        tools->setMinimumWidth(tools->sizeHint().width()+16);
        updateToolbarLayout();
        action(fileMenu,fileActions,gpdfTr("Exportar anotaciones…"),"exportAnnotationsAction",[this]{ if(!view->finishText() || !pdf.loaded()) return; QString selectedFilter; auto path=QFileDialog::getSaveFileName(this,gpdfTr("Exportar anotaciones"),QFileInfo(source).completeBaseName()+"-anotaciones.md",gpdfTr("Markdown (*.md);;Texto (*.txt);;HTML (*.html)"),&selectedFilter,fileDialogOptions()); if(!path.isEmpty() && QFileInfo(path).suffix().isEmpty()) path+=selectedFilter.startsWith("HTML") ? ".html" : selectedFilter.startsWith(gpdfTr("Texto")) ? ".txt" : ".md"; if(!path.isEmpty() && !exportAnnotationsTo(path)) error(gpdfTr("No se pudieron exportar las anotaciones.")); });
        auto exportAction=findChild<QAction*>("exportAnnotationsAction"); fileMenu->removeAction(exportAction); fileMenu->insertAction(findChild<QAction*>("propertiesAction"),exportAction);
        QSettings settings(QSettings::IniFormat,QSettings::UserScope,AppIdentity::settingsOrganization,AppIdentity::settingsApplication); dark->setChecked(settings.value("reading/darkMode",false).toBool()); applyDarkMode(dark->isChecked());
    }
    void updateDrawingIcons() {
        for(const auto& entry:QList<QPair<QString,QPair<int,QColor>>>{{"editTool3",{3,view->penColor}},{"highlightAction",{4,view->highlightColor}},{"underlineAction",{5,view->underlineColor}}})
            if(auto action=findChild<QAction*>(entry.first)) action->setIcon(QIcon(new GpdfToolIcon(entry.second.first,entry.second.second)));
    }
    QMenu* underlineMenu(QWidget* parent) {
        auto menu=new DownwardToolMenu(parent); auto panel=new QWidget; auto form=new QFormLayout(panel);
        auto colors=new QComboBox; colors->setObjectName("underlineColor");
        const QStringList names={gpdfTr("Negro"),gpdfTr("Azul"),gpdfTr("Rojo"),gpdfTr("Verde"),gpdfTr("Naranja"),gpdfTr("Amarillo"),gpdfTr("Violeta"),gpdfTr("Blanco")};
        const QStringList values={"#000000","#2855b4","#cc2244","#128012","#e88810","#ffd232","#8844cc","#ffffff"};
        for(int i=0;i<names.size();++i) { QPixmap swatch(16,16); swatch.fill(QColor(values[i])); colors->addItem(QIcon(swatch),names[i],QColor(values[i])); }
        auto width=new QDoubleSpinBox; width->setObjectName("underlineWidth"); width->setRange(0.5,12); width->setSingleStep(0.5); width->setSuffix(" pt");
        QSettings settings(QSettings::IniFormat,QSettings::UserScope,AppIdentity::settingsOrganization,AppIdentity::settingsApplication);
        auto saved=settings.value("drawing/underlineColor",view->underlineColor).value<QColor>();
        colors->setCurrentIndex(1); for(int i=0;i<colors->count();++i) if(colors->itemData(i).value<QColor>().rgb()==saved.rgb()) colors->setCurrentIndex(i);
        width->setValue(settings.value("drawing/underlineWidth",1.5).toDouble());
        form->addRow(gpdfTr("Color:"),colors); form->addRow(gpdfTr("Grosor:"),width);
        auto widgetAction=new QWidgetAction(menu); widgetAction->setDefaultWidget(panel); menu->addAction(widgetAction);
        auto update=[this,colors,width]{ view->underlineColor=colors->currentData().value<QColor>(); view->underlineWidth=width->value(); QSettings settings(QSettings::IniFormat,QSettings::UserScope,AppIdentity::settingsOrganization,AppIdentity::settingsApplication); settings.setValue("drawing/underlineColor",view->underlineColor); settings.setValue("drawing/underlineWidth",view->underlineWidth); updateDrawingIcons(); };
        connect(colors,&QComboBox::currentIndexChanged,this,update); connect(width,&QDoubleSpinBox::valueChanged,this,update); update(); return menu;
    }
    void jumpTo(int target) {
        if(!pdf.loaded() || target<0 || target>=pdf.pageCount() || !view->finishText()) return;
        stopAutoScroll(); bool previous=updatingView; updatingView=true;
        if(pdf.setPage(target)) { view->resetSelection(); view->refreshNavigation(); refreshPanel(); scroll->verticalScrollBar()->setValue(view->mode==PageView::Continuous ? int(view->activeRect().top()) : 0); }
        updatingView=previous;
    }
    void historyNavigate(int direction) {
        if(activeDocument<0 || !view->finishText()) return;
        auto& doc=*documents[activeDocument]; int next=doc.navigationIndex+direction;
        if(next<0 || next>=doc.navigation.size()) return;
        navigatingHistory=true; doc.navigationIndex=next; jumpTo(doc.navigation[next]); navigatingHistory=false; updateGoActions();
    }
    void createGoActions() {
        goMenu=new QMenu(gpdfTr("&Ir"),this);
        auto action=[this](const QString& title,const QString& name,auto handler,const QString& shortcut) {
            auto item=goMenu->addAction(title); item->setObjectName(name); item->setShortcut(QKeySequence(shortcut)); addAction(item); goActions.append(item); connect(item,&QAction::triggered,this,handler); return item;
        };
        action(gpdfTr("Página anterior"),"goPrevious",[this]{ page(-1); },"Left");
        action(gpdfTr("Página siguiente"),"goNext",[this]{ page(1); },"Right"); goMenu->addSeparator();
        action(gpdfTr("Principio del documento"),"goFirst",[this]{ jumpTo(0); },"Ctrl+Home");
        action(gpdfTr("Final del documento"),"goLast",[this]{ jumpTo(pdf.pageCount()-1); },"Ctrl+End"); goMenu->addSeparator();
        action(gpdfTr("Atrás"),"goBack",[this]{ historyNavigate(-1); },"Alt+Shift+Left");
        action(gpdfTr("Avanzar"),"goForward",[this]{ historyNavigate(1); },"Alt+Shift+Right"); goMenu->addSeparator();
        action(gpdfTr("Ir a página…"),"goPage",[this]{ if(!pdf.loaded()) return; bool ok=false; int target=QInputDialog::getInt(this,gpdfTr("Ir a página"),gpdfTr("Página:"),pdf.pageIndex()+1,1,pdf.pageCount(),1,&ok); if(ok) jumpTo(target-1); },"Ctrl+I");
        updateGoActions();
    }
    void updateGoActions() {
        for(int i=0;i<goActions.size();++i) {
            auto action=goActions[i]; bool enabled=pdf.loaded();
            const auto name=action->objectName();
            if(name=="goPrevious" || name=="goFirst") enabled=enabled && pdf.pageIndex()>0;
            if(name=="goNext" || name=="goLast") enabled=enabled && (name=="goNext" && view->mode==PageView::Double ? (pdf.pageIndex()/2+1)*2<pdf.pageCount() : pdf.pageIndex()+1<pdf.pageCount());
            if(name=="goBack" || name=="goForward") {
                enabled=enabled && activeDocument>=0;
                if(enabled) { const auto& doc=*documents[activeDocument]; enabled=name=="goBack" ? doc.navigationIndex>0 : doc.navigationIndex+1<doc.navigation.size(); }
            }
            action->setEnabled(enabled);
#ifdef Q_OS_WIN
            if(nativeMenu) EnableMenuItem(nativeMenu,goCommandBase+UINT(i),MF_BYCOMMAND|(enabled ? MF_ENABLED : MF_GRAYED));
#endif
        }
    }
    void showProperties() {
        if(!pdf.loaded() || !view->finishText()) return;
        QDialog dialog(this); dialog.setObjectName("propertiesDialog"); dialog.setWindowTitle(gpdfTr("Propiedades del archivo")); dialog.resize(650,620);
        auto layout=new QVBoxLayout(&dialog); auto form=new QFormLayout;
        auto info=QFileInfo(source); auto size=pdf.pageSize();
        QVector<QPair<QString,QString>> values={{gpdfTr("Nombre"),info.fileName()},{gpdfTr("Ubicación"),info.absolutePath()},{gpdfTr("Tamaño en disco"),QString("%1 bytes (%2 MiB)").arg(info.size()).arg(info.size()/1048576.0,0,'f',2)},{gpdfTr("Páginas"),QString::number(pdf.pageCount())},{gpdfTr("Tamaño de la página actual"),QString("%1 × %2 mm").arg(size.width()*25.4/72,0,'f',1).arg(size.height()*25.4/72,0,'f',1)}};
        values+=pdf.properties();
        for(const auto& value:values) { auto field=new QLineEdit(value.second); field->setReadOnly(true); field->setToolTip(value.second); form->addRow(value.first+":",field); }
        layout->addLayout(form); auto buttons=new QDialogButtonBox(QDialogButtonBox::Close); layout->addWidget(buttons); connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject); dialog.exec();
    }
    void createShortcutActions() {
        auto extra=[this](const QString& title,const QString& name,const QString& shortcut,auto handler) {
            auto action=new QAction(title,this); action->setObjectName(name); action->setShortcut(QKeySequence(shortcut)); addAction(action); connect(action,&QAction::triggered,this,handler); return action;
        };
        auto remove=extra(gpdfTr("Eliminar anotación seleccionada"),"deleteSelectionAction","Delete",[this]{ if(!view->finishText()) return; deleteSelected(); }); editActions.append(remove);
        extra(gpdfTr("Documento siguiente"),"nextDocumentAction","Ctrl+Tab",[this]{ if(documents.size()>1) switchDocument((activeDocument+1)%int(documents.size())); });
        extra(gpdfTr("Documento anterior"),"previousDocumentAction","Ctrl+Shift+Tab",[this]{ if(documents.size()>1) switchDocument((activeDocument+int(documents.size())-1)%int(documents.size())); });
        auto confirm=extra(gpdfTr("Confirmar texto (durante edición)"),"confirmTextAction","Return",[this]{ view->finishText(); }); removeAction(confirm);
        auto cancel=extra(gpdfTr("Cancelar / detener / salir de pantalla completa"),"cancelContextAction","Esc",[this]{ if(view->annotationDragging()) view->cancelAnnotationDrag(); else if(autoScrollTimer->isActive()) stopAutoScroll(); else if(view->commentVisible()) view->hideComment(); else if(view->findChild<QWidget*>("inlineTextEditor")) view->cancelText(); else if(isFullScreen()) { setFullscreen(false); } }); removeAction(cancel);
        auto enter=extra(gpdfTr("Buscar / resultado siguiente (en Búsqueda)"),"searchEnterAction","Return",[this]{ searchEnter(); }); removeAction(enter);
        shortcutsMenu=new QMenu(gpdfTr("&Atajos"),this);
        auto configure=extra(gpdfTr("Ver y modificar atajos…"),"configureShortcutsAction",{},[this]{ configureShortcuts(); }); shortcutsMenu->addAction(configure); shortcutMenuActions.append(configure);
        QSettings settings(QSettings::IniFormat,QSettings::UserScope,AppIdentity::settingsOrganization,AppIdentity::settingsApplication); settings.beginGroup("shortcuts");
        if(!settings.value("iconToolShortcutsMigrated",false).toBool()) {
            const QHash<QString,QString> requested={{"editTool0","F1"},{"editTool1","F2"},{"editTool2","F3"},{"editTool3","F4"},{"highlightAction","Ctrl+S"},{"secondaryHighlightAction","Ctrl+Shift+S"},{"saveFileAction","Ctrl+G"},{"saveAsFileAction","Ctrl+Shift+G"},{"goPage","Ctrl+I"}};
            const QHash<QString,QString> legacyDefaults={{"highlightAction","Ctrl+H"},{"saveFileAction","Ctrl+S"},{"saveAsFileAction","Ctrl+Shift+S"},{"goPage","Ctrl+G"}};
            for(auto i=requested.cbegin();i!=requested.cend();++i)
                if(!settings.contains(i.key()) || (legacyDefaults.contains(i.key()) && settings.value(i.key()).toString()==legacyDefaults.value(i.key()))) settings.setValue(i.key(),i.value());
            settings.setValue("iconToolShortcutsMigrated",true);
        }
        // This old, invisible list action used to own F2. It must not compete with Comentario.
        settings.remove("editSelectedAnnotationAction");
        settings.remove("menuFindAction");
        if(!settings.value("eraserShortcutMigrated",false).toBool()) {
            if(settings.contains("editTool4") && settings.value("editTool4").toString().isEmpty()) settings.remove("editTool4");
            settings.setValue("eraserShortcutMigrated",true);
        }
        if(!settings.value("spanishSearchShortcutMigrated",false).toBool()) {
            if(settings.value("focusSearch","Ctrl+F").toString()=="Ctrl+F") settings.setValue("focusSearch","Ctrl+B");
            settings.setValue("spanishSearchShortcutMigrated",true);
        }
        const bool english=currentUiLanguage()=="en";
        const QHash<QString,QString> spanishDefaults={{"saveFileAction","Ctrl+G"},{"saveAsFileAction","Ctrl+Shift+G"},{"highlightAction","Ctrl+S"},{"secondaryHighlightAction","Ctrl+Shift+S"},{"focusSearch","Ctrl+B"},{"goPage","Ctrl+I"},{"toggleAnnotations","Ctrl+N"},{"underlineAction","Ctrl+U"}};
        auto languageDefaults=spanishDefaults;
        if(english) {
            languageDefaults["saveFileAction"]="Ctrl+S"; languageDefaults["saveAsFileAction"]="Ctrl+Shift+S";
            languageDefaults["highlightAction"]="Ctrl+H"; languageDefaults["secondaryHighlightAction"]="Ctrl+Shift+H";
            languageDefaults["focusSearch"]="Ctrl+F"; languageDefaults["goPage"]="Ctrl+G";
        }
        // Migrate prior defaults, preserving explicit custom assignments.
        const bool previousEnglish=settings.value("defaultsLanguage","es").toString()=="en";
        auto previousDefaults=spanishDefaults;
        if(previousEnglish) {
            previousDefaults["saveFileAction"]="Ctrl+S"; previousDefaults["saveAsFileAction"]="Ctrl+Shift+S";
            previousDefaults["highlightAction"]="Ctrl+H"; previousDefaults["secondaryHighlightAction"]="Ctrl+Shift+H";
            previousDefaults["focusSearch"]="Ctrl+F"; previousDefaults["goPage"]="Ctrl+G";
        }
        if(!settings.value("releaseShortcutsMigrated",false).toBool()) {
            previousDefaults["toggleAnnotations"]="Ctrl+A"; previousDefaults["underlineAction"]="";
        }
        for(auto i=languageDefaults.cbegin();i!=languageDefaults.cend();++i) {
            auto item=findChild<QAction*>(i.key()); item->setShortcut(QKeySequence(i.value()));
            if(settings.contains(i.key()) && settings.value(i.key()).toString()==previousDefaults.value(i.key())) settings.remove(i.key());
        }
        settings.setValue("releaseShortcutsMigrated",true);
        settings.setValue("defaultsLanguage",english ? "en" : "es");
        if(!settings.value("incrementalRotationShortcutMigrated",false).toBool()) {
            settings.remove("rotateClockwiseAction");
            settings.setValue("incrementalRotationShortcutMigrated",true);
        }
        for(auto action:findChildren<QAction*>()) {
            auto name=action->objectName();
            if(action->property("rotation").isValid()) {
                action->setShortcut(QKeySequence()); settings.remove(name); continue;
            }
            if(action->text().trimmed().isEmpty() || name.isEmpty() || name.startsWith("recentFile") || name=="menuFindAction" || name=="searchAction" || action->isSeparator()) continue;
            shortcutActions.append(action); action->setProperty("defaultShortcut",action->shortcut().toString(QKeySequence::PortableText));
            if(name=="highlightAction" && !settings.value("highlightShortcutMigrated",false).toBool()) {
                if(settings.contains(name) && settings.value(name).toString().isEmpty()) settings.remove(name);
                settings.setValue("highlightShortcutMigrated",true);
            }
            if(settings.contains(name)) action->setShortcut(QKeySequence(settings.value(name).toString()));
        }
    }
    void updateToolTips() {
        for(const auto& name:{"editTool0","editTool1","editTool2","editTool3","editTool4","highlightAction","underlineAction","rotateClockwiseAction"}) {
            auto action=findChild<QAction*>(name); if(!action) continue;
            QString tip=action->text(); if(!action->shortcut().isEmpty()) tip+=" ("+action->shortcut().toString(QKeySequence::NativeText)+")";
            action->setToolTip(tip);
            auto toolbar=findChild<QToolBar*>("editingTools");
            if(auto button=qobject_cast<QToolButton*>(toolbar->widgetForAction(action))) { button->setToolTip(tip); button->setAccessibleName(action->text()); }
        }
    }
    void configureShortcuts() {
        QDialog dialog(this); dialog.setObjectName("shortcutsDialog"); dialog.setWindowTitle(gpdfTr("Atajos de teclado")); dialog.resize(700,650);
        auto layout=new QVBoxLayout(&dialog); layout->addWidget(new QLabel(gpdfTr("Elegir una función y presionar su nueva combinación. Un campo vacío elimina el atajo.")));
        auto table=new QTableWidget(shortcutActions.size(),2); table->setObjectName("shortcutsTable"); table->setHorizontalHeaderLabels({gpdfTr("Función"),gpdfTr("Atajo")}); table->verticalHeader()->hide(); table->horizontalHeader()->setSectionResizeMode(0,QHeaderView::Stretch); table->horizontalHeader()->setSectionResizeMode(1,QHeaderView::ResizeToContents);
        QVector<QKeySequenceEdit*> editors;
        for(int i=0;i<shortcutActions.size();++i) { auto item=new QTableWidgetItem(shortcutActions[i]->text().remove('&')); item->setFlags(item->flags() & ~Qt::ItemIsEditable); table->setItem(i,0,item); auto editor=new QKeySequenceEdit(shortcutActions[i]->shortcut()); editor->setClearButtonEnabled(true); editor->setMinimumWidth(230); editors.append(editor); table->setCellWidget(i,1,editor); }
        layout->addWidget(table);
        auto note=new QLabel(gpdfTr("Confirmar/cancelar texto funciona durante la edición. Buscar/resultado siguiente funciona en el campo y la lista de resultados de Búsqueda. Los cambios se guardan para las próximas aperturas.")); note->setWordWrap(true); layout->addWidget(note);
        auto buttons=new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel|QDialogButtonBox::RestoreDefaults); layout->addWidget(buttons);
        connect(buttons->button(QDialogButtonBox::RestoreDefaults),&QPushButton::clicked,&dialog,[&]{ for(int i=0;i<editors.size();++i) editors[i]->setKeySequence(QKeySequence(shortcutActions[i]->property("defaultShortcut").toString())); });
        connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
        connect(buttons,&QDialogButtonBox::accepted,&dialog,[&]{
            for(int i=0;i<editors.size();++i) {
                auto key=editors[i]->keySequence();
                if(key.count()>1) { QMessageBox::warning(&dialog,gpdfTr("Atajo inválido"),gpdfTr("Usar una sola combinación de teclas por función.")); return; }
                for(int j=0;j<i;++j) if(!key.isEmpty() && key==editors[j]->keySequence()) {
                    auto a=shortcutActions[i]->objectName(),b=shortcutActions[j]->objectName();
                    if((a=="confirmTextAction" && b=="searchEnterAction") || (a=="searchEnterAction" && b=="confirmTextAction")) continue;
                    QMessageBox::warning(&dialog,gpdfTr("Atajo repetido"),QString(gpdfTr("%1 ya está asignado a %2.")).arg(key.toString(QKeySequence::NativeText),shortcutActions[j]->text())); return; }
            }
            QSettings settings(QSettings::IniFormat,QSettings::UserScope,AppIdentity::settingsOrganization,AppIdentity::settingsApplication); settings.beginGroup("shortcuts");
            for(int i=0;i<editors.size();++i) { auto action=shortcutActions[i]; action->setShortcut(editors[i]->keySequence()); settings.setValue(action->objectName(),action->shortcut().toString(QKeySequence::PortableText)); }
            updateToolTips(); installFileMenu(); dialog.accept();
        });
        dialog.exec();
    }
    void installFileMenu() {
        if(isFullScreen()) {
#ifdef Q_OS_WIN
            if(QApplication::platformName()=="windows") { SetMenu(reinterpret_cast<HWND>(winId()),nullptr); DrawMenuBar(reinterpret_cast<HWND>(winId())); }
#endif
            menuBar()->hide(); return;
        }
#ifdef Q_OS_WIN
        if(QApplication::platformName()=="windows") {
            HMENU root=CreateMenu(); bool ok=root!=nullptr;
            for(int i=0;i<fileActions.size();++i) fileActions[i]->setProperty("nativeCommand",fileCommandBase+UINT(i));
            for(int i=0;i<editActions.size();++i) editActions[i]->setProperty("nativeCommand",editCommandBase+UINT(i));
            for(int i=0;i<viewActions.size();++i) viewActions[i]->setProperty("nativeCommand",viewCommandBase+UINT(i));
            for(int i=0;i<goActions.size();++i) goActions[i]->setProperty("nativeCommand",goCommandBase+UINT(i));
            for(int i=0;i<shortcutMenuActions.size();++i) shortcutMenuActions[i]->setProperty("nativeCommand",shortcutsCommandBase+UINT(i));
            for(int i=0;i<helpActions.size();++i) helpActions[i]->setProperty("nativeCommand",helpCommandBase+UINT(i));
            std::function<HMENU(const QList<QAction*>&)> popupMenu=[&](const QList<QAction*>& actions)->HMENU {
                HMENU popup=CreatePopupMenu(); if(!popup) return nullptr;
                for(auto item:actions) {
                    if(!item->isVisible()) continue;
                    QString shortcut=item->property("displayShortcut").toString();
                    if(shortcut.isEmpty()) shortcut=item->shortcut().toString(QKeySequence::NativeText);
                    QString label=item->text()+(shortcut.isEmpty() ? QString() : "\t"+shortcut);
                    HMENU child=item->menu() ? popupMenu(item->menu()->actions()) : nullptr;
                    if(item->menu() && !child) { DestroyMenu(popup); return nullptr; }
                    UINT flags=item->isSeparator() ? MF_SEPARATOR : MF_STRING;
                    if(child) flags|=MF_POPUP;
                    if(item->isChecked()) flags|=MF_CHECKED;
                    if(!item->isEnabled()) flags|=MF_GRAYED;
                    auto id=child ? reinterpret_cast<UINT_PTR>(child) : UINT_PTR(item->property("nativeCommand").toUInt());
                    if(!AppendMenuW(popup,flags,id,reinterpret_cast<LPCWSTR>(label.utf16()))) {
                        if(child) DestroyMenu(child);
                        DestroyMenu(popup); return nullptr;
                    }
                }
                return popup;
            };
            auto append=[&](const QList<QAction*>& actions,const QString& title){
                if(!ok) return;
                HMENU popup=popupMenu(actions);
                if(popup && AppendMenuW(root,MF_POPUP,reinterpret_cast<UINT_PTR>(popup),reinterpret_cast<LPCWSTR>(title.utf16()))) return;
                if(popup) DestroyMenu(popup);
                ok=false;
            };
            append(fileMenu->actions(),fileMenu->title()); append(viewMenu->actions(),viewMenu->title()); append(editActions,gpdfTr("&Editar")); append(goMenu->actions(),goMenu->title()); append(shortcutsMenu->actions(),shortcutsMenu->title()); append(helpMenu->actions(),helpMenu->title());
            HWND hwnd=reinterpret_cast<HWND>(winId());
            if(ok && SetMenu(hwnd,root)) { auto old=nativeMenu; nativeMenu=root; updateFileActions(); updateEditActions(); syncViewActions(); DrawMenuBar(hwnd); if(old) DestroyMenu(old); return; }
            if(root) DestroyMenu(root);
            if(nativeMenu) return;
        }
#endif
        menuBar()->show();
        if(!menuBar()->actions().contains(fileMenu->menuAction())) {
            menuBar()->addMenu(fileMenu); menuBar()->addMenu(viewMenu);
            auto edit=menuBar()->addMenu(gpdfTr("&Editar")); for(auto action:editActions) edit->addAction(action);
            menuBar()->addMenu(goMenu); menuBar()->addMenu(shortcutsMenu); menuBar()->addMenu(helpMenu);
        }
    }
    QTimer* autoScrollTimer=nullptr;
    QPoint autoScrollOrigin;
    NativeScrollCursor::Direction autoScrollDirection=NativeScrollCursor::Neutral;
    QTimer* scrollSyncTimer=nullptr;
    void updateAutoScrollCursor(int distance) {
        auto direction=distance < -12 ? NativeScrollCursor::Up : distance > 12 ? NativeScrollCursor::Down : NativeScrollCursor::Neutral;
        if(direction!=autoScrollDirection) { autoScrollDirection=direction; QApplication::changeOverrideCursor(NativeScrollCursor::cursor(direction)); }
#ifdef Q_OS_WIN
        if(QApplication::platformName()=="windows") if(auto cursor=NativeScrollCursor::handle(direction)) SetCursor(cursor);
#endif
    }
    void stopAutoScroll() {
        if(autoScrollTimer && autoScrollTimer->isActive()) { autoScrollTimer->stop(); QApplication::restoreOverrideCursor(); }
    }
    struct DocumentState {
        QString id=QUuid::createUuid().toString(QUuid::WithoutBraces);
        quint64 backupRevision=0;
        bool discardOnExit=false;
        PdfDocument stored;
        QJsonArray personalMarks;
        QString path,query;
        PageView::Mode mode=PageView::Continuous;
        PageView::Fit fit=PageView::Manual;
        double scale=1.25;
        int rotation=0,vertical=0,horizontal=0,scope=0,type=0;
        bool searchVisible=false,annotationsVisible=false;
        QVector<int> navigation;
        int navigationIndex=-1;
    };
    std::vector<std::unique_ptr<DocumentState>> documents;
    int activeDocument=-1;
    bool changingTabs=false;
    QToolBar* documentToolbar=nullptr;
    QTabBar* documentTabs=nullptr;
    void updateDocumentTabs() {
        QSignalBlocker blocker(documentTabs);
        while(documentTabs->count()>int(documents.size())) documentTabs->removeTab(documentTabs->count()-1);
        while(documentTabs->count()<int(documents.size())) documentTabs->addTab(QString());
        for(int i=0;i<int(documents.size());++i) {
            const auto& doc=*documents[i]; QString path=i==activeDocument ? source : doc.path;
            bool dirty=i==activeDocument ? pdf.dirty() : doc.stored.dirty();
            documentTabs->setTabText(i,QFileInfo(path).fileName()+(dirty ? " *" : "")); documentTabs->setTabToolTip(i,path);
        }
        documentTabs->setCurrentIndex(activeDocument); documentToolbar->setVisible(documents.size()>=2);
    }
    void storeDocument() {
        if(activeDocument<0) return;
        captureDocument(); auto& doc=*documents[activeDocument];
        resetSearch(); pdf.swap(doc.stored);
    }
    bool switchDocument(int index) {
        if(index<0 || index>=int(documents.size())) return false;
        if(index==activeDocument) return true;
        if(!view->finishText()) { updateDocumentTabs(); return false; }
        stopAutoScroll();
        changingTabs=true; storeDocument(); activeDocument=index;
        auto& doc=*documents[index]; pdf.swap(doc.stored); source=doc.path;
        resetSearch(); view->newDocument(); view->mode=doc.mode; view->fit=doc.fit; view->scale=doc.scale; view->rotation=doc.rotation;
        { QSignalBlocker a(viewMode),b(annotationScope),c(annotationTypeFilter); viewMode->setCurrentIndex(int(doc.mode)); annotationScope->setCurrentIndex(doc.scope); annotationTypeFilter->setCurrentIndex(doc.type); }
        searchAction->setChecked(doc.searchVisible); togglePanel->setChecked(doc.annotationsVisible); refresh();
        updatingView=true; scroll->horizontalScrollBar()->setValue(doc.horizontal); scroll->verticalScrollBar()->setValue(doc.vertical); updatingView=false;
        searchInput->setText(doc.query); changingTabs=false; updateDocumentTabs(); return true;
    }
    PdfDocument pdf;
    PageView* view;
    QListWidget* annotations;
    QComboBox* annotationScope;
    QComboBox* annotationTypeFilter;
    QScrollArea* scroll;
    QPushButton* editButton;
    QPushButton* colorButton;
    QPushButton* deleteButton;
    QLabel* pageLabel;
    QSpinBox* pageNumber;
    QSpinBox* zoomPercent;
    QString source;
    QComboBox* viewMode;
    QAction* togglePanel;
    QWidget* searchPanel;
    QLineEdit* searchInput;
    QLabel* searchStatus;
    QListWidget* searchResults;
    QAction* searchAction;
    QTimer* searchTask;
    QTimer* searchDebounce;
    QVector<PdfDocument::SearchMatch> matches;
    QString searchQuery;
    int nextSearchPage=0;
    bool searchBusy=false;
    std::atomic<int> searchGeneration{0};
    quint64 searchRevision=0;
    bool updatingView=false;
    QFileDialog::Options fileDialogOptions() const { return view->darkMode ? QFileDialog::DontUseNativeDialog : QFileDialog::Options(); }
    void error(QString value) { QMessageBox::warning(this,"GPDF",value); }
    void resetSearch() {
        searchRevision=0;
        ++searchGeneration; searchBusy=false;
        pendingSearchNext=false; lastSearchRow=-1;
        searchTask->stop(); searchDebounce->stop();
        QSignalBlocker blocker(searchInput); searchInput->clear();
        matches.clear(); searchResults->clear(); searchStatus->setText(gpdfTr("Ingresar texto para buscar."));
        view->searchPage=-1; view->searchRects.clear();
    }
    void startSearch() {
        ++searchGeneration; searchBusy=false;
        pendingSearchNext=false; lastSearchRow=-1;
        searchTask->stop(); searchDebounce->stop();
        searchQuery=searchInput->text().trimmed(); nextSearchPage=0; searchRevision=pdf.revision();
        matches.clear(); searchResults->clear(); view->searchPage=-1; view->searchRects.clear(); view->update();
        if(!pdf.loaded()) { searchStatus->setText(gpdfTr("Abrir un PDF para buscar.")); return; }
        if(searchQuery.isEmpty()) { searchStatus->setText(gpdfTr("Ingresar texto para buscar.")); return; }
        searchStatus->setText(gpdfTr("Buscando…")); searchTask->start();
    }
    void searchEnter() {
        if(searchDebounce->isActive() || searchQuery!=searchInput->text().trimmed() || searchQuery.isEmpty()) { startSearch(); return; }
        pendingSearchNext=true; advanceSearchResult();
    }
    void advanceSearchResult() {
        if(!pendingSearchNext || matches.isEmpty()) { if(!searchBusy && !searchTask->isActive()) pendingSearchNext=false; return; }
        int target=-1;
        auto focused=QApplication::focusWidget();
        const bool resultsFocused=focused==searchResults || (focused && searchResults->isAncestorOf(focused));
        if(resultsFocused && searchResults->currentRow()>=0) {
            if(searchResults->currentRow()+1<matches.size()) target=searchResults->currentRow()+1;
            else if(!searchBusy && !searchTask->isActive()) target=0;
            else return;
        } else if(lastSearchRow>=0 && lastSearchRow<matches.size() && matches[lastSearchRow].page==pdf.pageIndex() && view->first<0 && view->last<0) {
            if(lastSearchRow+1<matches.size()) target=lastSearchRow+1;
        } else {
            int selectedCharacter=std::max(view->first,view->last);
            for(int i=0;i<matches.size();++i) {
                const auto& match=matches[i];
                if(match.page>pdf.pageIndex() || (match.page==pdf.pageIndex() &&
                    (selectedCharacter>=0 ? match.first>selectedCharacter : (match.rectangles.isEmpty() || view->deviceRect(match.rectangles.first()).bottom()>=scroll->verticalScrollBar()->value())))) { target=i; break; }
            }
        }
        if(target<0 && !searchBusy && !searchTask->isActive()) target=0;
        if(target<0) return;
        pendingSearchNext=false;
        if(searchResults->currentRow()==target) showSearchResult(target); else searchResults->setCurrentRow(target);
    }
    void searchNextPage() {
        if(searchBusy) return;
        searchBusy=true; int generation=searchGeneration,index=nextSearchPage; QString query=searchQuery; quint64 revision=pdf.revision(); QPointer<Window> target(this);
        view->workers.start([target,generation,index,query,revision]{
            QVector<PdfDocument::SearchMatch> found;
            { std::lock_guard guard(PdfDocument::apiMutex()); if(target && target->searchGeneration==generation && target->pdf.revision()==revision) found=target->pdf.searchPage(index,query); }
            if(target) QMetaObject::invokeMethod(target.data(),[target,generation,revision,found]{ if(target && target->searchGeneration==generation) { target->searchBusy=false; if(target->pdf.revision()!=revision) { target->startSearch(); return; } target->appendSearchResults(found); } },Qt::QueuedConnection);
        });
    }
    void appendSearchResults(const QVector<PdfDocument::SearchMatch>& found) {
        for(const auto& match:found) {
            matches.append(match);
            auto item=new QListWidgetItem(QString(gpdfTr("Página %1 · %2")).arg(match.page+1).arg(match.context));
            item->setData(Qt::UserRole,QString(gpdfTr("<b>Página %1</b> · %2")).arg(match.page+1).arg(match.contextHtml));
            item->setToolTip(match.context); searchResults->addItem(item);
        }
        ++nextSearchPage;
        if(nextSearchPage>=pdf.pageCount()) {
            searchTask->stop();
            searchStatus->setText(matches.isEmpty() ? gpdfTr("Sin resultados.") : QString(gpdfTr("%1 resultados en %2 páginas.")).arg(matches.size()).arg(pdf.pageCount()));
        } else {
            searchStatus->setText(QString(gpdfTr("%1 resultados · buscando página %2 de %3…")).arg(matches.size()).arg(nextSearchPage+1).arg(pdf.pageCount()));
            searchTask->start();
        }
        advanceSearchResult();
    }
    void showSearchResult(int row) {
        if(row<0 || row>=matches.size()) return;
        if(!view->finishText()) return;
        const auto match=matches[row];
        bool previous=updatingView; updatingView=true;
        if(pdf.setPage(match.page)) {
            view->resetSelection(); view->refreshNavigation(); refreshPanel();
            view->searchPage=match.page; view->searchRects=match.rectangles;
            if(!match.rectangles.isEmpty()) {
                auto point=view->devicePoint(match.rectangles.first().center());
                scroll->ensureVisible(int(point.x()),int(point.y()),60,80);
            } else scroll->verticalScrollBar()->setValue(int(view->activeRect().top()));
            view->update();
            lastSearchRow=row;
        }
        updatingView=previous;
    }
    bool confirmDiscard() {
        if(!view->finishText()) return false;
        if(!pdf.dirty()) return true;
        auto answer=QMessageBox::question(this,gpdfTr("Cambios pendientes"),gpdfTr("¿Guardar los cambios antes de continuar?"),QMessageBox::Save|QMessageBox::Discard|QMessageBox::Cancel,QMessageBox::Save);
        if(answer==QMessageBox::Cancel) return false;
        if(answer==QMessageBox::Discard) { if(activeDocument>=0) documents[activeDocument]->discardOnExit=true; return true; }
        return save();
    }
    void documentOpened(const QString& path) {
        source=QFileInfo(path).absoluteFilePath();
        { QSignalBlocker scopeBlocker(annotationScope); annotationScope->setCurrentIndex(0); QSignalBlocker typeBlocker(annotationTypeFilter); annotationTypeFilter->setCurrentIndex(0); }
        togglePanel->setChecked(false); { QSignalBlocker blocker(annotationSearch); annotationSearch->clear(); } resetSearch(); view->newDocument(); refresh(); scroll->verticalScrollBar()->setValue(0);
        restoreReading(); refreshPersonalBookmarks();
        rememberFile(source); statusBar()->showMessage(gpdfTr("Abierto: ") + QFileInfo(source).fileName());
    }
    void openPath(QString path,bool confirm=true) {
        Q_UNUSED(confirm);
        if(path.isEmpty()) return;
        path=QFileInfo(path).absoluteFilePath();
        for(int i=0;i<int(documents.size());++i) if((i==activeDocument ? source : documents[i]->path).compare(path,Qt::CaseInsensitive)==0) {
            if(!switchDocument(i) || !view->finishText() || !pdf.diskChanged(path)) return;
            if(!confirmDiscard()) return;
            stopAutoScroll();
            documents[i]->discardOnExit=false;
            int page=pdf.pageIndex();
            if(!pdf.reload(path)) { error(pdf.error()); return; }
            pdf.setPage(std::min(page,pdf.pageCount()-1));
            resetSearch(); view->newDocument(); refresh(); refreshPersonalBookmarks(); updateDocumentTabs(); saveSession();
            return;
        }
        auto document=std::make_unique<DocumentState>();
        if(!document->stored.open(path)) {
            bool accepted=false;
            auto password=QInputDialog::getText(this,gpdfTr("Abrir PDF"),gpdfTr("Contraseña (si corresponde):"),QLineEdit::Password,{},&accepted);
            if(!accepted) return;
            if(!document->stored.open(path,password.toUtf8())) { error(document->stored.error()); return; }
        }
        if(!view->finishText()) return;
        storeDocument(); documents.push_back(std::move(document)); activeDocument=int(documents.size())-1;
        pdf.swap(documents.back()->stored); view->mode=PageView::Continuous; view->fit=PageView::Manual; view->scale=1.25; view->rotation=0;
        { QSignalBlocker blocker(viewMode); viewMode->setCurrentIndex(0); }
        documentOpened(path);
    }
    void open() {
        auto path=QFileDialog::getOpenFileName(this,gpdfTr("Abrir PDF"),{},"PDF (*.pdf)");
        if(!path.isEmpty()) openPath(path);
    }
    bool writeTo(const QString& path) {
        if(!pdf.saveCopy(path)) { error(pdf.error()); return false; }
        source=QFileInfo(path).absoluteFilePath(); persistReading();
        if(activeDocument>=0) { documents[activeDocument]->discardOnExit=false; documents[activeDocument]->path=source; }
        refresh(selectedAnnotationIndex());
        rememberFile(source); statusBar()->showMessage(gpdfTr("Guardado: ") + source); return true;
    }
    bool save() {
        if(!view->finishText()) return false;
        if(!pdf.loaded()) return false;
        return source.isEmpty() ? saveAs() : writeTo(source);
    }
    bool saveAs() {
        if(!view->finishText()) return false;
        if(!pdf.loaded()) return false;
        auto path=QFileDialog::getSaveFileName(this,gpdfTr("Guardar como"),source.isEmpty() ? "estudio-anotado.pdf" : source,"PDF (*.pdf)",nullptr,fileDialogOptions());
        if(path.isEmpty()) return false;
        if(!path.endsWith(".pdf",Qt::CaseInsensitive)) path+=".pdf";
        return writeTo(path);
    }
    void changeMode(PageView::Mode mode) {
        if(!view->finishText()) { QSignalBlocker blocker(viewMode); viewMode->setCurrentIndex(int(view->mode)); return; }
        updatingView=true;
        view->setViewportSize(scroll->viewport()->size());
        view->setMode(mode);
        scroll->verticalScrollBar()->setValue(mode==PageView::Continuous ? int(view->activeRect().top()) : 0);
        scroll->horizontalScrollBar()->setValue(0);
        refreshPanel(); updatingView=false;
    }
    void zoom(double delta) {
        setZoom(std::clamp((view->fit==PageView::Manual ? view->scale : view->effectiveScale)+delta,0.5,3.0));
    }
    void setZoom(double value,QPoint anchor=QPoint(-1,-1)) {
        if(!view->finishText()) { QSignalBlocker blocker(zoomPercent); zoomPercent->setValue(int(std::lround(view->scale*100))); return; }
        QPoint focus=anchor.x()<0 ? scroll->viewport()->rect().center() : anchor;
        QPointF content=focus+QPoint(scroll->horizontalScrollBar()->value(),scroll->verticalScrollBar()->value());
        int anchorPage=pdf.pageIndex();
        for(int i=0;i<view->pageRects.size();++i) if(view->pageRects[i].contains(content)) { anchorPage=i; break; }
        QRectF previousRect=view->pageRects.value(anchorPage);
        QPointF relative=previousRect.isEmpty() ? QPointF(0,0) : QPointF((content.x()-previousRect.left())/previousRect.width(),(content.y()-previousRect.top())/previousRect.height());
        updatingView=true;
        view->setFit(PageView::Manual);
        view->setZoom(std::clamp(value,0.5,3.0));
        QSignalBlocker blocker(zoomPercent);
        zoomPercent->setValue(int(std::lround(view->scale*100)));
        QRectF nextRect=view->pageRects.value(anchorPage);
        scroll->verticalScrollBar()->setValue(int(nextRect.top()+relative.y()*nextRect.height()-focus.y()));
        scroll->horizontalScrollBar()->setValue(int(nextRect.left()+relative.x()*nextRect.width()-focus.x()));
        updatingView=false; syncViewActions();
    }
    void scrolling() {
        if(updatingView || view->mode!=PageView::Continuous || !view->stroke.isEmpty() || !pdf.loaded()) return;
        std::unique_lock apiLock(PdfDocument::apiMutex(),std::try_to_lock);
        if(!apiLock.owns_lock()) {
            if(!scrollSyncTimer) { scrollSyncTimer=new QTimer(this); scrollSyncTimer->setSingleShot(true); connect(scrollSyncTimer,&QTimer::timeout,this,[this]{ scrolling(); }); }
            if(!scrollSyncTimer->isActive()) scrollSyncTimer->start(25);
            return;
        }
        double position=scroll->verticalScrollBar()->value()+scroll->viewport()->height()*0.35;
        int closest=-1; double distance=std::numeric_limits<double>::max();
        for(int i=0;i<view->pageRects.size();++i) {
            auto r=view->pageRects[i];
            double d=position<r.top() ? r.top()-position : (position>r.bottom() ? position-r.bottom() : 0);
            if(d<distance) { closest=i; distance=d; }
        }
        view->activate(closest);
    }
    void page(int delta) {
        if(!view->finishText()) return;
        int next=view->mode==PageView::Double ? (pdf.pageIndex()/2+delta)*2 : pdf.pageIndex()+delta;
        if(next<0 || next>=pdf.pageCount()) return;
        updatingView=true;
        if(pdf.setPage(next)) {
            view->resetSelection(view->annotationDragging()); view->refreshNavigation(); refreshPanel();
            scroll->verticalScrollBar()->setValue(view->mode==PageView::Continuous ? int(view->activeRect().top()) : 0);
        }
        updatingView=false;
    }
    int annotationRow(int page,int index) const {
        for(int row=0;row<annotations->count();++row) {
            auto item=annotations->item(row);
            if(item->data(Qt::UserRole).toInt()==page && item->data(Qt::UserRole+1).toInt()==index) return row;
        }
        return -1;
    }
    int selectedAnnotationIndex() const {
        auto item=annotations->currentItem();
        return item && item->data(Qt::UserRole).toInt()==pdf.pageIndex() ? item->data(Qt::UserRole+1).toInt() : view->selectedAnnotation;
    }
    void selectAnnotation(int row) {
        auto item=row>=0 ? annotations->item(row) : nullptr;
        int index=item ? item->data(Qt::UserRole+1).toInt() : -1;
        int page=item ? item->data(Qt::UserRole).toInt() : pdf.pageIndex();
        if(page!=pdf.pageIndex()) {
            if(!view->finishText()) return;
            bool previous=updatingView; updatingView=true;
            if(!pdf.setPage(page)) { updatingView=previous; return; }
            view->resetSelection(); view->refresh(); refreshPanel(index);
            updatingView=previous; return;
        }
        view->selectedAnnotation=index;
        if(index>=0) { view->first=view->last=-1; view->allTextSelected=false; }
        editButton->setEnabled(pdf.canEditAnnotation(index));
        deleteButton->setEnabled(pdf.canEditAnnotation(index));
        colorButton->setEnabled(pdf.canColorAnnotation(index));
        view->update();
        if(index>=0) {
            auto point=view->devicePoint(pdf.annotationRect(index).center());
            bool previous=updatingView; updatingView=true;
            scroll->ensureVisible(int(point.x()),int(point.y()),40,40);
            updatingView=previous;
        }
    }
    void editSelected() {
        int row=selectedAnnotationIndex();
        if(!pdf.canEditAnnotation(row)) return;
        if(pdf.annotationType(row)==FPDF_ANNOT_UNDERLINE) {
            QDialog dialog(this); dialog.setObjectName("underlineFormatDialog"); dialog.setWindowTitle(gpdfTr("Editar subrayado")); auto form=new QFormLayout(&dialog);
            QColor color=pdf.annotationColor(row); auto choose=new QPushButton(gpdfTr("Elegir color…")); choose->setObjectName("underlineFormatColor"); form->addRow(gpdfTr("Color:"),choose);
            auto updateSwatch=[&]{ QPixmap swatch(16,16); swatch.fill(color); choose->setIcon(QIcon(swatch)); }; updateSwatch();
            connect(choose,&QPushButton::clicked,&dialog,[&]{ auto next=QColorDialog::getColor(color,&dialog,gpdfTr("Color del subrayado")); if(next.isValid()) { color=next; updateSwatch(); } });
            auto width=new QDoubleSpinBox; width->setObjectName("underlineFormatWidth"); width->setRange(0.5,12); width->setSingleStep(0.5); width->setSuffix(" pt"); width->setValue(pdf.copyAnnotation(row).strokeWidth); form->addRow(gpdfTr("Grosor:"),width);
            auto buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel); form->addRow(buttons); connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept); connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
            if(dialog.exec()==QDialog::Accepted) { if(!pdf.formatDrawing(row,color,width->value())) error(pdf.error()); else refresh(row); } return;
        }
        bool accepted=false;
        bool freeText=pdf.annotationType(row)==FPDF_ANNOT_FREETEXT;
        auto value=studyMultilineText(this,freeText ? gpdfTr("Editar texto en la página") : gpdfTr("Editar comentario"),pdf.annotationContents(row),&accepted);
        if(!accepted) return;
        if(!pdf.editAnnotation(row,value)) { error(pdf.error()); return; }
        refresh(row); statusBar()->showMessage(gpdfTr("Anotación editada. Guardar para conservar el cambio."));
    }
    void colorSelected() {
        int row=selectedAnnotationIndex();
        if(!pdf.canColorAnnotation(row)) return;
        auto current=pdf.annotationColor(row);
        if(!current.isValid()) current=QColor("#ffd232");
        current.setAlpha(255);
        auto color=QColorDialog::getColor(current,this,gpdfTr("Color de la anotación"),view->darkMode ? QColorDialog::DontUseNativeDialog : QColorDialog::ColorDialogOptions());
        if(!color.isValid()) return;
        if(!pdf.recolorAnnotation(row,color)) { error(pdf.error()); return; }
        refresh(row); statusBar()->showMessage(gpdfTr("Color actualizado."));
    }
    void deleteSelected() {
        int row=selectedAnnotationIndex();
        if(!pdf.canEditAnnotation(row)) return;
        if(!pdf.removeAnnotation(row)) { error(pdf.error()); return; }
        refresh(std::min(row,pdf.annotationCount()-1));
        statusBar()->showMessage(gpdfTr("Anotación borrada. Guardar para conservar el cambio."));
    }
    void refresh(int selected=-1) {
        std::lock_guard guard(PdfDocument::apiMutex());
        bool previous=updatingView; updatingView=true;
#ifdef GPDF_PERF_PROBE
        QElapsedTimer profile; profile.start();
#endif
        view->refresh();
        if(searchRevision && searchRevision!=pdf.revision() && !searchInput->text().trimmed().isEmpty()) startSearch();
#ifdef GPDF_PERF_PROBE
        qInfo()<<"PERF refresh_view_ms"<<profile.nsecsElapsed()/1000000.0; profile.restart();
#endif
        refreshPanel(selected);
#ifdef GPDF_PERF_PROBE
        qInfo()<<"PERF refresh_panel_ms"<<profile.nsecsElapsed()/1000000.0;
#endif
        updatingView=previous;
    }
    quint64 annotationsRevision=0;
    bool annotationsBusy=false;
    QVector<PdfDocument::AnnotationEntry> panelEntries;
    QVector<PdfDocument::AnnotationEntry> annotationsForPanel() {
        if(annotationScope->currentIndex()==1) return pdf.pageAnnotations(pdf.pageIndex());
        if(pdf.pageCount()<=50) return pdf.documentAnnotations();
        if(!togglePanel->isChecked()) return pdf.pageAnnotations(pdf.pageIndex());
        quint64 revision=pdf.revision();
        if(annotationsRevision!=revision) { annotationsRevision=revision; annotationsBusy=false; panelEntries=pdf.pageAnnotations(pdf.pageIndex()); }
        if(!annotationsBusy) {
            annotationsBusy=true; int count=pdf.pageCount(); QPointer<Window> target(this);
            view->workers.start([target,revision,count]{
                QVector<PdfDocument::AnnotationEntry> entries;
                for(int page=0;page<count;++page) {
                    std::lock_guard guard(PdfDocument::apiMutex()); if(!target || target->pdf.revision()!=revision) return; entries+=target->pdf.pageAnnotations(page);
                }
                if(target) QMetaObject::invokeMethod(target.data(),[target,revision,entries]{ if(target && target->pdf.revision()==revision && target->annotationsRevision==revision) { target->panelEntries=entries; target->refreshPanel(target->view->selectedAnnotation); } },Qt::QueuedConnection);
            });
        }
        return panelEntries;
    }
    void refreshPanel(int selected=-1) {
        std::lock_guard guard(PdfDocument::apiMutex());
        updateNavigation(); refreshPersonalBookmarks();
        if(checkpointTimer && !restoringSession) checkpointTimer->start();
        if(pdf.loaded() && activeDocument>=0 && !navigatingHistory) {
            auto& doc=*documents[activeDocument];
            if(doc.navigationIndex<0 || doc.navigation[doc.navigationIndex]!=pdf.pageIndex()) {
                doc.navigation.resize(doc.navigationIndex+1); doc.navigation.append(pdf.pageIndex()); doc.navigationIndex=doc.navigation.size()-1;
            }
        }
        updateGoActions();
        { QSignalBlocker pageBlocker(pageNumber); pageNumber->setEnabled(pdf.loaded()); pageNumber->setRange(1,std::max(1,pdf.pageCount())); pageNumber->setValue(pdf.loaded() ? pdf.pageIndex()+1 : 1); }
        pageLabel->setText(QString("/ %1  ").arg(pdf.pageCount()));
        QSignalBlocker blocker(annotations);
        annotations->clear();
        auto entries=annotationsForPanel();
        auto normalized=[](QString value){ QString result; for(auto c:value.toCaseFolded().normalized(QString::NormalizationForm_D)) if(c.category()!=QChar::Mark_NonSpacing) result+=c; return result; };
        QString query=normalized(annotationSearch->text().trimmed());
        for(const auto& entry:entries) {
            if(!query.isEmpty() && !normalized(entry.text).contains(query)) continue;
            if(annotationScope->currentIndex()==1 && entry.page!=pdf.pageIndex()) continue;
            const int filter=annotationTypeFilter->currentIndex();
            if(filter==0 && entry.type!=FPDF_ANNOT_TEXT && entry.type!=FPDF_ANNOT_FREETEXT) continue;
            const int types[]={0,0,FPDF_ANNOT_TEXT,FPDF_ANNOT_FREETEXT,FPDF_ANNOT_INK,FPDF_ANNOT_HIGHLIGHT,FPDF_ANNOT_UNDERLINE};
            if(filter>=2 && entry.type!=types[filter]) continue;
            QString label;
            switch(entry.type) {
                case FPDF_ANNOT_TEXT: label=gpdfTr("Comentario"); break;
                case FPDF_ANNOT_FREETEXT: label=gpdfTr("Texto"); break;
                case FPDF_ANNOT_HIGHLIGHT: label=gpdfTr("Resaltado"); break;
                case FPDF_ANNOT_UNDERLINE: label=gpdfTr("Subrayado"); break;
                case FPDF_ANNOT_INK: label=gpdfTr("Lápiz"); break;
                default: label=gpdfTr("Anotación");
            }
            label=QString(gpdfTr("Página %1 · ")).arg(entry.page+1)+label;
            auto item=new QListWidgetItem(label+" · "+(entry.text.isEmpty() ? QString::number(entry.index+1) : entry.text),annotations);
            item->setData(Qt::UserRole,entry.page); item->setData(Qt::UserRole+1,entry.index);
            QString body=entry.text.isEmpty() ? QString::number(entry.index+1) : entry.text;
            item->setData(Qt::UserRole+2,QString("<b>%1</b> · %2").arg(label.toHtmlEscaped(),body.toHtmlEscaped().replace('\n',"<br/>")));
        }
        annotations->setCurrentRow(annotationRow(pdf.pageIndex(),selected));
        selectAnnotation(annotations->currentRow());
        updateFileActions(); updateEditActions(); syncViewActions();
        updateDocumentTabs();
        setWindowTitle(pdf.loaded() ? QString("%1%2 — GPDF").arg(source.isEmpty() ? gpdfTr("Sin título") : QFileInfo(source).fileName(),pdf.dirty() ? " *" : "") : "GPDF");
    }
};
