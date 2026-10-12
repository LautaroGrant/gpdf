#include "AnnotationExport.h"
#include <QFileInfo>
#include <QSaveFile>
#include <QMap>

bool AnnotationExport::write(const QString& path,const QString& name,const QVector<PdfDocument::AnnotationEntry>& entries) {
        bool html=QFileInfo(path).suffix().compare("html",Qt::CaseInsensitive)==0; bool markdown=QFileInfo(path).suffix().compare("md",Qt::CaseInsensitive)==0;
        QString output=html ? "<!doctype html><html lang=es><meta charset=utf-8><title>Anotaciones</title><style>body{font:16px Arial;max-width:850px;margin:40px auto;padding:0 20px}article{border-top:1px solid #ddd;padding:14px 0}p{white-space:pre-wrap}</style><h1>Anotaciones: "+name.toHtmlEscaped()+"</h1>" : (markdown ? "# Anotaciones: " : "Anotaciones: ")+name+"\n\n";
        const QHash<int,QString> types={{FPDF_ANNOT_TEXT,"Comentario"},{FPDF_ANNOT_FREETEXT,"Texto"},{FPDF_ANNOT_HIGHLIGHT,"Resaltado"},{FPDF_ANNOT_INK,"Lápiz"}};
        QMap<QString,QVector<PdfDocument::AnnotationEntry>> groups;
        for(const auto& entry:entries) groups[QString("%1").arg(entry.page+1,8,10,QChar('0'))].append(entry);
        for(auto group=groups.cbegin();group!=groups.cend();++group) {
            QString groupTitle=QString("Página %1").arg(group.key().toInt());
            output+=html ? "<h2>"+groupTitle.toHtmlEscaped()+"</h2>" : (markdown ? "## " : "")+groupTitle+"\n\n";
            for(const auto& entry:group.value()) {
            QString heading=QString("Página %1 · %2").arg(entry.page+1).arg(types.value(entry.type,"Anotación")); QString text=entry.text.isEmpty() ? "[Sin comentario escrito]" : entry.text;
            output+=html ? "<article><h3>"+heading.toHtmlEscaped()+"</h3><p>"+text.toHtmlEscaped()+"</p></article>" : (markdown ? "### "+heading+"\n\n> "+QString(text).replace("\n","\n> ")+"\n\n" : heading+"\n"+text+"\n\n");
        }
        }
        if(html) output+="</html>";
        QSaveFile file(path); auto bytes=output.toUtf8(); bool ok=file.open(QIODevice::WriteOnly) && file.write(bytes)==bytes.size() && file.commit(); return ok;
}
