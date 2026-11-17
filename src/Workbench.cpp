#include "Workbench.h"
#include <QDesktopServices>
#include <QListView>
#include <QUrl>
#include <QMimeData>
#include <QDrag>

void WorkbenchPageList::startDrag(Qt::DropActions) {
    const auto indexes=selectedIndexes();
    if(indexes.isEmpty())return;
    QDrag drag(this);
    drag.setMimeData(model()->mimeData(indexes));
    if(auto item=currentItem())drag.setPixmap(item->icon().pixmap(iconSize()));
    // dropEvent already moves the existing items. The default view drag handler
    // would remove the source rows again after a successful MoveAction.
    drag.exec(Qt::MoveAction,Qt::MoveAction);
    insertion=-1;viewport()->update();
}

void WorkbenchPageList::moveSelectionTo(int destination) {
    QList<QListWidgetItem*> selected;
    int before=0;
    for(int row=0;row<count();++row)if(item(row)->isSelected()){selected.append(item(row));if(row<destination)++before;}
    if(selected.isEmpty())return;
    for(auto item:selected)takeItem(row(item));
    clearSelection();
    destination=std::clamp(destination-before,0,count());
    for(auto item:selected){insertItem(destination++,item);item->setSelected(true);}
    setCurrentItem(selected.first(),QItemSelectionModel::NoUpdate);
    doItemsLayout();
    if(orderChanged)orderChanged();
}
int WorkbenchPageList::insertionAt(QPoint position) const {
    for(int row=0;row<count();++row) {
        const auto rect=visualItemRect(item(row));
        if(position.y()<rect.top() || (position.y()<=rect.bottom() && position.x()<rect.center().x()))return row;
    }
    return count();
}
void WorkbenchPageList::dragMoveEvent(QDragMoveEvent* event) {
    if(event->source()!=this){QListWidget::dragMoveEvent(event);return;}
    QAbstractItemView::dragMoveEvent(event);
    insertion=insertionAt(event->position().toPoint());
    event->setDropAction(Qt::MoveAction);event->accept();viewport()->update();
}
void WorkbenchPageList::dragLeaveEvent(QDragLeaveEvent* event) {
    insertion=-1;viewport()->update();QListWidget::dragLeaveEvent(event);
}
void WorkbenchPageList::paintEvent(QPaintEvent* event) {
    QListWidget::paintEvent(event);
    if(insertion<0 || !count())return;
    const auto rect=visualItemRect(item(std::min(insertion,count()-1)));
    const int x=insertion<count() ? rect.left()+2 : rect.right()-2;
    QPainter painter(viewport());painter.setPen(QPen(palette().highlight().color(),3));
    painter.drawLine(x,rect.top()+4,x,rect.bottom()-4);
    painter.drawLine(x-5,rect.top()+4,x+5,rect.top()+4);
    painter.drawLine(x-5,rect.bottom()-4,x+5,rect.bottom()-4);
}
void WorkbenchPageList::dropEvent(QDropEvent* event) {
    if(event->source()!=this){event->ignore();return;}
    int destination=insertionAt(event->position().toPoint());insertion=-1;viewport()->update();
    moveSelectionTo(destination);event->setDropAction(Qt::MoveAction);event->accept();
}

Workbench::Workbench(QWidget* parent):QDialog(parent) {
    setObjectName("workbenchDialog"); setWindowTitle(QStringLiteral("Workbench"));
    resize(1080,780); setMinimumSize(820,600); setWindowFlag(Qt::WindowContextHelpButtonHint,false);
    workers.setMaxThreadCount(1);
    auto root=new QVBoxLayout(this); root->setContentsMargins(18,18,18,14); root->setSpacing(12);
    auto body=new QHBoxLayout; body->setSpacing(18); root->addLayout(body,1);
    auto sidebar=new QVBoxLayout;
    auto brand=new QLabel(QStringLiteral("Workbench")); auto brandFont=brand->font(); brandFont.setPointSize(18); brandFont.setBold(true); brand->setFont(brandFont); sidebar->addWidget(brand);
    tools=new QListWidget; tools->setObjectName("workbenchTools"); tools->setFixedWidth(205); tools->setSpacing(3);
    tools->addItems({workbenchTr("Unir PDFs","Merge PDFs"),workbenchTr("Separar PDFs","Split PDFs"),workbenchTr("Remover páginas","Remove pages"),workbenchTr("Extraer páginas","Extract pages"),workbenchTr("Organizar PDFs","Organize PDFs"),workbenchTr("Guardar OCR en PDF","Save OCR to PDF")});
    for(int i=0;i<tools->count();++i) tools->item(i)->setSizeHint({190,42});
    sidebar->addWidget(tools,1); body->addLayout(sidebar);
    auto content=new QVBoxLayout; content->setSpacing(10); body->addLayout(content,1);
    title=new QLabel; auto heading=title->font(); heading.setPointSize(16); heading.setBold(true); title->setFont(heading); content->addWidget(title);
    description=new QLabel; description->setWordWrap(true); description->setMinimumHeight(40); content->addWidget(description);
    auto fileButtons=new QHBoxLayout;
    add=new QPushButton(workbenchTr("Añadir PDFs…","Add PDFs…")); add->setObjectName("workbenchAddFiles");
    current=new QPushButton(workbenchTr("Usar PDF actual","Use current PDF")); current->setObjectName("workbenchUseCurrent"); current->setEnabled(false);
    fileButtons->addWidget(add); fileButtons->addWidget(current); fileButtons->addStretch(); content->addLayout(fileButtons);
    files=new QListWidget; files->setObjectName("workbenchFiles"); files->setDragDropMode(QAbstractItemView::InternalMove); files->setMaximumHeight(112); files->setMinimumHeight(68); content->addWidget(files);
    fileControls=new QWidget; auto controls=new QHBoxLayout(fileControls); controls->setContentsMargins(0,0,0,0);
    fileUp=new QPushButton(workbenchTr("Subir","Move up")); fileDown=new QPushButton(workbenchTr("Bajar","Move down")); removeFile=new QPushButton(workbenchTr("Quitar archivo","Remove file"));
    controls->addWidget(fileUp); controls->addWidget(fileDown); controls->addWidget(removeFile); controls->addStretch(); content->addWidget(fileControls);
    pageSummary=new QLabel; pageSummary->setObjectName("workbenchPageSummary"); content->addWidget(pageSummary);
    rangeControls=new QWidget; auto rangeLayout=new QHBoxLayout(rangeControls); rangeLayout->setContentsMargins(0,0,0,0);
    rangeLayout->addWidget(new QLabel(workbenchTr("Páginas:","Pages:"))); range=new QLineEdit; range->setObjectName("workbenchRange"); range->setPlaceholderText(workbenchTr("Ejemplo: 1, 3-5","Example: 1, 3-5")); rangeLayout->addWidget(range,1); content->addWidget(rangeControls);
    splitControls=new QWidget; auto splitLayout=new QHBoxLayout(splitControls); splitLayout->setContentsMargins(0,0,0,0);
    splitMode=new QComboBox; splitMode->setObjectName("workbenchSplitMode"); splitMode->addItems({workbenchTr("Cada N páginas","Every N pages"),workbenchTr("Grupos de páginas","Page groups")});
    chunk=new QSpinBox; chunk->setObjectName("workbenchChunk"); chunk->setRange(1,100000); chunk->setValue(1);
    groups=new QLineEdit; groups->setObjectName("workbenchGroups"); groups->setPlaceholderText("1-3; 4-6; 7-10"); groups->hide();
    splitLayout->addWidget(splitMode); splitLayout->addWidget(chunk); splitLayout->addWidget(groups,1); content->addWidget(splitControls);
    pages=new WorkbenchPageList; pages->setObjectName("workbenchPages"); pages->setViewMode(QListView::IconMode); pages->setResizeMode(QListView::Adjust);
    pages->setMovement(QListView::Snap); pages->setWrapping(true); pages->setIconSize({104,132}); pages->setGridSize({136,174}); pages->setUniformItemSizes(true);
    pages->setSelectionMode(QAbstractItemView::ExtendedSelection); pages->setSpacing(4); pages->setMinimumHeight(174); content->addWidget(pages,1);
    pages->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    pages->setDefaultDropAction(Qt::MoveAction);pages->setDropIndicatorShown(false);
    static_cast<WorkbenchPageList*>(pages)->orderChanged=[this]{
        if(tool()!=Merge)return;
        const int selected=inputIndex();QSignalBlocker blocker(files);
        for(int row=0;row<pages->count();++row) {
            const int id=pages->item(row)->data(Qt::UserRole).toInt();
            for(int j=row;j<files->count();++j)if(files->item(j)->data(Qt::UserRole).toInt()==id){auto item=files->takeItem(j);files->insertItem(row,item);break;}
        }
        for(int row=0;row<files->count();++row)if(files->item(row)->data(Qt::UserRole).toInt()==selected)files->setCurrentRow(row);
    };
    pageControls=new QWidget; auto pageButtons=new QHBoxLayout(pageControls); pageButtons->setContentsMargins(0,0,0,0);
    selectAll=new QPushButton(workbenchTr("Seleccionar todo","Select all")); selectAll->setObjectName("workbenchSelectAll");
    rotate=new QPushButton(workbenchTr("Girar 90°","Rotate 90°")); rotate->setObjectName("workbenchRotate");
    removePage=new QPushButton(workbenchTr("Quitar selección","Remove selection")); removePage->setObjectName("workbenchRemovePages");
    reset=new QPushButton(workbenchTr("Restablecer","Reset")); reset->setObjectName("workbenchReset");
    pageButtons->addWidget(selectAll); pageButtons->addWidget(rotate); pageButtons->addWidget(removePage); pageButtons->addStretch(); pageButtons->addWidget(reset); content->addWidget(pageControls);
    auto output=new QHBoxLayout; output->addWidget(new QLabel(workbenchTr("Guardar en:","Save to:")));
    destination=new QLineEdit; destination->setObjectName("workbenchDestination"); output->addWidget(destination,1);
    destination->setReadOnly(true);
    QSettings settings(QSettings::IniFormat,QSettings::UserScope,AppIdentity::settingsOrganization,AppIdentity::settingsApplication);
    destination->setText(QDir::toNativeSeparators(settings.value("workbench/outputDirectory",QStandardPaths::writableLocation(QStandardPaths::DesktopLocation)).toString()));
    browse=new QPushButton(workbenchTr("Elegir…","Browse…")); browse->setObjectName("workbenchBrowse");output->addWidget(browse); content->addLayout(output);
    progress=new QProgressBar; progress->setObjectName("workbenchProgress"); progress->hide(); root->addWidget(progress);
    auto footer=new QHBoxLayout; status=new QLabel; status->setObjectName("workbenchStatus"); status->setWordWrap(true); status->setTextInteractionFlags(Qt::TextSelectableByMouse); footer->addWidget(status,1);
    cancel=new QPushButton(workbenchTr("Cerrar","Close")); save=new QPushButton; save->setObjectName("workbenchRun"); save->setDefault(true); footer->addWidget(cancel); footer->addWidget(save); root->addLayout(footer);
    reveal=new QPushButton(workbenchTr("Mostrar último archivo","Show last saved file"));reveal->setObjectName("workbenchReveal");reveal->setEnabled(false);footer->insertWidget(1,reveal);
    connect(reveal,&QPushButton::clicked,this,[this]{
        if(lastSavedFile.isEmpty())return;
#ifdef Q_OS_WIN
        QProcess explorer;explorer.setProgram("explorer.exe");explorer.setNativeArguments(QStringLiteral("/select,\"%1\"").arg(QDir::toNativeSeparators(lastSavedFile)));explorer.startDetached();
#else
        QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(lastSavedFile).absolutePath()));
#endif
    });
    connect(tools,&QListWidget::currentRowChanged,this,[this]{changeTool();});
    connect(add,&QPushButton::clicked,this,[this]{chooseFiles();});
    connect(current,&QPushButton::clicked,this,[this]{if(currentDocument) currentDocument(*this);});
    connect(files,&QListWidget::currentRowChanged,this,[this]{loadPages();});
    connect(files->model(),&QAbstractItemModel::rowsMoved,this,[this]{if(tool()==Merge)loadPages();});
    connect(fileUp,&QPushButton::clicked,this,[this]{moveFile(-1);}); connect(fileDown,&QPushButton::clicked,this,[this]{moveFile(1);});
    connect(removeFile,&QPushButton::clicked,this,[this]{int index=inputIndex();if(index<0)return;delete files->takeItem(files->currentRow());inputs[index].pdf.reset();loadPages();});
    connect(range,&QLineEdit::editingFinished,this,[this]{selectRange();});
    connect(splitMode,&QComboBox::currentIndexChanged,this,[this](int mode){chunk->setVisible(mode==0);groups->setVisible(mode==1);});
    connect(selectAll,&QPushButton::clicked,pages,&QListWidget::selectAll);
    connect(pages,&QListWidget::itemSelectionChanged,this,[this]{refreshSelection();});
    connect(rotate,&QPushButton::clicked,this,[this]{
        ++previewGeneration;failedThumbnails.clear();
        for(auto item:pages->selectedItems()) { int turn=(item->data(Qt::UserRole+1).toInt()+1)%4; item->setData(Qt::UserRole+1,turn);item->setIcon(QIcon());item->setText(workbenchTr("Página %1","Page %1").arg(item->data(Qt::UserRole).toInt()+1)+(turn ? QString(" · %1°").arg(turn*90) : QString())); }
    });
    connect(removePage,&QPushButton::clicked,this,[this]{for(auto item:pages->selectedItems()) delete pages->takeItem(pages->row(item));refreshSelection();});
    connect(reset,&QPushButton::clicked,this,[this]{loadPages();});
    connect(browse,&QPushButton::clicked,this,[this]{chooseDestination();});
    connect(save,&QPushButton::clicked,this,[this]{run();});
    connect(cancel,&QPushButton::clicked,this,[this]{if(busy){cancellation->store(true);cancel->setEnabled(false);status->setText(workbenchTr("Cancelando…","Cancelling…"));}else reject();});
    thumbnailTimer.setInterval(60); connect(&thumbnailTimer,&QTimer::timeout,this,[this]{thumbnail();}); thumbnailTimer.start();
    for(auto target:{static_cast<QWidget*>(this),files->viewport(),pages->viewport(),tools->viewport(),
                     static_cast<QWidget*>(destination),static_cast<QWidget*>(range),static_cast<QWidget*>(groups)}) {
        target->setAcceptDrops(true);
        target->installEventFilter(this);
    }
    tools->setCurrentRow(0);
}
Workbench::~Workbench() { thumbnailTimer.stop();if(cancellation)cancellation->store(true);workers.clear();workers.waitForDone(); }
void Workbench::reject() { if(busy){cancellation->store(true);return;} QDialog::reject(); }
void Workbench::closeEvent(QCloseEvent* event) {if(busy){cancellation->store(true);event->ignore();return;} QDialog::closeEvent(event);}
void Workbench::setCurrentDocument(std::function<void(Workbench&)> callback) {currentDocument=std::move(callback);current->setEnabled(bool(currentDocument));}
int Workbench::inputIndex() const {return files->currentItem() ? files->currentItem()->data(Qt::UserRole).toInt() : -1;}
bool Workbench::addFile(const QString& path,const QByteArray& password,const QString& label) {
    if(busy) return false;
    auto document=std::make_shared<PdfDocument>();
    {std::lock_guard lock(PdfDocument::apiMutex());if(!document->open(path,password)){needsPassword=FPDF_GetLastError()==FPDF_ERR_PASSWORD;showError(document->error());return false;}}
    needsPassword=false;
    const int index=inputs.size(); const auto name=label.isEmpty() ? QFileInfo(path).fileName() : label;
    inputs.append({document,path,name,path,WorkbenchPdf::canModify(*document)});
    auto item=new QListWidgetItem(QString("%1  ·  %2").arg(name,workbenchTr("%1 páginas","%1 pages").arg(document->pageCount())),files); item->setToolTip(path);item->setData(Qt::UserRole,index);
    files->setCurrentItem(item);save->setEnabled(true);return true;
}
void Workbench::useSnapshot(const PdfDocument& document,const QString& label,const QString& originalPath) {
    if(!WorkbenchPdf::canAssemble(document)){showError(workbenchTr("El PDF no permite reorganizar sus páginas.","The PDF does not allow page assembly."));return;}
    const auto path=snapshots.filePath(QUuid::createUuid().toString(QUuid::WithoutBraces)+".pdf");
    if(!document.saveRecovery(path)) {showError(workbenchTr("No se pudo preparar el PDF actual.","Could not prepare the current PDF."));return;}
    if(addFile(path,{},label)){inputs.last().original=originalPath;inputs.last().modifyPermitted=WorkbenchPdf::canModify(document);}
}
void Workbench::chooseFiles() {
    const auto paths=QFileDialog::getOpenFileNames(this,workbenchTr("Añadir PDFs","Add PDFs"),{},"PDF (*.pdf)");
    importFiles(paths);
}

bool Workbench::eventFilter(QObject* object,QEvent* event) {
    const auto type=event->type();
    if(type==QEvent::DragEnter || type==QEvent::DragMove || type==QEvent::Drop) {
        auto drop=static_cast<QDropEvent*>(event);
        if(drop->mimeData()->hasUrls()) {
            QStringList paths;
            if(!busy && drop->possibleActions().testFlag(Qt::CopyAction)) {
                for(const auto& url:drop->mimeData()->urls()) {
                    if(!url.isLocalFile()) continue;
                    const QFileInfo file(url.toLocalFile());
                    if(file.suffix().compare("pdf",Qt::CaseInsensitive)==0 && file.isFile())
                        paths.append(file.absoluteFilePath());
                }
            }
            if(paths.isEmpty()) drop->ignore();
            else {
                drop->setDropAction(Qt::CopyAction);
                drop->accept();
                if(type==QEvent::Drop) {
                    paths.removeDuplicates();
                    QTimer::singleShot(0,this,[this,paths]{ importFiles(paths); });
                }
            }
            return true;
        }
    }
    return QDialog::eventFilter(object,event);
}

void Workbench::importFiles(const QStringList& paths) {
    if(busy) return;
    auto previous=files->currentItem();
    QSignalBlocker blocker(files);
    for(const auto& path:paths) {
        if(addFile(path))continue;
        // Password prompts are optional after an unsuccessful open; invalid files
        // retain their original error rather than silently entering the project.
        while(needsPassword){bool ok=false;const auto password=QInputDialog::getText(this,QStringLiteral("Workbench"),workbenchTr("Contraseña del PDF:","PDF password:"),QLineEdit::Password,{},&ok);if(!ok)break;if(addFile(path,password.toUtf8()))break;}
    }
    if(previous)files->setCurrentItem(previous);
    if(tool()==Merge || !previous)loadPages();
}
void Workbench::changeTool() {
    if(tools->currentRow()<0)return;
    title->setText(tools->currentItem()->text());
    const QString descriptions[]={
        workbenchTr("Convierte en orden y unifica los archivos añadidos en un solo documento","Converts in order and combines the added files into a single document"),
        workbenchTr("Dividí el PDF seleccionado cada cierta cantidad de páginas o en grupos separados por punto y coma.","Split the selected PDF every few pages, or into groups separated by semicolons."),
        workbenchTr("Seleccioná las páginas que querés remover. El archivo nuevo conservará las demás.","Select pages to remove. The new file will keep the remaining pages."),
        workbenchTr("Seleccioná las páginas que querés extraer o ingresá un rango. Se guardan en un nuevo PDF.","Select pages to extract, or enter a range. They are saved in a new PDF."),
        workbenchTr("Arrastrá las páginas para ordenarlas. Podés seleccionar varias para girarlas o quitarlas.","Drag pages to reorder them. Select multiple pages to rotate or remove them."),
        workbenchTr("Guardá un PDF buscable con una capa de texto OCR invisible. Se conservan las imágenes y el texto existente. Usa los idiomas OCR instalados en Windows.","Save a searchable PDF with an invisible OCR text layer. Images and existing text are preserved. Uses OCR languages installed in Windows.")};
    description->setText(descriptions[int(tool())]);
    rangeControls->setVisible(tool()==Remove || tool()==Extract);
    files->setMaximumHeight(112);fileUp->setVisible(tool()==Merge);fileDown->setVisible(tool()==Merge);
    splitControls->setVisible(tool()==Split); pages->setVisible(true); pageControls->setVisible(tool()!=Merge && tool()!=Split && tool()!=Ocr);
    rotate->setVisible(tool()==Organize);removePage->setVisible(tool()==Organize);
    files->setSelectionMode(QAbstractItemView::SingleSelection);
    pages->setDragDropMode(tool()==Organize || tool()==Merge ? QAbstractItemView::InternalMove : QAbstractItemView::NoDragDrop);
    pages->setToolTip(tool()==Merge ? workbenchTr("Arrastrá las miniaturas para ordenar los archivos. La línea azul indica dónde se insertarán.","Drag thumbnails to reorder files. The blue line shows where they will be inserted.") : QString());
    pages->viewport()->setAcceptDrops(true);
    pages->setSelectionMode(tool()==Split || tool()==Ocr ? QAbstractItemView::NoSelection : QAbstractItemView::ExtendedSelection);
    save->setText(tool()==Ocr ? workbenchTr("Reconocer y guardar","Recognize and save") : workbenchTr("Guardar resultado","Save result"));
    status->clear();
    loadPages();
}
void Workbench::loadPages() {
    ++previewGeneration;thumbnailOrder.clear();failedThumbnails.clear();syncSelection=true;pages->clear();range->clear();syncSelection=false;
    const int index=inputIndex();save->setEnabled(index>=0);if(index<0){pageSummary->setText(workbenchTr("Añadí un PDF para comenzar.","Add a PDF to get started."));return;}
    auto& input=inputs[index];
    if(tool()==Merge) {
        for(int row=0;row<files->count();++row) {
            const int source=files->item(row)->data(Qt::UserRole).toInt();
            auto item=new QListWidgetItem(inputs[source].label,pages);
            item->setToolTip(files->item(row)->toolTip());item->setSizeHint({128,166});
            item->setData(Qt::UserRole,source);item->setData(Qt::UserRole+1,0);
        }
    } else for(int p=0;p<input.pdf->pageCount();++p){auto item=new QListWidgetItem(workbenchTr("Página %1","Page %1").arg(p+1),pages);item->setSizeHint({128,166});item->setData(Qt::UserRole,p);item->setData(Qt::UserRole+1,0);}
    refreshSelection();
}
void Workbench::refreshSelection() {
    if(syncSelection)return;
    const int index=inputIndex();if(index<0)return;
    if(tool()==Merge)pageSummary->setText(workbenchTr("%1 archivos","%1 files").arg(files->count()));
    else if(tool()==Split || tool()==Ocr)pageSummary->setText(workbenchTr("%1 páginas","%1 pages").arg(pages->count()));
    else pageSummary->setText(workbenchTr("%1 páginas · %2 seleccionadas","%1 pages · %2 selected").arg(pages->count()).arg(pages->selectedItems().size()));
    if(tool()==Remove || tool()==Extract){QStringList numbers;for(int p=0;p<pages->count();++p)if(pages->item(p)->isSelected())numbers.append(QString::number(pages->item(p)->data(Qt::UserRole).toInt()+1));range->setText(numbers.join(", "));}
}
void Workbench::selectRange() {
    if(inputIndex()<0 || range->text().trimmed().isEmpty())return;
    QVector<int> selection;QString error;
    if(!WorkbenchPdf::parsePages(range->text(),inputs[inputIndex()].pdf->pageCount(),selection,error)){showError(error);return;}
    QSet<int> chosen(selection.begin(),selection.end()); syncSelection=true;
    for(int i=0;i<pages->count();++i) pages->item(i)->setSelected(chosen.contains(pages->item(i)->data(Qt::UserRole).toInt()));
    syncSelection=false; // Preserve the user's explicit range ordering for extraction.
    pageSummary->setText(workbenchTr("%1 páginas · %2 seleccionadas","%1 pages · %2 selected").arg(pages->count()).arg(selection.size()));
}
void Workbench::moveFile(int direction) {
    const int row=files->currentRow(),target=row+direction;if(row<0 || target<0 || target>=files->count())return;
    auto item=files->takeItem(row);files->insertItem(target,item);files->setCurrentItem(item);refreshSelection();
}
void Workbench::thumbnail() {
    if(busy || thumbnailPending || !isVisible() || inputIndex()<0)return;
    const int columns=std::max(1,pages->viewport()->width()/pages->gridSize().width());
    const int start=std::max(0,pages->verticalScrollBar()->value()/pages->gridSize().height()-1)*columns;
    const int end=std::min(pages->count(),start+(pages->viewport()->height()/pages->gridSize().height()+4)*columns);
    for(int row=start;row<end;++row) {
        auto item=pages->item(row);
        if(!item->icon().isNull() || failedThumbnails.contains(item->data(Qt::UserRole).toInt()) || !pages->visualItemRect(item).intersects(pages->viewport()->rect()))continue;
        const int key=item->data(Qt::UserRole).toInt(),turn=item->data(Qt::UserRole+1).toInt(),generation=previewGeneration;
        const int page=tool()==Merge ? 0 : key;
        auto document=inputs[tool()==Merge ? key : inputIndex()].pdf;QPointer<Workbench> target(this);thumbnailPending=true;
        workers.start([target,document,page,key,turn,generation]{
            QImage image;
            {std::lock_guard lock(PdfDocument::apiMutex());const auto size=document->pageSize(page).scaled(QSizeF(104,132),Qt::KeepAspectRatio).toSize();image=document->renderPage(page,1,size);}
            if(turn && !image.isNull())image=image.transformed(QTransform().rotate(turn*90));
            if(!image.isNull()){QPainter border(&image);border.setPen(QColor("#aeb8c7"));border.drawRect(image.rect().adjusted(0,0,-1,-1));}
            if(target) QMetaObject::invokeMethod(target.data(),[target,page=key,turn,generation,image]{
                if(!target)return;target->thumbnailPending=false;
                if(generation!=target->previewGeneration)return;
                if(image.isNull()){target->failedThumbnails.insert(page);return;}
                for(int i=0;i<target->pages->count();++i){auto item=target->pages->item(i);if(item->data(Qt::UserRole).toInt()==page && item->data(Qt::UserRole+1).toInt()==turn){item->setIcon(QPixmap::fromImage(image));break;}}
                target->thumbnailOrder.append({page,turn});
                while(target->thumbnailOrder.size()>96){auto old=target->thumbnailOrder.takeFirst();for(int i=0;i<target->pages->count();++i){auto item=target->pages->item(i);if(item->data(Qt::UserRole).toInt()==old.first && item->data(Qt::UserRole+1).toInt()==old.second)item->setIcon(QIcon());}}
            },Qt::QueuedConnection);
        });return;
    }
}
void Workbench::chooseDestination() {
    const auto path=QFileDialog::getExistingDirectory(this,workbenchTr("Carpeta de salida","Output folder"),destination->text());
    if(path.isEmpty())return;
    destination->setText(QDir::toNativeSeparators(path));
    QSettings settings(QSettings::IniFormat,QSettings::UserScope,AppIdentity::settingsOrganization,AppIdentity::settingsApplication);
    settings.setValue("workbench/outputDirectory",path);
}
void Workbench::showError(const QString& error) {status->setText(error);}
void Workbench::setBusy(bool value) {
    busy=value;for(auto widget:QList<QWidget*>{tools,files,pages,fileControls,rangeControls,splitControls,pageControls,add,current,destination,browse})widget->setEnabled(!value);
    if(!value)current->setEnabled(bool(currentDocument));save->setEnabled(!value && inputIndex()>=0);cancel->setEnabled(true);cancel->setText(value ? workbenchTr("Cancelar","Cancel") : workbenchTr("Cerrar","Close"));progress->setVisible(value);
}
void Workbench::run() {
    if(busy || inputIndex()<0)return;
    const auto suggested=tool()==Merge ? workbenchTr("Unido.pdf","Merged.pdf") : QFileInfo(inputs[inputIndex()].label).completeBaseName()+"_"+workbenchTr("resultado.pdf","result.pdf");
    auto path=QFileDialog::getSaveFileName(this,tool()==Split ? workbenchTr("Elegir nombre base para las partes","Choose a base name for the parts") : workbenchTr("Guardar resultado","Save result"),QDir(destination->text()).filePath(suggested),"PDF (*.pdf)",nullptr,QFileDialog::DontConfirmOverwrite);
    if(path.isEmpty())return;
    if(!path.endsWith(".pdf",Qt::CaseInsensitive))path+=".pdf";
    path=QFileInfo(path).absoluteFilePath();
    QVector<WorkbenchOutput> outputs;
    auto ref=[&](int index,int page,int turn=0){const auto& in=inputs[index];return WorkbenchPage{in.pdf,page,turn,in.original};};
    const int index=inputIndex(),count=inputs[index].pdf->pageCount();
    if(tool()==Ocr && !inputs[index].modifyPermitted){showError(workbenchTr("El PDF no permite incorporar texto OCR.","The PDF does not allow adding OCR text."));return;}
    if(tool()==Split) {
        QVector<QVector<int>> parts;
        if(splitMode->currentIndex()==0){for(int p=0;p<count;p+=chunk->value()){QVector<int> part;for(int j=p;j<std::min(count,p+chunk->value());++j)part.append(j);parts.append(part);}}
        else {QString error;for(const auto& text:groups->text().split(';')){QVector<int> part;if(!WorkbenchPdf::parsePages(text,count,part,error)){showError(error);return;}parts.append(part);}}
        const auto base=QFileInfo(path).completeBaseName();
        for(int i=0;i<parts.size();++i){WorkbenchOutput output;output.path=QFileInfo(path).dir().filePath(base+QString("_%1.pdf").arg(i+1,3,10,QChar('0')));for(int p:parts[i])output.pages.append(ref(index,p));outputs.append(output);}
    } else {
        WorkbenchOutput output;output.path=path;output.ocr=tool()==Ocr;
        if(tool()==Merge){for(int row=0;row<files->count();++row){int id=files->item(row)->data(Qt::UserRole).toInt();for(int p=0;p<inputs[id].pdf->pageCount();++p)output.pages.append(ref(id,p));}}
        else if(tool()==Remove || tool()==Extract){
            QVector<int> selected;QString error;if(!WorkbenchPdf::parsePages(range->text(),count,selected,error)){showError(error);return;}
            if(tool()==Extract){for(int p:selected)output.pages.append(ref(index,p));}
            else {QSet<int> removed(selected.begin(),selected.end());for(int p=0;p<count;++p)if(!removed.contains(p))output.pages.append(ref(index,p));}
        } else for(int row=0;row<pages->count();++row){auto item=pages->item(row);output.pages.append(ref(index,item->data(Qt::UserRole).toInt(),item->data(Qt::UserRole+1).toInt()));}
        if(output.pages.isEmpty()){showError(workbenchTr("El resultado debe contener al menos una página.","The result must contain at least one page."));return;}
        if(QFileInfo::exists(path)) {
            for(const auto& input:inputs)if(!input.original.isEmpty() && QFileInfo(input.original).canonicalFilePath().compare(QFileInfo(path).canonicalFilePath(),Qt::CaseInsensitive)==0){showError(workbenchTr("Elegir un archivo distinto del original.","Choose a file different from the original."));return;}
            if(QMessageBox::question(this,QStringLiteral("Workbench"),workbenchTr("El archivo ya existe. ¿Reemplazarlo?","The file already exists. Replace it?"))!=QMessageBox::Yes)return;
            output.overwrite=true;
        }
        outputs.append(output);
    }
    cancellation=std::make_shared<std::atomic_bool>(false);auto cancelled=cancellation;
    setBusy(true);progress->setRange(0,0);status->setText(workbenchTr("Preparando archivos…","Preparing files…"));
    QPointer<Workbench> target(this);
    workers.start([target,outputs,cancelled]{
        const auto error=WorkbenchExport::run(outputs,*cancelled,[target](int done,int total){if(target)QMetaObject::invokeMethod(target.data(),[target,done,total]{if(target){target->progress->setRange(0,total);target->progress->setValue(done);target->status->setText(workbenchTr("Procesando página %1 de %2…","Processing page %1 of %2…").arg(done).arg(total));}},Qt::QueuedConnection);});
        if(target)QMetaObject::invokeMethod(target.data(),[target,error,outputs]{if(!target)return;target->setBusy(false);if(error.isEmpty()){target->lastSavedFile=outputs.last().path;target->reveal->setEnabled(true);target->reveal->setToolTip(QDir::toNativeSeparators(target->lastSavedFile));}target->status->setText(error.isEmpty() ? workbenchTr("Listo. Se guardaron %1 archivos en %2","Done. Saved %1 files in %2").arg(outputs.size()).arg(QDir::toNativeSeparators(QFileInfo(outputs.first().path).absolutePath())) : error);},Qt::QueuedConnection);
    });
}
