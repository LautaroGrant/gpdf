#include "AnnotationExport.h"
#include <QFileInfo>
#include <QSaveFile>
#include <QMap>

bool AnnotationExport::write(const QString& path,const QString& name,const QVector<PdfDocument::AnnotationEntry>& entries) {
        bool html=QFileInfo(path).suffix().compare("html",Qt::CaseInsensitive)==0; bool markdown=QFileInfo(path).suffix().compare("md",Qt::CaseInsensitive)==0;
        const auto title=gpdfTr("Anotaciones");
        QString output=html ? "<!doctype html><html lang="+(currentUiLanguage()=="en" ? QString("en") : QString("es"))+"><meta charset=utf-8><title>"+title.toHtmlEscaped()+"</title><style>body{font:16px Arial;max-width:850px;margin:40px auto;padding:0 20px}article{border-top:1px solid #ddd;padding:14px 0}p{white-space:pre-wrap}</style><h1>"+gpdfTr("Anotaciones: ").toHtmlEscaped()+name.toHtmlEscaped()+"</h1>" : (markdown ? "# " : "")+gpdfTr("Anotaciones: ")+name+"\n\n";
        const QHash<int,QString> types={{FPDF_ANNOT_TEXT,gpdfTr("Comentario")},{FPDF_ANNOT_FREETEXT,gpdfTr("Texto")},{FPDF_ANNOT_HIGHLIGHT,gpdfTr("Resaltado")},{FPDF_ANNOT_UNDERLINE,gpdfTr("Subrayado")},{FPDF_ANNOT_INK,gpdfTr("Lápiz")}};
        QSaveFile file(path);
        if(!file.open(QIODevice::WriteOnly)) return false;
        auto flush=[&]{ auto bytes=output.toUtf8(); output.clear(); return file.write(bytes)==bytes.size(); };
        if(!flush()) return false;
        QMap<int,QVector<const PdfDocument::AnnotationEntry*>> groups;
        for(const auto& entry:entries) groups[entry.page+1].append(&entry);
        for(auto group=groups.cbegin();group!=groups.cend();++group) {
            QString groupTitle=QString(gpdfTr("Página %1")).arg(group.key());
            output+=html ? "<h2>"+groupTitle.toHtmlEscaped()+"</h2>" : (markdown ? "## " : "")+groupTitle+"\n\n";
            if(!flush()) return false;
            for(const auto* pointer:group.value()) {
            const auto& entry=*pointer;
            QString heading=QString(gpdfTr("Página %1 · %2")).arg(entry.page+1).arg(types.value(entry.type,gpdfTr("Anotación"))); QString text=entry.text.isEmpty() ? gpdfTr("[Sin comentario escrito]") : entry.text;
            output+=html ? "<article><h3>"+heading.toHtmlEscaped()+"</h3><p>"+text.toHtmlEscaped()+"</p></article>" : (markdown ? "### "+heading+"\n\n> "+QString(text).replace("\n","\n> ")+"\n\n" : heading+"\n"+text+"\n\n");
            if(!flush()) return false;
        }
        }
        if(html) output+="</html>";
        return flush() && file.commit();
}
