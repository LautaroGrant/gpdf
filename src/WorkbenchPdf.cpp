#include "WorkbenchPdf.h"
#include "OcrService.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QSaveFile>
#include <QTemporaryDir>
#include <QEventLoop>
#include <QRegularExpression>
#include <QSet>
#include <fpdf_edit.h>
#include <fpdf_ppo.h>
#include <cmath>

WorkbenchPdf::~WorkbenchPdf() {
    std::lock_guard lock(PdfDocument::apiMutex());
    if(font_) FPDFFont_Close(font_);
}
bool WorkbenchPdf::canAssemble(const PdfDocument& source) {
    std::lock_guard lock(PdfDocument::apiMutex());
    return source.doc_ && (FPDF_GetSecurityHandlerRevision(source.doc_)<0 || (FPDF_GetDocPermissions(source.doc_)&((1u<<10)|(1u<<3))));
}
bool WorkbenchPdf::canModify(const PdfDocument& source) {
    std::lock_guard lock(PdfDocument::apiMutex());
    return source.doc_ && (FPDF_GetSecurityHandlerRevision(source.doc_)<0 || (FPDF_GetDocPermissions(source.doc_)&(1u<<3)));
}
bool WorkbenchPdf::parsePages(const QString& text,int count,QVector<int>& pages,QString& error) {
    pages.clear(); error.clear();
    const QRegularExpression range("^\\s*(\\d+)\\s*(?:-\\s*(\\d+)\\s*)?$");
    QSet<int> seen;
    for(const auto& part:text.split(',')) {
        const auto match=range.match(part);
        bool firstOk=false,lastOk=false;
        const int first=match.captured(1).toInt(&firstOk);
        const int last=match.captured(2).isEmpty() ? first : match.captured(2).toInt(&lastOk);
        if(!match.hasMatch() || !firstOk || (!match.captured(2).isEmpty() && !lastOk) || first<1 || last<1 || first>count || last>count) {
            error=workbenchTr("Ingresar páginas válidas, por ejemplo: 1, 3-5.","Enter valid pages, for example: 1, 3-5."); pages.clear(); return false;
        }
        const int step=first<=last ? 1 : -1;
        for(int p=first;;p+=step) {
            if(seen.contains(p)) { error=workbenchTr("Una página aparece más de una vez.","A page appears more than once."); pages.clear(); return false; }
            seen.insert(p); pages.append(p-1); if(p==last) break;
        }
    }
    return !pages.isEmpty();
}
bool WorkbenchPdf::appendPage(const PdfDocument& source,int page,int turns) {
    std::lock_guard lock(PdfDocument::apiMutex());
    if(!canAssemble(source)) return fail(workbenchTr("El PDF no permite reorganizar sus páginas.","The PDF does not allow page assembly."));
    if(page<0 || page>=source.pageCount()) return fail(workbenchTr("Página fuera de rango.","Page out of range."));
    if(!output_.doc_) {output_.doc_=FPDF_CreateNewDocument();output_.index_=-1;}
    const int index=output_.pageCount();
    if(!output_.doc_ || !FPDF_ImportPagesByIndex(output_.doc_,source.doc_,&page,1,index))
        return fail(workbenchTr("No se pudo importar la página.","Could not import the page."));
    if(turns%4) {
        auto imported=FPDF_LoadPage(output_.doc_,index);
        if(!imported) return fail(workbenchTr("No se pudo girar la página.","Could not rotate the page."));
        FPDFPage_SetRotation(imported,(FPDFPage_GetRotation(imported)+turns%4+4)%4);
        FPDF_ClosePage(imported);
    }
    return true;
}
bool WorkbenchPdf::addOcrText(int index,const QVector<PdfDocument::OcrWord>& words) {
    std::lock_guard lock(PdfDocument::apiMutex());
    if(words.isEmpty()) return true;
    if(!font_) {
        const auto path=qEnvironmentVariable("WINDIR","C:/Windows")+"/Fonts/arial.ttf";
        QFile file(path);
        if(!file.open(QIODevice::ReadOnly)) return fail(file.errorString());
        const auto bytes=file.readAll(); rawFont_=QRawFont(bytes,10);
        font_=FPDFText_LoadFont(output_.doc_,reinterpret_cast<const unsigned char*>(bytes.constData()),uint32_t(bytes.size()),FPDF_FONT_TRUETYPE,true);
        if(!font_) return fail(workbenchTr("No se pudo incorporar la fuente para OCR.","Could not embed the OCR font."));
    }
    auto page=FPDF_LoadPage(output_.doc_,index);
    if(!page) return fail(workbenchTr("No se pudo cargar la página.","Could not load the page."));
    std::unique_ptr<std::remove_pointer_t<FPDF_PAGE>,decltype(&FPDF_ClosePage)> held(page,&FPDF_ClosePage);
    const auto display=output_.pageSize(index);
    int length=0;
    for(const auto& word:words) {
        if(word.text.isEmpty() || word.rect.isEmpty()) continue;
        length+=word.text.size();
        if(length>100000) return fail(workbenchTr("La página supera el límite de texto OCR.","Page exceeds the OCR text limit."));
        if(rawFont_.glyphIndexesForString(word.text).contains(0))
            return fail(workbenchTr("La fuente OCR no admite algunos caracteres reconocidos.","The OCR font does not support some recognized characters."));
        auto text=FPDFPageObj_CreateTextObj(output_.doc_,font_,10);
        if(!text) return fail(workbenchTr("No se pudo crear la capa OCR.","Could not create the OCR layer."));
        bool ok=FPDFText_SetText(text,reinterpret_cast<FPDF_WIDESTRING>(word.text.utf16())) && FPDFTextObj_SetTextRenderMode(text,FPDF_TEXTRENDERMODE_INVISIBLE);
        float left=0,bottom=0,right=0,top=0;
        ok=ok && FPDFPageObj_GetBounds(text,&left,&bottom,&right,&top) && right>left && top>bottom;
        if(!ok) { FPDFPageObj_Destroy(text); return fail(workbenchTr("No se pudo posicionar el texto OCR.","Could not position OCR text.")); }
        // OCR rectangles are in displayed page coordinates, bottom-left origin.
        // Map a complete basis so rotated/cropped pages keep aligned text.
        const auto map=[&](QPointF p){return output_.normalizedToPage(index,{p.x()/display.width(),1-p.y()/display.height()});};
        const auto origin=map(word.rect.topLeft());
        const auto x=(map(word.rect.topRight())-origin)/(right-left);
        const auto y=(map(word.rect.bottomLeft())-origin)/(top-bottom);
        const auto translation=origin-x*left-y*bottom;
        FPDFPageObj_Transform(text,x.x(),x.y(),y.x(),y.y(),translation.x(),translation.y());
        FPDFPage_InsertObject(page,text);
    }
    if(!FPDFPage_GenerateContent(page)) return fail(workbenchTr("No se pudo guardar la capa OCR.","Could not generate the OCR layer."));
    return true;
}
bool WorkbenchPdf::save(const QString& path) {
    if(output_.pageCount()<1) return fail(workbenchTr("El resultado debe contener al menos una página.","The result must contain at least one page."));
    if(!output_.saveCopy(path)) return fail(output_.error());
    return true;
}

QString WorkbenchExport::run(const QVector<WorkbenchOutput>& outputs,const std::atomic_bool& cancelled,
                             std::function<void(int,int)> progress,Recognizer recognize) {
    const auto stopped=[] { return workbenchTr("Operación cancelada.","Operation cancelled."); };
    if(outputs.isEmpty()) return workbenchTr("No hay archivos para guardar.","There are no files to save.");
    QSet<QString> targets;
    int total=0,done=0;
    const auto identity=[](const QString& path){const QFileInfo file(path);return (file.exists() ? file.canonicalFilePath() : file.absoluteFilePath()).toCaseFolded();};
    QSet<QString> sources;
    for(const auto& output:outputs) for(const auto& page:output.pages) if(!page.sourcePath.isEmpty()) sources.insert(identity(page.sourcePath));
    for(const auto& output:outputs) {
        const auto target=identity(output.path);
        if(output.path.isEmpty() || output.pages.isEmpty() || targets.contains(target) || sources.contains(target))
            return workbenchTr("Elegir archivos de salida distintos de los originales y con al menos una página.","Choose distinct output files, different from the originals, with at least one page.");
        if(QFileInfo::exists(output.path) && !output.overwrite)
            return workbenchTr("Ya existe un archivo de salida: ","An output file already exists: ")+output.path;
        if(outputs.size()>1 && output.overwrite)
            return workbenchTr("La separación requiere nombres de archivo nuevos.","Splitting requires new file names.");
        targets.insert(target); total+=output.pages.size();
    }
    if(!recognize) recognize=[&](const QImage& image,QSizeF size,QVector<PdfDocument::OcrWord>& words,QString& error){
        OcrService service; QEventLoop loop; QTimer poll; bool finished=false;
        QObject::connect(&poll,&QTimer::timeout,&loop,[&]{if(cancelled){service.cancel();error=stopped();finished=true;loop.quit();}});
        poll.start(40);
        service.recognize(image,size,[&](auto result,auto message){words=std::move(result);error=message;finished=true;loop.quit();});
        if(!finished) loop.exec();
        return error.isEmpty() && !cancelled;
    };
    // Stage every result before publishing any. Failed/cancelled splits leave no
    // partial outputs. Existing single-file results use QSaveFile replacement.
    std::vector<std::unique_ptr<QTemporaryDir>> staging;
    QStringList staged;
    for(const auto& output:outputs) {
        if(cancelled) return stopped();
        auto directory=std::make_unique<QTemporaryDir>(QFileInfo(output.path).absolutePath()+"/.gpdf-workbench-XXXXXX");
        if(!directory->isValid()) return directory->errorString();
        const auto path=directory->filePath("result.pdf");
        WorkbenchPdf result;
        for(const auto& ref:output.pages) {
            if(cancelled) return stopped();
            if(!ref.document) return workbenchTr("Falta un documento de origen.","A source document is missing.");
            if(!result.appendPage(*ref.document,ref.page,ref.quarterTurns)) return result.error();
            if(output.ocr) {
                if(!WorkbenchPdf::canModify(*ref.document)) return workbenchTr("El PDF no permite incorporar texto OCR.","The PDF does not allow adding OCR text.");
                if(ref.document->pageText(ref.page).trimmed().isEmpty() || ref.document->pageNeedsOcr(ref.page)) {
                    const int outputPage=result.document().pageCount()-1;
                    const auto size=result.document().pageSize(outputPage);
                    const double scale=std::min(3.0,2400.0/std::max(size.width(),size.height()));
                    const auto image=result.document().renderPage(outputPage,scale);
                    QVector<PdfDocument::OcrWord> words; QString error;
                    if(!recognize(image,size,words,error)) return error.isEmpty() ? stopped() : error;
                    if(!result.addOcrText(result.document().pageCount()-1,words)) return result.error();
                }
            }
            if(progress) progress(++done,total);
        }
        if(cancelled) return stopped();
        if(!result.save(path)) return result.error();
        staged.append(path); staging.push_back(std::move(directory));
    }
    QStringList published;
    for(int i=0;i<outputs.size();++i) {
        QString error;
        if(cancelled) error=stopped();
        else if(outputs[i].overwrite) {
            QFile input(staged[i]); QSaveFile target(outputs[i].path);
            if(!input.open(QIODevice::ReadOnly) || !target.open(QIODevice::WriteOnly)) error=workbenchTr("No se pudo abrir el archivo de salida.","Could not open the output file.");
            else {
                while(!input.atEnd() && error.isEmpty()) {
                    if(cancelled) { error=stopped(); break; }
                    auto bytes=input.read(1024*1024);
                    if(bytes.isEmpty() || target.write(bytes)!=bytes.size()) error=workbenchTr("No se pudo escribir el archivo de salida.","Could not write the output file.");
                }
                if(error.isEmpty() && !target.commit()) error=target.errorString();
            }
        } else if(!QFile::rename(staged[i],outputs[i].path)) error=workbenchTr("No se pudo crear el archivo de salida: ","Could not create the output file: ")+outputs[i].path;
        if(!error.isEmpty()) { for(const auto& path:published) QFile::remove(path); return error; }
        published.append(outputs[i].path);
    }
    return {};
}
