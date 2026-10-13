#include "Window.h"
#include "UiFont.h"
#include <QTemporaryDir>
#include <QProcess>
#include <QDir>
#include <QElapsedTimer>
#include <QPrintPreviewWidget>
#include <QShowEvent>
#include <QAbstractNativeEventFilter>
#include <iostream>
#include <thread>
#include <stdexcept>

#ifdef Q_OS_WIN
class FullscreenFrameWatch : public QAbstractNativeEventFilter {
public:
    HWND hwnd=nullptr; RECT expected{}; bool collapsed=false; int observed=0;
    bool nativeEventFilter(const QByteArray&,void* message,qintptr*) override {
        auto msg=static_cast<MSG*>(message);
        if(msg->hwnd==hwnd && msg->message==WM_WINDOWPOSCHANGED) {
            RECT actual{}; if(GetWindowRect(hwnd,&actual) && IsWindowVisible(hwnd)) {
                ++observed;
                if(IsIconic(hwnd) || actual.right-actual.left<expected.right-expected.left-32 || actual.bottom-actual.top<expected.bottom-expected.top-32) collapsed=true;
            }
        }
        return false;
    }
};
#endif

int main(int argc,char** argv) {
    configureUiFontBackend();
    QApplication app(argc,argv); app.setStyle("Fusion"); app.setFont(gpdfUiFont());
    if(app.arguments().size()>=3 && app.arguments()[1]=="--instance-client") {
        SingleInstance client(app.arguments()[2]);
        return client.start(app.arguments().mid(3))==SingleInstance::Forwarded ? 0 : 2;
    }
    try {
        auto require=[](bool ok,const char* message){ if(!ok) throw std::runtime_error(message); };
        QString instanceKey="GPDF-test-"+QString::number(QCoreApplication::applicationPid());
        SingleInstance instance(instanceKey);
        require(instance.start({})==SingleInstance::Primary,"single instance becomes owner");
        QStringList delivered;
        instance.received=[&](QStringList paths){ delivered=paths; };
        QProcess client;
        QStringList expected{"C:/Documentos/Prueba con espacios y á.pdf","C:/Documentos/Segundo.pdf"};
        client.start(QCoreApplication::applicationFilePath(),QStringList{"--instance-client",instanceKey}+expected);
        QElapsedTimer ipcTimeout; ipcTimeout.start();
        while(client.state()!=QProcess::NotRunning && ipcTimeout.elapsed()<8000) { app.processEvents(); client.waitForFinished(10); }
        app.processEvents();
        require(client.state()==QProcess::NotRunning && client.exitCode()==0 && delivered==expected,"secondary process forwards Unicode document paths");
        QTemporaryDir directory;
        QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,directory.path());
        QString fixture=directory.path()+"/ui.pdf";
        QString output=argc>1 ? QString::fromLocal8Bit(argv[1]) : QString();
        if(!output.isEmpty()) QDir().mkpath(output);
        {
            PdfDocument pdf;
            require(pdf.createSample(5),"create fixture");
            require(pdf.note({60,680},"Comentario original"),"create note");
            require(pdf.freeText({60,620},"Texto original"),"create text");
            require(pdf.saveCopy(fixture),"save fixture");
            if(!output.isEmpty()) require(pdf.saveCopy(output+"/views-sample.pdf"),"save view demonstration");
        }
        { QSettings settings(QSettings::IniFormat,QSettings::UserScope,AppIdentity::settingsOrganization,AppIdentity::settingsApplication); settings.setValue("shortcuts/highlightAction","Ctrl+H"); settings.setValue("shortcuts/saveFileAction","Ctrl+S"); settings.setValue("shortcuts/saveAsFileAction","Ctrl+Shift+S"); settings.setValue("shortcuts/goPage","Ctrl+G"); settings.setValue("shortcuts/editSelectedAnnotationAction","F2"); }
        Window window;
        window.show();
        for(auto button:window.findChildren<QPushButton*>()) require(button->text()!="Aplicar a la anotación seleccionada","drawing menus omit apply-to-selected action");
        require(window.findChild<QSpinBox*>("highlightOpacity")->value()==60,"default highlight opacity is sixty percent");
        {
            QDialog nativeContainer(&window);
            nativeContainer.setAttribute(Qt::WA_DontShowOnScreen);
            QShowEvent show;
            QApplication::sendEvent(&nativeContainer,&show);
            require(nativeContainer.internalWinId()==0,"theme handling leaves invisible native-dialog containers unmaterialized");
        }
        auto searchPanelControls=window.findChild<QWidget*>("searchPanel");
        for(auto button:searchPanelControls->findChildren<QPushButton*>()) require(button->text()!="×","search panel omits close button");
        auto annotationsLayout=qobject_cast<QVBoxLayout*>(window.findChild<QWidget*>("annotationsPanel")->layout());
        require(annotationsLayout->itemAt(annotationsLayout->count()-1)->widget()==window.findChild<QPushButton*>("exportFromAnnotations"),"export annotations is the bottom panel control");
        auto iconToolbar=window.findChild<QToolBar*>("editingTools");
        for(int tool=0;tool<5;++tool) {
            auto action=window.findChild<QAction*>(tool<4 ? QString("editTool%1").arg(tool) : QString("highlightAction"));
            auto button=qobject_cast<QToolButton*>(iconToolbar->widgetForAction(action));
            require(button && button->toolButtonStyle()==Qt::ToolButtonIconOnly && !action->icon().isNull(),"editing tools show vector icons without labels");
            require(button->accessibleName()==action->text() && button->toolTip().contains(action->text()) && button->toolTip().contains(action->shortcut().toString(QKeySequence::NativeText)),"icon buttons retain accessible names and shortcut tooltips");
            if(tool>=3) require(button->menu() && button->popupMode()==QToolButton::MenuButtonPopup,"pencil and highlight retain dropdowns");
        }
        auto checkToolIcons=[&](bool dark) {
            for(const auto& name:{"editTool0","editTool1","editTool2","editTool3","highlightAction"}) {
                auto image=window.findChild<QAction*>(name)->icon().pixmap(24,24).toImage(); int readable=0;
                require(image.pixelColor(0,0).alpha()==0 && image.pixelColor(image.width()-1,0).alpha()==0,"vector icons have transparent backgrounds");
                for(int y=0;y<image.height();++y) for(int x=0;x<image.width();++x) { auto color=image.pixelColor(x,y); if(color.alpha()>100 && (dark ? color.lightness()>150 : color.lightness()<100)) ++readable; }
                require(readable>30,"vector tool icons follow theme contrast");
            }
        };
        checkToolIcons(false);
        require(window.findChild<QAction*>("saveFileAction")->shortcut()==QKeySequence("Ctrl+G") && window.findChild<QAction*>("saveAsFileAction")->shortcut()==QKeySequence("Ctrl+Shift+G") && window.findChild<QAction*>("goPage")->shortcut()==QKeySequence("Ctrl+I"),"requested shortcuts migrate saved legacy assignments without conflicts");
        auto toolbarHost=window.findChild<QWidget*>("toolbarHost");
        require(toolbarHost,"toolbar available");
        for(auto button:toolbarHost->findChildren<QToolButton*>())
            require(button->text()!="Inicio","Inicio removed from toolbar");
#ifdef Q_OS_WIN
        if(QApplication::platformName()=="windows") {
            auto native=GetMenu(reinterpret_cast<HWND>(window.winId()));
            require(native && GetMenuItemCount(native)==5,"native Archivo Ver and Editar menus attached");
            auto popup=GetSubMenu(native,0);
            require(popup && GetMenuItemCount(popup)==14,"native file menu sections available");
        }
#endif
        {
            QDialogButtonBox translated(QDialogButtonBox::Save|QDialogButtonBox::Cancel|QDialogButtonBox::RestoreDefaults);
            require(translated.button(QDialogButtonBox::Save)->text().remove('&')=="Guardar" && translated.button(QDialogButtonBox::Cancel)->text().remove('&')=="Cancelar","standard Qt buttons translated into Spanish");
            QKeySequenceEdit shortcut; require(shortcut.findChild<QLineEdit*>()->placeholderText()=="Presionar un atajo","shortcut placeholder translated into neutral Spanish");
        }
        window.loadPath(fixture);
        app.processEvents();
        auto list=window.findChild<QListWidget*>("annotationList");
        auto scope=window.findChild<QComboBox*>("annotationScope");
        require(scope && scope->currentIndex()==0,"document annotations are default");
        require(list && list->count()==2 && list->item(0)->text().contains("Página 1"),"document entries include page number");
        scope->setCurrentIndex(1);
        PageView* view=nullptr;
        for(auto widget:window.findChildren<QWidget*>()) if(auto page=dynamic_cast<PageView*>(widget)) { view=page; break; }
        auto button=[&](QString label){ for(auto b:window.findChildren<QPushButton*>()) if(b->text()==label) return b; return static_cast<QPushButton*>(nullptr); };
        auto edit=button("Editar"), remove=button("Borrar"), color=button("Color");
        require(list && view && edit && remove && color,"UI controls available");
        for(int tool=0;tool<4;++tool) {
            QKeyEvent key(QEvent::KeyPress,Qt::Key_F1+tool,Qt::NoModifier); QApplication::sendEvent(view,&key);
            require(view->tool==PageView::Tool(tool) && window.findChild<QAction*>(QString("editTool%1").arg(tool))->isChecked(),"function keys select editing tools and preserve active indication");
        }
        QKeyEvent selectTool(QEvent::KeyPress,Qt::Key_F1,Qt::NoModifier); QApplication::sendEvent(view,&selectTool);
        bool pageShortcutOpened=false;
        QTimer::singleShot(0,[&]{ if(auto input=qobject_cast<QInputDialog*>(QApplication::activeModalWidget())) { pageShortcutOpened=true; input->setIntValue(view->pdf.pageIndex()+1); input->accept(); } });
        QKeyEvent pageKey(QEvent::KeyPress,Qt::Key_I,Qt::ControlModifier); QApplication::sendEvent(view,&pageKey); require(pageShortcutOpened,"Ctrl I opens go-to-page dialog");
        auto zoomPercent=window.findChild<QSpinBox*>("zoomPercent");
        require(zoomPercent && zoomPercent->value()==125,"zoom percentage reflects initial scale");
        zoomPercent->setValue(100);
        require(view->scale==1.0,"editable percentage changes document zoom");
        QAction* zoomPlus=nullptr; QAction* zoomMinus=nullptr;
        for(auto a:window.findChildren<QAction*>()) { if(a->text()=="+") zoomPlus=a; if(a->text()=="−") zoomMinus=a; }
        require(zoomPlus && zoomMinus,"zoom buttons available");
        zoomPlus->trigger(); require(zoomPercent->value()==110 && qFuzzyCompare(view->scale,1.10),"toolbar plus advances ten percentage points");
        zoomMinus->trigger(); require(zoomPercent->value()==100 && view->scale==1.0,"toolbar minus decreases ten percentage points");
        zoomPercent->setValue(50); zoomMinus->trigger(); require(zoomPercent->value()==50,"zoom lower limit");
        zoomPercent->setValue(300); zoomPlus->trigger(); require(zoomPercent->value()==300,"zoom upper limit");
        zoomPercent->setValue(125);
        auto mode=window.findChild<QComboBox*>("viewMode");
        require(mode && mode->itemText(0)=="Vista continua","continuous view label");
        for(int index=0;index<3;++index) {
            mode->setCurrentIndex(index); mode->showPopup(); app.processEvents();
            require(mode->view()->window()->pos().y()>=mode->mapToGlobal(QPoint(0,mode->height())).y(),"view popup opens below selector");
            mode->hidePopup();
        }
        mode->setCurrentIndex(0);
        auto scroll=window.findChild<QScrollArea*>();
        auto toggle=window.findChild<QAction*>("toggleAnnotations");
        auto panel=window.findChild<QWidget*>("annotationsPanel");
        require(toggle && panel && !toggle->isChecked() && panel->isHidden(),"annotations hidden on open");
        require(window.windowTitle().contains("ui.pdf"),"title shows file name");
        toggle->trigger(); app.processEvents();
        require(!panel->isHidden(),"annotations can be shown");
        auto snapshot=[&](QString name){ for(int pass=0;pass<4;++pass) { app.processEvents(); view->workers.waitForDone(); } if(!output.isEmpty()) { require(window.grab().save(output+"/"+name+".png"),"save view screenshot"); } };
        require(mode && scroll && mode->currentIndex()==0 && view->mode==PageView::Continuous,"continuous is default");
        require(view->pageRects.size()==5 && view->pageRects[4].top()>view->pageRects[3].bottom(),"continuous includes all pages with spacing");
        auto find=window.findChild<QAction*>("focusSearch");
        auto searchPanel=window.findChild<QWidget*>("searchPanel");
        auto searchInput=window.findChild<QLineEdit*>("searchInput");
        auto results=window.findChild<QListWidget*>("searchResults");
        auto searchToggle=window.findChild<QAction*>("searchAction");
        require(find && searchPanel && searchInput && results && searchToggle,"search UI available");
        find->trigger();
        require(!searchPanel->isHidden(),"find opens left panel");
        searchInput->setText("SELECT");
        QMetaObject::invokeMethod(searchInput,"returnPressed");
        QElapsedTimer timeout; timeout.start();
        while(results->count()<5 && timeout.elapsed()<2000) app.processEvents(QEventLoop::AllEvents,20);
        require(results->count()==5,"all document pages searched case-insensitively");
        results->setCurrentRow(4);
        require(view->pdf.pageIndex()==4 && view->searchPage==4 && !view->searchRects.isEmpty(),"search result navigates and marks text");
        mode->setCurrentIndex(1); results->setCurrentRow(2);
        require(view->pdf.pageIndex()==2 && !view->pageRects[2].isEmpty() && view->searchPage==2,"result navigation in single mode");
        mode->setCurrentIndex(2); results->setCurrentRow(1);
        require(view->pdf.pageIndex()==1 && !view->pageRects[0].isEmpty() && !view->pageRects[1].isEmpty(),"result navigation in double mode");
        snapshot("search-results");
        searchToggle->setChecked(false);
        mode->setCurrentIndex(0);
        scroll->verticalScrollBar()->setValue(0); app.processEvents();
        scroll->verticalScrollBar()->setValue(650); snapshot("continuous-view");
        scroll->verticalScrollBar()->setValue(int(view->pageRects[1].top()));
        timeout.restart();
        while((view->pdf.pageIndex()!=1 || list->count()!=0) && timeout.elapsed()<2000)
            app.processEvents(QEventLoop::AllEvents,20);
        require(view->pdf.pageIndex()==1 && list->count()==0,"scroll activates page and its annotations");
        // Place a note on page two through the canvas, not through the document API.
        view->tool=PageView::Note;
        auto notePoint=view->devicePoint({70,680});
        QTimer::singleShot(0,[]{ if(auto dialog=qobject_cast<QInputDialog*>(QApplication::activeModalWidget())) { dialog->setTextValue("Nota de página dos"); dialog->accept(); } });
        QMouseEvent press(QEvent::MouseButtonPress,notePoint,notePoint,Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);
        QApplication::sendEvent(view,&press);
        require(view->pdf.pageIndex()==1 && list->count()==1 && list->item(0)->text().contains("Nota de página dos"),"continuous annotation belongs to clicked page");
        require(view->commentVisible() && view->findChild<QTextEdit*>("commentContents")->toPlainText()=="Nota de página dos","new comment opens its contents on the page");
        mode->setCurrentIndex(1);
        require(view->pageRects[0].isEmpty() && !view->pageRects[1].isEmpty() && view->pageRects[2].isEmpty(),"single mode shows only current page");
        snapshot("single-view");
        mode->setCurrentIndex(2);
        app.processEvents();
        require(!view->pageRects[0].isEmpty() && !view->pageRects[1].isEmpty() && view->pageRects[2].isEmpty(),"double mode shows current pair");
        require(view->pageRects[0].right()<view->pageRects[1].left() && view->width()<=scroll->viewport()->width(),"two pages fit side by side");
        snapshot("double-view");
        QAction* next=nullptr; QAction* previous=nullptr;
        for(auto action:window.findChildren<QAction*>()) { if(action->text()=="›") next=action; if(action->text()=="‹") previous=action; }
        require(next && previous,"navigation actions exist");
        next->trigger();
        require(view->pdf.pageIndex()==2 && !view->pageRects[2].isEmpty() && !view->pageRects[3].isEmpty(),"double navigation advances one pair");
        next->trigger();
        require(view->pdf.pageIndex()==4 && !view->pageRects[4].isEmpty(),"odd last page displayed");
        next->trigger(); require(view->pdf.pageIndex()==4,"next at end stays in range");
        previous->trigger(); previous->trigger();
        mode->setCurrentIndex(1);
        next->trigger(); require(view->pdf.pageIndex()==1,"single navigation advances one page");
        previous->trigger();
        mode->setCurrentIndex(0);
        require(view->pdf.pageIndex()==0 && list->count()==2,"switching views preserves page annotations");
        view->tool=PageView::Select;
        require(!edit->isEnabled() && !remove->isEnabled(),"actions disabled without selection");
        list->setCurrentRow(1);
        require(edit->isEnabled() && remove->isEnabled() && !color->isEnabled(),"text selection enables correct actions");
        require(view->selectedAnnotation==1,"selection marks page annotation");
        QTimer::singleShot(0,[]{ auto dialog=qobject_cast<QInputDialog*>(QApplication::activeModalWidget()); if(dialog) { dialog->setTextValue("Texto editado desde interfaz"); dialog->accept(); } });
        edit->click();
        require(list->count()==2 && list->item(1)->text().contains("Texto editado desde interfaz"),"dialog updates selected text");
        require(window.windowTitle().contains("*"),"title indicates pending changes");
        QAction* save=nullptr;
        for(auto action:window.findChildren<QAction*>()) if(action->text()=="Guardar") save=action;
        require(save,"save action exists");
        QKeyEvent saveKey(QEvent::KeyPress,Qt::Key_G,Qt::ControlModifier); QApplication::sendEvent(view,&saveKey);
        require(!view->pdf.dirty() && !window.windowTitle().contains("*"),"in-place save clears pending changes");
        QTimer::singleShot(0,[]{ if(auto dialog=qobject_cast<QInputDialog*>(QApplication::activeModalWidget())) dialog->reject(); });
        edit->click();
        require(list->count()==2 && list->item(1)->text().contains("Texto editado desde interfaz"),"cancel preserves text");
        remove->click();
        require(list->count()==1 && list->currentRow()==0 && list->item(0)->text().contains("Comentario original"),"delete selects remaining neighbor");
        remove->click();
        require(list->count()==0 && view->selectedAnnotation==-1 && !edit->isEnabled() && !remove->isEnabled(),"last deletion clears selection and actions");
        require(view->pdf.saveCopy(directory.path()+"/views.pdf"),"save annotations after view changes");
        require(view->pdf.open(directory.path()+"/views.pdf") && view->pdf.setPage(1),"reopen second page");
        require(view->pdf.annotationCount()==1 && view->pdf.annotationContents(0)=="Nota de página dos","second-page note persists after deleting first-page annotations");
        window.loadPath(fixture);
        require(results->count()==0 && searchInput->text().isEmpty(),"new document clears search results");
        require(panel->isHidden() && !toggle->isChecked(),"opening another PDF hides annotations again");
        require(view->pdf.annotationContents(1)=="Texto editado desde interfaz","in-place save persisted edited text");
        const int beforeInline=view->pdf.annotationCount();
        view->tool=PageView::Text;
        auto inlinePoint=view->devicePoint({60,400});
        QMouseEvent inlineClick(QEvent::MouseButtonPress,inlinePoint,inlinePoint,Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);
        QApplication::sendEvent(view,&inlineClick);
        auto inlineInput=view->findChild<QTextEdit*>("inlineTextInput");
        auto inlineSize=view->findChild<QSpinBox*>("inlineTextSize");
        require(inlineInput && inlineSize && !QApplication::activeModalWidget(),"text editor opens on page without modal dialog");
        require(inlineInput->placeholderText()=="Escribir aquí…","inline text placeholder omits keyboard instructions");
        require(view->textStyle.family=="Arial" && view->textStyle.color==Qt::black,"Arial and black are defaults");
        inlineInput->setPlainText("Texto directo con acentos: acción"); inlineSize->setValue(22);
        for(auto a:view->findChildren<QAction*>()) if(a->text()=="Azul") a->trigger();
        save->trigger();
        require(view->pdf.annotationCount()==beforeInline+1 && !view->pdf.dirty(),"save commits pending inline text");
        require(view->textStyle.size==22 && view->textStyle.color==QColor("#0078d4"),"inline size and color selected");
        auto savedInline=view->pdf.render(1.0);
        view->beginText({60,300}); view->cancelText();
        require(view->pdf.annotationCount()==beforeInline+1,"discard does not create annotation");
        window.loadPath(fixture);
        require(view->pdf.render(1.0)==savedInline,"inline text persists when reopened");
        view->beginText({60,300});
        auto direct=view->findChildren<QTextEdit*>("inlineTextInput");
        QTextEdit* activeInput=nullptr;
        for(auto input:direct) if(input->isVisible()) activeInput=input;
        require(activeInput && activeInput->styleSheet().contains("background: transparent"),"inline text background transparent");
        activeInput->setPlainText("Confirmado al hacer clic fuera");
        auto outside=view->devicePoint({450,500});
        QMouseEvent outsideClick(QEvent::MouseButtonPress,outside,outside,Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);
        QApplication::sendEvent(view,&outsideClick);
        require(view->pdf.annotationCount()==beforeInline+2,"outside click commits text once");
        for(auto editor:view->findChildren<QWidget*>("inlineTextEditor")) require(!editor->isVisible(),"outside click does not create another editor");
        view->beginText({60,250});
        QApplication::sendEvent(view,&outsideClick);
        require(view->pdf.annotationCount()==beforeInline+2,"empty editor discarded on outside click");
        for(auto editor:view->findChildren<QWidget*>("inlineTextEditor")) require(!editor->isVisible(),"empty editor closes without replacement");
        auto center=window.findChild<QToolBar*>("centerTools");
        auto editing=window.findChild<QToolBar*>("editingTools");
        require(center && editing && editing->actions().size()==6,"six annotation tools including underline in left group");
        require(center->widgetForAction(center->actions()[0])==mode,"view comes first in center group");
        require(center->actions()[2]->text()=="‹" && center->actions()[4]->text()=="›" && center->actions()[6]->text()=="−","pages precede zoom in center group");
        require(view->finishText(),"close pending editor");
        const int existingCount=view->pdf.annotationCount();
        auto textRect=view->pdf.annotationRect(existingCount-1);
        auto hitPoint=view->devicePoint(textRect.center());
        QMouseEvent editClick(QEvent::MouseButtonPress,hitPoint,hitPoint,Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);
        view->tool=PageView::Text;
        QApplication::sendEvent(view,&editClick);
        QTextEdit* reopened=nullptr;
        for(auto input:view->findChildren<QTextEdit*>("inlineTextInput")) if(input->isVisible()) reopened=input;
        require(reopened && reopened->toPlainText()=="Confirmado al hacer clic fuera","Text tool reopens existing annotation");
        auto box=reopened->parentWidget();
        auto family=box->findChild<QFontComboBox*>("inlineTextFont");
        require(family,"inline font chooser available"); family->setCurrentFont(QFont("Segoe UI"));
        TextResizeGrip* grip=nullptr;
        for(auto widget:box->findChildren<QWidget*>()) if(auto candidate=dynamic_cast<TextResizeGrip*>(widget)) grip=candidate;
        require(grip,"resize handle available");
        QPointF gripPosition(8,8),globalGrip=grip->mapToGlobal(QPoint(8,8));
        QMouseEvent gripPress(QEvent::MouseButtonPress,gripPosition,globalGrip,Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);
        QApplication::sendEvent(grip,&gripPress);
        QMouseEvent gripMove(QEvent::MouseMove,gripPosition+QPointF(40,30),globalGrip+QPointF(40,30),Qt::NoButton,Qt::LeftButton,Qt::NoModifier);
        QApplication::sendEvent(grip,&gripMove); app.processEvents();
        reopened->setPlainText("Texto reeditado");
        require(view->finishText() && view->pdf.annotationCount()==existingCount,"editing updates without duplicate annotation");
        require(view->pdf.annotationTextStyle(existingCount-1).family=="Segoe UI","changed font retained");
        auto editedRect=view->pdf.annotationRect(existingCount-1);
        require(editedRect.width()>textRect.width(),"expanded annotation width stored");
        hitPoint=view->devicePoint(editedRect.center());
        QMouseEvent selectClick(QEvent::MouseButtonPress,hitPoint,hitPoint,Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);
        view->tool=PageView::Select; QApplication::sendEvent(view,&selectClick);
        require(view->selectedAnnotation==existingCount-1,"Select tool selects text annotation");
        QMouseEvent doubleClick(QEvent::MouseButtonDblClick,hitPoint,hitPoint,Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);
        QApplication::sendEvent(view,&doubleClick);
        bool editorShown=false;
        for(auto editor:view->findChildren<QWidget*>("inlineTextEditor")) editorShown|=editor->isVisible();
        require(editorShown && view->tool==PageView::Select,"double click edits while preserving Select tool");
        view->cancelText();
        for(int index=0;index<3;++index) {
            mode->setCurrentIndex(index); previous->trigger(); previous->trigger(); previous->trigger();
            QKeyEvent rightKey(QEvent::KeyPress,Qt::Key_Right,Qt::NoModifier);
            QApplication::sendEvent(view,&rightKey);
            require(view->pdf.pageIndex()==(index==2 ? 2 : 1),"right arrow navigates in each view");
            QKeyEvent leftKey(QEvent::KeyPress,Qt::Key_Left,Qt::NoModifier);
            QApplication::sendEvent(view,&leftKey);
            require(view->pdf.pageIndex()==0,"left arrow navigates in each view");
        }
        require(window.findChild<QTimer*>("searchDebounce")->interval()==600,"automatic search waits 0.6 seconds");
        scope->setCurrentIndex(0);
        int noteOnSecond=-1;
        for(int row=0;row<list->count();++row) {
            auto item=list->item(row);
            require(item->text().contains(QString("Página %1").arg(item->data(Qt::UserRole).toInt()+1)),"all annotation entries show page");
            if(item->data(Qt::UserRole).toInt()==1) noteOnSecond=row;
        }
        require(noteOnSecond>=0,"global list contains second-page annotation");
        list->setCurrentRow(noteOnSecond);
        require(view->pdf.pageIndex()==1 && view->selectedAnnotation==0,"global annotation selection navigates to correct page");
        int totalAnnotations=list->count(); remove->click();
        require(view->pdf.annotationCount()==0 && list->count()==totalAnnotations-1,"global deletion affects correct page");
        view->setFocus(); auto undoAction=window.findChild<QAction*>("undoAction"); auto redoAction=window.findChild<QAction*>("redoAction");
        require(undoAction && redoAction,"history menu actions available");
        undoAction->trigger(); require(view->pdf.annotationCount()==1 && list->count()==totalAnnotations,"menu undo restores annotation");
        redoAction->trigger(); require(view->pdf.annotationCount()==0,"menu redo repeats deletion");
        mode->setCurrentIndex(1); previous->trigger();
        int movable=view->pdf.annotationCount()-1;
        auto moveRect=view->pdf.annotationRect(movable); auto moveStart=view->devicePoint(moveRect.center());
        view->tool=PageView::Select;
        QMouseEvent movePress(QEvent::MouseButtonPress,moveStart,moveStart,Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);
        QApplication::sendEvent(view,&movePress);
        QMouseEvent moveMotion(QEvent::MouseMove,moveStart+QPointF(30,20),moveStart+QPointF(30,20),Qt::NoButton,Qt::LeftButton,Qt::NoModifier);
        QApplication::sendEvent(view,&moveMotion);
        QMouseEvent moveRelease(QEvent::MouseButtonRelease,moveStart+QPointF(30,20),moveStart+QPointF(30,20),Qt::LeftButton,Qt::NoButton,Qt::NoModifier);
        QApplication::sendEvent(view,&moveRelease);
        require(view->pdf.annotationRect(movable).left()>moveRect.left(),"Select tool drags existing text");
        view->setFocus(); window.findChild<QAction*>("copyAction")->trigger();
        int countBeforePaste=view->pdf.annotationCount();
        window.findChild<QAction*>("pasteAnnotationAction")->trigger();
        require(view->pdf.annotationCount()==countBeforePaste+1,"native menu copy and paste annotation work");
        window.findChild<QAction*>("selectPageTextAction")->trigger();
        window.findChild<QAction*>("copyPlainAction")->trigger();
        require(QApplication::clipboard()->text().contains("Page 1") && !QApplication::clipboard()->text().contains('\n'),"copy page text without line breaks");
        window.findChild<QAction*>("selectAllAction")->trigger(); window.findChild<QAction*>("copyAction")->trigger();
        require(QApplication::clipboard()->text().contains("Page 5"),"select all copies document text");
        require(!window.findChild<QAction*>("menuFindAction"),"Edit menu no longer has Search");
        auto menuView=window.findChild<QMenu*>("viewMenu");
        auto menuItems=menuView->actions(); int toolbarIndex=menuItems.indexOf(window.findChild<QAction*>("toggleToolbarAction"));
        const QStringList panelNames={"Búsqueda","Anotaciones","Miniaturas","Marcadores del PDF","Marcadores Personales"};
        require(menuItems[toolbarIndex+1]->isSeparator(),"View panel section follows toolbar section");
        for(int i=0;i<panelNames.size();++i) require(menuItems[toolbarIndex+2+i]->text()==panelNames[i],"View panels are in requested order");
        require(menuItems[toolbarIndex+7]->isSeparator() && menuItems[toolbarIndex+8]->text()=="Modo oscuro" && menuItems[toolbarIndex+9]->text()=="Pantalla completa" && menuItems.size()==toolbarIndex+10,"View ends with separate dark and fullscreen section");
        window.findChild<QAction*>("searchAction")->setChecked(false); find->trigger(); require(!searchPanel->isHidden(),"View Search opens panel");
        find->trigger(); require(searchPanel->isHidden(),"Ctrl F closes open search panel");
        find->trigger(); require(!searchPanel->isHidden(),"Ctrl F reopens closed search panel");
        auto typeFilter=window.findChild<QComboBox*>("annotationTypeFilter");
        require(typeFilter && typeFilter->currentIndex()==0 && typeFilter->itemText(0)=="Predeterminado","default annotation type filter");
        const int visibleDefault=list->count();
        view->first=0; view->last=4;
        require(view->pdf.highlight(0,4) && view->pdf.ink({{60,180},{110,160},{150,180}}),"add hidden annotation types");
        scope->setCurrentIndex(1); scope->setCurrentIndex(0);
        require(list->count()==visibleDefault,"default filter hides highlight and ink");
        typeFilter->setCurrentIndex(1); require(list->count()==visibleDefault+2,"all annotations includes highlights and ink");
        typeFilter->setCurrentIndex(4); require(list->count()==1 && list->item(0)->text().contains("Lápiz"),"ink-only filter");
        typeFilter->setCurrentIndex(5); require(list->count()==1 && list->item(0)->text().contains("Resaltado"),"highlight-only filter");
        typeFilter->setCurrentIndex(3);
        for(int row=0;row<list->count();++row) require(list->item(row)->text().contains("Texto"),"text-only filter");
        typeFilter->setCurrentIndex(2);
        for(int row=0;row<list->count();++row) require(list->item(row)->text().contains("Comentario"),"comments-only filter");
        typeFilter->setCurrentIndex(0);
        searchToggle->setChecked(false); mode->setCurrentIndex(1); app.processEvents();
        auto fitWidth=window.findChild<QAction*>("fitMode1");
        fitWidth->trigger(); app.processEvents();
        require(view->fit==PageView::Width && view->width()<=scroll->viewport()->width()+2,"fit width adapts document to viewport");
        require(zoomPercent->value()==int(std::lround(view->effectiveScale*100)),"fit updates displayed percentage");
        window.findChild<QAction*>("fitMode2")->trigger(); app.processEvents();
        require(view->fit==PageView::Page && view->height()<=scroll->viewport()->height()+2,"fit page fits document height");
        require(!window.findChild<QAction*>("fitMode3") && !window.findChild<QAction*>("drawingFormatAction"),"removed View menu options");
        for(auto menu:window.findChildren<QMenu*>()) if(menu->title().contains("Editar")) for(auto item:menu->actions()) require(item->objectName()!="editTool4" && item->objectName()!="secondaryHighlightAction","removed Edit menu options");
        window.findChild<QAction*>("viewActualSize")->trigger();
        require(view->fit==PageView::Manual && view->scale==1.0 && zoomPercent->value()==100,"actual size restores 100 percent");
        window.findChild<QAction*>("viewZoomIn")->trigger(); require(view->scale==1.25,"View menu zoom in");
        window.findChild<QAction*>("viewZoomOut")->trigger(); require(view->scale==1.0,"View menu zoom out");
        for(int rotation=0;rotation<4;++rotation) {
            window.findChild<QAction*>(QString("viewRotation%1").arg(rotation))->trigger();
            require(view->rotation==rotation,"orientation menu rotates viewer");
            QPointF original(150,400),mapped=view->pdfPoint(view->devicePoint(original));
            require(QLineF(original,mapped).length()<0.001,"rotation preserves text and annotation coordinates");
            require(view->pageRects[0].width()==(rotation%2 ? 842 : 595),"rotation updates page dimensions");
        }
        window.findChild<QAction*>("viewRotation0")->trigger();
        window.findChild<QAction*>("menuPageMode2")->trigger(); require(view->mode==PageView::Double,"view mode submenu controls toolbar");
        window.findChild<QAction*>("menuPageMode0")->trigger(); require(view->mode==PageView::Continuous,"continuous mode from View menu");
        snapshot("toolbar-inline");
        auto toolbar=window.findChild<QToolBar*>("mainToolbar");
        auto toolbarAction=window.findChild<QAction*>("toggleToolbarAction");
        require(toolbar && toolbarAction && toolbar->contextMenuPolicy()==Qt::PreventContextMenu,"toolbar context menu disabled");
        toolbarAction->trigger(); require(toolbar->isHidden(),"View menu hides toolbar");
        toolbarAction->trigger(); require(!toolbar->isHidden(),"View menu restores toolbar");
        find->trigger(); searchInput->setText("SELECT"); QMetaObject::invokeMethod(searchInput,"returnPressed");
        timeout.restart(); while(results->count()<5 && timeout.elapsed()<2000) app.processEvents(QEventLoop::AllEvents,20);
        require(results->count()==5,"Enter starts a new search immediately");
        require(results->item(0)->data(Qt::UserRole).toString().contains("<b>Página 1</b>") && results->item(0)->data(Qt::UserRole).toString().count("<b>")==2,"search page and matched passage bold");
        results->setCurrentRow(1); QMetaObject::invokeMethod(searchInput,"returnPressed");
        require(view->pdf.pageIndex()==2 && results->currentRow()==2,"Enter advances existing search");
        results->setCurrentRow(4); QMetaObject::invokeMethod(searchInput,"returnPressed");
        require(view->pdf.pageIndex()==0 && results->currentRow()==0,"Enter wraps after last result");
        results->setCurrentRow(1); next->trigger(); QMetaObject::invokeMethod(searchInput,"returnPressed");
        require(view->pdf.pageIndex()==2 && results->currentRow()==2,"Enter follows current document location");
        bool previewRendered=false;
        QTimer::singleShot(100,[&]{
            for(auto widget:QApplication::topLevelWidgets()) if(auto dialog=qobject_cast<QPrintPreviewDialog*>(widget)) {
                auto preview=dialog->findChild<QPrintPreviewWidget*>(); previewRendered=preview && preview->pageCount()==5; dialog->reject();
            }
        });
        window.findChild<QAction*>("printPreviewAction")->trigger();
        require(previewRendered,"print preview renders every page");
        save->trigger();
        auto recent=window.findChild<QAction*>("recentFile0");
        require(recent && recent->isVisible() && recent->isEnabled(),"opened file available in recent menu");
        window.findChild<QAction*>("closeDocumentAction")->trigger();
        require(!view->pdf.loaded() && results->count()==0 && window.windowTitle()=="GPDF","close document restores start screen");
        require(!window.findChild<QAction*>("printAction")->isEnabled(),"print disabled without a document");
        recent->trigger(); require(view->pdf.loaded() && window.windowTitle().contains("ui.pdf"),"recent menu reopens saved document");
        require(results->item(0)==nullptr,"new document clears results");
        auto tabs=window.findChild<QTabBar*>("documentTabs");
        auto tabsToolbar=window.findChild<QToolBar*>("documentToolbar");
        require(tabs && tabs->count()==1 && tabsToolbar->isHidden(),"one document hides tabs");
        QString second=directory.path()+"/second.pdf";
        { PdfDocument document; require(document.createSample(2),"second fixture"); require(document.saveCopy(second),"save second fixture"); }
        view->pdf.setPage(2); view->refresh();
        require(view->pdf.note({100,650},"Cambio sin guardar"),"first document pending change");
        window.openDocument(second); app.processEvents();
        require(tabs->count()==2 && !tabsToolbar->isHidden() && view->pdf.pageCount()==2,"two documents show tabs");
        require(tabsToolbar->geometry().bottom()<=toolbar->geometry().top(),"tabs appear above tools");
        snapshot("document-tabs");
        window.findChild<QAction*>("toggleAnnotations")->setChecked(true); app.processEvents();
        for(auto name:{"annotationScope","annotationTypeFilter"}) {
            auto combo=window.findChild<QComboBox*>(name);
            combo->showPopup(); app.processEvents();
            require(combo->view()->window()->pos().y()>=combo->mapToGlobal(QPoint(0,combo->height())).y(),"annotation popup opens downwards");
            combo->hidePopup();
        }

        require(view->pdf.freeText({100,650},"Segundo documento"),"second independent change");
        tabs->setCurrentIndex(0); app.processEvents();
        require(view->pdf.pageCount()==5 && view->pdf.pageIndex()==2 && view->pdf.dirty(),"switch restores first page and changes");
        require(view->pdf.annotationContents(view->pdf.annotationCount()-1)=="Cambio sin guardar","first annotations preserved");
        window.openDocument(second); require(tabs->count()==2 && tabs->currentIndex()==1,"opening existing document activates tab");
        require(view->pdf.annotationContents(0)=="Segundo documento" && view->pdf.canUndo(),"second annotations and history preserved");
        QKeyEvent cycle(QEvent::KeyPress,Qt::Key_Tab,Qt::ControlModifier); QApplication::sendEvent(searchInput,&cycle);
        require(tabs->currentIndex()==0,"Ctrl Tab cycles documents from search input");
        QApplication::sendEvent(view,&cycle); require(tabs->currentIndex()==1,"Ctrl Tab wraps documents");
        QKeyEvent backwards(QEvent::KeyPress,Qt::Key_Backtab,Qt::ControlModifier|Qt::ShiftModifier); QApplication::sendEvent(view,&backwards);
        require(tabs->currentIndex()==0,"Ctrl Shift Tab goes backwards");
        auto autoTimer=window.findChild<QTimer*>("autoScrollTimer");
        auto scrollArea=window.findChild<QScrollArea*>();
        QPoint global=scrollArea->viewport()->mapToGlobal(scrollArea->viewport()->rect().center()),local=view->mapFromGlobal(global);
        QMouseEvent middle(QEvent::MouseButtonPress,local,global,Qt::MiddleButton,Qt::MiddleButton,Qt::NoModifier);
        QApplication::sendEvent(view,&middle);
        require(autoTimer->isActive() && QApplication::overrideCursor(),"middle click starts scrolling cursor");
        auto anchor=window.findChild<QLabel*>("autoScrollAnchor");
        require(!anchor,"autoscroll does not create a fixed indicator on the page");
#ifdef Q_OS_WIN
        require(NativeScrollCursor::handle(NativeScrollCursor::Neutral) && NativeScrollCursor::handle(NativeScrollCursor::Up) && NativeScrollCursor::handle(NativeScrollCursor::Down),"Windows supplies all three native vertical scroll cursors");
        require(QApplication::overrideCursor()->pixmap().cacheKey()==NativeScrollCursor::cursor(NativeScrollCursor::Neutral).pixmap().cacheKey(),"autoscroll starts with native neutral arrows");
        if(!output.isEmpty()) {
            NativeScrollCursor::cursor(NativeScrollCursor::Neutral).pixmap().save(output+"/native-scroll-neutral.png");
            NativeScrollCursor::cursor(NativeScrollCursor::Up).pixmap().save(output+"/native-scroll-up.png");
            NativeScrollCursor::cursor(NativeScrollCursor::Down).pixmap().save(output+"/native-scroll-down.png");
        }
#endif
        QCursor::setPos(global+QPoint(0,150));
        int before=scrollArea->verticalScrollBar()->value(); QMetaObject::invokeMethod(autoTimer,"timeout");
        require(scrollArea->verticalScrollBar()->value()>before,"cursor below anchor scrolls down");
#ifdef Q_OS_WIN
        require(QApplication::overrideCursor()->pixmap().cacheKey()==NativeScrollCursor::cursor(NativeScrollCursor::Down).pixmap().cacheKey(),"downward scrolling uses Windows down cursor");
        if(QApplication::platformName()=="windows") require(GetCursor()==NativeScrollCursor::handle(NativeScrollCursor::Down),"native Windows cursor handle is installed for downward scrolling");
#endif
        QCursor::setPos(global-QPoint(0,150)); QMetaObject::invokeMethod(autoTimer,"timeout");
        require(scrollArea->verticalScrollBar()->value()==before,"cursor above anchor scrolls up");
#ifdef Q_OS_WIN
        require(QApplication::overrideCursor()->pixmap().cacheKey()==NativeScrollCursor::cursor(NativeScrollCursor::Up).pixmap().cacheKey(),"upward scrolling uses Windows up cursor");
#endif
        QCursor::setPos(global+QPoint(0,300)); QMetaObject::invokeMethod(autoTimer,"timeout");
        require(scrollArea->verticalScrollBar()->value()-before>50,"autoscroll accelerates at greater distance");

        QKeyEvent escapeScroll(QEvent::KeyPress,Qt::Key_Escape,Qt::NoModifier); QApplication::sendEvent(view,&escapeScroll);
        require(!autoTimer->isActive() && !QApplication::overrideCursor(),"Escape restores cursor");
        QApplication::sendEvent(view,&middle); QApplication::sendEvent(view,&middle);
        require(!autoTimer->isActive(),"another middle click stops scrolling");
        QMouseEvent middleDouble(QEvent::MouseButtonDblClick,local,global,Qt::MiddleButton,Qt::MiddleButton,Qt::NoModifier);
        QApplication::sendEvent(view,&middleDouble); require(autoTimer->isActive(),"rapid middle click restarts scrolling");
        QApplication::sendEvent(view,&middleDouble); require(!autoTimer->isActive() && !QApplication::overrideCursor(),"rapid middle click stops scrolling cleanly");
        window.findChild<QAction*>("toggleAnnotations")->setChecked(true); app.processEvents();
        QLabel* filterLabel=nullptr; for(auto label:window.findChildren<QLabel*>()) if(label->text()=="Filtro:") filterLabel=label;
        require(filterLabel && std::abs(filterLabel->geometry().center().y()-typeFilter->geometry().center().y())<3,"filter label and dropdown share row");
        tabs->setCurrentIndex(1);
        window.findChild<QAction*>("editTool0")->trigger();
        view->setFocus();
        QPointF textPoint=view->devicePoint(view->pdf.annotationRect(0).center());
        QMouseEvent choose(QEvent::MouseButtonPress,textPoint,view->mapToGlobal(textPoint.toPoint()),Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);
        QApplication::sendEvent(view,&choose);
        QKeyEvent removeText(QEvent::KeyPress,Qt::Key_Delete,Qt::NoModifier); QApplication::sendEvent(view,&removeText);
        require(view->pdf.annotationCount()==0,"Delete removes selected text box");
        window.findChild<QAction*>("undoAction")->trigger(); require(view->pdf.annotationCount()==1,"delete can be undone");
        for(int kind=0;kind<3;++kind) {
            int beforeDelete=view->pdf.annotationCount();
            bool created=kind==0 ? view->pdf.note({350,500},"Comentario para borrar") : kind==1 ? view->pdf.ink({{300,400},{400,400}}) : view->pdf.highlight(0,4);
            require(created,"create annotation for selection deletion"); view->refresh();
            QPointF target=view->devicePoint(view->pdf.annotationRect(beforeDelete).center());
            QMouseEvent click(QEvent::MouseButtonPress,target,view->mapToGlobal(target.toPoint()),Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);
            QApplication::sendEvent(view,&click); require(view->selectedAnnotation==beforeDelete,"Select detects comment ink and highlight");
            QApplication::sendEvent(view,&removeText); require(view->pdf.annotationCount()==beforeDelete,"Delete removes selected annotation of each type");
        }
        for(auto tool:{PageView::Note,PageView::Pen,PageView::Text}) {
            view->tool=tool; QPointF target=view->devicePoint({300,500});
            QMouseEvent hover(QEvent::MouseMove,target,view->mapToGlobal(target.toPoint()),Qt::NoButton,Qt::NoButton,Qt::NoModifier); QApplication::sendEvent(view,&hover);
            require(view->cursor().shape()==(tool==PageView::Text ? Qt::IBeamCursor : Qt::CrossCursor),"tool cursor matches precision mode");
        }
        view->tool=PageView::Select;
        int beforeEnter=view->pdf.annotationCount(); view->beginText({100,500});
        auto enterInput=view->findChild<QTextEdit*>("inlineTextInput");
        for(auto input:view->findChildren<QTextEdit*>("inlineTextInput")) if(input->isVisible()) enterInput=input;
        enterInput->setPlainText("Primera"); enterInput->moveCursor(QTextCursor::End);
        QKeyEvent newline(QEvent::KeyPress,Qt::Key_Return,Qt::ShiftModifier); QApplication::sendEvent(enterInput,&newline);
        require(enterInput->toPlainText()=="Primera\n" && view->pdf.annotationCount()==beforeEnter,"Shift Enter adds newline without committing");
        enterInput->insertPlainText("Segunda"); QKeyEvent enter(QEvent::KeyPress,Qt::Key_Return,Qt::NoModifier); QApplication::sendEvent(enterInput,&enter);
        require(view->pdf.annotationCount()==beforeEnter+1 && view->pdf.annotationContents(beforeEnter)=="Primera\nSegunda","Enter commits inline text with newline");
        window.findChild<QAction*>("undoAction")->trigger();
        bool commentAccepted=false;
        QTimer::singleShot(0,[&]{ auto dialog=qobject_cast<QInputDialog*>(QApplication::activeModalWidget()); if(!dialog) return; auto input=dialog->findChild<QPlainTextEdit*>(); if(!input) { dialog->reject(); return; } input->setPlainText("Comentario"); input->moveCursor(QTextCursor::End); QApplication::sendEvent(input,&newline); input->insertPlainText("Segunda"); QApplication::sendEvent(input,&enter); if(dialog->isVisible()) dialog->reject(); });
        auto commentValue=studyMultilineText(&window,"Comentario de prueba",{},&commentAccepted);
        require(commentAccepted && commentValue=="Comentario\nSegunda","comments use Enter to accept and Shift Enter for newline");
        require(window.findChild<QAction*>("selectAllAction")->shortcut()==QKeySequence("Ctrl+Shift+A"),"select all shortcut changed");
        QKeyEvent annotationsKey(QEvent::KeyPress,Qt::Key_A,Qt::ControlModifier); bool visible=window.findChild<QAction*>("toggleAnnotations")->isChecked(); QApplication::sendEvent(view,&annotationsKey);
        require(window.findChild<QAction*>("toggleAnnotations")->isChecked()!=visible,"Ctrl A toggles annotations");
        window.findChild<QAction*>("goFirst")->trigger();
        window.findChild<QAction*>("goLast")->trigger(); require(view->pdf.pageIndex()==1,"go to document end");
        window.findChild<QAction*>("goBack")->trigger(); require(view->pdf.pageIndex()==0,"navigation back");
        window.findChild<QAction*>("goForward")->trigger(); require(view->pdf.pageIndex()==1,"navigation forward");
        bool propertiesShown=false;
        QTimer::singleShot(0,[&]{ for(auto widget:QApplication::topLevelWidgets()) if(widget->objectName()=="propertiesDialog") { auto dialog=qobject_cast<QDialog*>(widget); propertiesShown=dialog->findChildren<QLineEdit*>().size()>=15; if(!output.isEmpty()) dialog->grab().save(output+"/light-properties.png"); dialog->accept(); } });
        window.findChild<QAction*>("propertiesAction")->trigger(); require(propertiesShown,"file properties show metadata and file details");
        bool shortcutsShown=false;
        QTimer::singleShot(0,[&]{ for(auto widget:QApplication::topLevelWidgets()) if(widget->objectName()=="shortcutsDialog") {
            auto dialog=qobject_cast<QDialog*>(widget); auto table=dialog->findChild<QTableWidget*>("shortcutsTable"); shortcutsShown=table && table->rowCount()>30;
            int f2Count=0; QHash<QString,QString> assignedShortcuts;
            for(int row=0;row<table->rowCount();++row) {
                require(!table->item(row,0)->text().trimmed().isEmpty(),"every shortcut has a visible function name");
                auto key=qobject_cast<QKeySequenceEdit*>(table->cellWidget(row,1))->keySequence();
                if(!key.isEmpty()) {
                    auto sequence=key.toString(QKeySequence::PortableText); auto name=table->item(row,0)->text();
                    if(assignedShortcuts.contains(sequence)) require(sequence=="Return" && ((name.startsWith("Confirmar texto") && assignedShortcuts[sequence].startsWith("Buscar /")) || (name.startsWith("Buscar /") && assignedShortcuts[sequence].startsWith("Confirmar texto"))),"shortcut assignments have no overlapping functions");
                    assignedShortcuts.insert(sequence,name);
                }
                if(key==QKeySequence("F2")) { ++f2Count; require(table->item(row,0)->text()=="Comentario","F2 belongs only to Comentario"); }
            }
            require(f2Count==1 && window.findChild<QAction*>("editSelectedAnnotationAction")->shortcut().isEmpty(),"legacy invisible F2 does not conflict");
            for(int row=0;row<table->rowCount();++row) if(table->item(row,0)->text()=="Final del documento") qobject_cast<QKeySequenceEdit*>(table->cellWidget(row,1))->setKeySequence(QKeySequence("Alt+End"));
            dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Save)->click();
        } });
        window.findChild<QAction*>("configureShortcutsAction")->trigger();
        require(shortcutsShown && window.findChild<QAction*>("goLast")->shortcut()==QKeySequence("Alt+End"),"shortcut dialog applies customization");
        { Window restored; require(restored.findChild<QAction*>("goLast")->shortcut()==QKeySequence("Alt+End"),"custom shortcuts persist across windows"); }
        window.findChild<QAction*>("goFirst")->trigger(); QKeyEvent customLast(QEvent::KeyPress,Qt::Key_End,Qt::AltModifier); QApplication::sendEvent(view,&customLast);
        require(view->pdf.pageIndex()==1,"customized navigation shortcut works");
        QFile originalFile(second); require(originalFile.open(QIODevice::ReadOnly),"read original before recovery"); auto originalBytes=originalFile.readAll(); originalFile.close();
        window.findChild<QComboBox*>("viewMode")->setCurrentIndex(1); window.findChild<QSpinBox*>("zoomPercent")->setValue(150);
        view->beginText({100,600}); window.findChild<QTextEdit*>("inlineTextInput")->setPlainText("Borrador en progreso");
        require(window.saveSession(),"checkpoint document session");
        require(originalFile.open(QIODevice::ReadOnly) && originalFile.readAll()==originalBytes,"recovery never overwrites original"); originalFile.close();
        {
            Window recovered;
            QTimer::singleShot(0,[&]{ for(auto widget:QApplication::topLevelWidgets()) if(auto box=qobject_cast<QMessageBox*>(widget)) box->button(QMessageBox::Yes)->click(); });
            recovered.restoreSession();
            auto recoveredTabs=recovered.findChild<QTabBar*>("documentTabs"); PageView* recoveredView=nullptr;
            for(auto widget:recovered.findChildren<QWidget*>()) if(auto page=dynamic_cast<PageView*>(widget)) recoveredView=page;
            require(recoveredTabs->count()==2 && recoveredTabs->currentIndex()==1,"session restores documents and active tab");
            require(recoveredView && recoveredView->pdf.dirty() && !recoveredView->pdf.documentAnnotations().isEmpty() && recoveredView->pdf.documentAnnotations().first().text=="Segundo documento","crash recovery restores unsaved annotations");
            require(recoveredView->pdf.pageIndex()==1,"session restores current page");
            require(recoveredView->mode==PageView::Single && recoveredView->scale==1.5,"session restores view and zoom");
            auto draft=recovered.findChild<QTextEdit*>("inlineTextInput"); require(draft && draft->toPlainText()=="Borrador en progreso","recovery restores uncommitted inline text");
        }
        view->cancelText();
        save->trigger();
        window.findChild<QAction*>("closeDocumentAction")->trigger(); app.processEvents();
        require(tabs->count()==1 && tabsToolbar->isHidden() && view->pdf.pageCount()==5,"close tab returns remaining document");
        save->trigger();
        window.findChild<QAction*>("closeDocumentAction")->trigger();
        require(tabs->count()==0 && !view->pdf.loaded(),"close last tab returns start");
        QString large=directory.path()+"/large.pdf";
        { PdfDocument engine; require(engine.createSample(100),"large document fixture"); engine.setPage(99); require(engine.note({60,650},"Última página"),"late annotation fixture"); require(engine.saveCopy(large),"large fixture saved"); }
        window.openDocument(large); window.findChild<QAction*>("toggleAnnotations")->setChecked(true); app.processEvents();
        int heartbeats=0; qint64 longestHeartbeatGap=0; QElapsedTimer heartbeatGap; heartbeatGap.start();
        QTimer heartbeat; heartbeat.setInterval(0); QObject::connect(&heartbeat,&QTimer::timeout,[&]{ longestHeartbeatGap=std::max(longestHeartbeatGap,heartbeatGap.restart()); ++heartbeats; }); heartbeat.start();
        searchInput->setText("select"); QMetaObject::invokeMethod(searchInput,"returnPressed");
        QElapsedTimer largeTimeout; largeTimeout.start(); while(results->count()<100 && largeTimeout.elapsed()<8000) app.processEvents(QEventLoop::AllEvents,20);
        heartbeat.stop(); require(results->count()==100 && heartbeats>0 && longestHeartbeatGap<150,"large document search keeps event loop responsive");
        require(window.findChild<QListWidget*>("annotationList")->count()==1,"large document annotations indexed in background");
        searchInput->setText("Page"); QMetaObject::invokeMethod(searchInput,"returnPressed"); window.openDocument(second); app.processEvents();
        require(results->count()==0 && view->pdf.pageCount()==2,"switching document cancels stale search jobs");
        QString bookmarksFile=QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("../tests/data/bookmarks.pdf");
        window.openDocument(bookmarksFile); window.findChild<QAction*>("showBookmarksAction")->trigger(); app.processEvents();
        auto bookmarkTree=window.findChild<QTreeWidget*>("documentBookmarks");
        require(bookmarkTree->topLevelItemCount()==2 && bookmarkTree->topLevelItem(0)->childCount()==1,"native PDF bookmark hierarchy");
        bookmarkTree->itemClicked(bookmarkTree->topLevelItem(0)->child(0),0); require(view->pdf.pageIndex()==1,"bookmark navigates to target page");
        window.findChild<QAction*>("showThumbnailsAction")->trigger(); app.processEvents();
        auto thumbs=window.findChild<QListWidget*>("thumbnails"); QElapsedTimer thumbsWait; thumbsWait.start(); while(thumbs->item(0)->icon().isNull() && thumbsWait.elapsed()<3000) app.processEvents();
        require(thumbs->count()==2 && !thumbs->item(0)->icon().isNull(),"thumbnails render asynchronously");
        thumbs->itemClicked(thumbs->item(0)); require(view->pdf.pageIndex()==0,"thumbnail navigates to page"); snapshot("study-navigation");
        auto primaryHighlight=window.findChild<QAction*>("highlightAction"); auto secondaryHighlight=window.findChild<QAction*>("secondaryHighlightAction");
        require(primaryHighlight->shortcut()==QKeySequence("Ctrl+S") && secondaryHighlight->shortcut()==QKeySequence("Ctrl+Shift+S"),"two highlight shortcuts");
        require(view->highlightColor.alpha()==153 && view->secondaryHighlightColor.alpha()==153,"sixty percent highlight opacity");
        require(view->secondaryHighlightColor.rgb()==QColor("#e88810").rgb(),"secondary defaults orange");
        view->first=0; view->last=5; int beforePrimary=view->pdf.annotationCount(); view->setFocus();
        QKeyEvent primaryKey(QEvent::KeyPress,Qt::Key_S,Qt::ControlModifier); QApplication::sendEvent(view,&primaryKey);
        require(view->pdf.annotationCount()==beforePrimary+1,"Ctrl S keyboard event highlights selection after migration");
        window.findChild<QAction*>("undoAction")->trigger(); snapshot("before-wheel-zoom");
        view->first=0; view->last=5; int beforeHighlights=view->pdf.annotationCount(); QKeyEvent secondaryKey(QEvent::KeyPress,Qt::Key_S,Qt::ControlModifier|Qt::ShiftModifier); QApplication::sendEvent(view,&secondaryKey);
        require(view->pdf.annotationCount()==beforeHighlights+1,"secondary shortcut creates highlight");
        require(view->pdf.annotationColor(beforeHighlights).rgb()==view->secondaryHighlightColor.rgb(),"secondary highlight color");
        window.findChild<QAction*>("undoAction")->trigger();
        snapshot("zoom-preview-ready");
        // Await an actual baseline image, rather than a fixed number of event-loop turns.
        auto pageDarkPixels=[&]{ auto image=view->grab(view->activeRect().toRect()).toImage(); int count=0; for(int y=0;y<image.height();++y) for(int x=0;x<image.width();++x) if(image.pixelColor(x,y).lightness()<100) ++count; return count; };
        QElapsedTimer baselineWait; baselineWait.start();
        while(pageDarkPixels()<=100 && baselineWait.elapsed()<3000) { app.processEvents(); QThread::msleep(1); }
        require(pageDarkPixels()>100,"zoom baseline rendering ready");
        std::unique_lock previewGuard(PdfDocument::apiMutex());
        zoomPercent->setValue(100); double beforeWheel=view->scale;
        QWheelEvent halfWheel(QPointF(20,20),QPointF(20,20),QPoint(),QPoint(0,60),Qt::NoButton,Qt::ControlModifier,Qt::NoScrollPhase,false);
        QApplication::sendEvent(view,&halfWheel); require(view->scale==beforeWheel,"fractional wheel event waits for a full wheel step");
        QApplication::sendEvent(view,&halfWheel); require(qFuzzyCompare(view->scale,beforeWheel+0.25),"fractional wheel events accumulate into twenty-five-percent steps");
        beforeWheel=view->scale;
        QWheelEvent wheel(QPointF(20,20),QPointF(20,20),QPoint(),QPoint(0,120),Qt::NoButton,Qt::ControlModifier,Qt::NoScrollPhase,false);
        QApplication::sendEvent(view,&wheel); require(qFuzzyCompare(view->scale,beforeWheel+0.25),"control wheel zooms exactly twenty-five percent");
        auto zoomPreview=view->grab(view->activeRect().toRect()).toImage(); int darkPixels=0;
        for(int y=0;y<zoomPreview.height();++y) for(int x=0;x<zoomPreview.width();++x) if(zoomPreview.pixelColor(x,y).lightness()<100) ++darkPixels;
        require(darkPixels>100,"zoom retains previous page rendering while replacement is pending");
        previewGuard.unlock();
        auto unmodified=view->pdf.render(1); auto dark=window.findChild<QAction*>("darkModeAction"); dark->trigger(); require(view->darkMode && view->pdf.render(1)==unmodified,"dark mode only changes display"); snapshot("study-dark");
        require(view->grab(QRect(view->activeRect().topLeft().toPoint()+QPoint(20,20),QSize(1,1))).toImage().pixelColor(0,0)==Qt::white,"dark pages stay white");
        checkToolIcons(true);
        auto darkSearch=window.findChild<QWidget*>("searchPanel"); auto annotationPanel=list->parentWidget();
        bool searchWasVisible=!darkSearch->isHidden(),annotationsWereVisible=!annotationPanel->isHidden();
        darkSearch->show(); annotationPanel->show(); app.processEvents();
        for(auto widget:{static_cast<QWidget*>(list),static_cast<QWidget*>(window.findChild<QListWidget*>("searchResults")),static_cast<QWidget*>(searchInput),static_cast<QWidget*>(typeFilter)})
            require(widget->palette().color(QPalette::Text).lightness()>180,"dark panel text remains light");
        require(searchInput->palette().color(QPalette::PlaceholderText).lightness()>140,"dark placeholders remain readable");
        bool darkBookmarkDialog=false;
        QTimer::singleShot(0,[&]{ if(auto dialog=qobject_cast<QInputDialog*>(QApplication::activeModalWidget())) { darkBookmarkDialog=dialog->palette().color(QPalette::Window).lightness()<100; if(!output.isEmpty()) dialog->grab().save(output+"/dark-bookmark-dialog.png"); dialog->reject(); } });
        window.findChild<QAction*>("addPersonalBookmarkAction")->trigger(); require(darkBookmarkDialog,"bookmark dialog uses dark background");
        bool printReentryBlocked=false;
        QTimer::singleShot(0,[&]{
            auto preview=qobject_cast<QPrintPreviewDialog*>(QApplication::activeModalWidget());
            if(!preview) return;
            window.findChild<QAction*>("printAction")->trigger();
            printReentryBlocked=QApplication::activeModalWidget()==preview;
            preview->reject();
        });
        window.findChild<QAction*>("printPreviewAction")->trigger();
        require(printReentryBlocked,"printing cannot open a second dialog while preview is active");
        for(const auto& name:{"propertiesAction","configureShortcutsAction","goPage","saveAsFileAction","exportAnnotationsAction","printPreviewAction"}) {
            bool themed=false;
            QTimer::singleShot(0,[&]{ if(auto dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget())) { themed=dialog->palette().color(QPalette::Window).lightness()<100; if(!output.isEmpty()) dialog->grab().save(output+"/dark-"+QString(name)+".png"); dialog->reject(); } });
            window.findChild<QAction*>(name)->trigger(); require(themed,"application dialog follows night theme");
        }
        // Native Print dialog dismissal test temporarily omitted at the user's request.
        for(const auto& name:{"openFileAction"}) {
#ifdef Q_OS_WIN
            if(QApplication::platformName()=="windows") {
                std::atomic<bool> nativeSeen=false,finished=false;
                std::cerr<<"Native dialog test: "<<name<<std::endl;
                std::thread dismiss([&]{ auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(12); while(!finished) {
                    if(std::chrono::steady_clock::now()>deadline) { std::cerr<<"Native dialog test timed out; seen="<<nativeSeen<<std::endl; std::_Exit(1); }
                    EnumWindows([](HWND hwnd,LPARAM data)->BOOL {
                        DWORD process=0; GetWindowThreadProcessId(hwnd,&process); if(process!=GetCurrentProcessId()) return TRUE;
                        wchar_t kind[128]{}; GetClassNameW(hwnd,kind,128);
                        if(IsWindowVisible(hwnd) && wcscmp(kind,L"#32770")==0) { *reinterpret_cast<std::atomic<bool>*>(data)=true; std::this_thread::sleep_for(std::chrono::milliseconds(250)); PostMessageW(hwnd,WM_COMMAND,MAKEWPARAM(IDCANCEL,BN_CLICKED),reinterpret_cast<LPARAM>(GetDlgItem(hwnd,IDCANCEL))); }
                        return TRUE;
                    },reinterpret_cast<LPARAM>(&nativeSeen));
                    std::this_thread::sleep_for(std::chrono::milliseconds(25));
                } });
                window.findChild<QAction*>(name)->trigger(); finished=true; dismiss.join(); std::cerr<<"Native dialog returned: "<<name<<std::endl; require(nativeSeen,"Open keeps original Windows dialog at night");
                continue;
            }
#endif
            bool shown=false;
            QTimer::singleShot(0,[&]{ if(auto dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget())) { shown=true; dialog->reject(); } });
            window.findChild<QAction*>(name)->trigger(); require(shown,"Open dialog available on non-native test platform");
        }
        auto navTabs=window.findChild<QTabWidget*>("navigationPanel"); int previousNavigation=navTabs->currentIndex(); navTabs->setCurrentIndex(2);
        if(!output.isEmpty()) iconToolbar->grab().save(output+"/tool-icons-dark.png");
        snapshot("dark-panels"); navTabs->setCurrentIndex(previousNavigation);
        darkSearch->setVisible(searchWasVisible); annotationPanel->setVisible(annotationsWereVisible);
        dark->trigger();
        require(list->palette().color(QPalette::Text).lightness()<100,"light panel text restored");
        checkToolIcons(false); if(!output.isEmpty()) iconToolbar->grab().save(output+"/tool-icons-light.png");
        auto full=window.findChild<QAction*>("fullscreenAction"); full->trigger(); app.processEvents(); require(window.isFullScreen(),"fullscreen enters");
#ifdef Q_OS_WIN
        if(QApplication::platformName()=="windows") require(GetMenu(reinterpret_cast<HWND>(window.winId()))==nullptr,"fullscreen hides native menu");
#endif
        full->trigger(); app.processEvents(); require(!window.isFullScreen(),"fullscreen exits");
        QRect previousNormal=window.geometry(); window.showMaximized(); app.processEvents();
#ifdef Q_OS_WIN
        FullscreenFrameWatch frameWatch;
        if(QApplication::platformName()=="windows") { frameWatch.hwnd=reinterpret_cast<HWND>(window.winId()); GetWindowRect(frameWatch.hwnd,&frameWatch.expected); }
#endif
        full->trigger(); app.processEvents(); require(window.isFullScreen(),"fullscreen enters from maximized state");
#ifdef Q_OS_WIN
        if(frameWatch.hwnd) app.installNativeEventFilter(&frameWatch);
#endif
        full->trigger(); app.processEvents(); require(window.isMaximized() && !window.isMinimized(),"fullscreen restores maximized state");
#ifdef Q_OS_WIN
        if(frameWatch.hwnd) { app.removeNativeEventFilter(&frameWatch); require(frameWatch.observed>0 && !frameWatch.collapsed,"fullscreen exit never displays a reduced intermediate native frame"); }
#endif
        full->trigger(); app.processEvents(); QKeyEvent leaveFull(QEvent::KeyPress,Qt::Key_Escape,Qt::NoModifier); QApplication::sendEvent(view,&leaveFull); app.processEvents();
        require(window.isMaximized() && !full->isChecked(),"Escape restores maximized state and action checkbox");
        window.showNormal(); app.processEvents();
#ifdef Q_OS_WIN
        if(QApplication::platformName()=="windows") require(window.geometry()==previousNormal,"fullscreen transition preserves normal restore geometry");
#endif
        window.setGeometry(previousNormal); app.processEvents();
        QRect normalBeforeFull=window.geometry(); full->trigger(); app.processEvents(); full->trigger(); app.processEvents();
        require(!window.isMaximized() && window.geometry()==normalBeforeFull,"fullscreen restores normal geometry");
        QSize normalSize=window.size(); window.resize(1095,760); app.processEvents(); require(window.width()==1095,"window can shrink to requested width");
        auto centerToolbar=window.findChild<QToolBar*>("centerTools");
        require(zoomPercent->isVisible() && centerToolbar->geometry().bottom()<=window.findChild<QWidget*>("toolbarHost")->height(),"compact toolbar keeps zoom visible");
        require(centerToolbar->width()>=centerToolbar->sizeHint().width(),"compact toolbar does not overflow page and zoom controls"); snapshot("compact-toolbar");
        window.resize(normalSize); app.processEvents();
        auto thumbnailList=window.findChild<QListWidget*>("thumbnails"); require(thumbnailList->movement()==QListView::Static && thumbnailList->dragDropMode()==QAbstractItemView::NoDragDrop,"thumbnails cannot be rearranged");
        require(window.exportAnnotationsTo(directory.path()+"/summary.html"),"export all document annotations"); QFile summary(directory.path()+"/summary.html"); require(summary.open(QIODevice::ReadOnly),"read exported summary"); auto summaryText=QString::fromUtf8(summary.readAll()); require(summaryText.contains("Página 1") && summaryText.contains("Página 2") && summaryText.contains("Movible"),"export includes page references and text");
        view->pdf.setPage(0); auto countBefore=view->pdf.annotationCount(); view->penColor=QColor(220,30,30,180); view->penWidth=6;
        require(view->pdf.ink({{100,450},{200,450}},view->penColor,view->penWidth),"formatted ink created"); view->refresh();
        window.findChild<QAction*>("editTool4")->trigger(); QPointF erasePoint=view->devicePoint({150,450}); QMouseEvent eraseEvent(QEvent::MouseButtonPress,erasePoint,view->mapToGlobal(erasePoint.toPoint()),Qt::LeftButton,Qt::LeftButton,Qt::NoModifier); QApplication::sendEvent(view,&eraseEvent);
        require(view->pdf.annotationCount()==countBefore,"eraser removes hit ink stroke"); window.findChild<QAction*>("undoAction")->trigger(); require(view->pdf.annotationCount()==countBefore+1,"eraser supports undo");
        {
            Window geometryWindow; geometryWindow.show(); geometryWindow.resize(1100,720); app.processEvents(); QSize savedWindowSize=geometryWindow.size(); geometryWindow.close();
            Window restoredWindow; restoredWindow.restoreWindow(); app.processEvents(); require(restoredWindow.size()==savedWindowSize,"window dimensions restored");
            QSettings settings(QSettings::IniFormat,QSettings::UserScope,AppIdentity::settingsOrganization,AppIdentity::settingsApplication); settings.setValue("window/state",int(Qt::WindowMaximized)); restoredWindow.restoreWindow(); require(restoredWindow.isMaximized(),"maximized state restored");
            settings.setValue("window/state",int(Qt::WindowMinimized)); restoredWindow.restoreWindow(); require(!restoredWindow.isMinimized(),"startup never restores a minimized window");
        }
        {
            Window exporter; exporter.loadPath(fixture); exporter.openDocument(second);
            auto readBytes=[](const QString& path) { QFile file(path); if(!file.open(QIODevice::ReadOnly)) return QByteArray(); return file.readAll(); };
            auto original=readBytes(fixture), other=readBytes(second);
            require(!exporter.exportAnnotationsTo(second) && !exporter.exportAnnotationsTo(fixture),"export cannot overwrite either open PDF");
            require(readBytes(fixture)==original && readBytes(second)==other,"rejected export preserves PDF bytes");
            require(!exporter.exportAnnotationsTo(directory.path()+"/invalid.pdf"),"export requires supported extension");
            for(const auto& extension:{"md","txt","html"}) require(exporter.exportAnnotationsTo(directory.path()+"/safe."+extension),"supported export formats remain available");
        }
        {
            QString cleanSelection=directory.path()+"/selection.pdf"; { PdfDocument sample; require(sample.createSample(2) && sample.saveCopy(cleanSelection),"clean selection regression fixture"); }
            Window selectionWindow; selectionWindow.show(); selectionWindow.loadPath(cleanSelection); selectionWindow.openDocument(second);
            auto tabs=selectionWindow.findChild<QTabBar*>("documentTabs"); tabs->setCurrentIndex(0); tabs->setCurrentIndex(1); tabs->setCurrentIndex(0);
            PageView* canvas=nullptr; for(auto widget:selectionWindow.findChildren<QWidget*>()) if(auto page=dynamic_cast<PageView*>(widget)) canvas=page;
            require(canvas,"multi document selection canvas");
            auto settle=[&]{ for(int pass=0;pass<4;++pass) { app.processEvents(); canvas->workers.waitForDone(); } };
            settle(); auto rectangles=canvas->pdf.selectionRects(0,10); require(!rectangles.isEmpty(),"selection fixture geometry");
            canvas->searchPage=canvas->pdf.pageIndex(); canvas->searchRects=rectangles;
            auto point=canvas->deviceRect(rectangles.first()).center();
            QMouseEvent selectPress(QEvent::MouseButtonPress,point,canvas->mapToGlobal(point.toPoint()),Qt::LeftButton,Qt::LeftButton,Qt::NoModifier); QApplication::sendEvent(canvas,&selectPress);
            require(canvas->searchPage==-1 && canvas->searchRects.isEmpty(),"manual selection clears search overlay after switching documents");
            canvas->first=0; canvas->last=10;
            // A queued search result must not paint yellow over an active selection.
            canvas->searchPage=canvas->pdf.pageIndex(); canvas->searchRects=rectangles;
            auto selectionImage=canvas->grab(canvas->deviceRect(rectangles.first()).toAlignedRect()).toImage(); int blue=0,yellow=0;
            for(int y=0;y<selectionImage.height();++y) for(int x=0;x<selectionImage.width();++x) { auto color=selectionImage.pixelColor(x,y); if(color.blue()>color.red()+10 && color.blue()>color.green()+5) ++blue; if(color.red()>color.blue()+20 && color.green()>color.blue()+10) ++yellow; }
            if(!output.isEmpty()) selectionImage.save(output+"/selection-blue.png");
            if(blue<=20 || yellow>0) std::cerr<<"Selection pixels: blue="<<blue<<", yellow="<<yellow<<"\n";
            require(blue>20 && yellow==0,"normal selection remains blue with active search and multiple documents");
            canvas->resetSelection(); canvas->searchPage=-1; canvas->searchRects.clear();
            selectionWindow.findChild<QComboBox*>("viewMode")->setCurrentIndex(2); settle();
            auto contentPixels=[&]{ auto image=canvas->grab(canvas->pageRects[0].toAlignedRect()).toImage(); int dark=0; for(int y=0;y<image.height();++y) for(int x=0;x<image.width();++x) if(image.pixelColor(x,y).lightness()<100) ++dark; return dark; };
            require(contentPixels()>100,"double page baseline rendering ready");
            canvas->setViewportSize(QSize(780,650));
            require(contentPixels()>100,"viewport resize retains page preview in double view");
            for(double scale:{1.4,1.6,1.8,1.3}) { canvas->setZoom(scale); require(contentPixels()>100,"rapid zoom retains visible page content"); }
            settle();
        }
        {
            QString readingPdf=directory.path()+"/reading.pdf"; { PdfDocument sample; require(sample.createSample(3) && sample.saveCopy(readingPdf),"reading persistence fixture"); }
            Window reader; reader.show(); reader.loadPath(readingPdf);
            PageView* canvas=nullptr; for(auto widget:reader.findChildren<QWidget*>()) if(auto page=dynamic_cast<PageView*>(widget)) canvas=page;
            reader.findChild<QComboBox*>("viewMode")->setCurrentIndex(1); reader.findChild<QSpinBox*>("zoomPercent")->setValue(175); reader.findChild<QAction*>("goLast")->trigger();
            QString marker=reader.addPersonalBookmark("Repasar"), temporary=reader.addPersonalBookmark("Temporal",0);
            require(!marker.isEmpty() && reader.renamePersonalBookmark(marker,"Tema del examen") && reader.removePersonalBookmark(temporary),"personal bookmarks create rename and remove");
            reader.findChild<QAction*>("showPersonalBookmarksAction")->trigger(); auto marks=reader.findChild<QListWidget*>("personalBookmarks"); require(marks->count()==1 && marks->item(0)->text().contains("Página 3"),"personal bookmark page and name shown");
            require(canvas->pdf.note({80,650},"Síntesis de Cortázar"),"study annotation fixture");
            auto scope=reader.findChild<QComboBox*>("annotationScope"); scope->setCurrentIndex(1); scope->setCurrentIndex(0);
            auto list=reader.findChild<QListWidget*>("annotationList"); list->setCurrentRow(0);
            require(!reader.findChild<QPushButton*>("editAnnotationTags") && !reader.findChild<QComboBox*>("annotationTagFilter") && !reader.findChild<QComboBox*>("annotationExportGrouping"),"annotation tag controls removed");
            auto query=reader.findChild<QLineEdit*>("annotationSearch"); query->setText("cortazar"); require(list->count()==1,"annotation search ignores accents"); query->setText("SÍNTESIS"); require(list->count()==1,"annotation text search ignores case"); query->setText("inexistente"); require(list->count()==0,"annotation search filters unmatched entries"); query->clear();
            QString summaryPath=directory.path()+"/page-summary.md"; require(reader.exportAnnotationsTo(summaryPath),"export by page"); QFile summary(summaryPath); require(summary.open(QIODevice::ReadOnly),"read page summary"); auto text=QString::fromUtf8(summary.readAll()); require(text.contains("Página 3") && text.contains("Cortázar") && !text.contains("Etiqueta:"),"summary retains text and page references");
            reader.findChild<QAction*>("saveFileAction")->trigger(); reader.findChild<QAction*>("closeDocumentAction")->trigger(); require(!canvas->pdf.loaded(),"close reading tab"); reader.openDocument(readingPdf);
            require(canvas->pdf.pageIndex()==2 && canvas->mode==PageView::Single && qFuzzyCompare(canvas->scale,1.75),"reopen closed tab restores per-file page view and zoom");
            require(marks->count()==1 && marks->item(0)->text().contains("Tema del examen"),"personal bookmarks persist after closing tab");
            reader.findChild<QAction*>("goFirst")->trigger(); marks->itemClicked(marks->item(0)); require(canvas->pdf.pageIndex()==2,"personal bookmark navigates");
            reader.findChild<QAction*>("toggleAnnotations")->setChecked(true);
            if(!output.isEmpty()) { for(int pass=0;pass<4;++pass) { app.processEvents(); canvas->workers.waitForDone(); } reader.grab().save(output+"/study-tools.png"); }
            require(reader.saveSession(),"persist reading checkpoint");
            reader.findChild<QComboBox*>("viewMode")->setCurrentIndex(0); reader.findChild<QAction*>("goLast")->trigger(); app.processEvents();
            auto readingScroll=reader.findChild<QScrollArea*>(); int savedScroll=readingScroll->verticalScrollBar()->value(); require(savedScroll>0,"continuous reading offset fixture");
            reader.findChild<QAction*>("closeDocumentAction")->trigger(); reader.openDocument(readingPdf);
            require(canvas->pdf.pageIndex()==2 && canvas->mode==PageView::Continuous && readingScroll->verticalScrollBar()->value()==savedScroll,"continuous page and scroll offset restored after tab close");
            reader.findChild<QComboBox*>("viewMode")->setCurrentIndex(1); require(reader.saveSession(),"persist reading after view change");
            Window later; later.loadPath(readingPdf); PageView* reopened=nullptr; for(auto widget:later.findChildren<QWidget*>()) if(auto page=dynamic_cast<PageView*>(widget)) reopened=page;
            require(reopened->pdf.pageIndex()==2 && qFuzzyCompare(reopened->scale,1.75) && later.findChild<QListWidget*>("personalBookmarks")->count()==1,"reading state and personal bookmarks survive new window");
        }
        // The original fixture remains alive. Its checkpoint must not overwrite
        // the independent recovery fixtures during their nested password dialogs.
        window.findChild<QTimer*>("checkpointTimer")->stop();
        QSettings sessionSettings(QSettings::IniFormat,QSettings::UserScope,AppIdentity::settingsOrganization,AppIdentity::settingsApplication);
        QString sessionRoot=QFileInfo(sessionSettings.fileName()).absolutePath()+"/recovery";
        auto sessionEntry=[](QString path) { return QJsonObject{{"path",path},{"id",QUuid::createUuid().toString(QUuid::WithoutBraces)}}; };
        auto writeManifest=[&](QJsonArray entries,int active,QString activeId=QString()) {
            QDir().mkpath(sessionRoot); QFile file(sessionRoot+"/session.json"); require(file.open(QIODevice::WriteOnly),"write recovery regression manifest");
            auto bytes=QJsonDocument(QJsonObject{{"documents",entries},{"active",active},{"activeId",activeId}}).toJson(); require(file.write(bytes)==bytes.size(),"write complete regression manifest");
        };
        auto missing=sessionEntry(directory.path()+"/missing.pdf"), firstEntry=sessionEntry(fixture), secondEntry=sessionEntry(second);
        writeManifest({missing,firstEntry,secondEntry},1);
        {
            Window restored; restored.restoreSession(); auto tabs=restored.findChild<QTabBar*>("documentTabs");
            require(tabs->count()==2 && tabs->currentIndex()==0,"legacy active tab maps around missing file");
        }
        writeManifest({missing,firstEntry,secondEntry},1,secondEntry["id"].toString());
        {
            Window restored; restored.restoreSession(); auto tabs=restored.findChild<QTabBar*>("documentTabs");
            require(tabs->count()==2 && tabs->currentIndex()==1,"stable document identity restores active tab");
        }
        QString pendingName=firstEntry["id"].toString()+".pdf";
        QString encryptedFixture=QFileInfo(QString::fromUtf8(__FILE__)).absolutePath()+"/data/recovery-encrypted.pdf";
        require(QFile::copy(encryptedFixture,sessionRoot+"/"+pendingName),"copy encrypted recovery fixture");
        auto pending=firstEntry; pending["backup"]=pendingName; writeManifest({pending,secondEntry},0,firstEntry["id"].toString());
        bool acceptPassword=false; int passwordPrompts=0;
        QTimer recoveryDialogs; recoveryDialogs.setInterval(10);
        QObject::connect(&recoveryDialogs,&QTimer::timeout,[&]{ for(auto widget:QApplication::topLevelWidgets()) {
            if(auto box=qobject_cast<QMessageBox*>(widget); box && box->isVisible() && box->standardButtons().testFlag(QMessageBox::Yes)) box->button(QMessageBox::Yes)->click();
            if(auto input=qobject_cast<QInputDialog*>(widget); input && input->isVisible()) { ++passwordPrompts; if(acceptPassword) { input->setTextValue("audit-test"); input->accept(); } else input->reject(); }
        } }); recoveryDialogs.start();
        {
            Window cancelled; cancelled.restoreSession();
            require(cancelled.findChild<QTabBar*>("documentTabs")->count()==1 && passwordPrompts==1,"password cancellation skips only pending document");
            require(cancelled.saveSession() && cancelled.saveSession(true),"checkpoints succeed after cancelled recovery");
            require(QFileInfo::exists(sessionRoot+"/"+pendingName),"cancelled recovery backup retained across checkpoints and clean exit");
            QFile manifestFile(sessionRoot+"/session.json"); require(manifestFile.open(QIODevice::ReadOnly),"read retained recovery manifest");
            bool pendingRetained=false; for(auto value:QJsonDocument::fromJson(manifestFile.readAll()).object()["documents"].toArray()) if(value.toObject()["backup"].toString()==pendingName) pendingRetained=true;
            require(pendingRetained,"pending recovery retained in manifest for retry");
        }
        acceptPassword=true;
        {
            Window retried; retried.restoreSession(); auto tabs=retried.findChild<QTabBar*>("documentTabs"); require(tabs->count()==2 && passwordPrompts==2,"pending encrypted recovery retried on next launch"); tabs->setCurrentIndex(1);
            PageView* recoveredView=nullptr; for(auto widget:retried.findChildren<QWidget*>()) if(auto page=dynamic_cast<PageView*>(widget)) recoveredView=page;
            require(recoveredView && recoveredView->pdf.dirty(),"retried recovery retains pending edits");
        }
        recoveryDialogs.stop();
        {
            QString largeAnnotations=directory.path()+"/large-annotations.pdf";
            { PdfDocument sample; require(sample.createSample(60),"large annotation fixture"); require(sample.note({60,650},"Primera página"),"first page note"); require(sample.setPage(59) && sample.note({60,650},"Última página") && sample.saveCopy(largeAnnotations),"last page note"); }
            Window annotated; annotated.show(); annotated.loadPath(largeAnnotations);
            PageView* canvas=nullptr; for(auto widget:annotated.findChildren<QWidget*>()) if(auto page=dynamic_cast<PageView*>(widget)) canvas=page;
            auto list=annotated.findChild<QListWidget*>("annotationList"); auto panel=annotated.findChild<QAction*>("toggleAnnotations");
            panel->setChecked(true);
            auto waitForCount=[&](int count){ QElapsedTimer wait; wait.start(); while(list->count()!=count && wait.elapsed()<3000) { app.processEvents(); QThread::msleep(1); } return list->count()==count; };
            require(waitForCount(2) && list->item(1)->text().contains("Página 60"),"opening hidden panel loads annotations from whole large document");
            annotated.findChild<QComboBox*>("annotationTypeFilter")->setCurrentIndex(1);
            canvas->first=0; canvas->last=8; annotated.findChild<QAction*>("highlightAction")->trigger(); require(waitForCount(3),"large panel refreshes after highlight");
            annotated.findChild<QAction*>("undoAction")->trigger(); require(waitForCount(2),"large panel refreshes after undo");
            annotated.findChild<QAction*>("redoAction")->trigger(); require(waitForCount(3),"large panel refreshes after redo");
            panel->setChecked(false); annotated.findChild<QComboBox*>("annotationScope")->setCurrentIndex(1); panel->setChecked(true);
            require(list->count()==2,"page-only filter retains current page annotations");
            annotated.findChild<QComboBox*>("annotationScope")->setCurrentIndex(0); require(waitForCount(3),"document filter restores annotations from other pages");
            auto query=annotated.findChild<QLineEdit*>("annotationSearch"); query->setText("Última"); require(waitForCount(1),"filter by contents on another page");
            panel->setChecked(false); canvas->first=0; canvas->last=8; annotated.findChild<QAction*>("highlightAction")->trigger();
            require(query->text()=="Última","hidden-panel update retains annotation search");
            panel->setChecked(true); require(waitForCount(1) && list->item(0)->text().contains("Página 60"),"reopening panel restores searched contents");
        }
        {
            Window searching; PdfDocument fixture;
            require(fixture.createSample(12,"ALPHA study search") && fixture.saveCopy(directory.path()+"/search-edit.pdf"),"concurrent search fixture");
            searching.show(); searching.loadPath(directory.path()+"/search-edit.pdf");
            auto input=searching.findChild<QLineEdit*>("searchInput"); auto results=searching.findChild<QListWidget*>("searchResults");
            PageView* canvas=nullptr; for(auto widget:searching.findChildren<QWidget*>()) if(auto page=dynamic_cast<PageView*>(widget)) canvas=page;
            require(canvas && input && results,"concurrent search controls");
            input->setText("ALPHA"); QMetaObject::invokeMethod(input,"returnPressed");
            // Queue the first page search behind an edit and change its revision before dispatch.
            { std::lock_guard guard(PdfDocument::apiMutex());
                app.processEvents();
                canvas->first=0; canvas->last=4; searching.findChild<QAction*>("highlightAction")->trigger();
            }
            QElapsedTimer wait; wait.start(); while(results->count()!=12 && wait.elapsed()<4000) { app.processEvents(); QThread::msleep(1); }
            require(results->count()==12,"editing during search does not omit any page");
            input->setText("missing"); QMetaObject::invokeMethod(input,"returnPressed");
            input->setText("study"); QMetaObject::invokeMethod(input,"returnPressed");
            wait.restart(); while(results->count()!=12 && wait.elapsed()<4000) { app.processEvents(); QThread::msleep(1); }
            require(results->count()==12,"cancelled searches cannot replace current results");
        }
        {
            // Exercise the real second-process IPC path while the reader is minimized.
            Window receiving; receiving.openDocument(fixture); receiving.showMaximized(); app.processEvents();
            require(receiving.isMaximized(),"external-opening fixture is maximized");
            receiving.showMinimized(); app.processEvents();
            require(receiving.isMinimized(),"external-opening fixture is minimized");
            instance.received=[&](const QStringList& paths){ receiving.openExternalDocuments(paths); };
            QProcess forwarded; forwarded.start(QCoreApplication::applicationFilePath(),QStringList{"--instance-client",instanceKey,second});
            QElapsedTimer wait; wait.start();
            auto tabs=receiving.findChild<QTabBar*>("documentTabs");
            while((forwarded.state()!=QProcess::NotRunning || tabs->count()!=2) && wait.elapsed()<8000) {
                app.processEvents(); if(forwarded.state()!=QProcess::NotRunning) forwarded.waitForFinished(10);
            }
            app.processEvents();
            require(forwarded.state()==QProcess::NotRunning && forwarded.exitCode()==0 && tabs->count()==2,"external process adds the PDF to the existing reader");
            require(receiving.isMaximized() && !receiving.isMinimized(),"external PDF restores the maximized window");
#ifdef Q_OS_WIN
            if(QApplication::platformName()=="windows") {
                auto hwnd=reinterpret_cast<HWND>(receiving.winId());
                require(IsZoomed(hwnd) && !IsIconic(hwnd),"external PDF restores the actual Windows maximized frame");
            }
#endif
            receiving.openExternalDocuments({fixture}); app.processEvents();
            require(receiving.isMaximized() && tabs->count()==2,"opening another PDF preserves a visible maximized window");
            receiving.showNormal(); receiving.resize(1100,720); app.processEvents(); auto normal=receiving.size();
            receiving.showMinimized(); app.processEvents(); receiving.openExternalDocuments({second}); app.processEvents();
            require(!receiving.isMinimized() && !receiving.isMaximized() && receiving.size()==normal,"external PDF restores a normal window without maximizing it");
            instance.received=[&](QStringList paths){ delivered=paths; };
        }
        {
            const QString contents="Comentario de prueba: Cortázar y acción.\nSegunda línea con <texto literal> y símbolos.\nEste comentario se lee sin modificar el PDF.";
            PdfDocument fixture; require(fixture.createSample(2) && fixture.note({60,680},contents),"popup fixture");
            const QString path=directory.path()+"/comment-popup.pdf"; require(fixture.saveCopy(path),"save popup fixture");
            Window reader; reader.show(); reader.openDocument(path); app.processEvents();
            PageView* canvas=nullptr; for(auto widget:reader.findChildren<QWidget*>()) if(auto page=dynamic_cast<PageView*>(widget)) canvas=page;
            auto modes=reader.findChild<QComboBox*>("viewMode"); auto area=reader.findChild<QScrollArea*>();
            require(canvas && modes && area,"popup controls");
            auto click=[&](QPointF position) {
                QMouseEvent event(QEvent::MouseButtonPress,position,canvas->mapToGlobal(position.toPoint()),Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);
                QApplication::sendEvent(canvas,&event);
            };
            for(int mode=0;mode<3;++mode) {
                modes->setCurrentIndex(mode); app.processEvents(); canvas->tool=PageView::Select;
                click(canvas->devicePoint(canvas->pdf.annotationRect(0).center()));
                auto popup=canvas->findChild<QFrame*>("commentPopup"); auto text=canvas->findChild<QTextEdit*>("commentContents");
                require(canvas->commentVisible() && text && text->toPlainText()==contents && text->isReadOnly(),"Select opens the existing comment in each view");
                auto visible=QRect(canvas->mapFrom(area->viewport(),QPoint()),area->viewport()->size()).intersected(canvas->activeRect().toAlignedRect());
                require(popup && visible.contains(popup->geometry()),"comment popup stays within the visible page");
                require(!canvas->pdf.dirty() && canvas->pdf.annotationCount()==1,"reading a comment does not modify or duplicate it");
                QKeyEvent escape(QEvent::KeyPress,Qt::Key_Escape,Qt::NoModifier); QApplication::sendEvent(text,&escape);
                require(!canvas->commentVisible(),"Escape closes the comment popup");
                canvas->tool=PageView::Note;
                bool dialogOpened=false; QTimer guard; guard.setSingleShot(true);
                QObject::connect(&guard,&QTimer::timeout,[&]{ if(auto dialog=qobject_cast<QInputDialog*>(QApplication::activeModalWidget())) { dialogOpened=true; dialog->reject(); } });
                guard.start(0); click(canvas->devicePoint(canvas->pdf.annotationRect(0).center())); guard.stop();
                require(!dialogOpened && canvas->commentVisible() && canvas->pdf.annotationCount()==1,"Comment tool opens a note instead of creating another one");
                text->setFocus(); QKeyEvent select(QEvent::KeyPress,Qt::Key_A,Qt::ControlModifier); QApplication::sendEvent(text,&select);
                QKeyEvent copy(QEvent::KeyPress,Qt::Key_C,Qt::ControlModifier); QApplication::sendEvent(text,&copy);
                require(QApplication::clipboard()->text()==contents,"popup text supports select-all and copy");
                if(!output.isEmpty()) {
                    for(int pass=0;pass<3;++pass) { app.processEvents(); canvas->workers.waitForDone(); }
                    reader.grab().save(output+QString("/comment-popup-mode-%1.png").arg(mode));
                }
                reader.findChild<QToolButton*>("closeCommentPopup")->click(); require(!canvas->commentVisible(),"close button dismisses comment");
            }
            modes->setCurrentIndex(0); canvas->tool=PageView::Select;
            click(canvas->devicePoint(canvas->pdf.annotationRect(0).center()));
            auto popupDark=reader.findChild<QAction*>("darkModeAction"); if(!popupDark->isChecked()) popupDark->trigger(); app.processEvents();
            require(canvas->darkMode && canvas->commentVisible(),"comment remains available in dark mode");
            if(!output.isEmpty()) reader.grab().save(output+"/comment-popup-dark.png");
            QMouseEvent outside(QEvent::MouseButtonPress,QPointF(4,4),reader.mapToGlobal(QPoint(4,4)),Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);
            QApplication::sendEvent(reader.findChild<QToolBar*>("mainToolbar"),&outside);
            require(!canvas->commentVisible(),"click outside the popup dismisses it");
            if(popupDark->isChecked()) popupDark->trigger();
        }
        {
            PdfDocument fixture; require(fixture.createSample(3) && fixture.note({70,650},"Mover comentario"),"drag comment fixture");
            QString path=directory.path()+"/drag-comment.pdf"; require(fixture.saveCopy(path),"save drag fixture");
            Window reader; reader.openDocument(path); reader.show(); app.processEvents();
            PageView* canvas=nullptr; for(auto widget:reader.findChildren<QWidget*>()) if(auto page=dynamic_cast<PageView*>(widget)) canvas=page;
            auto modes=reader.findChild<QComboBox*>("viewMode"); auto number=reader.findChild<QSpinBox*>("pageNumber");
            require(number && number->maximum()==3 && number->value()==1,"page field reflects document bounds");
            auto pageInput=number->findChild<QLineEdit*>(); pageInput->setFocus(); pageInput->selectAll(); pageInput->setText("3");
            QKeyEvent pageEnter(QEvent::KeyPress,Qt::Key_Return,Qt::NoModifier); QApplication::sendEvent(pageInput,&pageEnter);
            require(canvas->pdf.pageIndex()==2 && number->value()==3,"typing a page and Enter navigates directly"); number->setValue(1);
            for(int mode=0;mode<3;++mode) { modes->setCurrentIndex(mode); number->setValue(3); app.processEvents(); require(canvas->pdf.pageIndex()==2 && number->value()==3,"page field navigates in each view mode"); number->setValue(1); }
            modes->setCurrentIndex(2); app.processEvents(); canvas->tool=PageView::Select;
            auto start=canvas->devicePoint(canvas->pdf.annotationRect(0).center()); auto end=canvas->pageRects[1].center();
            QMouseEvent press(QEvent::MouseButtonPress,start,canvas->mapToGlobal(start.toPoint()),Qt::LeftButton,Qt::LeftButton,Qt::NoModifier); QApplication::sendEvent(canvas,&press);
            QMouseEvent move(QEvent::MouseMove,end,canvas->mapToGlobal(end.toPoint()),Qt::NoButton,Qt::LeftButton,Qt::NoModifier); QApplication::sendEvent(canvas,&move);
            require(canvas->annotationDragging() && !canvas->commentVisible(),"comment drag hides its popup");
            QMouseEvent release(QEvent::MouseButtonRelease,end,canvas->mapToGlobal(end.toPoint()),Qt::LeftButton,Qt::NoButton,Qt::NoModifier); QApplication::sendEvent(canvas,&release);
            require(canvas->pdf.pageIndex()==1 && canvas->pdf.annotationContents(0)=="Mover comentario" && number->value()==2,"Select drops comment on another page and updates page field");
            require(canvas->pdf.undo() && canvas->pdf.setPage(0) && canvas->pdf.annotationCount()==1,"drag transfer undoes as one edit");
            modes->setCurrentIndex(0); number->setValue(1); app.processEvents();
            start=canvas->devicePoint(canvas->pdf.annotationRect(0).center()); end=canvas->pageRects[2].center();
            QMouseEvent continuousPress(QEvent::MouseButtonPress,start,canvas->mapToGlobal(start.toPoint()),Qt::LeftButton,Qt::LeftButton,Qt::NoModifier); QApplication::sendEvent(canvas,&continuousPress);
            QMouseEvent continuousMove(QEvent::MouseMove,end,canvas->mapToGlobal(end.toPoint()),Qt::NoButton,Qt::LeftButton,Qt::NoModifier); QApplication::sendEvent(canvas,&continuousMove);
            canvas->activate(2);
            QMouseEvent continuousRelease(QEvent::MouseButtonRelease,end,canvas->mapToGlobal(end.toPoint()),Qt::LeftButton,Qt::NoButton,Qt::NoModifier); QApplication::sendEvent(canvas,&continuousRelease);
            require(canvas->pdf.pageIndex()==2 && canvas->pdf.annotationContents(0)=="Mover comentario","continuous drag survives active page change while scrolling");
            require(canvas->pdf.undo() && canvas->pdf.setPage(0),"undo continuous drag");
            auto find=reader.findChild<QAction*>("focusSearch"); require(find->shortcut()==QKeySequence("Ctrl+B") && find->property("defaultShortcut").toString()=="Ctrl+B","Spanish search shortcut and restored default");
            auto search=reader.findChild<QAction*>("searchAction"); search->setChecked(false); canvas->setFocus();
            QKeyEvent searchKey(QEvent::KeyPress,Qt::Key_B,Qt::ControlModifier); QApplication::sendEvent(canvas,&searchKey); require(search->isChecked(),"Ctrl+B opens search panel");
            reader.openDocument(directory.path()+"/ui.pdf"); app.processEvents(); auto tabs=reader.findChild<QTabBar*>("documentTabs"); require(tabs->count()==2,"middle close fixture has two documents");
            auto point=tabs->tabRect(1).center();
            QMouseEvent down(QEvent::MouseButtonPress,point,tabs->mapToGlobal(point),Qt::MiddleButton,Qt::MiddleButton,Qt::NoModifier); QApplication::sendEvent(tabs,&down);
            QMouseEvent up(QEvent::MouseButtonRelease,point,tabs->mapToGlobal(point),Qt::MiddleButton,Qt::NoButton,Qt::NoModifier); QApplication::sendEvent(tabs,&up);
            require(tabs->count()==1,"middle button closes document tab");
            require(canvas->pdf.note({100,500},"Sin guardar"),"unsaved middle close fixture");
            reader.openDocument(directory.path()+"/ui.pdf"); app.processEvents(); require(tabs->count()==2,"unsaved tab remains alongside second document");
            QTimer dismiss; bool prompted=false; QObject::connect(&dismiss,&QTimer::timeout,[&]{ for(auto widget:QApplication::topLevelWidgets()) if(auto box=qobject_cast<QMessageBox*>(widget)) if(auto cancel=box->button(QMessageBox::Cancel)) { prompted=true; cancel->click(); } }); dismiss.start(0);
            point=tabs->tabRect(0).center(); QMouseEvent dirtyDown(QEvent::MouseButtonPress,point,tabs->mapToGlobal(point),Qt::MiddleButton,Qt::MiddleButton,Qt::NoModifier); QApplication::sendEvent(tabs,&dirtyDown);
            QMouseEvent dirtyUp(QEvent::MouseButtonRelease,point,tabs->mapToGlobal(point),Qt::MiddleButton,Qt::NoButton,Qt::NoModifier); QApplication::sendEvent(tabs,&dirtyUp); dismiss.stop();
            require(prompted && tabs->count()==2 && canvas->pdf.dirty(),"Cancel retains unsaved tab after middle close");
        }
        {
            PdfDocument fixture; require(fixture.createSample(2,"Neutral markup example"),"underline UI fixture"); QString path=directory.path()+"/underline-ui.pdf"; require(fixture.saveCopy(path),"save underline UI fixture");
            Window reader; reader.openDocument(path); reader.show(); app.processEvents();
            PageView* canvas=nullptr; for(auto widget:reader.findChildren<QWidget*>()) if(auto page=dynamic_cast<PageView*>(widget)) canvas=page;
            auto tools=reader.findChild<QToolBar*>("editingTools"); auto underline=reader.findChild<QAction*>("underlineAction"); auto highlight=reader.findChild<QAction*>("highlightAction");
            require(tools->actions().indexOf(underline)==tools->actions().indexOf(highlight)+1,"underline follows highlight in toolbar");
            auto colors=reader.findChild<QComboBox*>("underlineColor"); auto width=reader.findChild<QDoubleSpinBox*>("underlineWidth");
            require(colors && width,"underline has color and width controls"); colors->setCurrentIndex(2); width->setValue(4);
            auto indicator=[&](QAction* action,QColor color){ auto image=action->icon().pixmap(24,24).toImage(); int pixels=0; for(int y=21;y<image.height();++y) for(int x=0;x<image.width();++x) { auto actual=image.pixelColor(x,y); if(actual.alpha()>150 && std::abs(actual.red()-color.red())<5 && std::abs(actual.green()-color.green())<5 && std::abs(actual.blue()-color.blue())<5) ++pixels; } return pixels>10; };
            require(indicator(underline,colors->currentData().value<QColor>()),"underline icon shows selected color");
            for(const auto& pair:QList<QPair<QString,QString>>{{"highlightColor","highlightAction"},{"penColor","editTool3"}}) { auto combo=reader.findChild<QComboBox*>(pair.first); combo->setCurrentIndex(3); require(indicator(reader.findChild<QAction*>(pair.second),combo->currentData().value<QColor>()),"drawing icon follows selected color"); }
            canvas->first=0; canvas->last=6; underline->trigger();
            require(canvas->pdf.annotationType(0)==FPDF_ANNOT_UNDERLINE && canvas->pdf.copyAnnotation(0).strokeWidth==4 && canvas->pdf.annotationColor(0).rgb()==QColor("#cc2244").rgb(),"toolbar creates underline with selected settings");
            reader.findChild<QAction*>("toggleAnnotations")->setChecked(true); reader.findChild<QComboBox*>("annotationTypeFilter")->setCurrentIndex(6); app.processEvents();
            auto list=reader.findChild<QListWidget*>("annotationList"); require(list->count()==1 && list->item(0)->data(Qt::UserRole+2).toString().startsWith("<b>Página 1 · Subrayado</b>"),"annotation page and type are bold");
            list->setCurrentRow(0); bool formatted=false; QTimer formatDialog;
            QObject::connect(&formatDialog,&QTimer::timeout,[&]{ if(auto dialog=reader.findChild<QDialog*>("underlineFormatDialog")) { dialog->findChild<QDoubleSpinBox*>("underlineFormatWidth")->setValue(5); formatted=true; dialog->accept(); } }); formatDialog.start(0);
            for(auto button:reader.findChildren<QPushButton*>()) if(button->text()=="Editar") { button->click(); break; } formatDialog.stop();
            require(formatted && canvas->pdf.copyAnnotation(0).strokeWidth==5 && canvas->pdf.annotationContents(0).isEmpty(),"Edit modifies underline format without creating a comment body");
            if(!output.isEmpty()) { canvas->workers.waitForDone(); app.processEvents(); reader.grab().save(output+"/underline-and-annotations.png"); }
            Window next; PageView* nextCanvas=nullptr; for(auto widget:next.findChildren<QWidget*>()) if(auto page=dynamic_cast<PageView*>(widget)) nextCanvas=page;
            require(nextCanvas->underlineWidth==4 && nextCanvas->underlineColor.rgb()==QColor("#cc2244").rgb(),"underline settings persist across windows");
        }
        {
            class StartupWindow : public Window { public: int shows=0; bool firstMaximized=false,firstMinimized=false; protected: void showEvent(QShowEvent* event) override { if(shows++==0) { firstMaximized=isMaximized(); firstMinimized=isMinimized(); } Window::showEvent(event); } };
            QSettings settings(QSettings::IniFormat,QSettings::UserScope,AppIdentity::settingsOrganization,AppIdentity::settingsApplication); settings.setValue("window/state",int(Qt::WindowMaximized|Qt::WindowMinimized));
            StartupWindow reader; require(reader.shows==0 && !reader.isVisible(),"startup construction does not show a preliminary window"); reader.restoreWindow(); app.processEvents();
            require(reader.shows==1 && reader.firstMaximized && !reader.firstMinimized,"first visible startup frame has restored maximized state");
            settings.setValue("window/state",0);
        }
        if(QApplication::platformName()=="windows") {
            QPixmap sample(440,76); sample.fill(Qt::white);
            QPainter painter(&sample); painter.setPen(Qt::black); painter.setFont(gpdfUiFont()); painter.drawText(8,26,"C S P 1 2 — Copia de texto permitida: Sí · 125%");
            auto bold=gpdfUiFont(); bold.setBold(true); painter.setFont(bold); painter.drawText(8,56,"Página 12 · Propiedades · C S P 1 2"); painter.end();
            auto image=sample.toImage(); int colored=0,smoothed=0;
            for(int y=0;y<image.height();++y) for(int x=0;x<image.width();++x) { auto c=image.pixelColor(x,y); if(std::max({c.red(),c.green(),c.blue()})-std::min({c.red(),c.green(),c.blue()})>1) ++colored; if(c.red()>0 && c.red()<255 && c.red()==c.green() && c.red()==c.blue()) ++smoothed; }
            const bool grayscale=gpdfUiFont().styleStrategy() & QFont::NoSubpixelAntialias;
            if(grayscale && qgetenv("QT_QPA_PLATFORM").contains("fontengine=gdi")) require(colored==0 && smoothed>100,"Inter renders normal and bold text with grayscale smoothing and no colored fringes");
            else require(colored+smoothed>100,"Inter renders normal and bold text with smoothing");
            std::cout << "Inter rendering: colored=" << colored << " grayscale=" << smoothed << '\n';
            if(!output.isEmpty()) sample.save(output+(grayscale ? "/inter-grayscale.png" : "/inter-subpixel.png"));
        }
        std::cout << "PASS: three view modes, scroll, paired navigation, correct-page annotations, editing and deletion.\n";
        return 0;
    } catch(const std::exception& e) { std::cerr << "FAIL: " << e.what() << '\n'; return 1; }
}

