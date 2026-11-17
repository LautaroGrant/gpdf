#pragma once
#include "UiSupport.h"
#include "Translations.h"

inline void showWelcome(QWidget* parent) {
    auto dialog=new QDialog(parent);
    dialog->setObjectName("welcomeDialog");
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    const bool english=currentUiLanguage()=="en";
    dialog->setWindowTitle(english ? "Welcome to GPDF" : "Bienvenido a GPDF");
    dialog->setWindowFlag(Qt::WindowContextHelpButtonHint,false);
    auto layout=new QVBoxLayout(dialog);layout->setContentsMargins(24,24,24,20);layout->setSpacing(20);
    const auto body=english ? QStringLiteral(
        "This program offers three main modes:\n\n"
        "1- Normal mode: a PDF reader with a UI focused on basic annotation and simplified editing for everyday work or study.\n\n"
        "2- Professional mode: minimizes the UI as much as possible, showing only open documents (similar to programs such as EMACS, Vim, etc.). All normal-mode features remain available. Switch to this mode from the View menu or with Ctrl + Shift + P.\n\n"
        "3- Workbench: a workspace alongside the reader with tools for working with documents, such as merging or splitting PDFs, extracting or removing pages, etc.") : QStringLiteral(
        "Este programa presenta tres modos de trabajo:\n\n"
        "1- Modo normal: un lector de PDF con UI enfocada en funcionalidades básicas de anotación y edición. Simplificada para el trabajo o estudio diario.\n\n"
        "2- Modo profesional: minimiza la UI lo máximo posible (similar a programas como EMACS, Vim, etc) y solo muestra los documentos abiertos. Todas las funcionalidades del modo normal permanecen. Es posible alternar a este modo mediante la opción en la pestaña \"Ver\" o mediante el atajo Ctrl + Shift + P.\n\n"
        "3- Workbench: una mesa de trabajo paralela al lector con diferentes funcionalidades para trabajar con documentos como unir o separar PDFs, extraer o eliminar páginas, etc.");
    QString html;
    for(const auto& paragraph:body.split(QStringLiteral("\n\n")))
        html+=QStringLiteral("<p align=\"justify\">%1</p>").arg(paragraph.toHtmlEscaped());
    auto message=new QLabel;
    message->setTextFormat(Qt::RichText);
    message->setText(html);
    message->setWordWrap(true);
    message->setMinimumWidth(560);
    layout->addWidget(message);
    auto buttons=new QDialogButtonBox(QDialogButtonBox::Ok);layout->addWidget(buttons);
    QObject::connect(buttons,&QDialogButtonBox::accepted,dialog,&QDialog::accept);
    dialog->open();
}

inline void showWelcomeOnce(QWidget* parent) {
    QSettings settings(QSettings::IniFormat,QSettings::UserScope,AppIdentity::settingsOrganization,AppIdentity::settingsApplication);
    if(settings.value("onboarding/generalModesShown",false).toBool())return;
    showWelcome(parent);
    settings.setValue("onboarding/generalModesShown",true);
    settings.sync();
}
