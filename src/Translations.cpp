#include "UiSupport.h"
#include "Translations.h"
namespace { QString activeLanguage; }
class SpanishUiFallback : public QTranslator {
public:
    using QTranslator::QTranslator;
    bool isEmpty() const override { return false; }
    QString translate(const char* context,const char* source,const char*,int) const override {
        if(qstrcmp(context,"QKeySequenceEdit")==0 && qstrcmp(source,"Press shortcut")==0) return "Presionar un atajo";
        return {};
    }
};
QString savedUiLanguage() {
    QSettings settings(QSettings::IniFormat,QSettings::UserScope,AppIdentity::settingsOrganization,AppIdentity::settingsApplication);
    return settings.value("ui/language").toString()=="en" ? "en" : "es";
}
QString currentUiLanguage() { return activeLanguage; }
bool initializeUiLanguage(const QString& language) {
    QSettings settings(QSettings::IniFormat,QSettings::UserScope,AppIdentity::settingsOrganization,AppIdentity::settingsApplication);
    if(settings.contains("ui/language")) return true;
    return saveUiLanguage(language);
}
bool saveUiLanguage(const QString& language) {
    if(language!="es" && language!="en") return false;
    QSettings settings(QSettings::IniFormat,QSettings::UserScope,AppIdentity::settingsOrganization,AppIdentity::settingsApplication);
    settings.setValue("ui/language",language); settings.sync();
    return settings.status()==QSettings::NoError;
}
void installUiTranslations() {
    if(!activeLanguage.isEmpty()) return;
    activeLanguage=savedUiLanguage();
    auto translator=new QTranslator(qApp); translator->setObjectName("gpdfUiTranslator");
    const auto catalog=activeLanguage=="en" ? ":/translations/gpdf_en.qm" : ":/translations/qtbase_es.qm";
    if(translator->load(catalog)) qApp->installTranslator(translator);
    if(activeLanguage=="es") {
        auto fallback=new SpanishUiFallback(qApp); qApp->installTranslator(fallback);
    }
}
