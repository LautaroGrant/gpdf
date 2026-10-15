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
            return 0;
        }
        QTemporaryDir directory; require(directory.isValid(),"isolated settings directory");
        QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,directory.path());
        const QString language=args.value(1,"es"); const bool english=language=="en";
        require(!saveUiLanguage("invalid"),"unsupported language rejected");
        require(initializeUiLanguage(language),"installer initializes language");
        require(initializeUiLanguage(english ? "es" : "en") && savedUiLanguage()==language,"upgrade preserves preferred language");
        QSettings settings(QSettings::IniFormat,QSettings::UserScope,AppIdentity::settingsOrganization,AppIdentity::settingsApplication);
        settings.setValue("shortcuts/focusSearch","Ctrl+Alt+F");
        settings.setValue("shortcuts/toggleAnnotations","Ctrl+A");
        settings.setValue("shortcuts/underlineAction","");
        settings.setValue("shortcuts/viewRotation1","Ctrl+R");
        settings.setValue("shortcuts/rotateClockwiseAction","R");
        settings.setValue("shortcuts/saveFileAction","Ctrl+Alt+G"); settings.sync();
        Window window; window.show(); app.processEvents();
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
            require(menu && GetMenuItemCount(menu)==6,"native menu count");
            GetMenuStringW(menu,5,text,64,MF_BYPOSITION);
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
