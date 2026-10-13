#pragma once
#include <QCoreApplication>
#include <QDir>
#include <QSettings>
#include <QDebug>
#ifdef Q_OS_WIN
#include <windows.h>
#include <shlobj.h>
#endif

// Advertise GPDF for PDFs for this user; Windows keeps the user's default choice.
inline void registerPdfApplication() {
#ifdef Q_OS_WIN
    const auto exe=QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
    const auto command=QString("\"%1\" \"%2\"").arg(exe,QStringLiteral("%1"));
    const auto appIcon=QString("\"%1\",0").arg(exe);
    const auto pdfIcon=QString("\"%1\",1").arg(exe);
    QSettings registry("HKEY_CURRENT_USER\\Software",QSettings::NativeFormat);
    bool changed=false;
    auto set=[&](const QString& key,const QString& value) {
        if(!registry.contains(key) || registry.value(key).toString()!=value) { registry.setValue(key,value); changed=true; }
    };
    set("Classes/GPDF.PDF/.","Documento PDF de GPDF");
    set("Classes/GPDF.PDF/DefaultIcon/.",pdfIcon);
    set("Classes/GPDF.PDF/shell/open/command/.",command);
    set("Classes/.pdf/OpenWithProgids/GPDF.PDF","");
    set("Classes/Applications/GPDF.exe/FriendlyAppName","GPDF");
    set("Classes/Applications/GPDF.exe/DefaultIcon/.",pdfIcon);
    set("Classes/Applications/GPDF.exe/shell/open/command/.",command);
    set("Classes/Applications/GPDF.exe/SupportedTypes/.pdf","");
    set("GPDF/Capabilities/ApplicationName","GPDF");
    set("GPDF/Capabilities/ApplicationDescription","Lector y editor de anotaciones PDF");
    set("GPDF/Capabilities/ApplicationIcon",appIcon);
    set("GPDF/Capabilities/FileAssociations/.pdf","GPDF.PDF");
    set("RegisteredApplications/GPDF","Software\\GPDF\\Capabilities");
    registry.sync();
    if(registry.status()!=QSettings::NoError) qWarning("No se pudo registrar GPDF para abrir PDF.");
    else if(changed) SHChangeNotify(SHCNE_ASSOCCHANGED,SHCNF_IDLIST,nullptr,nullptr);
#endif
}
