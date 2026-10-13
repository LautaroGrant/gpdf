#include "UiSupport.h"
class SpanishUiFallback : public QTranslator {
public:
    using QTranslator::QTranslator;
    bool isEmpty() const override { return false; }
    QString translate(const char* context,const char* source,const char*,int) const override {
        if(qstrcmp(context,"QKeySequenceEdit")==0 && qstrcmp(source,"Press shortcut")==0) return "Presionar un atajo";
        return {};
    }
};
void installSpanishTranslations() {
    if(qApp->findChild<QTranslator*>("gpdfSpanishTranslator")) return;
    auto translator=new QTranslator(qApp); translator->setObjectName("gpdfSpanishTranslator");
    if(translator->load(":/translations/qtbase_es.qm")) qApp->installTranslator(translator);
    auto fallback=new SpanishUiFallback(qApp); qApp->installTranslator(fallback);
}
