#include "Window.h"
#include "UiFont.h"
#include "WelcomeDialog.h"
#include <QPdfWriter>
#include <QMimeData>
#include <QTemporaryDir>
#include <iostream>
#include <stdexcept>
#ifdef Q_OS_WIN
#include <shlobj.h>
#endif

static void require(bool ok,const char* message) { if(!ok)throw std::runtime_error(message); }
static QByteArray digest(const QString& path) {QFile file(path);require(file.open(QIODevice::ReadOnly),"read hash input");return QCryptographicHash::hash(file.readAll(),QCryptographicHash::Sha256);}
int main(int argc,char** argv) {
 QApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
 QApplication app(argc,argv);app.setStyle("Fusion");app.setFont(gpdfUiFont());
 try {
  QTemporaryDir temp;require(temp.isValid(),"temporary directory");
  QSettings::setDefaultFormat(QSettings::IniFormat);QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,temp.path());app.setApplicationName("GPDF-Workbench-Test");
  const bool english=app.arguments().contains("--english");initializeUiLanguage(english ? "en" : "es");
  const auto first=temp.filePath("first.pdf"),second=temp.filePath("second.pdf"),scan=temp.filePath("scan.pdf");
  auto a=std::make_shared<PdfDocument>(),b=std::make_shared<PdfDocument>();
  require(a->createSample(5),"first fixture");require(a->setPage(2) && a->note({100,600},"Workbench annotation"),"annotated page");require(a->saveCopy(first),"save first");
  require(b->createSample(2,"Second document"),"second fixture");require(b->saveCopy(second),"save second");
  const auto originalHash=digest(first);
  QVector<int> parsed;QString error;
  require(WorkbenchPdf::parsePages("5, 1, 3-4",5,parsed,error) && parsed==QVector<int>({4,0,2,3}),"range order");
  require(WorkbenchPdf::parsePages("5-3",5,parsed,error) && parsed==QVector<int>({4,3,2}),"descending ranges");
  for(const auto& invalid:{"","0","6","1,,2","1-","1,1","1-3,2","999999999999999"})require(!WorkbenchPdf::parsePages(invalid,5,parsed,error),"reject invalid ranges");
  std::atomic_bool cancelled=false;
  WorkbenchOutput job;job.path=temp.filePath("core-merged.pdf");for(int p:{4,0,2})job.pages.append({a,p,0,first});job.pages.append({b,1,0,second});
  error=WorkbenchExport::run({job},cancelled);require(error.isEmpty(),qPrintable(error));
  PdfDocument reopened;require(reopened.open(job.path) && reopened.pageCount()==4,"merged count");require(reopened.pageText(0).contains("Page 5") && reopened.pageText(3).contains("Second"),"merged order");
  require(reopened.setPage(2) && reopened.annotationCount()==1 && reopened.annotationContents(0)=="Workbench annotation","annotations preserved");
  WorkbenchOutput invalid=job;invalid.path=first;require(!WorkbenchExport::run({invalid},cancelled).isEmpty() && digest(first)==originalHash,"never overwrite original");
  auto existingHash=digest(job.path);require(!WorkbenchExport::run({job},cancelled).isEmpty() && digest(job.path)==existingHash,"no implicit replacement");
  WorkbenchOutput part=job;part.path=temp.filePath("part-one.pdf");invalid.path=temp.filePath("part-two.pdf");invalid.pages.last().page=999;
  require(!WorkbenchExport::run({part,invalid},cancelled).isEmpty() && !QFileInfo::exists(part.path) && !QFileInfo::exists(invalid.path),"failed split publishes no partial output");
  require(!WorkbenchExport::run({part},cancelled,[&](int,int){cancelled=true;}).isEmpty() && !QFileInfo::exists(part.path),"cancelled export creates no file");cancelled=false;
  const auto restrictedPath=QString(GPDF_SOURCE_DIR)+"/tests/data/workbench-restricted.pdf";
  const auto restrictedHash=digest(restrictedPath);
  auto restricted=std::make_shared<PdfDocument>();
  require(restricted->open(restrictedPath) &&
          restricted->properties().contains({gpdfTr("Modificación permitida"),gpdfTr("No")}),"restricted PDF opens without a password");
  WorkbenchOutput restrictedOcr;
  restrictedOcr.path=temp.filePath("restricted-ocr.pdf");
  restrictedOcr.ocr=true;
  restrictedOcr.pages.append({restricted,0,0,restrictedPath});
  error=WorkbenchExport::run({restrictedOcr},cancelled,{},[](const QImage&,QSizeF,auto&,QString&){return true;});
  require(error.isEmpty(),qPrintable(error));
  require(reopened.open(restrictedOcr.path) &&
          reopened.properties().contains({gpdfTr("Seguridad"),gpdfTr("Sin cifrado")}),"OCR export removes source permissions");
  {
   QImage image(1200,500,QImage::Format_RGB32);image.fill(Qt::white);QPainter p(&image);p.setFont(QFont("Arial",48));p.drawText(50,150,"GPDF OCR test 123");p.end();
   QPdfWriter writer(scan);writer.setResolution(72);writer.setPageSize(QPageSize(QSizeF(600,250),QPageSize::Point));writer.setPageMargins(QMarginsF(),QPageLayout::Point);QPainter painter(&writer);painter.drawImage(QRectF(0,0,600,250),image);
  }
  auto scanned=std::make_shared<PdfDocument>();require(scanned->open(scan) && scanned->pageNeedsOcr(0),"scan fixture has no text");
  for(int turn=0;turn<4;++turn){
   WorkbenchPdf result;require(result.appendPage(*scanned,0,turn),"import rotated scan");auto before=result.document().renderPage(0,1);
   const QVector<PdfDocument::OcrWord> words={{"OCR",{30,30,80,20},0},{QString::fromUtf8("áéíóú ñ 123"),{120,30,130,20},0}};
   require(result.addOcrText(0,words),qPrintable(result.error()));const auto out=temp.filePath(QString("ocr-%1.pdf").arg(turn));require(result.save(out),"save OCR");
   PdfDocument check;require(check.open(out),"reopen OCR");require(check.pageText(0).contains("OCR") && check.pageText(0).contains(QString::fromUtf8("áéíóú")),"Unicode OCR persists as native PDF text");
   require(check.renderPage(0,1)==before,"OCR invisible layer preserves pixels");auto matches=check.searchPage(0,"OCR");require(!matches.isEmpty(),"persisted OCR searchable");
   const auto normalized=check.pageToNormalized(0,matches.first().rectangles.first().center());const auto size=check.pageSize(0);
   require(std::abs(normalized.x()*size.width()-70)<4 && std::abs((1-normalized.y())*size.height()-40)<4,"OCR text aligns on rotated pages");
  }
  WorkbenchOutput ocrJob;ocrJob.path=temp.filePath("ocr-pipeline.pdf");ocrJob.ocr=true;ocrJob.pages.append({scanned,0,0,scan});int recognized=0;
  require(WorkbenchExport::run({ocrJob},cancelled,{},[&](const QImage& image,QSizeF size,auto& words,QString&){require(!image.isNull() && !size.isEmpty(),"OCR receives rendered page");words={{"Pipeline",{20,20,120,20},0}};++recognized;return true;}).isEmpty() && recognized==1,"OCR export pipeline");
  require(reopened.open(ocrJob.path) && reopened.pageText(0).contains("Pipeline"),"pipeline saves searchable layer");
  if(app.arguments().contains("--native-ocr")){
   ocrJob.path=temp.filePath("ocr-windows.pdf");auto nativeError=WorkbenchExport::run({ocrJob},cancelled);require(nativeError.isEmpty(),qPrintable(nativeError));require(reopened.open(ocrJob.path) && reopened.pageText(0).contains("123"),"Windows OCR recognized and persisted");
  }
  Window window;window.show();app.processEvents();
  showWelcomeOnce(&window);
  auto welcome=window.findChild<QDialog*>("welcomeDialog");
  require(welcome && welcome->isVisible() && welcome->windowTitle()==(english ? "Welcome to GPDF" : "Bienvenido a GPDF"),"welcome appears for user without onboarding marker");
  require(welcome->findChild<QLabel*>()->text().contains("Ctrl + Shift + P") && welcome->findChild<QLabel*>()->text().contains("Workbench"),"welcome describes modes and shortcut");
  for(const auto& arg:app.arguments())if(arg.startsWith("--snapshot-dir=")){QDir folder(arg.mid(15));folder.mkpath(".");require(welcome->grab().save(folder.filePath("welcome.png")),"save welcome preview");}
  welcome->accept();QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);
  showWelcomeOnce(&window);require(!window.findChild<QDialog*>("welcomeDialog"),"welcome is shown only once");
  const auto helpItems=window.findChild<QMenu*>("helpMenu")->actions();
  auto howTo=window.findChild<QAction*>("howToUseGpdfAction");
  const auto howToIndex=helpItems.indexOf(howTo);
  require(howToIndex>=2 && howToIndex+1<helpItems.size() &&
          helpItems[howToIndex-2]->menu()==window.findChild<QMenu*>("languageMenu") &&
          helpItems[howToIndex-1]->isSeparator() &&
          helpItems[howToIndex+1]->objectName()=="aboutGpdfAction","how-to shares the About section after Language");
  howTo->trigger();
  welcome=window.findChild<QDialog*>("welcomeDialog");
  require(welcome && welcome->isVisible(),"Help reopens welcome after onboarding");
  welcome->accept();QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);
  auto action=window.findChild<QAction*>("workbenchAction");require(action && !action->menu() && action->text()=="Workbench","direct Workbench action");
  bool opened=false;QTimer::singleShot(0,[&]{auto dialog=window.findChild<QDialog*>("workbenchDialog");opened=dialog && dialog->windowTitle()=="Workbench";if(dialog)dialog->reject();});
#ifdef Q_OS_WIN
  if(QApplication::platformName()=="windows"){
   auto menu=GetMenu(reinterpret_cast<HWND>(window.winId()));require(menu && GetMenuItemCount(menu)==7 && !GetSubMenu(menu,5),"native Workbench is a command after Shortcuts");
   wchar_t label[64]{};GetMenuStringW(menu,5,label,64,MF_BYPOSITION);require(QString::fromWCharArray(label)=="Workbench","native Workbench label");
   SendMessageW(reinterpret_cast<HWND>(window.winId()),WM_COMMAND,GetMenuItemID(menu,5),0);
  }else
#endif
  action->trigger();require(opened,"click opens Workbench directly");
  {
   QMimeData mime;mime.setUrls({QUrl::fromLocalFile(first),QUrl::fromLocalFile(second)});
   QDragEnterEvent enter({20,20},Qt::CopyAction,&mime,Qt::LeftButton,Qt::NoModifier);QApplication::sendEvent(&window,&enter);
   auto hint=window.findChild<QLabel*>("fileDropHint");const auto normalHint=hint->text();
   QDragMoveEvent move({20,20},Qt::CopyAction,&mime,Qt::LeftButton,Qt::ControlModifier);QApplication::sendEvent(&window,&move);
   require(move.isAccepted() && hint->text()!=normalHint && hint->text().contains("Workbench"),"Ctrl updates drop hint");
   QDropEvent drop({20,20},Qt::CopyAction,&mime,Qt::LeftButton,Qt::ControlModifier);QApplication::sendEvent(&window,&drop);
   bool imported=false;QTimer check;check.setInterval(10);QObject::connect(&check,&QTimer::timeout,[&]{if(auto dialog=window.findChild<QDialog*>("workbenchDialog")){auto list=dialog->findChild<QListWidget*>("workbenchFiles");if(list && list->count()==2){imported=true;dialog->reject();}}});check.start();
   QTimer timeout;timeout.setSingleShot(true);QObject::connect(&timeout,&QTimer::timeout,[&window]{if(auto dialog=window.findChild<QDialog*>("workbenchDialog"))dialog->reject();});timeout.start(3000);
   app.processEvents();check.stop();timeout.stop();require(imported && window.findChild<QTabBar*>("documentTabs")->count()==0,"Ctrl drop imports into Workbench without opening reader tabs");
  }
  if(app.arguments().contains("--dark")){window.findChild<QAction*>("darkModeAction")->trigger();require(window.menuBar()->actions().at(5)==action && !action->menu(),"direct Workbench action in dark menu bar");}
  Workbench workbench(&window);workbench.show();
  require(window.screen()->availableGeometry().size().expandedTo(workbench.frameGeometry().size())==window.screen()->availableGeometry().size(),"Workbench fits available screen");
#ifdef Q_OS_WIN
  {
   Workbench attachments(&window);attachments.show();
   auto list=attachments.findChild<QListWidget*>("workbenchFiles");
   const struct { UINT count; FILEDESCRIPTORW files[3]; } group{3,{
    {.cFileName=L"not-a-pdf.txt"}, {.cFileName=L"Informe \u00f1.PDF"}, {.cFileName=L"Informe \u00f1.PDF"}
   }};
   QMimeData mime;
   mime.setData("application/x-qt-windows-mime;value=\"FileGroupDescriptorW\"",
                QByteArray(reinterpret_cast<const char*>(&group),sizeof(group)));
   QFile firstPdf(first),secondPdf(second);
   require(firstPdf.open(QIODevice::ReadOnly) && secondPdf.open(QIODevice::ReadOnly),"read attachment fixtures");
   mime.setData("application/x-qt-windows-mime;value=\"FileContents\";index=1",firstPdf.readAll());
   mime.setData("application/x-qt-windows-mime;value=\"FileContents\";index=2",secondPdf.readAll());
   QDragEnterEvent enter({5,5},Qt::CopyAction,&mime,Qt::LeftButton,Qt::NoModifier);
   QApplication::sendEvent(list->viewport(),&enter);
   QDropEvent drop({5,5},Qt::CopyAction,&mime,Qt::LeftButton,Qt::NoModifier);
   QApplication::sendEvent(list->viewport(),&drop);
   app.processEvents();
   require(enter.isAccepted() && drop.isAccepted() && list->count()==2,"import both virtual PDFs and skip non-PDF");
   require(list->item(0)->text().startsWith(QString::fromWCharArray(group.files[1].cFileName)) &&
           list->item(0)->text().endsWith(english ? "5 pages" : "5 páginas") &&
           list->item(1)->text().endsWith(english ? "2 pages" : "2 páginas"),"keep filename and attachment indexes");
  }
#endif
  auto viewport=workbench.findChild<QListWidget*>("workbenchFiles")->viewport();
  QMimeData urls;
  urls.setUrls({QUrl::fromLocalFile(first), QUrl::fromLocalFile(second), QUrl::fromLocalFile(temp.path())});
  QDragEnterEvent enter({5,5},Qt::CopyAction,&urls,Qt::LeftButton,Qt::NoModifier);
  QApplication::sendEvent(viewport,&enter);
  require(enter.isAccepted(),"file list accepts external PDF drag");
  QDropEvent drop({5,5},Qt::CopyAction,&urls,Qt::LeftButton,Qt::NoModifier);
  QApplication::sendEvent(viewport,&drop);
  app.processEvents();
  require(drop.isAccepted() && workbench.findChild<QListWidget*>("workbenchFiles")->count()==2,"drop imports both PDFs and skips directory");
  auto tools=workbench.findChild<QListWidget*>("workbenchTools");auto files=workbench.findChild<QListWidget*>("workbenchFiles");auto pages=workbench.findChild<QListWidget*>("workbenchPages");
  require(workbench.findChild<QComboBox*>("workbenchSplitMode")->currentIndex()==1,"split defaults to page groups");
  pages->setCurrentRow(1);
  require(files->currentRow()==1,"merge thumbnail selects matching file");
  files->setCurrentRow(0);
  require(pages->currentRow()==0,"merge file selects matching thumbnail");
  pages->clearSelection();
  files->setCurrentRow(0);
  workbench.findChild<QPushButton*>("workbenchFileDown")->click();
  require(files->item(0)->data(Qt::UserRole)==pages->item(0)->data(Qt::UserRole) &&
          pages->item(0)->text()==QFileInfo(second).fileName() && pages->currentRow()==1,
          "move down updates thumbnail order and preserves moved selection");
  workbench.findChild<QPushButton*>("workbenchFileUp")->click();
  require(pages->item(0)->text()==QFileInfo(first).fileName() &&
          files->item(0)->data(Qt::UserRole)==pages->item(0)->data(Qt::UserRole) &&
          pages->currentRow()==0,"move up updates thumbnails");
  const auto movedDocument=files->currentItem()->data(Qt::UserRole);
  require(files->model()->moveRows({},0,1,{},2),"move file row through drag model");
  require(files->currentItem()->data(Qt::UserRole)==movedDocument &&
          pages->currentItem()->data(Qt::UserRole)==movedDocument &&
          pages->currentRow()==files->currentRow() &&
          pages->item(0)->data(Qt::UserRole)==files->item(0)->data(Qt::UserRole),
          "file drag synchronizes thumbnail order and selected document");
  require(files->model()->moveRows({},1,1,{},0),"restore file order through drag model");
  pages->clearSelection();
  require(pages->toolTip().isEmpty() && pages->item(0)->toolTip().isEmpty() && files->item(0)->toolTip().isEmpty(),"Workbench list tooltips removed");
  require(tools->count()==6 && tools->item(0)->text()==(english ? "Merge PDFs" : "Unir PDFs"),"six translated tools");
  auto dest=workbench.findChild<QLineEdit*>("workbenchDestination");auto range=workbench.findChild<QLineEdit*>("workbenchRange");auto run=workbench.findChild<QPushButton*>("workbenchRun");
  auto pump=[&](int ms){QElapsedTimer t;t.start();while(t.elapsed()<ms){app.processEvents();QThread::msleep(1);}};
  QElapsedTimer previewWait; previewWait.start();
  while(previewWait.elapsed()<5000 && (pages->count()!=2 || pages->item(0)->icon().isNull() || pages->item(1)->icon().isNull())) pump(10);
  require(!pages->isHidden() && pages->count()==2 && !pages->item(0)->icon().isNull() && !pages->item(1)->icon().isNull(),"merge displays one rendered preview per input file");
  require(pages->item(0)->text()==QFileInfo(first).fileName() && pages->item(1)->text()==QFileInfo(second).fileName(),"merge previews follow input order");
  require(QDir::fromNativeSeparators(dest->text())==QStandardPaths::writableLocation(QStandardPaths::DesktopLocation),"default output folder is Desktop");
  QTimer::singleShot(0,[&]{auto dialog=qobject_cast<QFileDialog*>(QApplication::activeModalWidget());require(dialog,"folder chooser shown");dialog->setDirectory(temp.path());dialog->selectFile(temp.path());QMetaObject::invokeMethod(dialog,"accept");});
  workbench.findChild<QPushButton*>("workbenchBrowse")->click();
  {Workbench restored;require(QDir::fromNativeSeparators(restored.findChild<QLineEdit*>("workbenchDestination")->text())==temp.path(),"chosen output folder persists");}
  auto reveal=workbench.findChild<QPushButton*>("workbenchReveal");require(!reveal->isEnabled(),"reveal disabled before first successful export");
  auto execute=[&](QString path){
   if(QFileInfo(path).isDir())path=QDir(path).filePath("first.pdf");
   bool prompted=false;QTimer::singleShot(0,[&,path]{auto dialog=qobject_cast<QFileDialog*>(QApplication::activeModalWidget());require(dialog,"save dialog shown");require(dialog->directory().absolutePath()==QDir::fromNativeSeparators(dest->text()),"save dialog starts in configured folder");prompted=true;dialog->setDirectory(QFileInfo(path).absolutePath());dialog->selectFile(QFileInfo(path).fileName());QMetaObject::invokeMethod(dialog,"accept");});
   run->click();require(prompted,"save requests output name");QElapsedTimer timer;timer.start();while(!run->isEnabled() && timer.elapsed()<20000)pump(5);require(run->isEnabled(),"UI export completes");require(workbench.findChild<QLabel*>("workbenchStatus")->text().startsWith(english ? "Done." : "Listo."),qPrintable(workbench.findChild<QLabel*>("workbenchStatus")->text()));require(reveal->isEnabled() && reveal->toolTip().isEmpty(),"last saved output can be revealed");
  };
  pages->item(1)->setSelected(true);dynamic_cast<WorkbenchPageList*>(pages)->moveSelectionTo(0);
  require(files->item(0)->data(Qt::UserRole)==pages->item(0)->data(Qt::UserRole),"thumbnail reorder synchronizes file list");
  require(files->currentRow()==pages->currentRow() &&
          files->currentItem()->data(Qt::UserRole)==pages->currentItem()->data(Qt::UserRole),
          "thumbnail reorder keeps list selection on moved document");
  execute(temp.filePath("ui-merge.pdf"));require(reopened.open(temp.filePath("ui-merge.pdf")) && reopened.pageCount()==7 && reopened.pageText(0).contains("Second"),"UI merge respects thumbnail order");pages->clearSelection();pages->item(1)->setSelected(true);dynamic_cast<WorkbenchPageList*>(pages)->moveSelectionTo(0);files->setCurrentRow(0);
  tools->setCurrentRow(Workbench::Split);files->setCurrentRow(0);workbench.findChild<QComboBox*>("workbenchSplitMode")->setCurrentIndex(0);workbench.findChild<QSpinBox*>("workbenchChunk")->setValue(2);QDir().mkpath(temp.filePath("parts"));execute(temp.filePath("parts"));
  require(reopened.open(temp.filePath("parts/first_003.pdf")) && reopened.pageCount()==1,"UI split remainder");
  workbench.findChild<QComboBox*>("workbenchSplitMode")->setCurrentIndex(1);workbench.findChild<QLineEdit*>("workbenchGroups")->setText("1-2; 5,3");QDir().mkpath(temp.filePath("groups"));execute(temp.filePath("groups"));require(reopened.open(temp.filePath("groups/first_002.pdf")) && reopened.pageCount()==2 && reopened.pageText(0).contains("Page 5"),"UI split custom groups");
  tools->setCurrentRow(Workbench::Remove);range->setText("2,4");
  QMetaObject::invokeMethod(range,"editingFinished");
  range->setText("2-");
  QMetaObject::invokeMethod(range,"editingFinished");
  auto rangeStatus=workbench.findChild<QLabel*>("workbenchStatus");
  require(pages->selectedItems().isEmpty() && !pages->item(1)->font().strikeOut() &&
          !rangeStatus->text().isEmpty(),"invalid range clears stale preview");
  range->setText("2,4");
  QMetaObject::invokeMethod(range,"editingFinished");
  require(rangeStatus->text().isEmpty(),"valid range clears previous validation error");
  require(pages->item(1)->font().strikeOut() && pages->item(3)->data(Qt::UserRole+2).toBool() &&
          !pages->item(0)->data(Qt::UserRole+2).toBool(),"remove preview marks excluded pages");
  execute(temp.filePath("ui-remove.pdf"));require(reopened.open(temp.filePath("ui-remove.pdf")) && reopened.pageCount()==3 && reopened.pageText(1).contains("Page 3"),"UI remove");
  tools->setCurrentRow(Workbench::Extract);range->setText("5,1,3-4");execute(temp.filePath("ui-extract.pdf"));require(reopened.open(temp.filePath("ui-extract.pdf")) && reopened.pageCount()==4 && reopened.pageText(0).contains("Page 5"),"UI extract range order");
  tools->setCurrentRow(Workbench::Organize);pages->item(4)->setSelected(true);dynamic_cast<WorkbenchPageList*>(pages)->moveSelectionTo(0);auto last=pages->item(0);require(last->data(Qt::UserRole).toInt()==4 && pages->selectedItems().size()==1,"drag reorder preserves selection and page identity");workbench.findChild<QPushButton*>("workbenchRotate")->click();
  execute(temp.filePath("ui-organize.pdf"));require(reopened.open(temp.filePath("ui-organize.pdf")) && reopened.pageText(0).contains("Page 5") && reopened.pageSize(0).width()>reopened.pageSize(0).height(),"UI organize and rotate");
  {
   QMimeData mime;mime.setUrls({QUrl::fromLocalFile(second)});const int before=files->count();
   QDragEnterEvent enter({5,5},Qt::CopyAction,&mime,Qt::LeftButton,Qt::NoModifier);QApplication::sendEvent(pages->viewport(),&enter);
   QDropEvent drop({5,5},Qt::CopyAction,&mime,Qt::LeftButton,Qt::NoModifier);QApplication::sendEvent(pages->viewport(),&drop);app.processEvents();
   require(drop.isAccepted() && files->count()==before+1 && pages->item(0)==last && last->data(Qt::UserRole+1).toInt()==1,"drop onto page previews adds file and preserves organization edits");
  }
  tools->setCurrentRow(Workbench::Ocr);execute(temp.filePath("ui-native-text.pdf"));require(reopened.open(temp.filePath("ui-native-text.pdf")) && reopened.pageText(0).contains("Page 1"),"UI OCR preserves existing text");
  const auto nativeSnapshot=temp.filePath("current.pdf");a->setPage(0);require(a->note({40,500},"Unsaved workbench note"),"unsaved note");workbench.useSnapshot(*a,"Current PDF",first);tools->setCurrentRow(Workbench::Extract);range->setText("1");execute(nativeSnapshot);require(reopened.open(nativeSnapshot) && reopened.annotationCount()==1,"current PDF includes unsaved annotations");
  workbench.importFiles({restrictedPath});
  files->setCurrentRow(files->count()-1);
  tools->setCurrentRow(Workbench::Organize);
  pages->item(0)->setSelected(true);
  workbench.findChild<QPushButton*>("workbenchRotate")->click();
  const auto rotatedRestricted=temp.filePath("restricted-rotated.pdf");
  execute(rotatedRestricted);
  const auto originalSize=restricted->pageSize(0);
  require(reopened.open(rotatedRestricted) &&
          reopened.properties().contains({gpdfTr("Seguridad"),gpdfTr("Sin cifrado")}) &&
          reopened.pageSize(0)==originalSize.transposed(),"rotate and export without source encryption");
  require(digest(restrictedPath)==restrictedHash,"restricted source is unchanged");
  const auto encryptedPath=QString(GPDF_SOURCE_DIR)+"/tests/data/annotations-restricted.pdf";
  const int count=files->count();
  workbench.importFiles({encryptedPath});
  require(files->count()==count,"Workbench skips encrypted file requiring an opening password");
  PdfDocument encrypted;
  require(encrypted.open(encryptedPath,"read-only"),"load encrypted fixture in reader");
  workbench.useSnapshot(encrypted,"Opened in reader",encryptedPath);
  require(files->count()==count+1,"use previously opened encrypted PDF without another password request");
  {
   Workbench currentPdf;
   auto stack=currentPdf.findChild<QListWidget*>("workbenchFiles");
   auto previews=currentPdf.findChild<QListWidget*>("workbenchPages");
   auto modes=currentPdf.findChild<QListWidget*>("workbenchTools");
   auto useCurrent=currentPdf.findChild<QPushButton*>("workbenchUseCurrent");
   currentPdf.setCurrentDocument([&](Workbench& target){target.useSnapshot(*a,"Current snapshot",first);});
   useCurrent->click();
   require(stack->count()==1 && previews->count()==1 &&
           stack->currentRow()==0 && previews->currentRow()==0 &&
           previews->item(0)->text()=="Current snapshot","current PDF refreshes empty merge view");
   currentPdf.importFiles({second});
   useCurrent->click();
   require(stack->count()==3 && previews->count()==3 &&
           stack->currentRow()==2 && previews->currentRow()==2 &&
           stack->currentItem()->data(Qt::UserRole)==previews->currentItem()->data(Qt::UserRole),
           "current PDF refreshes existing merge stack and selects added snapshot");
   for(int mode=Workbench::Split;mode<=Workbench::Ocr;++mode){
    modes->setCurrentRow(mode);
    useCurrent->click();
    require(stack->currentRow()==stack->count()-1 && previews->count()==a->pageCount(),
            "current PDF refreshes pages in each operation");
   }
  }
  {
   Workbench deleting;
   deleting.importFiles({first,second,first});
   deleting.show();deleting.activateWindow();
   auto previews=deleting.findChild<QListWidget*>("workbenchPages");
   auto stack=deleting.findChild<QListWidget*>("workbenchFiles");
   previews->setFocus();app.processEvents();
   previews->setCurrentRow(0);
   previews->item(1)->setSelected(true);
   QCursor::setPos(previews->viewport()->mapToGlobal(previews->visualItemRect(previews->item(0)).center()));
   QKeyEvent removeSelection(QEvent::KeyPress,Qt::Key_Delete,Qt::NoModifier);
   QApplication::sendEvent(previews,&removeSelection);app.processEvents();
   require(stack->count()==1 && previews->count()==1,"Delete removes selected documents from stack and thumbnails");
   previews->clearSelection();
   QCursor::setPos(previews->viewport()->mapToGlobal(previews->visualItemRect(previews->item(0)).center()));
   QKeyEvent removeHovered(QEvent::KeyPress,Qt::Key_Delete,Qt::NoModifier);
   QApplication::sendEvent(previews,&removeHovered);app.processEvents();
   require(stack->count()==0 && previews->count()==0 &&
           !deleting.findChild<QPushButton*>("workbenchRun")->isEnabled(),"Delete removes hovered document and handles empty stack");
   deleting.importFiles({first});
   deleting.findChild<QListWidget*>("workbenchTools")->setCurrentRow(Workbench::Organize);
   previews->setFocus();
   previews->setCurrentRow(0);
   previews->item(1)->setSelected(true);
   QCursor::setPos(previews->viewport()->mapToGlobal(previews->visualItemRect(previews->item(0)).center()));
   QKeyEvent removePages(QEvent::KeyPress,Qt::Key_Delete,Qt::NoModifier);
   QApplication::sendEvent(previews,&removePages);app.processEvents();
   require(previews->count()==3 && stack->count()==1 &&
           previews->item(0)->data(Qt::UserRole).toInt()==2,"Delete in Organize removes selected pages while retaining document");
  }
  require(digest(first)==originalHash,"source PDF unchanged after all operations");
  for(const auto& arg:app.arguments())if(arg.startsWith("--snapshot-dir=")){
   QDir folder(arg.mid(15));folder.mkpath(".");files->setCurrentRow(0);
   for(int i=0;i<6;++i){tools->setCurrentRow(i);pump(1200);require(workbench.grab().save(folder.filePath(QString("workbench-%1-%2.png").arg(english ? "en" : "es").arg(i))),"save Workbench preview");}
  }
  std::cout<<"PASS Workbench: merge, split, remove, extract, organize, persisted OCR, permissions, cancellation, originals, direct menu and localization\n";
 }catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<std::endl;return 1;}
}



