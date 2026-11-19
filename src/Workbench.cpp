#include "Workbench.h"
#include <QDesktopServices>
#include <QListView>
#include <QUrl>
#include <QMimeData>
#include <QDrag>
#include <QSaveFile>
#include <QScreen>
#include <QStyledItemDelegate>
#include <QShortcut>
#include <QCursor>

namespace {
class WorkbenchPageDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    void paint(QPainter* painter,const QStyleOptionViewItem& option,const QModelIndex& index) const override {
        painter->save();
        if(index.data(Qt::UserRole+2).toBool())painter->setOpacity(0.3);
        QStyledItemDelegate::paint(painter,option,index);
        painter->restore();
    }
};
}
#ifdef Q_OS_WIN
#include <shlobj.h>
#include <cstring>

namespace {
const QString fileContents=QStringLiteral("application/x-qt-windows-mime;value=\"FileContents\"");
using Attachments=QList<QPair<int,QString>>;

template<class Descriptor>
Attachments pdfAttachments(const QByteArray& data) {
    UINT count=0;
    if(data.size()<qsizetype(sizeof(count)))return {};
    std::memcpy(&count,data.constData(),sizeof(count));
    if(count>(data.size()-sizeof(count))/sizeof(Descriptor))return {};
    Attachments result;
    for(UINT index=0;index<count;++index) {
        Descriptor descriptor;
        std::memcpy(&descriptor,data.constData()+sizeof(count)+index*sizeof(descriptor),sizeof(descriptor));
        if((descriptor.dwFlags & FD_ATTRIBUTES) && (descriptor.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))continue;
        const auto begin=std::begin(descriptor.cFileName),end=std::end(descriptor.cFileName);
        const auto terminator=std::find(begin,end,0);
        if(terminator==end)continue;
        QString name;
        if constexpr(std::is_same_v<Descriptor,FILEDESCRIPTORW>)
            name=QString::fromWCharArray(begin,terminator-begin);
        else
            name=QString::fromLocal8Bit(begin,terminator-begin);
        name=QFileInfo(QDir::fromNativeSeparators(name)).fileName();
        if(name.endsWith(".pdf",Qt::CaseInsensitive))result.append({int(index),name});
    }
    return result;
}

Attachments pdfAttachments(const QMimeData& data) {
    const auto wide=data.data(QStringLiteral("application/x-qt-windows-mime;value=\"FileGroupDescriptorW\""));
    if(!wide.isEmpty())return pdfAttachments<FILEDESCRIPTORW>(wide);
    return pdfAttachments<FILEDESCRIPTORA>(data.data(QStringLiteral("application/x-qt-windows-mime;value=\"FileGroupDescriptor\"")));
}
}
#endif

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
    {
        QSignalBlocker blocker(this);
        for(auto item:selected)takeItem(row(item));
        clearSelection();
        destination=std::clamp(destination-before,0,count());
        for(auto item:selected){insertItem(destination++,item);item->setSelected(true);}
        setCurrentItem(selected.first(),QItemSelectionModel::NoUpdate);
    }
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
    setWindowFlag(Qt::WindowContextHelpButtonHint,false);
    const QSize available=(parent ? parent->screen() : screen())->availableGeometry().size();
    const bool compact=available.height()<850 || available.width()<1150;
    const auto initialSize=QSize(1080,780).boundedTo({qRound(available.width()*0.88),qRound(available.height()*0.88)});
    workers.setMaxThreadCount(1);
    auto root=new QVBoxLayout(this);
    const int margin=compact ? 10 : 18;
    root->setContentsMargins(margin,margin,margin,compact ? 8 : 14); root->setSpacing(compact ? 7 : 12);
    auto body=new QHBoxLayout; body->setSpacing(margin); root->addLayout(body,1);
    auto sidebar=new QVBoxLayout;
    auto brand=new QLabel(QStringLiteral("Workbench")); auto brandFont=brand->font(); brandFont.setPointSize(18); brandFont.setBold(true); brand->setFont(brandFont); sidebar->addWidget(brand);
    tools=new QListWidget; tools->setObjectName("workbenchTools"); tools->setFixedWidth(205); tools->setSpacing(3);
    tools->addItems({workbenchTr("Unir PDFs","Merge PDFs"),workbenchTr("Separar PDFs","Split PDFs"),workbenchTr("Remover páginas","Remove pages"),workbenchTr("Extraer páginas","Extract pages"),workbenchTr("Organizar PDFs","Organize PDFs"),workbenchTr("Guardar OCR en PDF","Save OCR to PDF")});
    for(int i=0;i<tools->count();++i) tools->item(i)->setSizeHint({190,compact ? 32 : 42});
    sidebar->addWidget(tools,1); body->addLayout(sidebar);
    auto content=new QVBoxLayout; content->setSpacing(compact ? 6 : 10); body->addLayout(content,1);
    title=new QLabel; auto heading=title->font(); heading.setPointSize(16); heading.setBold(true); title->setFont(heading); content->addWidget(title);
    description=new QLabel; description->setWordWrap(true); description->setMinimumHeight(40); content->addWidget(description);
    auto fileButtons=new QHBoxLayout;
    add=new QPushButton(workbenchTr("Añadir PDFs…","Add PDFs…")); add->setObjectName("workbenchAddFiles");
    current=new QPushButton(workbenchTr("Usar PDF actual","Use current PDF")); current->setObjectName("workbenchUseCurrent"); current->setEnabled(false);
    fileButtons->addWidget(add); fileButtons->addWidget(current); fileButtons->addStretch(); content->addLayout(fileButtons);
    files=new QListWidget; files->setObjectName("workbenchFiles"); files->setDragDropMode(QAbstractItemView::InternalMove); files->setMaximumHeight(compact ? 76 : 112); files->setMinimumHeight(compact ? 48 : 68); content->addWidget(files);
    fileControls=new QWidget; auto controls=new QHBoxLayout(fileControls); controls->setContentsMargins(0,0,0,0);
    fileUp=new QPushButton(workbenchTr("Subir","Move up")); fileDown=new QPushButton(workbenchTr("Bajar","Move down")); removeFile=new QPushButton(workbenchTr("Quitar archivo","Remove file"));
    fileUp->setObjectName("workbenchFileUp");fileDown->setObjectName("workbenchFileDown");
    controls->addWidget(fileUp); controls->addWidget(fileDown); controls->addWidget(removeFile); controls->addStretch(); content->addWidget(fileControls);
    pageSummary=new QLabel; pageSummary->setObjectName("workbenchPageSummary"); content->addWidget(pageSummary);
    rangeControls=new QWidget; auto rangeLayout=new QHBoxLayout(rangeControls); rangeLayout->setContentsMargins(0,0,0,0);
    rangeLayout->addWidget(new QLabel(workbenchTr("Páginas:","Pages:"))); range=new QLineEdit; range->setObjectName("workbenchRange"); range->setPlaceholderText(workbenchTr("Ejemplo: 1, 3-5","Example: 1, 3-5")); rangeLayout->addWidget(range,1); content->addWidget(rangeControls);
    splitControls=new QWidget; auto splitLayout=new QHBoxLayout(splitControls); splitLayout->setContentsMargins(0,0,0,0);
    splitMode=new ViewModeCombo; splitMode->setObjectName("workbenchSplitMode"); splitMode->addItems({workbenchTr("Cada N páginas","Every N pages"),workbenchTr("Grupos de páginas","Page groups")});
    splitMode->setStyleSheet("QComboBox { combobox-popup: 0; }");
    chunk=new QSpinBox; chunk->setObjectName("workbenchChunk"); chunk->setRange(1,100000); chunk->setValue(1);
    groups=new QLineEdit; groups->setObjectName("workbenchGroups"); groups->setPlaceholderText("1-3; 4-6; 7-10"); groups->hide();
    splitLayout->addWidget(splitMode); splitLayout->addWidget(chunk); splitLayout->addWidget(groups,1); content->addWidget(splitControls);
    pages=new WorkbenchPageList; pages->setObjectName("workbenchPages"); pages->setViewMode(QListView::IconMode); pages->setResizeMode(QListView::Adjust);
    pages->setItemDelegate(new WorkbenchPageDelegate(pages));
    thumbnailItemSize=compact ? QSize(104,128) : QSize(128,166);
    pages->setMovement(QListView::Snap); pages->setWrapping(true); pages->setIconSize(compact ? QSize(80,98) : QSize(104,132)); pages->setGridSize(thumbnailItemSize+QSize(8,8)); pages->setUniformItemSizes(true);
    pages->setSelectionMode(QAbstractItemView::ExtendedSelection); pages->setSpacing(4); pages->setMinimumHeight(compact ? 100 : 174); content->addWidget(pages,1);
    pages->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    pages->setDefaultDropAction(Qt::MoveAction);pages->setDropIndicatorShown(false);
    static_cast<WorkbenchPageList*>(pages)->orderChanged=[this]{
        if(tool()!=Merge)return;
        const int selected=pages->currentItem() ? pages->currentItem()->data(Qt::UserRole).toInt() : inputIndex();
        QSignalBlocker blocker(files);
        for(int row=0;row<pages->count();++row) {
            const int id=pages->item(row)->data(Qt::UserRole).toInt();
            for(int j=row;j<files->count();++j)if(files->item(j)->data(Qt::UserRole).toInt()==id){auto item=files->takeItem(j);files->insertItem(row,item);break;}
        }
        for(int row=0;row<files->count();++row)if(files->item(row)->data(Qt::UserRole).toInt()==selected){
            files->setCurrentRow(row);
            files->scrollToItem(files->currentItem());
        }
        refreshSelection();
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
    connect(files,&QListWidget::currentRowChanged,this,[this](int row){
        if(tool()!=Merge){loadPages();return;}
        if(syncSelection || row<0 || row>=pages->count())return;
        QSignalBlocker blocker(pages);
        pages->setCurrentRow(row,QItemSelectionModel::ClearAndSelect);
    });
    connect(files->model(),&QAbstractItemModel::rowsAboutToBeMoved,this,[this]{syncSelection=true;});
    connect(files->model(),&QAbstractItemModel::rowsMoved,this,[this]{syncSelection=false;if(tool()==Merge)loadPages();});
    connect(fileUp,&QPushButton::clicked,this,[this]{moveFile(-1);}); connect(fileDown,&QPushButton::clicked,this,[this]{moveFile(1);});
    connect(removeFile,&QPushButton::clicked,this,[this]{removeInputs({inputIndex()});});
    auto deleteFiles=new QShortcut(QKeySequence(Qt::Key_Delete),this);
    deleteFiles->setAutoRepeat(false);
    connect(deleteFiles,&QShortcut::activated,this,[this]{
        if(busy || (tool()!=Merge && tool()!=Organize))return;
        auto focus=QApplication::focusWidget();
        if(qobject_cast<QLineEdit*>(focus) || qobject_cast<QAbstractSpinBox*>(focus))return;
        const auto hovered=pages->itemAt(pages->viewport()->mapFromGlobal(QCursor::pos()));
        if(tool()==Organize){
            if(hovered && !hovered->isSelected()){
                pages->setCurrentItem(hovered,QItemSelectionModel::ClearAndSelect);
            }
            if(pages->hasFocus() || hovered)removePage->click();
            return;
        }
        QSet<int> indexes;
        if(hovered && !hovered->isSelected())indexes.insert(hovered->data(Qt::UserRole).toInt());
        else if(pages->hasFocus() || hovered) {
            for(auto item:pages->selectedItems())indexes.insert(item->data(Qt::UserRole).toInt());
        } else if(files->hasFocus())indexes.insert(inputIndex());
        removeInputs(indexes);
    });
    connect(range,&QLineEdit::editingFinished,this,[this]{selectRange();});
    connect(range,&QLineEdit::textEdited,this,[this]{selectRange();});
    connect(splitMode,&QComboBox::currentIndexChanged,this,[this](int mode){chunk->setVisible(mode==0);groups->setVisible(mode==1);});
    splitMode->setCurrentIndex(1);
    connect(selectAll,&QPushButton::clicked,pages,&QListWidget::selectAll);
    connect(pages,&QListWidget::itemSelectionChanged,this,[this]{refreshSelection();});
    connect(pages,&QListWidget::currentRowChanged,this,[this](int row){
        if(tool()!=Merge || syncSelection || row<0)return;
        QSignalBlocker blocker(files);
        files->setCurrentRow(row);
        files->scrollToItem(files->currentItem());
    });
    connect(rotate,&QPushButton::clicked,this,[this]{
        ++previewGeneration;failedThumbnails.clear();
        for(auto item:pages->selectedItems()) { int turn=(item->data(Qt::UserRole+1).toInt()+1)%4; item->setData(Qt::UserRole+1,turn);item->setIcon(QIcon());item->setText(workbenchTr("Página %1","Page %1").arg(item->data(Qt::UserRole).toInt()+1)+(turn ? QString(" · %1°").arg(turn*90) : QString())); }
    });
    connect(removePage,&QPushButton::clicked,this,[this]{for(auto item:pages->selectedItems()) delete pages->takeItem(pages->row(item));refreshSelection();});
    connect(reset,&QPushButton::clicked,this,[this]{loadPages();});
    connect(browse,&QPushButton::clicked,this,[this]{chooseDestination();});
    connect(save,&QPushButton::clicked,this,[this]{run();});
    connect(cancel,&QPushButton::clicked,this,[this]{if(busy){cancellation->store(true);cancel->setEnabled(false);status->setText(workbenchTr("Cancelando…","Cancelling…"));}else reject();});
    thumbnailTimer.setSingleShot(true);
    thumbnailTimer.setInterval(16);
    connect(&thumbnailTimer,&QTimer::timeout,this,[this]{thumbnail();});
    for(auto target:{static_cast<QWidget*>(this),files->viewport(),pages->viewport(),tools->viewport(),
                     static_cast<QWidget*>(destination),static_cast<QWidget*>(range),static_cast<QWidget*>(groups)}) {
        target->setAcceptDrops(true);
        target->installEventFilter(this);
    }
    tools->setCurrentRow(0);
    resize(initialSize);
}
Workbench::~Workbench() { thumbnailTimer.stop();if(cancellation)cancellation->store(true);workers.clear();workers.waitForDone(); }
void Workbench::reject() { if(busy){cancellation->store(true);return;} QDialog::reject(); }
void Workbench::closeEvent(QCloseEvent* event) {if(busy){cancellation->store(true);event->ignore();return;} QDialog::closeEvent(event);}
void Workbench::setCurrentDocument(std::function<void(Workbench&)> callback) {currentDocument=std::move(callback);current->setEnabled(bool(currentDocument));}
int Workbench::inputIndex() const {return files->currentItem() ? files->currentItem()->data(Qt::UserRole).toInt() : -1;}
bool Workbench::addFile(const QString& path,const QByteArray& password,const QString& label) {
    if(busy) return false;
    auto document=std::make_shared<PdfDocument>();
    if(!document->open(path,password)){showError(document->error());return false;}
    const int index=inputs.size(); const auto name=label.isEmpty() ? QFileInfo(path).fileName() : label;
    inputs.append({document,path,name,path});
    auto item=new QListWidgetItem(QString("%1  ·  %2").arg(name,workbenchTr("%1 páginas","%1 pages").arg(document->pageCount())),files); item->setData(Qt::UserRole,index);
    files->setCurrentItem(item);save->setEnabled(true);return true;
}
void Workbench::useSnapshot(const PdfDocument& document,const QString& label,const QString& originalPath) {
    if(busy)return;
    const auto path=snapshots.filePath(QUuid::createUuid().toString(QUuid::WithoutBraces)+".pdf");
    if(!document.saveRecovery(path)) {showError(workbenchTr("No se pudo preparar el PDF actual.","Could not prepare the current PDF."));return;}
    {
        QSignalBlocker blocker(files);
        if(!addFile(path,document.password_,label))return;
        inputs.last().original=originalPath;
    }
    loadPages();
}
void Workbench::chooseFiles() {
    const auto paths=QFileDialog::getOpenFileNames(this,workbenchTr("Añadir PDFs","Add PDFs"),{},"PDF (*.pdf)");
    importFiles(paths);
}

bool Workbench::eventFilter(QObject* object,QEvent* event) {
    const auto type=event->type();
    if(object==pages->viewport() && type==QEvent::Paint && !busy && !thumbnailPending && !thumbnailTimer.isActive())
        thumbnailTimer.start();
    if(type==QEvent::DragEnter || type==QEvent::DragMove || type==QEvent::Drop) {
        auto drop=static_cast<QDropEvent*>(event);
        const auto& data=*drop->mimeData();
        const bool canCopy=!busy && drop->possibleActions().testFlag(Qt::CopyAction);
        QStringList paths;
        if(canCopy) {
            for(const auto& url:data.urls()) {
                if(!url.isLocalFile()) continue;
                const QFileInfo file(url.toLocalFile());
                if(file.suffix().compare("pdf",Qt::CaseInsensitive)==0 && file.isFile())
                    paths.append(file.absoluteFilePath());
            }
            paths.removeDuplicates();
        }
        QHash<QString,QString> labels;
        bool failedAttachment=false;
#ifdef Q_OS_WIN
        if(canCopy && paths.isEmpty() && !drop->source()) {
            const auto attachments=pdfAttachments(data);
            if(!attachments.isEmpty() && type!=QEvent::Drop) {
                drop->setDropAction(Qt::CopyAction);
                drop->accept();
                return true;
            }
            // The source's MIME data is only valid during the drop event.
            for(const auto& [index,name]:attachments) {
                const auto bytes=data.data(fileContents+QStringLiteral(";index=%1").arg(index));
                if(!snapshots.isValid() || bytes.isEmpty()) {
                    failedAttachment=true;
                    continue;
                }
                const auto path=snapshots.filePath(QUuid::createUuid().toString(QUuid::WithoutBraces)+".pdf");
                QSaveFile file(path);
                if(!file.open(QIODevice::WriteOnly) || file.write(bytes)!=bytes.size() || !file.commit()) {
                    failedAttachment=true;
                    continue;
                }
                paths.append(path);
                labels.insert(path,name);
            }
        }
#endif
        if(!paths.isEmpty()) {
            drop->setDropAction(Qt::CopyAction);
            drop->accept();
        } else if(data.hasUrls() || failedAttachment) {
            drop->ignore();
        }
        if(type==QEvent::Drop && (!paths.isEmpty() || failedAttachment)) {
            QTimer::singleShot(0,this,[this,paths,labels,failedAttachment]{
                if(failedAttachment)showError(workbenchTr("No se pudieron leer uno o más adjuntos PDF.","Could not read one or more PDF attachments."));
                importFiles(paths,labels);
            });
        }
        if(!paths.isEmpty() || data.hasUrls() || failedAttachment)
            return true;
    }
    return QDialog::eventFilter(object,event);
}

void Workbench::importFiles(const QStringList& paths) {
    importFiles(paths,{});
}
void Workbench::importFiles(const QStringList& paths,const QHash<QString,QString>& labels) {
    if(busy || paths.isEmpty()) return;
    auto previous=files->currentItem();
    QSignalBlocker blocker(files);
    for(const auto& path:paths) addFile(path,{},labels.value(path));
    if(previous)files->setCurrentItem(previous);
    if(tool()==Merge || !previous)loadPages();
}
void Workbench::changeTool() {
    if(tools->currentRow()<0)return;
    title->setText(tools->currentItem()->text());
    const QString descriptions[]={
        workbenchTr("Convierte en orden y unifica los archivos añadidos en un solo documento","Converts in order and combines the added files into a single document"),
        workbenchTr("Dividir el PDF seleccionado cada cierta cantidad de páginas o en grupos separados.","Split the selected PDF every few pages or into separate groups."),
        workbenchTr("Seleccionar las páginas a remover. El documento final conservará las restantes.","Select the pages to remove. The final document will keep the remaining pages."),
        workbenchTr("Seleccionar las páginas a extraer en un nuevo documento.","Select the pages to extract into a new document."),
        workbenchTr("Herramienta para ordenar, quitar, y girar páginas de un documento.","Tool for reordering, removing, and rotating pages in a document."),
        workbenchTr("Guardar un documento añadiendo una capa de texto OCR invisible.","Save a document by adding an invisible OCR text layer.")};
    description->setText(descriptions[int(tool())]);
    rangeControls->setVisible(tool()==Remove || tool()==Extract);
    fileUp->setVisible(tool()==Merge);fileDown->setVisible(tool()==Merge);
    splitControls->setVisible(tool()==Split); pages->setVisible(true); pageControls->setVisible(tool()!=Merge && tool()!=Split && tool()!=Ocr);
    rotate->setVisible(tool()==Organize);removePage->setVisible(tool()==Organize);
    files->setSelectionMode(QAbstractItemView::SingleSelection);
    pages->setDragDropMode(tool()==Organize || tool()==Merge ? QAbstractItemView::InternalMove : QAbstractItemView::NoDragDrop);
    pages->viewport()->setAcceptDrops(true);
    pages->setSelectionMode(tool()==Split || tool()==Ocr ? QAbstractItemView::NoSelection : QAbstractItemView::ExtendedSelection);
    save->setText(tool()==Ocr ? workbenchTr("Reconocer y guardar","Recognize and save") : workbenchTr("Guardar resultado","Save result"));
    status->clear();
    loadPages();
}
void Workbench::loadPages() {
    if(!rangeError.isEmpty() && status->text()==rangeError)status->clear();
    rangeError.clear();
    ++previewGeneration;thumbnailOrder.clear();failedThumbnails.clear();syncSelection=true;pages->clear();range->clear();syncSelection=false;
    const int index=inputIndex();save->setEnabled(index>=0);if(index<0){pageSummary->clear();return;}
    auto& input=inputs[index];
    if(tool()==Merge) {
        for(int row=0;row<files->count();++row) {
            const int source=files->item(row)->data(Qt::UserRole).toInt();
            auto item=new QListWidgetItem(inputs[source].label,pages);
            item->setSizeHint(thumbnailItemSize);
            item->setData(Qt::UserRole,source);item->setData(Qt::UserRole+1,0);
        }
    } else for(int p=0;p<input.pdf->pageCount();++p){auto item=new QListWidgetItem(workbenchTr("Página %1","Page %1").arg(p+1),pages);item->setSizeHint(thumbnailItemSize);item->setData(Qt::UserRole,p);item->setData(Qt::UserRole+1,0);}
    if(tool()==Merge){
        QSignalBlocker blocker(pages);
        pages->setCurrentRow(files->currentRow(),QItemSelectionModel::ClearAndSelect);
    }
    refreshSelection();
}
void Workbench::refreshSelection() {
    if(syncSelection)return;
    if(!rangeError.isEmpty() && status->text()==rangeError)status->clear();
    rangeError.clear();
    const int index=inputIndex();if(index<0)return;
    if(tool()==Merge)pageSummary->setText(workbenchTr("%1 archivos","%1 files").arg(files->count()));
    else if(tool()==Split || tool()==Ocr)pageSummary->setText(workbenchTr("%1 páginas","%1 pages").arg(pages->count()));
    else pageSummary->setText(workbenchTr("%1 páginas · %2 seleccionadas","%1 pages · %2 selected").arg(pages->count()).arg(pages->selectedItems().size()));
    if(tool()==Remove || tool()==Extract){QStringList numbers;for(int p=0;p<pages->count();++p)if(pages->item(p)->isSelected())numbers.append(QString::number(pages->item(p)->data(Qt::UserRole).toInt()+1));range->setText(numbers.join(", "));}
    updatePagePreview();
}
void Workbench::updatePagePreview() {
    if(tool()!=Remove && tool()!=Extract)return;
    const bool hasSelection=!pages->selectedItems().isEmpty();
    for(int row=0;row<pages->count();++row) {
        auto item=pages->item(row);
        const bool excluded=(tool()==Remove && item->isSelected()) ||
                            (tool()==Extract && hasSelection && !item->isSelected());
        if(item->data(Qt::UserRole+2).toBool()==excluded)continue;
        item->setData(Qt::UserRole+2,excluded);
        auto font=item->font();
        font.setStrikeOut(tool()==Remove && excluded);
        item->setFont(font);
    }
}
void Workbench::selectRange() {
    if(inputIndex()<0)return;
    QVector<int> selection;QString error;
    if(!range->text().trimmed().isEmpty() &&
       !WorkbenchPdf::parsePages(range->text(),inputs[inputIndex()].pdf->pageCount(),selection,error))selection.clear();
    if(!rangeError.isEmpty() && status->text()==rangeError)status->clear();
    rangeError=error;
    if(!error.isEmpty())showError(error);
    QSet<int> chosen(selection.begin(),selection.end()); syncSelection=true;
    for(int i=0;i<pages->count();++i) pages->item(i)->setSelected(chosen.contains(pages->item(i)->data(Qt::UserRole).toInt()));
    syncSelection=false; // Preserve the user's explicit range ordering for extraction.
    updatePagePreview();
    pageSummary->setText(workbenchTr("%1 páginas · %2 seleccionadas","%1 pages · %2 selected").arg(pages->count()).arg(selection.size()));
}
void Workbench::moveFile(int direction) {
    if(busy)return;
    const int row=files->currentRow(),target=row+direction;if(row<0 || target<0 || target>=files->count())return;
    {
        QSignalBlocker blocker(files);
        auto item=files->takeItem(row);
        files->insertItem(target,item);
        files->setCurrentItem(item);
    }
    loadPages();
    pages->setCurrentRow(target,QItemSelectionModel::ClearAndSelect);
}
void Workbench::removeInputs(const QSet<int>& indexes) {
    if(busy || indexes.isEmpty())return;
    const int previousRow=files->currentRow();
    {
        QSignalBlocker blocker(files);
        for(int row=files->count()-1;row>=0;--row) {
            const int index=files->item(row)->data(Qt::UserRole).toInt();
            if(!indexes.contains(index))continue;
            delete files->takeItem(row);
            inputs[index]=Input{};
        }
        if(files->count() && files->currentRow()<0)
            files->setCurrentRow(std::clamp(previousRow,0,files->count()-1));
    }
    loadPages();
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
        workers.start([target,document,page,key,turn,generation,row]{
            QImage image;
            {std::lock_guard lock(PdfDocument::apiMutex());const auto size=document->pageSize(page).scaled(QSizeF(104,132),Qt::KeepAspectRatio).toSize();image=document->renderPage(page,1,size);}
            if(turn && !image.isNull())image=image.transformed(QTransform().rotate(turn*90));
            if(!image.isNull()){QPainter border(&image);border.setPen(QColor("#aeb8c7"));border.drawRect(image.rect().adjusted(0,0,-1,-1));}
            if(target) QMetaObject::invokeMethod(target.data(),[target,page=key,turn,generation,row,image]{
                if(!target)return;target->thumbnailPending=false;
                target->thumbnailTimer.start();
                if(generation!=target->previewGeneration)return;
                if(image.isNull()){target->failedThumbnails.insert(page);return;}
                auto item=target->pages->item(row);
                if(!item || item->data(Qt::UserRole).toInt()!=page || item->data(Qt::UserRole+1).toInt()!=turn)return;
                item->setIcon(QPixmap::fromImage(image));
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
        if(target)QMetaObject::invokeMethod(target.data(),[target,error,outputs]{if(!target)return;target->setBusy(false);if(error.isEmpty()){target->lastSavedFile=outputs.last().path;target->reveal->setEnabled(true);}target->status->setText(error.isEmpty() ? workbenchTr("Listo. Se guardaron %1 archivos en %2","Done. Saved %1 files in %2").arg(outputs.size()).arg(QDir::toNativeSeparators(QFileInfo(outputs.first().path).absolutePath())) : error);},Qt::QueuedConnection);
    });
}
