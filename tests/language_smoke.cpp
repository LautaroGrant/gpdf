#include "Window.h"
#include "UiFont.h"
#include <QTemporaryDir>
#include <QProcess>
#include <QXmlStreamReader>
#include <QRegularExpression>
#include <iostream>
#include <stdexcept>

int main(int argc,char** argv) {
    QApplication app(argc,argv); app.setStyle("Fusion"); app.setFont(gpdfUiFont());
    auto require=[](bool ok,const char* message){ if(!ok) throw std::runtime_error(message); };
    try {
        const auto args=app.arguments();
        if(args.size()==4 && args[1]=="--restart") {
            QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,args[2]);
            installUiTranslations();
            require(currentUiLanguage()==args[3],"saved language restored in new process");
            Window restarted;
            require(restarted.findChild<QAction*>("saveAsFileAction")->shortcut()==QKeySequence(args[3]=="en" ? "Ctrl+Shift+S" : "Ctrl+Shift+G"),"default shortcuts follow language after restart");
            require(restarted.findChild<QAction*>("focusSearch")->shortcut()==QKeySequence("Ctrl+Alt+F"),"custom shortcut survives language restart");
            require(restarted.findChild<QAction*>("workbenchAction")->shortcut()==QKeySequence("Ctrl+Alt+W"),"custom Workbench shortcut survives restart");
            return 0;
        }
        QTemporaryDir directory; require(directory.isValid(),"isolated settings directory");
        QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,directory.path());
        const QString language=args.value(1,"es"); const bool english=language=="en";
        require(!saveUiLanguage("invalid"),"unsupported language rejected");
        require(initializeUiLanguage(language),"installer initializes language");
        require(initializeUiLanguage(english ? "es" : "en") && savedUiLanguage()==language,"upgrade preserves preferred language");
        QSettings settings(QSettings::IniFormat,QSettings::UserScope,AppIdentity::settingsOrganization,AppIdentity::settingsApplication);
        settings.setValue("drawing/highlightOpacity",english ? 35 : 60);
        settings.setValue("shortcuts/focusSearch","Ctrl+Alt+F");
        settings.setValue("shortcuts/toggleAnnotations","Ctrl+A");
        settings.setValue("shortcuts/underlineAction","");
        settings.setValue("shortcuts/viewRotation1","Ctrl+R");
        settings.setValue("shortcuts/rotateClockwiseAction","R");
        settings.setValue("shortcuts/saveFileAction","Ctrl+Alt+G"); settings.sync();
        Window window; window.show(); app.processEvents();
        require(window.findChild<QSpinBox*>("highlightOpacity")->value()==(english ? 35 : 80),"highlight default migrates while custom opacity is preserved");
        auto professional=window.findChild<QAction*>("professionalModeAction");
        auto tools=window.findChild<QToolBar*>("mainToolbar");
        auto documents=window.findChild<QToolBar*>("documentToolbar");
        require(professional && professional->text()==(english ? "Professional mode" : "Modo profesional"),"professional mode translated");
        require(professional->shortcut()==QKeySequence("Ctrl+Shift+P"),"professional mode has exit shortcut");
        const auto flags=window.windowFlags();
        professional->trigger(); app.processEvents();
        require(professional->isChecked() && tools->isHidden() && documents->isHidden() && window.menuBar()->isHidden(),"professional mode hides tools and menus without empty tab bar");
        require(window.windowFlags()==flags && !window.isFullScreen(),"professional mode preserves ordinary window frame");
#ifdef Q_OS_WIN
        if(QApplication::platformName()=="windows") require(!GetMenu(reinterpret_cast<HWND>(window.winId())),"professional mode detaches native menu");
#endif
        const auto first=directory.filePath("professional-first.pdf"),second=directory.filePath("professional-second.pdf");
        { PdfDocument sample; require(sample.createSample(3) && sample.saveCopy(first) && sample.saveCopy(second),"professional mode fixtures"); }
        window.openDocument(first); require(documents->isHidden(),"single document needs no tab bar");
        window.openDocument(second); require(!documents->isHidden() && tools->isHidden(),"multiple documents retain tab bar in professional mode");
        window.findChild<QAction*>("closeDocumentAction")->trigger();
        require(documents->isHidden() && professional->isChecked(),"closing second document hides tab bar");
        auto sendKey=[&](QEvent::Type type,int code,bool repeat=false) {
            QKeyEvent key(type,code,Qt::NoModifier,QString(),repeat); QApplication::sendEvent(&window,&key);
        };
        auto settleFade=[&] { QEventLoop loop; QTimer::singleShot(280,&loop,&QEventLoop::quit); loop.exec(); };
        sendKey(QEvent::KeyPress,Qt::Key_Control);
        auto indicator=window.findChild<QLabel*>("professionalPageIndicator");
        require(indicator && !indicator->isHidden() && indicator->text()=="1 / 3","Ctrl reveals current and total page count");
        require(indicator->parentWidget()->width()-indicator->x()-indicator->width()==12
                && indicator->parentWidget()->height()-indicator->y()-indicator->height()==12,
                "page hint sits at bottom right");
        window.findChild<QAction*>("goNext")->trigger();
        require(indicator->text()=="2 / 3","held hint updates on navigation");
        sendKey(QEvent::KeyPress,Qt::Key_Space);
        sendKey(QEvent::KeyRelease,Qt::Key_Control); settleFade();
        require(!indicator->isHidden(),"Space keeps hint visible after releasing Ctrl");
        sendKey(QEvent::KeyRelease,Qt::Key_Space,true); settleFade();
        require(!indicator->isHidden(),"autorepeat release does not hide hint");
        sendKey(QEvent::KeyRelease,Qt::Key_Space);
        require(!indicator->isHidden(),"key release starts a fade instead of abruptly hiding");
        sendKey(QEvent::KeyPress,Qt::Key_Control); settleFade();
        require(!indicator->isHidden(),"pressing during fade keeps hint visible");
        sendKey(QEvent::KeyRelease,Qt::Key_Control); settleFade();
        require(indicator->isHidden(),"hint disappears after fading");
        sendKey(QEvent::KeyPress,Qt::Key_Control);
        QEvent deactivated(QEvent::WindowDeactivate); QApplication::sendEvent(&window,&deactivated); settleFade();
        require(indicator->isHidden(),"lost focus clears held keys");
        window.findChild<QAction*>("goNext")->trigger();
        require(!indicator->isHidden() && indicator->text()=="3 / 3","navigation alone briefly shows the page indicator");
        QEventLoop navigationHint;
        QTimer::singleShot(1100,&navigationHint,&QEventLoop::quit);
        navigationHint.exec();
        require(indicator->isHidden(),"navigation hint fades without a key release");
        QLineEdit editor(&window); editor.setText("a");
        QKeyEvent typedSpace(QEvent::KeyPress,Qt::Key_Space,Qt::NoModifier," "); QApplication::sendEvent(&editor,&typedSpace);
        require(editor.text()=="a " && indicator->isHidden(),"space still types in text controls");
        auto fullscreen=window.findChild<QAction*>("fullscreenAction");
        fullscreen->trigger(); fullscreen->trigger(); app.processEvents();
        require(professional->isChecked() && tools->isHidden() && window.menuBar()->isHidden(),"fullscreen roundtrip retains professional mode");
        auto dark=window.findChild<QAction*>("darkModeAction"); dark->trigger(); app.processEvents();
        require(window.menuBar()->isHidden(),"theme change cannot reveal professional menu");
        dark->trigger();
        QKeyEvent escape(QEvent::KeyPress,Qt::Key_Escape,Qt::NoModifier); QApplication::sendEvent(&window,&escape);
        require(!professional->isChecked() && !tools->isHidden(),"Escape restores tools");
        auto toolbarToggle=window.findChild<QAction*>("toggleToolbarAction");
        toolbarToggle->trigger(); professional->trigger(); professional->trigger();
        require(tools->isHidden(),"professional mode preserves manually hidden toolbar");
        toolbarToggle->trigger(); window.findChild<QAction*>("closeDocumentAction")->trigger();
        auto workbench=window.findChild<QAction*>("workbenchAction");
        require(workbench->shortcut()==QKeySequence("Ctrl+Shift+W"),"Workbench default shortcut");
        bool workbenchOpened=false;
        QTimer::singleShot(0,[&]{ if(auto dialog=window.findChild<QDialog*>("workbenchDialog")) { workbenchOpened=true; dialog->reject(); } });
        QKeyEvent workbenchKey(QEvent::KeyPress,Qt::Key_W,Qt::ControlModifier|Qt::ShiftModifier); QApplication::sendEvent(&window,&workbenchKey);
        require(workbenchOpened,"Workbench opens through its keyboard shortcut");
        bool sorted=true,edited=false;
        QTimer::singleShot(0,[&]{
            auto dialog=window.findChild<QDialog*>("shortcutsDialog"); if(!dialog) return;
            auto table=dialog->findChild<QTableWidget*>("shortcutsTable");
            QCollator collator(QLocale(english ? "en_US" : "es_AR")); collator.setCaseSensitivity(Qt::CaseInsensitive); collator.setNumericMode(true);
            for(int row=0;row<table->rowCount();++row) {
                if(row && collator.compare(table->item(row-1,0)->text(),table->item(row,0)->text())>0) sorted=false;
                if(table->item(row,0)->text()=="Workbench") {
                    auto edit=qobject_cast<QKeySequenceEdit*>(table->cellWidget(row,1)); edit->setKeySequence(QKeySequence("Ctrl+Alt+W")); edited=true;
                }
            }
            dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Save)->click();
        });
        window.findChild<QAction*>("configureShortcutsAction")->trigger();
        require(sorted && edited && workbench->shortcut()==QKeySequence("Ctrl+Alt+W"),"localized sorted rows save the correct shortcut action");
        bool defaultRestored=false;
        QTimer::singleShot(0,[&]{
            auto dialog=window.findChild<QDialog*>("shortcutsDialog"); if(!dialog) return;
            dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::RestoreDefaults)->click();
            auto table=dialog->findChild<QTableWidget*>("shortcutsTable");
            for(int row=0;row<table->rowCount();++row) if(table->item(row,0)->text()=="Workbench")
                defaultRestored=qobject_cast<QKeySequenceEdit*>(table->cellWidget(row,1))->keySequence()==QKeySequence("Ctrl+Shift+W");
            dialog->reject();
        });
        window.findChild<QAction*>("configureShortcutsAction")->trigger();
        require(defaultRestored && workbench->shortcut()==QKeySequence("Ctrl+Alt+W"),"restore defaults uses sorted mapping and cancel preserves customization");
        require(currentUiLanguage()==language,"startup language applied");
        require(window.findChild<QAction*>("openFileAction")->text()==(english ? "Open" : "Abrir"),"file menu translation");
        require(window.findChild<QAction*>("highlightAction")->text()==(english ? "Highlight" : "Resaltar"),"tool translation");
        require(window.findChild<QPushButton*>("addPersonalBookmarkButton")->text()==(english ? "Add" : "Añadir"),"personal bookmark add label translated");
        require(window.findChild<QComboBox*>("viewMode")->itemText(0)==(english ? "Continuous view" : "Vista continua"),"view mode translation");
        require(window.findChild<QLineEdit*>("searchInput")->placeholderText()==(english ? "Search text…" : "Texto a buscar…"),"search translation");
        auto rotate=window.findChild<QAction*>("rotateClockwiseAction");
        require(rotate && rotate->shortcut()==QKeySequence("Ctrl+R"),"rotation shortcut in both languages");
        require(rotate->text()==(english ? "Rotate 90° clockwise" : "Girar 90° a la derecha"),"rotation tooltip translation");
        for(int turn=0;turn<4;++turn) {
            auto orientation=window.findChild<QAction*>(QString("viewRotation%1").arg(turn));
            require(orientation && orientation->shortcut().isEmpty() && !orientation->property("defaultShortcut").isValid(),"fixed orientations are excluded from configurable shortcuts in both languages");
        }
        settings.sync(); require(!settings.contains("shortcuts/viewRotation1"),"obsolete saved rotation shortcut removed");
        require(window.findChild<QAction*>("focusSearch")->shortcut()==QKeySequence("Ctrl+Alt+F"),"custom search shortcut preserved");
        require(window.findChild<QAction*>("saveFileAction")->shortcut()==QKeySequence("Ctrl+Alt+G"),"custom save shortcut survives legacy migration");
        const QHash<QString,QString> expectedShortcuts={{"saveFileAction",english ? "Ctrl+S" : "Ctrl+G"},{"saveAsFileAction",english ? "Ctrl+Shift+S" : "Ctrl+Shift+G"},{"highlightAction",english ? "Ctrl+H" : "Ctrl+S"},{"secondaryHighlightAction",english ? "Ctrl+Shift+H" : "Ctrl+Shift+S"},{"focusSearch",english ? "Ctrl+F" : "Ctrl+B"},{"goPage",english ? "Ctrl+G" : "Ctrl+I"},{"toggleAnnotations","Ctrl+N"},{"underlineAction","Ctrl+U"}};
        for(auto i=expectedShortcuts.cbegin();i!=expectedShortcuts.cend();++i) {
            auto action=window.findChild<QAction*>(i.key());
            require(action->property("defaultShortcut").toString()==i.value(),"shortcut defaults match interface language");
            if(i.key()!="saveFileAction" && i.key()!="focusSearch") require(action->shortcut()==QKeySequence(i.value()),"language shortcut active");
        }
        auto search=window.findChild<QAction*>("focusSearch");
        search->trigger(); app.processEvents(); require(search->isChecked(),"View search marked when opened");
        window.findChild<QAction*>("searchAction")->trigger(); app.processEvents(); require(!search->isChecked(),"View search follows toolbar toggle");
        auto thumbnailAction=window.findChild<QAction*>("showThumbnailsAction");
        thumbnailAction->trigger(); app.processEvents(); require(thumbnailAction->isChecked(),"thumbnails marked when opened");
        window.findChild<QTabWidget*>("navigationPanel")->setCurrentIndex(1); app.processEvents();
        require(!thumbnailAction->isChecked() && window.findChild<QAction*>("showBookmarksAction")->isChecked(),"panel checks follow navigation tab");
        thumbnailAction->trigger(); thumbnailAction->trigger(); app.processEvents(); require(!thumbnailAction->isChecked(),"thumbnails unmarked when closed");
#ifdef Q_OS_WIN
        if(QApplication::platformName()=="windows") {
            auto menu=GetMenu(reinterpret_cast<HWND>(window.winId())); wchar_t text[64]{};
            require(menu && GetMenuItemCount(menu)==7,"native menu count");
            require(!GetSubMenu(menu,4),"Shortcuts is a direct native button");
            require(!GetSubMenu(menu,5),"Workbench is a direct native button");
            GetMenuStringW(menu,5,text,64,MF_BYPOSITION);require(QString::fromWCharArray(text)=="Workbench","Workbench name is identical in both languages");
            GetMenuStringW(menu,6,text,64,MF_BYPOSITION);
            require(QString::fromWCharArray(text)==(english ? "&Help" : "&Ayuda"),"native Help translation");
        }
#endif
        bool aboutCorrect=false;
        QTimer::singleShot(0,[&]{
            auto dialog=window.findChild<QDialog*>("aboutGpdfDialog");
            if(dialog) {
                auto label=dialog->findChild<QLabel*>("aboutGpdfText");
                aboutCorrect=label && label->text()==QString(english ? "Grant's PDF Reader version %1\nCreated by Lautaro Grant" : "Grant's PDF Reader versión %1\nCreado por Lautaro Grant").arg(GPDF_VERSION);
                dialog->accept();
            }
        });
        window.findChild<QAction*>("aboutGpdfAction")->trigger(); require(aboutCorrect,"About dialog translation");
        QVector<PdfDocument::AnnotationEntry> entries{{0,0,FPDF_ANNOT_TEXT,"Contenido del usuario: á ñ <>&"}};
        for(const auto& extension:{"md","txt","html"}) {
            const auto path=directory.path()+"/annotations."+extension;
            require(AnnotationExport::write(path,"Documento personal",entries),"export succeeds");
            QFile file(path);require(file.open(QIODevice::ReadOnly),"export readable");
            const auto content=QString::fromUtf8(file.readAll());
            require(content.contains(english ? "Annotations" : "Anotaciones") && content.contains(english ? "Page 1" : "Página 1"),"export headings translated");
            require(content.contains(extension==QString("html") ? entries[0].text.toHtmlEscaped() : entries[0].text),"user comment preserved");
            if(extension==QString("html")) require(content.contains(english ? "lang=en" : "lang=es"),"HTML language matches interface");
        }
        QFile catalog(QString(GPDF_SOURCE_DIR)+"/resources/translations/gpdf_en.ts");
        require(catalog.open(QIODevice::ReadOnly),"catalog readable"); QXmlStreamReader xml(&catalog);
        QTranslator translator;require(translator.load(":/translations/gpdf_en.qm"),"compiled English catalog loads");
        QString source;int messages=0;
        while(!xml.atEnd()) {
            xml.readNext();if(!xml.isStartElement()) continue;
            if(xml.name()==u"source") source=xml.readElementText();
            else if(xml.name()==u"translation") {
                const auto translation=xml.readElementText();require(!translation.isEmpty(),"no empty translation");
                require(translator.translate("GPDF",source.toUtf8().constData())==translation,"compiled catalog matches source");
                for(int i=1;i<=3;++i) require(source.count("%"+QString::number(i))==translation.count("%"+QString::number(i)),"format placeholders preserved");
                ++messages;
            }
        }
        require(!xml.hasError() && messages>=250,"complete valid catalog");
        const QRegularExpression literals(R"rx(gpdfTr\("((?:[^"\\]|\\.)*)"\))rx");
        QDir sources(QString(GPDF_SOURCE_DIR)+"/src");
        for(const auto& name:sources.entryList({"*.h","*.cpp"},QDir::Files)) {
            QFile file(sources.filePath(name));require(file.open(QIODevice::ReadOnly),"source readable");
            auto matches=literals.globalMatch(QString::fromUtf8(file.readAll()));
            while(matches.hasNext()) {
                const auto source=matches.next().captured(1).replace("\\n","\n");
                require(!translator.translate("GPDF",source.toUtf8().constData()).isEmpty(),"every interface message has an English translation");
            }
        }
        if(args.size()>2) require(window.grab().save(args[2]),"language screenshot saved");
        const auto nextLanguage=english ? QString("es") : QString("en");
        bool restartNotice=false;
        QTimer::singleShot(0,[&]{ if(auto box=qobject_cast<QMessageBox*>(app.activeModalWidget())) { restartNotice=box->text()==gpdfTr("El idioma se aplicará la próxima vez que inicie GPDF."); box->accept(); } });
        auto languageAction=window.findChild<QAction*>(english ? "languageSpanishAction" : "languageEnglishAction");
#ifdef Q_OS_WIN
        if(QApplication::platformName()=="windows") SendMessageW(reinterpret_cast<HWND>(window.winId()),WM_COMMAND,languageAction->property("nativeCommand").toUInt(),0);
        else
#endif
        languageAction->trigger();
        require(restartNotice && savedUiLanguage()==nextLanguage && currentUiLanguage()==language,"menu saves choice for next startup only");
        QProcess child;child.start(app.applicationFilePath(),{"--restart",directory.path(),nextLanguage});
        require(child.waitForFinished(10000) && child.exitCode()==0,"selected language applied on restart");
        std::cout<<"PASS language "<<language.toStdString()<<": menus, tools, About, exports, catalog, shortcuts and restart\n";
        return 0;
    } catch(const std::exception& error) { std::cerr<<error.what()<<'\n';return 1; }
}
