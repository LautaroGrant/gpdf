#pragma once
#include <QCoreApplication>
#include <QString>

// Translate interface text only; PDF content and persisted identifiers stay intact.
inline QString gpdfTr(const char* source) {
    return QCoreApplication::translate("GPDF",source);
}
void installUiTranslations();
QString currentUiLanguage();
QString savedUiLanguage();
bool saveUiLanguage(const QString& language);
bool initializeUiLanguage(const QString& language);
