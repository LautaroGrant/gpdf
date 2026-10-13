#include "PdfDocument.h"
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRandomGenerator>
#include <QTemporaryDir>
#include <iostream>
#include <limits>
#include <stdexcept>

int main(int argc, char** argv) {
    QApplication app(argc,argv);
    try {
        PdfDocument pdf;
        auto require=[&](bool ok,const char* step){ if(!ok) throw std::runtime_error(std::string(step)+": "+pdf.error().toStdString()); };
        if(app.arguments().size()==4 && app.arguments()[1]=="--highlight-probe") {
            require(pdf.open(app.arguments()[2]),"open highlight probe PDF");
            auto text=pdf.pageText(0); int first=text.indexOf("Analista de siniestros con"),last=text.indexOf("traduzco",first)+6;
            require(first>=0 && last>first,"find probe line"); int pieces=pdf.selectionRects(first,last).size();
            require(pdf.highlight(first,last),"highlight probe line"); auto copy=pdf.copyAnnotation(pdf.annotationCount()-1);
            std::cout<<"Original line rectangles: "<<pieces<<"; highlight quadrilaterals: "<<copy.quads.size()<<"\n";
            require(copy.quads.size()==1,"fragmented real PDF line has continuous highlight");
            auto bounds=copy.rect; require(pdf.render(1.75).save(app.arguments()[3]),"save highlight visual probe");
            require(pdf.undo() && pdf.redo(),"probe undo redo"); return 0;
        }
        QTemporaryDir temporary;
        require(temporary.isValid(),"temporary directory");
        QString directory=argc>1 ? QString::fromLocal8Bit(argv[1]) : temporary.path();
        QDir().mkpath(directory);
        require(pdf.createSample(),"create sample");
        require(pdf.saveCopy(directory+"/sample.pdf"),"save sample");
        require(pdf.open(directory+"/sample.pdf"),"open sample");
        auto baseline=pdf.render(1.0);
        require(!baseline.isNull(),"render");
        require(!pdf.selectionText(0,8).isEmpty(),"extract text");
        require(!pdf.selectionRects(0,8).isEmpty(),"selection geometry");
        auto searchMatches=pdf.searchPage(0,"sTuDy");
        require(searchMatches.size()==1 && searchMatches.first().first==0 && searchMatches.first().count==5 && !searchMatches.first().rectangles.isEmpty(),"case-insensitive search with geometry");
        require(pdf.searchPage(0,"missing phrase").isEmpty() && pdf.searchPage(0,"").isEmpty(),"no matches and empty query");
        require(pdf.pageIndex()==0 && !pdf.dirty(),"search preserves page and document state");
        {
            PdfDocument words; require(words.createSample(1,"Cortázar's examen, 2026."),"Unicode word selection fixture");
            auto word=words.wordRange(3); require(words.selectionText(word.first,word.second)=="Cortázar's","word selection preserves accents and internal apostrophe");
            auto punctuation=words.wordRange(17); require(words.selectionText(punctuation.first,punctuation.second)==",","punctuation is separate from the word");
            require(words.wordRange(-1)==QPair<int,int>(-1,-1) && words.lineRange(words.characterCount())==QPair<int,int>(-1,-1),"out of range selection is empty");
        }
        auto textBounds=pdf.selectionRects(0,43).first();
        auto beforeSpace=pdf.selectionRects(4,4).first();
        auto afterSpace=pdf.selectionRects(6,6).first();
        QPointF space((beforeSpace.right()+afterSpace.left())/2,textBounds.center().y());
        require(pdf.textAt(0,space),"hover recognizes inter-word whitespace");
        require(pdf.textAt(0,{textBounds.left()-2,textBounds.center().y()},3),"hover includes small margin");
        require(!pdf.textAt(0,{textBounds.left()-15,textBounds.center().y()},3),"hover excludes distant whitespace");
        require(pdf.highlight(0,8),"highlight");
        auto highlighted=pdf.render(1.0);
        require(highlighted!=baseline,"highlight must be visible");
        require(pdf.note({60,680},QString::fromUtf8("Comentario: estudiar esta sección.")),"note");
        auto noted=pdf.render(1.0);
        require(noted!=highlighted,"note icon must be visible");
        require(pdf.freeText({60,620},QString::fromUtf8("Repasar: acción y explicación.")),"free text");
        auto written=pdf.render(1.0);
        require(written!=noted,"free text must be visible");
        require(pdf.ink({{60,550},{100,530},{140,555},{180,535}}),"ink");
        require(pdf.annotationCount()==4,"annotation count");
        auto before=pdf.render(1.0);
        require(before!=written,"ink must be visible");
        require(before!=baseline,"annotations must be visible");
        require(pdf.saveCopy(directory+"/annotated.pdf"),"save annotated");
        require(pdf.open(directory+"/annotated.pdf"),"reopen annotated");
        require(pdf.annotationCount()==4,"persisted annotation count");
        require(pdf.annotationContents(1)==QString::fromUtf8("Comentario: estudiar esta sección."),"persisted Unicode comment");
        require(pdf.annotationContents(2)==QString::fromUtf8("Repasar: acción y explicación."),"persisted free text");
        auto after=pdf.render(1.0);
        require(before==after,"identical render after reopening");
        after.save(directory+"/annotated.png");
        require(!pdf.highlight(-1,-1),"invalid selection rejected");
        require(!pdf.setPage(99),"invalid page rejected");
        require(!pdf.open(directory+"/missing.pdf"),"missing file rejected");
        require(pdf.annotationCount()==4,"failed open preserves current document");
        require(pdf.canEditAnnotation(1) && pdf.canEditAnnotation(2),"editable note and text");
        require(pdf.canColorAnnotation(0) && pdf.canColorAnnotation(3),"colorable highlight and ink");
        require(!pdf.canColorAnnotation(2),"free text has its own appearance");
        require(!pdf.editAnnotation(-1,"invalid") && !pdf.removeAnnotation(99),"invalid annotation index rejected");
        require(!pdf.editAnnotation(2,"  "),"empty edit rejected");
        require(!pdf.editAnnotation(2,QString("Texto largo ").repeated(2000)),"oversized edit rejected");
        require(pdf.annotationCount()==4 && pdf.render(1.0)==after && !pdf.dirty(),"failed edits preserve document and dirty state");
        const QString revisedNote=QString::fromUtf8("Comentario editado: repasar mañana.");
        const QString revisedText=QString::fromUtf8("Texto editado: síntesis.\nSegunda línea con acentos.");
        require(pdf.editAnnotation(1,revisedNote),"edit comment");
        require(pdf.editAnnotation(2,revisedText),"edit free text");
        auto edited=pdf.render(1.0);
        require(edited!=after && pdf.annotationCount()==4 && pdf.dirty(),"editing updates appearance without duplicates");
        require(pdf.recolorAnnotation(0,QColor("#49ba75")),"recolor highlight");
        auto recolored=pdf.render(1.0);
        require(recolored!=edited,"highlight color visibly changes");
        require(pdf.recolorAnnotation(3,QColor("#ce3a68")),"recolor ink");
        edited=pdf.render(1.0);
        require(edited!=recolored,"ink color visibly changes");
        require(pdf.saveCopy(directory+"/edited.pdf") && pdf.open(directory+"/edited.pdf"),"save and reopen edits");
        require(pdf.annotationContents(1)==revisedNote && pdf.annotationContents(2)==revisedText,"edited Unicode text persists");
        auto closeColor=[](QColor actual,QColor expected){ return actual.isValid() && std::abs(actual.red()-expected.red())<=1 && std::abs(actual.green()-expected.green())<=1 && std::abs(actual.blue()-expected.blue())<=1; };
        require(closeColor(pdf.annotationColor(0),QColor("#49ba75")) && closeColor(pdf.annotationColor(3),QColor("#ce3a68")),"edited colors persist");
        require(pdf.render(1.0)==edited,"edited appearance persists");
        edited.save(directory+"/edited.png");
        require(pdf.removeAnnotation(1),"delete middle annotation");
        require(pdf.annotationCount()==3 && pdf.annotationType(1)==FPDF_ANNOT_FREETEXT && pdf.annotationContents(1)==revisedText,"deletion preserves neighbors and updates indices");
        auto deleted=pdf.render(1.0);
        require(deleted!=edited,"deleted note disappears");
        require(pdf.saveCopy(directory+"/deleted.pdf") && pdf.open(directory+"/deleted.pdf"),"save and reopen deletion");
        require(pdf.annotationCount()==3 && pdf.render(1.0)==deleted,"deletion persists");
        require(pdf.removeAnnotation(1),"delete free text");
        require(pdf.removeAnnotation(0),"delete highlight");
        require(pdf.removeAnnotation(0),"delete ink");
        require(pdf.annotationCount()==0 && pdf.render(1.0)==baseline,"all types removed, original content intact");
        require(pdf.saveCopy(directory+"/clean.pdf") && pdf.open(directory+"/clean.pdf"),"save and reopen clean document");
        require(pdf.annotationCount()==0 && pdf.render(1.0)==baseline,"clean document persists");
        require(pdf.createSample(1,QString::fromUtf8("Cortázar CORTAZAR acción pingüino")),"create accented search sample");
        require(pdf.saveCopy(directory+"/accents.pdf") && pdf.open(directory+"/accents.pdf"),"reopen accented sample");
        auto accented=pdf.searchPage(0,"cortazar");
        require(accented.size()==2 && pdf.searchPage(0,QString::fromUtf8("Cortázar")).size()==2,"search ignores accents in both directions");
        require(pdf.selectionText(accented[0].first,accented[0].first+accented[0].count-1)==QString::fromUtf8("Cortázar"),"accented match maps to original PDF text");
        require(!accented[0].rectangles.isEmpty() && !accented[1].rectangles.isEmpty(),"accented results have geometry");
        require(pdf.searchPage(0,"accion").size()==1 && pdf.searchPage(0,"pinguino").size()==1,"accent and diaeresis ignored");
        require(pdf.createSample(),"styled text fixture");
        require(pdf.freeText({60,620},"Formato",{"Segoe UI",26,QColor("#cc2244")}),"create styled text");
        auto styled=pdf.render(1.0);
        require(pdf.saveCopy(directory+"/styled.pdf") && pdf.open(directory+"/styled.pdf"),"reopen styled text");
        require(pdf.render(1.0)==styled,"font size and color persist");
        require(pdf.editAnnotation(0,"Formato editado"),"edit styled text");
        auto styledEdited=pdf.render(1.0);
        require(pdf.removeAnnotation(0) && pdf.freeText({60,620},"Formato editado",{"Segoe UI",26,QColor("#cc2244")}),"recreate equivalent styled text");
        require(pdf.render(1.0)==styledEdited,"editing retains original font size and color");
        require(pdf.createSample(2),"history and document annotation fixture");
        require(pdf.freeText({60,620},"Movible",{"Segoe UI",20,Qt::red}),"movable text fixture");
        require(pdf.saveCopy(directory+"/movable.pdf") && pdf.open(directory+"/movable.pdf"),"reopen movable text fixture");
        auto originalRect=pdf.annotationRect(0); auto originalRender=pdf.render(1.0);
        require(pdf.moveTextAnnotation(0,{120,550}),"move text annotation");
        auto movedRender=pdf.render(1.0);
        require(pdf.annotationRect(0).left()==120 && movedRender!=originalRender,"move changes geometry and appearance");
        require(pdf.undo() && pdf.render(1.0)==originalRender,"undo restores text position");
        require(pdf.redo() && pdf.render(1.0)==movedRender,"redo restores text move");
        auto copied=pdf.copyAnnotation(0);
        require(pdf.setPage(1) && pdf.pasteAnnotation(copied,{60,620}),"paste annotation on another page");
        auto entries=pdf.documentAnnotations();
        require(entries.size()==2 && entries[0].page==0 && entries[1].page==1 && pdf.pageIndex()==1,"all-document annotation enumeration preserves page");
        require(pdf.undo() && pdf.annotationCount()==0 && pdf.redo() && pdf.annotationCount()==1,"paste supports undo and redo");
        require(pdf.saveCopy(directory+"/moved.pdf") && pdf.open(directory+"/moved.pdf"),"save and reopen moved and copied text");
        require(pdf.annotationRect(0).left()==120 && pdf.documentAnnotations().size()==2,"move and paste persist");
        require(pdf.createSample(),"drawing styles fixture");
        require(pdf.highlight(0,8,QColor(20,180,70,80)),"custom highlight style");
        require(pdf.ink({{60,500},{160,500}},QColor(220,30,30,150),6),"custom ink style");
        require(pdf.copyAnnotation(1).strokeWidth==6 && pdf.annotationColor(1).alpha()==150,"width and opacity stored");
        require(pdf.formatDrawing(1,QColor(30,40,220,120),9),"edit ink style");
        require(pdf.copyAnnotation(1).strokeWidth==9 && pdf.annotationColor(1).alpha()==120,"edit updates width and alpha");
        require(pdf.drawingAt({100,500})==1 && pdf.drawingAt({100,550})==-1,"eraser tests actual stroke geometry");
        require(pdf.saveCopy(directory+"/drawing-styles.pdf") && pdf.open(directory+"/drawing-styles.pdf"),"drawing style roundtrip");
        require(pdf.copyAnnotation(1).strokeWidth==9 && pdf.annotationColor(1).alpha()==120,"drawing style persists");
        require(pdf.saveRecovery(directory+"/recovery.pdf"),"write recovery copy");
        require(pdf.note({50,650},"Only recovery") && pdf.saveRecovery(directory+"/recovery.pdf"),"update recovery without saving source");
        PdfDocument recovery; require(recovery.open(directory+"/recovery.pdf"),"recovery can be reopened"); recovery.markRecovered(); require(recovery.dirty() && recovery.annotationCount()==3,"recovered copy marked pending");
        require(pdf.createSample(2),"invalid render scale fixture");
        for(double scale:{0.0,-1.0,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()})
            require(pdf.renderPage(1,scale).isNull(),"invalid render scale rejected");
        require(!pdf.renderPage(1,1).isNull() && pdf.pageIndex()==0,"valid noncurrent render after invalid requests");
        QByteArray payload(8*1024*1024,Qt::Uninitialized); QRandomGenerator random(42);
        for(qsizetype i=0;i<payload.size();++i) payload[i]=char(random.generate()&255);
        QByteArray historyBaseBytes="%PDF-1.7\n"; QVector<qsizetype> offsets{0};
        auto object=[&](int id,const QByteArray& value){ offsets.append(historyBaseBytes.size()); historyBaseBytes+=QByteArray::number(id)+" 0 obj\n"+value+"\nendobj\n"; };
        object(1,"<< /Type /Catalog /Pages 2 0 R /Names << /EmbeddedFiles << /Names [(data.bin) 5 0 R] >> >> >>");
        object(2,"<< /Type /Pages /Kids [3 0 R 6 0 R] /Count 2 >>"); object(3,"<< /Type /Page /Parent 2 0 R /MediaBox [0 0 595 842] /Resources <<>> >>");
        object(4,"<< /Type /EmbeddedFile /Length "+QByteArray::number(payload.size())+" >>\nstream\n"+payload+"\nendstream"); object(5,"<< /Type /Filespec /F (data.bin) /EF << /F 4 0 R >> >>"); object(6,"<< /Type /Page /Parent 2 0 R /MediaBox [0 0 595 842] /Resources <<>> >>");
        qsizetype xref=historyBaseBytes.size(); historyBaseBytes+="xref\n0 7\n0000000000 65535 f \n"; for(int i=1;i<offsets.size();++i) historyBaseBytes+=QByteArray::number(offsets[i]).rightJustified(10,'0')+" 00000 n \n";
        historyBaseBytes+="trailer\n<< /Size 7 /Root 1 0 R >>\nstartxref\n"+QByteArray::number(xref)+"\n%%EOF\n";
        QFile padded(directory+"/history-base.pdf"); require(padded.open(QIODevice::WriteOnly) && padded.write(historyBaseBytes)==historyBaseBytes.size(),"write PDF with large embedded payload"); padded.close(); require(pdf.open(directory+"/history-base.pdf"),"open large history baseline");
        for(int i=0;i<10;++i) require(pdf.note({50.0+i*10,650},QString("Comentario %1").arg(i)),"incremental annotation history");
        require(pdf.historyStorageBytes()<historyBaseBytes.size()*3,"history shares PDF base instead of storing ten full copies");
        std::cout<<"History storage: "<<pdf.historyStorageBytes()<<" bytes; PDF base: "<<historyBaseBytes.size()<<" bytes.\n";
        require(pdf.undo() && pdf.annotationCount()==9 && pdf.redo() && pdf.annotationCount()==10,"incremental undo and redo preserve annotations");
        require(pdf.editAnnotation(0,"Comentario editado"),"edit incremental history");
        require(pdf.saveCopy(directory+"/history-edited.pdf") && !pdf.dirty(),"saved history state marked clean");
        require(pdf.undo() && pdf.dirty() && pdf.redo() && !pdf.dirty(),"saved marker survives undo and redo");
        require(pdf.open(directory+"/history-edited.pdf") && pdf.documentAnnotations().first().text=="Comentario editado","edited contents persist in PDF");
        require(pdf.freeText({100,500},"Texto para copiar"),"free text created");
        auto textCopy=pdf.copyAnnotation(10); require(pdf.setPage(1) && pdf.pasteAnnotation(textCopy,{100,600}) && pdf.annotationContents(0)=="Texto para copiar","copied free text preserves contents");
        require(pdf.undo() && pdf.annotationCount()==0 && pdf.redo() && pdf.annotationContents(0)=="Texto para copiar","paste is a single undo operation");
        QString encrypted=QFileInfo(QString::fromUtf8(__FILE__)).absolutePath()+"/data/recovery-encrypted.pdf";
        require(pdf.open(encrypted,"audit-test"),"open encrypted incremental history"); auto originalContents=pdf.annotationContents(0);
        require(pdf.editAnnotation(0,"Privado"),"encrypted edit");
        require(pdf.undo() && pdf.annotationContents(0)==originalContents && !pdf.dirty() && pdf.redo() && pdf.annotationContents(0)=="Privado","encrypted edit undo and redo");
        require(pdf.saveRecovery(directory+"/encrypted-edited.pdf") && pdf.open(directory+"/encrypted-edited.pdf","audit-test") && pdf.annotationContents(0)=="Privado","encrypted recovery preserves edits");
        {
            PdfDocument cached,other;
            require(cached.createSample(3),"annotation cache fixture");
            require(cached.setPage(2) && cached.note({50,650},"Otra página"),"annotation on last page");
            require(cached.documentAnnotations().size()==1,"populate document annotation cache");
            require(cached.setPage(0) && cached.note({50,650},"Página actual"),"add after cache population");
            require(cached.documentAnnotations().size()==2 && cached.pageAnnotations(2).size()==1,"cache updates changed page and preserves other pages");
            require(cached.editAnnotation(0,"Editado"),"edit cached annotation");
            auto entries=cached.documentAnnotations(); require(entries.first().text=="Editado","document cache exposes edited content");
            require(cached.removeAnnotation(0) && cached.documentAnnotations().size()==1,"cache drops removed annotation");
            require(cached.undo() && cached.documentAnnotations().size()==2 && cached.redo() && cached.documentAnnotations().size()==1,"cache rebuilt after undo and redo");
            require(other.createSample() && other.note({50,650},"Otro documento"),"second annotation cache fixture");
            other.documentAnnotations(); cached.swap(other);
            require(cached.documentAnnotations().first().text=="Otro documento" && other.documentAnnotations().first().page==2,"annotation caches follow document swap");
        }
        {
            QByteArray content="BT /F1 12 Tf 1 0 0 1 50 700 Tm (Alpha) Tj /F2 12 Tf 1 0 0 1 86 700.3 Tm (Beta) Tj /F1 12 Tf 1 0 0 1 116 700 Tm (Gamma) Tj 1 0 0 1 350 700 Tm (Column) Tj 1 0 0 1 50 650 Tm (Second line) Tj ET";
            QVector<QByteArray> objects={"<< /Type /Catalog /Pages 2 0 R >>","<< /Type /Pages /Kids [3 0 R] /Count 1 >>","<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Resources << /Font << /F1 4 0 R /F2 5 0 R >> >> /Contents 6 0 R >>","<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>","<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica-Bold >>","<< /Length "+QByteArray::number(content.size())+" >>\nstream\n"+content+"\nendstream"};
            QByteArray bytes="%PDF-1.7\n"; QVector<int> offsets;
            for(int i=0;i<objects.size();++i) { offsets.append(bytes.size()); bytes+=QByteArray::number(i+1)+" 0 obj\n"+objects[i]+"\nendobj\n"; }
            int xref=bytes.size(); bytes+="xref\n0 7\n0000000000 65535 f \n"; for(int offset:offsets) bytes+=QByteArray::number(offset).rightJustified(10,'0')+" 00000 n \n";
            bytes+="trailer\n<< /Size 7 /Root 1 0 R >>\nstartxref\n"+QByteArray::number(xref)+"\n%%EOF\n";
            QFile fixture(directory+"/fragmented-lines.pdf"); require(fixture.open(QIODevice::WriteOnly) && fixture.write(bytes)==bytes.size(),"write fragmented text fixture"); fixture.close();
            PdfDocument geometry; require(geometry.open(fixture.fileName()),"open fragmented text fixture");
            int last=geometry.characterCount()-1; require(geometry.selectionRects(0,last).size()>3,"PDFium splits font runs in fixture");
            require(geometry.highlight(0,last),"highlight fragmented text"); auto copy=geometry.copyAnnotation(0);
            require(copy.quads.size()==3,"highlight merges adjacent runs but keeps columns and lines separate");
            require(copy.color.alpha()==153,"default highlight alpha is sixty percent");
            require(geometry.saveCopy(directory+"/fragmented-highlighted.pdf") && geometry.open(directory+"/fragmented-highlighted.pdf") && geometry.copyAnnotation(0).quads.size()==3,"continuous highlight geometry persists");
        }
        {
            PdfDocument indexed,other;
            require(indexed.createSample(3,QString::fromUtf8("Cortázar ALPHA")),"search index fixture");
            auto initial=indexed.searchPage(0,"cortazar");
            auto warm=indexed.searchPage(0,"CORTAZAR");
            require(initial.size()==1 && warm.size()==1 && initial[0].first==warm[0].first && initial[0].count==warm[0].count && initial[0].rectangles==warm[0].rectangles && initial[0].contextHtml==warm[0].contextHtml,"cached search preserves accent folding and exact result geometry");
            require(indexed.searchPage(0,"missing").isEmpty(),"cached negative search");
            require(indexed.searchCacheStorageBytes()>0 && indexed.searchCacheStorageBytes()<=16LL*1024*1024,"search cache has bounded accounted storage");
            require(indexed.note({60,600},"Nota") && indexed.searchPage(0,"alpha").size()==1,"edited page search cache invalidated safely");
            require(indexed.undo() && indexed.searchPage(0,"alpha").size()==1 && indexed.redo() && indexed.searchPage(0,"alpha").size()==1,"search remains correct after undo and redo");
            require(other.createSample(1,"OMEGA") && other.searchPage(0,"omega").size()==1,"second search index fixture");
            indexed.swap(other);
            require(indexed.searchPage(0,"omega").size()==1 && indexed.searchPage(0,"alpha").isEmpty() && other.searchPage(0,"alpha").size()==1 && other.searchPage(0,"omega").isEmpty(),"search indices follow document swaps");
            require(indexed.render(std::numeric_limits<double>::quiet_NaN()).isNull() && indexed.render(std::numeric_limits<double>::infinity()).isNull() && indexed.render(-1).isNull(),"invalid direct rendering scales rejected");
            auto revision=other.revision(); auto count=other.annotationCount(); auto dirty=other.dirty();
            QFile empty(directory+"/no-pages.pdf"); require(empty.open(QIODevice::WriteOnly),"empty PDF fixture");
            empty.write("%PDF-1.4\n1 0 obj << /Type /Catalog /Pages 2 0 R >> endobj\n2 0 obj << /Type /Pages /Count 0 /Kids [] >> endobj\ntrailer << /Root 1 0 R >>\n%%EOF\n"); empty.close();
            require(!other.open(empty.fileName()) && other.revision()==revision && other.annotationCount()==count && other.dirty()==dirty && other.canUndo(),"failed no-page open preserves document, dirty state and history");
            require(indexed.createSample(1,"GAMMA") && indexed.searchPage(0,"gamma").size()==1 && indexed.searchPage(0,"omega").isEmpty(),"new document drops old search index");
        }
        {
            PdfDocument protectedPdf;
            QString restricted=QFileInfo(QString::fromUtf8(__FILE__)).absolutePath()+"/data/annotations-restricted.pdf";
            require(protectedPdf.open(restricted,"read-only"),"open restricted PDF with reader password");
            auto revision=protectedPdf.revision();
            PdfDocument::AnnotationCopy copied; copied.type=FPDF_ANNOT_TEXT; copied.rect=QRectF(50,600,20,20); copied.text="Copiada";
            require(!protectedPdf.note({50,600},"Nota") && !protectedPdf.freeText({50,500},"Texto") && !protectedPdf.ink({{50,400},{100,400}}) && !protectedPdf.highlight(0,2) && !protectedPdf.pasteAnnotation(copied,{50,500}),"all annotation creation paths respect document permissions");
            require(protectedPdf.annotationCount()==0 && !protectedPdf.dirty() && protectedPdf.revision()==revision && !protectedPdf.canUndo(),"rejected protected edits leave document and undo history unchanged");
            require(protectedPdf.open(restricted,"owner-test") && protectedPdf.note({50,600},"Permitida"),"owner password permits annotations");
            require(protectedPdf.saveCopy(directory+"/restricted-note.pdf") && protectedPdf.open(directory+"/restricted-note.pdf","read-only") && !protectedPdf.canEditAnnotation(0) && !protectedPdf.removeAnnotation(0),"restricted existing annotation remains protected after save");
        }
        {
            PdfDocument stress;
            require(stress.createSample(8,QString::fromUtf8("ALPHA Cortázar stress")),"stress fixture");
            for(int step=0;step<80;++step) {
                require(stress.setPage(step%8),"stress page change");
                int before=stress.annotationCount();
                QString text=QString("Cambio %1").arg(step);
                require(stress.note({50.0+(step%10)*20,650},text),"stress add");
                if(step%5==0) require(stress.undo() && stress.annotationCount()==before && stress.redo() && stress.annotationContents(before)==text,"stress undo redo matches exact contents");
                if(step%7==0) require(stress.removeAnnotation(0) && stress.undo() && stress.annotationCount()==before+1 && stress.redo() && stress.annotationCount()==before,"stress delete undo redo");
                if(step%10==0) {
                    auto entries=stress.documentAnnotations(); require(stress.saveCopy(directory+"/stress.pdf"),"stress save");
                    PdfDocument reopened; require(reopened.open(directory+"/stress.pdf"),"stress reopen");
                    auto actual=reopened.documentAnnotations(); require(actual.size()==entries.size(),"stress document count persists");
                    for(int i=0;i<entries.size();++i) require(actual[i].page==entries[i].page && actual[i].index==entries[i].index && actual[i].text==entries[i].text,"stress annotation pages and contents persist exactly");
                    for(int page=0;page<8;++page) require(stress.searchPage(page,"cortazar").size()==1,"stress accent-insensitive search remains correct");
                }
            }
            require(stress.historyStorageBytes()<64LL*1024*1024,"stress history memory remains bounded");
            auto revision=stress.revision(); auto contents=stress.documentAnnotations(); auto dirty=stress.dirty();
            for(int variant=0;variant<16;++variant) {
                QFile broken(directory+"/malformed.pdf"); require(broken.open(QIODevice::WriteOnly),"malformed fixture"); broken.write(QByteArray(variant*73+1,char(variant+1))); broken.close();
                require(!stress.open(broken.fileName()) && stress.revision()==revision && stress.dirty()==dirty && stress.documentAnnotations().size()==contents.size(),"malformed input does not replace active document");
            }
            require(stress.canUndo() && stress.undo(),"stress history survives malformed opens");
            std::cout<<"PASS: 80 edits across eight pages, undo redo, save reopen and malformed files.\n";
        }
        {
            PdfDocument damaged;
            auto file=QFileInfo(QString::fromUtf8(__FILE__)).absolutePath()+"/data/invalid-last-page.pdf";
            require(damaged.open(file) && damaged.pageCount()==2,"partially damaged PDF fixture");
            require(damaged.note({50,600},"Conservar"),"annotation on good page of damaged PDF");
            auto image=damaged.render(1.0); auto revision=damaged.revision();
            require(!damaged.setPage(1) && damaged.pageIndex()==0 && damaged.annotationContents(0)=="Conservar" && damaged.render(1.0)==image && damaged.revision()==revision,"failed page load preserves active page, rendering and annotations");
            require(damaged.setPage(0) && damaged.undo() && damaged.annotationCount()==0,"same-page navigation and history remain usable after failed load");
        }
        {
            PdfDocument moved; require(moved.createSample(3) && moved.note({50,650},QString::fromUtf8("Comentario á\nsegunda línea")),"comment move fixture");
            auto text=moved.annotationContents(0); auto color=moved.annotationColor(0); moved.documentAnnotations();
            require(moved.moveCommentAnnotation(0,0,0,{100,600})==0 && moved.annotationRect(0).left()==100,"comment moves within a page");
            require(moved.moveCommentAnnotation(0,0,2,{120,550})==0 && moved.pageIndex()==2 && moved.annotationContents(0)==text && moved.annotationColor(0)==color,"comment moves between pages preserving contents and color");
            auto entries=moved.documentAnnotations(); require(entries.size()==1 && entries[0].page==2,"source and destination annotation caches update");
            require(moved.undo() && moved.setPage(0) && moved.annotationCount()==1 && moved.annotationRect(0).left()==100 && moved.setPage(2) && moved.annotationCount()==0,"cross-page move is one undo step");
            require(moved.redo() && moved.setPage(2) && moved.annotationContents(0)==text,"redo restores destination comment");
            auto revision=moved.revision(); require(moved.moveCommentAnnotation(2,0,8,{0,0})<0 && moved.revision()==revision && moved.annotationCount()==1,"invalid move leaves comment and history intact");
            require(moved.saveCopy(directory+"/moved-comment.pdf"),"save moved comment"); PdfDocument reopened;
            require(reopened.open(directory+"/moved-comment.pdf") && reopened.setPage(0) && reopened.annotationCount()==0 && reopened.setPage(2) && reopened.annotationContents(0)==text,"moved comment persists on correct page");
            auto locked=reopened.copyAnnotation(0); locked.flags|=FPDF_ANNOT_FLAG_LOCKED;
            require(reopened.pasteAnnotation(locked,{200,500}),"locked comment fixture"); revision=reopened.revision();
            require(reopened.moveCommentAnnotation(2,1,0,{50,600})<0 && reopened.revision()==revision && reopened.pageIndex()==2 && reopened.annotationCount()==2,"locked comment transfer is rejected without changing history");
        }
        {
            PdfDocument markup; require(markup.createSample(2,"Underline thickness and highlight"),"markup fixture");
            auto bluePixels=[](const QImage& image){ int count=0; for(int y=0;y<image.height();++y) for(int x=0;x<image.width();++x) { auto c=image.pixelColor(x,y); if(c.blue()>c.red()+60 && c.blue()>c.green()+40) ++count; } return count; };
            require(markup.underline(0,8,QColor("#2855b4"),0.5),"create standard underline");
            require(markup.annotationType(0)==FPDF_ANNOT_UNDERLINE && markup.annotationContents(0).isEmpty(),"underline is text markup without a comment body");
            auto thin=markup.render(2); require(bluePixels(thin)>0,"underline has a visible appearance");
            auto textBottom=markup.copyAnnotation(0).quads[0].y3;
            double highestBlue=-1;
            for(int y=0;y<thin.height();++y) for(int x=0;x<thin.width();++x) { auto c=thin.pixelColor(x,y); if(c.blue()>c.red()+60 && c.blue()>c.green()+40) highestBlue=std::max(highestBlue,markup.pageSize().height()-y/2.0); }
            require(highestBlue<textBottom-0.25,"underline leaves a visible gap below text without shifting text quads");
            require(markup.formatDrawing(0,QColor("#2855b4"),6),"change underline thickness"); auto thick=markup.render(2);
            require(bluePixels(thick)>bluePixels(thin)*3,"selected thickness changes actual rendered line");
            auto copy=markup.copyAnnotation(0); require(std::abs(copy.strokeWidth-6)<0.01 && !copy.quads.isEmpty(),"underline stores width and selected text geometry");
            require(markup.drawingAt({copy.quads[0].x3+2,copy.quads[0].y3})==0,"underline can be selected or erased by its line");
            require(markup.undo() && markup.render(2)==thin && markup.redo() && markup.render(2)==thick,"underline format undo and redo");
            require(markup.recolorAnnotation(0,QColor("#cc2244")),"underline recolor succeeds");
            require(markup.annotationColor(0).red()>180,"underline stores recolored value");
            require(markup.copyAnnotation(0).strokeWidth==6,"underline recolor retains width");
            require(markup.highlight(10,18) && markup.annotationCount()==2 && markup.annotationContents(1).isEmpty(),"highlight creates one markup annotation without a comment");
            auto entries=markup.documentAnnotations(); require(entries[0].text.contains("Underline") && !entries[1].text.isEmpty(),"markup excerpts remain available to panel and export");
            auto revision=markup.revision(); require(!markup.underline(0,8,Qt::blue,std::numeric_limits<double>::quiet_NaN()) && markup.revision()==revision,"invalid underline width leaves history unchanged");
            const QString path=directory+"/text-markup.pdf"; require(markup.saveCopy(path),"save markup fixture"); auto image=markup.render(2);
            PdfDocument reopened; require(reopened.open(path) && reopened.render(2)==image && reopened.copyAnnotation(0).strokeWidth==6,"markup appearance and thickness survive reopening");
            auto raw=FPDF_LoadDocument(QFile::encodeName(path).constData(),nullptr); auto page=FPDF_LoadPage(raw,0);
            require(page && FPDFPage_GetAnnotCount(page)==2,"PDF contains only the two markup annotations");
            for(int i=0;i<2;++i) { auto annot=FPDFPage_GetAnnot(page,i); require(!FPDFAnnot_HasKey(annot,"Contents") && !FPDFAnnot_HasKey(annot,"Popup"),"saved markup has neither comment body nor popup"); FPDFPage_CloseAnnot(annot); }
            FPDF_ClosePage(page); FPDF_CloseDocument(raw);
            require(reopened.setPage(1) && reopened.pasteAnnotation(copy,{100,600}) && reopened.copyAnnotation(0).strokeWidth==6 && reopened.annotationType(0)==FPDF_ANNOT_UNDERLINE,"underline copy and paste preserves thickness on another page");
        }
        std::cout << "PASS: annotation creation, editing, colors, deletion, failure rollback and persistence.\n";
        return 0;
    } catch(const std::exception& e) { std::cerr << "FAIL: " << e.what() << '\n'; return 1; }
}
